#include "platform/shared/accessibility.h"
#include "accessibility_protocol.h"
#include "accessibility_text_linux.h"
#include "internal/unicode.h"
#include "oneui/controls/text_field.h"
#include "oneui/platform/window.h"
#include <pango/pango.h>
#include <gio/gio.h>
#include <atspi/atspi-constants.h>
#include <algorithm>
#include <charconv>
#include <climits>
#include <cmath>
#include <cstring>
#include <clocale>
#include <set>

namespace oneui::platform {
namespace {
constexpr auto base = "/org/a11y/atspi/accessible";
constexpr auto rootPath = "/org/a11y/atspi/accessible/root";
constexpr auto nullPath = "/org/a11y/atspi/null";
constexpr auto accessible = "org.a11y.atspi.Accessible";
constexpr auto application = "org.a11y.atspi.Application";
constexpr auto action = "org.a11y.atspi.Action";
constexpr auto component = "org.a11y.atspi.Component";
constexpr auto textInterface = "org.a11y.atspi.Text";
constexpr auto editableText = "org.a11y.atspi.EditableText";
constexpr auto valueInterface = "org.a11y.atspi.Value";
// AT-SPI offsets count Unicode scalars, not UTF-8 bytes or UTF-16 units.
static_assert(sizeof(wchar_t) == 4);
using Key = std::pair<std::uint64_t, std::uint64_t>;
std::string path(Key k) {
    return k.first ? std::string(base) + "/w" + std::to_string(k.first) + "_" + std::to_string(k.second) : rootPath;
}
bool parse(const char* p, Key& k) {
    if (std::strcmp(p, rootPath) == 0) { k = {}; return true; }
    const std::string prefix = std::string(base) + "/w";
    if (std::strncmp(p, prefix.c_str(), prefix.size())) return false;
    const char* end = p + std::strlen(p);
    auto first = std::from_chars(p + prefix.size(), end, k.first);
    if (first.ec != std::errc{} || first.ptr == end || *first.ptr != '_' || !k.first) return false;
    auto second = std::from_chars(first.ptr + 1, end, k.second);
    return second.ec == std::errc{} && second.ptr == end;
}
unsigned role(const AccessibleNode& n) {
    if (n.sensitive) return ATSPI_ROLE_PASSWORD_TEXT;
    switch (n.info.role) {
    case AccessibilityRole::Button: return ATSPI_ROLE_PUSH_BUTTON;
    case AccessibilityRole::Text: return ATSPI_ROLE_LABEL;
    case AccessibilityRole::TextBox: return ATSPI_ROLE_ENTRY;
    case AccessibilityRole::CheckBox: return ATSPI_ROLE_CHECK_BOX;
    case AccessibilityRole::RadioButton: return ATSPI_ROLE_RADIO_BUTTON;
    case AccessibilityRole::RadioGroup: return ATSPI_ROLE_GROUPING;
    case AccessibilityRole::ComboBox: return ATSPI_ROLE_COMBO_BOX;
    case AccessibilityRole::Slider: return ATSPI_ROLE_SLIDER;
    case AccessibilityRole::ProgressBar: return ATSPI_ROLE_PROGRESS_BAR;
    case AccessibilityRole::Tab: return ATSPI_ROLE_PAGE_TAB;
    case AccessibilityRole::TabList: return ATSPI_ROLE_PAGE_TAB_LIST;
    case AccessibilityRole::List: return ATSPI_ROLE_LIST;
    case AccessibilityRole::ListItem: return ATSPI_ROLE_LIST_ITEM;
    case AccessibilityRole::Table: return ATSPI_ROLE_TABLE;
    case AccessibilityRole::Row: return ATSPI_ROLE_TABLE_ROW;
    case AccessibilityRole::Cell: return ATSPI_ROLE_TABLE_CELL;
    case AccessibilityRole::Popup: return ATSPI_ROLE_DIALOG;
    case AccessibilityRole::Window: return ATSPI_ROLE_FRAME;
    default: return ATSPI_ROLE_PANEL;
    }
}
const char* roleName(unsigned r) {
    switch (r) {
    case ATSPI_ROLE_APPLICATION: return "application";
    case ATSPI_ROLE_PUSH_BUTTON: return "push button";
    case ATSPI_ROLE_LABEL: return "label";
    case ATSPI_ROLE_ENTRY: return "entry";
    case ATSPI_ROLE_PASSWORD_TEXT: return "password text";
    case ATSPI_ROLE_CHECK_BOX: return "check box";
    case ATSPI_ROLE_RADIO_BUTTON: return "radio button";
    case ATSPI_ROLE_GROUPING: return "grouping";
    case ATSPI_ROLE_COMBO_BOX: return "combo box";
    case ATSPI_ROLE_SLIDER: return "slider";
    case ATSPI_ROLE_PROGRESS_BAR: return "progress bar";
    case ATSPI_ROLE_PAGE_TAB: return "page tab";
    case ATSPI_ROLE_PAGE_TAB_LIST: return "page tab list";
    case ATSPI_ROLE_LIST: return "list";
    case ATSPI_ROLE_LIST_ITEM: return "list item";
    case ATSPI_ROLE_TABLE: return "table";
    case ATSPI_ROLE_TABLE_ROW: return "table row";
    case ATSPI_ROLE_TABLE_CELL: return "table cell";
    case ATSPI_ROLE_DIALOG: return "dialog";
    case ATSPI_ROLE_FRAME: return "frame";
    default: return "panel";
    }
}
struct Entry { std::shared_ptr<AccessibilityContext> context; AccessibleNode node; std::size_t order = 0; };
// One application connection, shared by all native windows. GDBus dispatches on
// the same GLib main context already pumped by both desktop backends.
class Service : public std::enable_shared_from_this<Service> {
public:
    GDBusConnection* bus = nullptr;
    GDBusNodeInfo* protocol = g_dbus_node_info_new_for_xml(accessibilityProtocol, nullptr);
    guint registration = 0;
    int appId = 0;
    std::map<std::uint64_t, std::shared_ptr<AccessibilityContext>> windows;
    std::map<Key, AccessibleNode> previous;
    std::map<Key, std::vector<Key>> previousChildren;
    bool connecting = false;
    gint64 retryAfter = 0;
    ~Service() {
        if (bus) {
            if (registration) g_dbus_connection_unregister_subtree(bus, registration);
            g_dbus_connection_close(bus, nullptr, nullptr, nullptr);
            g_object_unref(bus);
        }
        g_dbus_node_info_unref(protocol);
    }
    using Hold = std::shared_ptr<Service>;
    static Hold take(gpointer p) { std::unique_ptr<Hold> h(static_cast<Hold*>(p)); return *h; }
    void connect() {
        if (bus && g_dbus_connection_is_closed(bus)) {
            g_dbus_connection_unregister_subtree(bus, registration);
            g_clear_object(&bus); registration = 0; previous.clear(); previousChildren.clear();
        }
        if (bus || connecting || windows.empty() || g_get_monotonic_time() < retryAfter) return;
        const char* disabled = g_getenv("NO_AT_BRIDGE");
        if (disabled && std::strcmp(disabled, "1") == 0) return;
        connecting = true;
        retryAfter = g_get_monotonic_time() + 5000000;
        g_bus_get(G_BUS_TYPE_SESSION, nullptr, [](GObject*, GAsyncResult* result, gpointer data) {
            auto s = take(data);
            GError* error = nullptr;
            auto* session = g_bus_get_finish(result, &error);
            if (!session) { g_clear_error(&error); s->connecting = false; return; }
            if (s->windows.empty()) { g_object_unref(session); s->connecting = false; return; }
            g_dbus_connection_call(session, "org.a11y.Bus", "/org/a11y/bus", "org.a11y.Bus", "GetAddress",
                nullptr, G_VARIANT_TYPE("(s)"), G_DBUS_CALL_FLAGS_NONE, 1500, nullptr,
                [](GObject* source, GAsyncResult* result, gpointer data) {
                    auto s = take(data); GError* error = nullptr;
                    auto* reply = g_dbus_connection_call_finish(G_DBUS_CONNECTION(source), result, &error);
                    if (!reply) { g_clear_error(&error); s->connecting = false; return; }
                    if (s->windows.empty()) { g_variant_unref(reply); s->connecting = false; return; }
                    const char* address = nullptr; g_variant_get(reply, "(&s)", &address);
                    g_dbus_connection_new_for_address(address,
                        GDBusConnectionFlags(G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT | G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION),
                        nullptr, nullptr, [](GObject*, GAsyncResult* result, gpointer data) {
                            auto s = take(data); GError* error = nullptr;
                            s->bus = g_dbus_connection_new_for_address_finish(result, &error);
                            s->connecting = false;
                            if (!s->bus) { g_clear_error(&error); return; }
                            g_dbus_connection_set_exit_on_close(s->bus, FALSE);
                            s->registration = g_dbus_connection_register_subtree(s->bus, base, &subtree,
                                G_DBUS_SUBTREE_FLAGS_DISPATCH_TO_UNENUMERATED_NODES, s.get(), nullptr, &error);
                            if (!s->registration) { g_clear_error(&error); g_clear_object(&s->bus); return; }
                            // Embed must be asynchronous: the registry writes Application.Id
                            // back to us before replying. A synchronous call deadlocks the UI.
                            g_dbus_connection_call(s->bus, "org.a11y.atspi.Registry", rootPath,
                                "org.a11y.atspi.Socket", "Embed", g_variant_new("((so))", s->name(), rootPath),
                                nullptr, G_DBUS_CALL_FLAGS_NONE, 2000, nullptr,
                                [](GObject* source, GAsyncResult* result, gpointer data) {
                                    auto s = take(data); GError* error = nullptr;
                                    auto* reply = g_dbus_connection_call_finish(G_DBUS_CONNECTION(source), result, &error);
                                    if (reply) g_variant_unref(reply);
                                    else { g_dbus_connection_close(s->bus, nullptr, nullptr, nullptr); g_clear_error(&error); }
                                }, new Hold(s));
                            s->sync();
                        }, new Hold(s));
                    g_variant_unref(reply);
                }, new Hold(s));
            g_object_unref(session);
        }, new Hold(shared_from_this()));
    }
    const char* name() const { return g_dbus_connection_get_unique_name(bus); }
    GVariant* reference(Key k) const { return g_variant_new("(so)", name(), path(k).c_str()); }
    GVariant* nullReference() const { return g_variant_new("(so)", "", nullPath); }
    std::map<Key, Entry> tree() {
        std::map<Key, Entry> t;
        AccessibleNode app; app.info.name = L"OneUI";
        t[{}] = {nullptr, app};
        for (const auto& [id, c] : windows) {
            if (!c->valid() || !c->exposed()) continue;
            AccessibleNode window; window.info.name = c->title(); window.info.role = AccessibilityRole::Window;
            t[{id, 0}] = {c, window};
            std::size_t order = 0;
            for (auto& n : c->snapshot()) t[{id, n.id}] = {c, n, order++};
        }
        return t;
    }
    static Key parent(Key k, const AccessibleNode& n) { return k.second ? Key{k.first, n.parent} : Key{}; }
    static std::vector<Key> children(Key k, const std::map<Key, Entry>& t) {
        std::vector<Key> out;
        for (const auto& [key, e] : t) if (key != Key{} && parent(key, e.node) == k) out.push_back(key);
        if (k.first) std::stable_sort(out.begin(), out.end(), [&](Key a, Key b) { return t.at(a).order < t.at(b).order; });
        return out;
    }
    static std::vector<const char*> interfaces(Key k, const AccessibleNode& n) {
        std::vector<const char*> v{accessible};
        if (!k.first) v.push_back(application);
        if (k.second) {
            v.push_back(component);
            if (AccessibilityContext::pressable(n.info.role)) v.push_back(action);
            if (n.text) { v.push_back(textInterface); v.push_back(editableText); }
            if (n.number) v.push_back(valueInterface);
        }
        return v;
    }
    void event(Key k, const char* signal, const char* detail, int number, GVariant* value, int second = 0) {
        g_dbus_connection_emit_signal(bus, nullptr, path(k).c_str(), "org.a11y.atspi.Event.Object", signal,
            g_variant_new("(siiv@a{sv})", detail, number, second, value,
                          g_variant_new_array(G_VARIANT_TYPE("{sv}"), nullptr, 0)), nullptr);
    }
    void sync() {
        connect();
        if (!bus || !registration) return;
        const auto current = tree();
        std::map<Key, std::vector<Key>> hierarchy;
        for (const auto& [k, e] : current) {
            hierarchy.try_emplace(k);
            if (k.first) hierarchy[parent(k, e.node)].push_back(k);
        }
        for (auto& [k, list] : hierarchy) if (k.first)
            std::stable_sort(list.begin(), list.end(), [&](Key a, Key b) { return current.at(a).order < current.at(b).order; });
        // Readers may cache children. Explicit remove/add sequences also cover
        // reparenting and reordering, while preserving each object's identity.
        for (const auto& [p, list] : previousChildren) {
            const auto found = hierarchy.find(p);
            if (found != hierarchy.end() && found->second == list) continue;
            for (std::size_t i = list.size(); i > 0; --i)
                event(p, "ChildrenChanged", "remove", int(i - 1), reference(list[i - 1]));
        }
        for (const auto& [p, list] : hierarchy) {
            const auto found = previousChildren.find(p);
            if (found != previousChildren.end() && found->second == list) continue;
            for (std::size_t i = 0; i < list.size(); ++i)
                event(p, "ChildrenChanged", "add", int(i), reference(list[i]));
        }
        previousChildren = std::move(hierarchy);
        for (const auto& [k, n] : previous) if (k.first && !current.count(k)) {
            event(k, "StateChanged", "defunct", 1, g_variant_new_int32(0));
        }
        for (const auto& [k, e] : current) {
            const auto old = previous.find(k);
            const auto& n = e.node;
            if (old == previous.end()) {
                if (n.info.state.focused) event(k, "StateChanged", "focused", 1, g_variant_new_int32(0));
            } else {
                const auto& before = old->second.info;
                if (role(old->second) != role(n)) event(k, "PropertyChange", "accessible-role", 0, g_variant_new_uint32(role(n)));
                if (before.name != n.info.name) event(k, "PropertyChange", "accessible-name", 0,
                    g_variant_new_string(unicode::toUtf8(n.info.name).c_str()));
                if (before.description != n.info.description) event(k, "PropertyChange", "accessible-description", 0,
                    g_variant_new_string(unicode::toUtf8(n.info.description).c_str()));
                const Key eventKey = k;
                const auto state = [&](bool a, bool b, const char* key) {
                    if (a != b) event(eventKey, "StateChanged", key, b, g_variant_new_int32(0));
                };
                state(before.state.focused, n.info.state.focused, "focused");
                state(!before.state.disabled, !n.info.state.disabled, "enabled");
                state(!before.state.disabled, !n.info.state.disabled, "sensitive");
                state(before.state.checked, n.info.state.checked, "checked");
                state(before.state.selected, n.info.state.selected, "selected");
                state(before.state.expanded, n.info.state.expanded, "expanded");
                state(before.state.pressed, n.info.state.pressed, "pressed");
                state(before.state.readOnly, n.info.state.readOnly, "read-only");
                state(before.state.required, n.info.state.required, "required");
                state(before.state.invalid, n.info.state.invalid, "invalid-entry");
                if (before.value != n.info.value) event(k, "VisibleDataChanged", "", 0, g_variant_new_int32(0));
                if (n.number && (!old->second.number || old->second.number->value != n.number->value))
                    event(k, "PropertyChange", "accessible-value", 0, g_variant_new_double(n.number->value));
                if (n.text && old->second.text) {
                    const auto& a = *old->second.text; const auto& b = *n.text;
                    if (a.value != b.value) {
                        std::size_t prefix = 0, suffix = 0;
                        while (prefix < std::min(a.value.size(), b.value.size()) && a.value[prefix] == b.value[prefix]) ++prefix;
                        while (suffix < std::min(a.value.size(), b.value.size()) - prefix &&
                               a.value[a.value.size() - suffix - 1] == b.value[b.value.size() - suffix - 1]) ++suffix;
                        const auto removed = a.value.substr(prefix, a.value.size() - prefix - suffix);
                        const auto inserted = b.value.substr(prefix, b.value.size() - prefix - suffix);
                        if (!removed.empty()) event(k, "TextChanged", "delete", int(prefix),
                            g_variant_new_string(unicode::toUtf8(removed).c_str()), int(removed.size()));
                        if (!inserted.empty()) event(k, "TextChanged", "insert", int(prefix),
                            g_variant_new_string(unicode::toUtf8(inserted).c_str()), int(inserted.size()));
                    }
                    if (a.caret != b.caret) event(k, "TextCaretMoved", "", int(b.caret), g_variant_new_int32(0));
                    if (a.start != b.start || a.end != b.end) event(k, "TextSelectionChanged", "", 0, g_variant_new_int32(0));
                }
            }
        }
        previous.clear();
        for (const auto& [k, e] : current) previous.emplace(k, e.node);
    }
    static GDBusInterfaceInfo** introspect(GDBusConnection*, const gchar*, const gchar*, const gchar* node, gpointer data) {
        auto& s = *static_cast<Service*>(data);
        Key k; const auto p = std::string(base) + "/" + (node ? node : "");
        auto t = s.tree();
        if (!parse(p.c_str(), k) || !t.count(k)) return nullptr;
        auto list = interfaces(k, t.at(k).node);
        auto** out = g_new0(GDBusInterfaceInfo*, list.size() + 1);
        for (std::size_t i = 0; i < list.size(); ++i)
            out[i] = g_dbus_interface_info_ref(g_dbus_node_info_lookup_interface(s.protocol, list[i]));
        return out;
    }
    static GVariant* property(GDBusConnection*, const gchar*, const gchar* p, const gchar* iface, const gchar* prop, GError** error, gpointer data) {
        try { return readProperty(p, iface, prop, error, data); }
        catch (...) { g_set_error_literal(error, G_DBUS_ERROR, G_DBUS_ERROR_FAILED, "Accessibility query failed"); return nullptr; }
    }
    static GVariant* readProperty(const gchar* p, const gchar* iface, const gchar* prop, GError** error, gpointer data) {
        auto keep = static_cast<Service*>(data)->shared_from_this();
        auto t = keep->tree(); Key k;
        if (!parse(p, k) || !t.count(k)) { g_set_error_literal(error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_OBJECT, "Defunct accessible"); return nullptr; }
        const auto& n = t.at(k).node;
        const std::string field(prop);
        if (!std::strcmp(iface, textInterface) && n.text) {
            if (field == "CharacterCount") return g_variant_new_int32(n.text->value.size());
            if (field == "CaretOffset") return g_variant_new_int32(n.text->caret);
        }
        if (!std::strcmp(iface, valueInterface) && n.number) {
            if (field == "CurrentValue") return g_variant_new_double(n.number->value);
            if (field == "MinimumValue") return g_variant_new_double(n.number->minimum);
            if (field == "MaximumValue") return g_variant_new_double(n.number->maximum);
            if (field == "MinimumIncrement") return g_variant_new_double(n.number->step);
            if (field == "Text") return g_variant_new_string(unicode::toUtf8(n.info.value).c_str());
        }
        if (field == "Name") return g_variant_new_string(unicode::toUtf8(n.info.name).c_str());
        if (field == "Description" || field == "HelpText") return g_variant_new_string(unicode::toUtf8(n.info.description).c_str());
        if (field == "Parent") return k.first ? keep->reference(parent(k, n)) : g_variant_new("(so)", "org.a11y.atspi.Registry", rootPath);
        if (field == "ChildCount") return g_variant_new_int32(children(k, t).size());
        if (field == "NActions") return g_variant_new_int32(AccessibilityContext::pressable(n.info.role) ? 1 : 0);
        if (field == "Id") return g_variant_new_int32(keep->appId);
        if (field == "version" || field == "InterfaceVersion") return g_variant_new_uint32(1);
        if (field == "ToolkitName") return g_variant_new_string("OneUI");
        if (field == "Version" || field == "ToolkitVersion") return g_variant_new_string("0.1");
        if (field == "AtspiVersion") return g_variant_new_string("2.1");
        if (field == "Locale") return g_variant_new_string(setlocale(LC_MESSAGES, nullptr));
        if (field == "AccessibleId") return g_variant_new_string(p);
        g_set_error_literal(error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_PROPERTY, "Unknown property"); return nullptr;
    }
    static gboolean setProperty(GDBusConnection*, const gchar*, const gchar* p, const gchar* iface, const gchar* prop, GVariant* value, GError** error, gpointer data) {
        auto keep = static_cast<Service*>(data)->shared_from_this();
        bool ok = false;
        try { ok = writeProperty(p, iface, prop, value, data); }
        catch (...) {}
        if (!ok) g_set_error_literal(error, G_DBUS_ERROR, G_DBUS_ERROR_FAILED, "Value is not writable or is outside its range");
        if (keep->windows.empty()) {
            // GDBus queues Properties.Set's reply after this callback returns.
            g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, [](gpointer data) -> gboolean {
                auto keep = *static_cast<Hold*>(data); keep->flushReply(); return G_SOURCE_REMOVE;
            }, new Hold(keep), [](gpointer data) { delete static_cast<Hold*>(data); });
        }
        return ok;
    }
    static bool writeProperty(const gchar* p, const gchar* iface, const gchar* prop, GVariant* value, gpointer data) {
        if (!std::strcmp(p, rootPath) && !std::strcmp(iface, application) && !std::strcmp(prop, "Id")) {
            static_cast<Service*>(data)->appId = g_variant_get_int32(value); return TRUE;
        }
        if (!std::strcmp(iface, valueInterface) && !std::strcmp(prop, "CurrentValue")) {
            auto s = static_cast<Service*>(data)->shared_from_this(); Key k;
            if (!parse(p, k)) return false;
            auto t = s->tree(); const auto found = t.find(k);
            return found != t.end() && found->second.context && found->second.context->setNumber(k.second, g_variant_get_double(value));
        }
        return FALSE;
    }
    void flushReply() {
        if (!bus) return;
        g_dbus_connection_flush(bus, nullptr, [](GObject* source, GAsyncResult* result, gpointer data) {
            auto keep = take(data); GError* error = nullptr;
            g_dbus_connection_flush_finish(G_DBUS_CONNECTION(source), result, &error); g_clear_error(&error);
        }, new Hold(shared_from_this()));
    }
    static void method(GDBusConnection*, const gchar*, const gchar* p, const gchar* iface, const gchar* method,
                       GVariant* args, GDBusMethodInvocation* call, gpointer data) {
        auto keep = static_cast<Service*>(data)->shared_from_this();
        try { invoke(p, iface, method, args, call, data); }
        catch (...) {
            g_dbus_method_invocation_return_dbus_error(call, "org.freedesktop.DBus.Error.Failed", "Accessibility callback failed");
        }
        // Returning a value only queues it. Closing the last window from an
        // action must not tear down the connection before that reply is sent.
        // Flush asynchronously to keep a slow reader off the UI critical path.
        if (keep->windows.empty()) keep->flushReply();
    }
    static void invoke(const gchar* p, const gchar* iface, const gchar* method,
                       GVariant* args, GDBusMethodInvocation* call, gpointer data) {
        auto s = static_cast<Service*>(data)->shared_from_this(); // Actions may destroy the last window.
        auto t = s->tree(); Key k;
        if (!parse(p, k) || !t.count(k)) {
            g_dbus_method_invocation_return_dbus_error(call, "org.freedesktop.DBus.Error.UnknownObject", "Defunct accessible"); return;
        }
        const auto entry = t.at(k); const auto& n = entry.node;
        const std::string m(method), i(iface);
        GVariant* out = nullptr;
        auto tuple = [](GVariant* value) { return g_variant_new_tuple(&value, 1); };
        if (i == accessible) {
            if (m == "GetChildren") {
                GVariantBuilder b; g_variant_builder_init(&b, G_VARIANT_TYPE("a(so)"));
                for (auto child : children(k, t)) g_variant_builder_add_value(&b, s->reference(child));
                out = tuple(g_variant_builder_end(&b));
            } else if (m == "GetChildAtIndex") {
                int index; g_variant_get(args, "(i)", &index); const auto c = children(k, t);
                out = tuple(index >= 0 && std::size_t(index) < c.size() ? s->reference(c[index]) : s->nullReference());
            } else if (m == "GetIndexInParent") {
                const auto c = children(parent(k, n), t); const auto pos = std::find(c.begin(), c.end(), k);
                out = g_variant_new("(i)", pos == c.end() ? -1 : int(pos - c.begin()));
            } else if (m == "GetRole") out = g_variant_new("(u)", k.first ? role(n) : ATSPI_ROLE_APPLICATION);
            else if (m == "GetRoleName" || m == "GetLocalizedRoleName") {
                out = g_variant_new("(s)", roleName(k.first ? role(n) : ATSPI_ROLE_APPLICATION));
            } else if (m == "GetApplication") out = tuple(s->reference({}));
            else if (m == "GetRelationSet") out = tuple(g_variant_new_array(G_VARIANT_TYPE("(ua(so))"), nullptr, 0));
            else if (m == "GetInterfaces") {
                auto list = interfaces(k, n); out = tuple(g_variant_new_strv(list.data(), list.size()));
            } else if (m == "GetAttributes") {
                GVariantBuilder b; g_variant_builder_init(&b, G_VARIANT_TYPE("a{ss}"));
                // Safe descriptive value for widgets without a Text/Value interface.
                if (!n.info.value.empty()) g_variant_builder_add(&b, "{ss}", "valuetext", unicode::toUtf8(n.info.value).c_str());
                out = tuple(g_variant_builder_end(&b));
            } else if (m == "GetState") {
                std::uint64_t states = 0;
                auto set = [&](unsigned bit, bool on) { if (on) states |= std::uint64_t(1) << bit; };
                set(ATSPI_STATE_VISIBLE, true); set(ATSPI_STATE_SHOWING, true);
                set(ATSPI_STATE_ENABLED, !n.info.state.disabled); set(ATSPI_STATE_SENSITIVE, !n.info.state.disabled);
                set(ATSPI_STATE_FOCUSABLE, n.focusable); set(ATSPI_STATE_FOCUSED, n.info.state.focused);
                set(ATSPI_STATE_CHECKED, n.info.state.checked); set(ATSPI_STATE_SELECTED, n.info.state.selected);
                set(ATSPI_STATE_EXPANDED, n.info.state.expanded); set(ATSPI_STATE_PRESSED, n.info.state.pressed);
                set(ATSPI_STATE_REQUIRED, n.info.state.required); set(ATSPI_STATE_INVALID_ENTRY, n.info.state.invalid);
                set(ATSPI_STATE_READ_ONLY, n.info.state.readOnly);
                set(ATSPI_STATE_EDITABLE, n.text && !n.info.state.readOnly && !n.info.state.disabled);
                set(ATSPI_STATE_MULTI_LINE, n.text && n.text->multiline);
                set(ATSPI_STATE_SINGLE_LINE, n.text && !n.text->multiline);
                guint32 words[]{guint32(states), guint32(states >> 32)};
                out = tuple(g_variant_new_fixed_array(G_VARIANT_TYPE_UINT32, words, 2, sizeof(guint32)));
            }
        } else if (i == textInterface || i == editableText) {
            out = accessibleTextCall(i, m, args, entry.context, n);
        } else if (i == application) {
            out = g_variant_new("(s)", m == "GetLocale" ? setlocale(LC_MESSAGES, nullptr) : "");
        } else if (i == action) {
            int index = -1; if (g_variant_n_children(args)) g_variant_get(args, "(i)", &index);
            if (m == "DoAction") out = g_variant_new("(b)", index == 0 && entry.context->press(k.second));
            else if (m == "GetActions") {
                GVariantBuilder b; g_variant_builder_init(&b, G_VARIANT_TYPE("a(sss)"));
                g_variant_builder_add(&b, "(sss)", "click", "Activate", ""); out = tuple(g_variant_builder_end(&b));
            } else out = g_variant_new("(s)", index == 0 && m != "GetKeyBinding" ? "click" : "");
        } else if (i == component) {
            unsigned coords = ATSPI_COORD_TYPE_WINDOW; int x = 0, y = 0;
            if (m == "GetExtents" || m == "GetPosition") g_variant_get(args, "(u)", &coords);
            if (m == "Contains" || m == "GetAccessibleAtPoint") g_variant_get(args, "(iiu)", &x, &y, &coords);
            auto rect = entry.context->bounds(n.frame, coords == ATSPI_COORD_TYPE_SCREEN);
            if (coords == ATSPI_COORD_TYPE_PARENT && n.parent) {
                auto p = t.find({k.first, n.parent});
                if (p != t.end() && rect) {
                    auto origin = entry.context->bounds(p->second.node.frame, false);
                    if (origin) { rect->x -= origin->x; rect->y -= origin->y; }
                }
            } else if (coords > ATSPI_COORD_TYPE_PARENT) rect.reset();
            const int rx = rect ? int(std::floor(rect->x)) : INT_MIN, ry = rect ? int(std::floor(rect->y)) : INT_MIN;
            const int w = rect ? int(std::ceil(rect->width)) : 0, h = rect ? int(std::ceil(rect->height)) : 0;
            if (m == "GetExtents") out = g_variant_new("((iiii))", rx, ry, w, h);
            else if (m == "GetPosition") out = g_variant_new("(ii)", rx, ry);
            else if (m == "GetSize") out = g_variant_new("(ii)", w, h);
            else if (m == "Contains" || m == "GetAccessibleAtPoint") {
                const bool inside = rect && x >= rx && y >= ry && x < double(rx) + w && y < double(ry) + h;
                out = m == "Contains" ? g_variant_new("(b)", inside) : tuple(inside ? s->reference(k) : s->nullReference());
            } else if (m == "GetLayer") out = g_variant_new("(u)", ATSPI_LAYER_WIDGET);
            else if (m == "GetMDIZOrder") out = g_variant_new("(n)", 0);
            else if (m == "GetAlpha") out = g_variant_new("(d)", 1.0);
            else if (m == "GrabFocus") out = g_variant_new("(b)", entry.context->focusNode(k.second));
            else out = g_variant_new("(b)", FALSE); // Geometry changes/scrolling are not implemented.
        }
        if (out) g_dbus_method_invocation_return_value(call, out);
        else g_dbus_method_invocation_return_dbus_error(call, "org.freedesktop.DBus.Error.UnknownMethod", "Unsupported method");
    }
    static const GDBusInterfaceVTable vtable;
    static const GDBusSubtreeVTable subtree;
};
const GDBusInterfaceVTable Service::vtable{Service::method, Service::property, Service::setProperty, {}};
const GDBusSubtreeVTable Service::subtree{
    [](GDBusConnection*, const gchar*, const gchar*, gpointer) -> gchar** { return g_new0(gchar*, 1); },
    Service::introspect,
    [](GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar*, gpointer* out, gpointer data) -> const GDBusInterfaceVTable* {
        *out = data; return &Service::vtable;
    }, {}};
class LinuxAccessibility final : public NativeAccessibility {
    std::shared_ptr<Service> service_;
    std::uint64_t id_;
public:
    explicit LinuxAccessibility(std::shared_ptr<AccessibilityContext> context) {
        static std::weak_ptr<Service> shared;
        static std::uint64_t next = 1;
        service_ = shared.lock();
        if (!service_) shared = service_ = std::make_shared<Service>();
        id_ = next++;
        service_->windows.emplace(id_, std::move(context));
        service_->connect();
    }
    ~LinuxAccessibility() override { service_->windows.erase(id_); service_->sync(); }
    void sync() override { service_->sync(); }
};
} // namespace
std::unique_ptr<NativeAccessibility> makeNativeAccessibility(std::shared_ptr<AccessibilityContext> context, void*) {
    return std::make_unique<LinuxAccessibility>(std::move(context));
}
} // namespace oneui::platform
