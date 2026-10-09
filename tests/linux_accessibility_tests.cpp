#include "platform/shared/desktop_window.h"
#include "oneui/controls/button.h"
#include "oneui/controls/text_field.h"
#include "oneui/controls/slider.h"
#include "oneui/controls/progress_bar.h"
#include <gio/gio.h>
#include <atspi/atspi-constants.h>
#include <atomic>
#include <chrono>
#include <cstring>
#include <future>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>

namespace {
constexpr auto rootPath = "/org/a11y/atspi/accessible/root";
constexpr auto accessible = "org.a11y.atspi.Accessible";
struct Ref { std::string bus, path; };
using Variant = std::unique_ptr<GVariant, decltype(&g_variant_unref)>;
Variant call(GDBusConnection* bus, const Ref& ref, const char* interface, const char* method, GVariant* args = nullptr) {
    GError* error = nullptr;
    auto* result = g_dbus_connection_call_sync(bus, ref.bus.c_str(), ref.path.c_str(), interface, method,
        args, nullptr, G_DBUS_CALL_FLAGS_NONE, 2000, nullptr, &error);
    if (!result) { std::string message = error ? error->message : "D-Bus call failed"; g_clear_error(&error); throw std::runtime_error(message); }
    return {result, g_variant_unref};
}
std::vector<Ref> children(GDBusConnection* bus, const Ref& ref) {
    auto result = call(bus, ref, accessible, "GetChildren");
    Variant array(g_variant_get_child_value(result.get(), 0), g_variant_unref);
    GVariantIter iter; g_variant_iter_init(&iter, array.get());
    std::vector<Ref> out; const char *name, *path;
    while (g_variant_iter_loop(&iter, "(&s&o)", &name, &path)) out.push_back({name, path});
    return out;
}
std::string name(GDBusConnection* bus, const Ref& ref) {
    auto result = call(bus, ref, "org.freedesktop.DBus.Properties", "Get", g_variant_new("(ss)", accessible, "Name"));
    GVariant* value; g_variant_get(result.get(), "(v)", &value);
    std::string out = g_variant_get_string(value, nullptr); g_variant_unref(value); return out;
}
bool press(GDBusConnection* bus, const Ref& ref) {
    auto result = call(bus, ref, "org.a11y.atspi.Action", "DoAction", g_variant_new("(i)", 0));
    gboolean value; g_variant_get(result.get(), "(b)", &value); return value;
}
}
int main(int argc, char**) {
    using namespace oneui;
    g_unsetenv("NO_AT_BRIDGE");
    WindowOptions options; options.title = L"OneUI AT-SPI integration";
    auto window = Window::create(options); window->initialize(); window->show();
    auto* desktop = dynamic_cast<platform::DesktopWindow*>(window.get());
    auto root = std::make_shared<View>();
    auto button = std::make_shared<Button>(L"Unicode \u4e2d\u6587");
    auto password = std::make_shared<TextField>(L"Secret");
    const std::wstring sample = L"A\U0001f600\u4e2de\u0301\nSecond line";
    auto editor = std::make_shared<TextField>(L"Editor"); editor->setMultiline(true); editor->setText(sample);
    auto clipboard = std::make_shared<MemoryClipboard>(); editor->setClipboard(clipboard);
    auto slider = std::make_shared<Slider>(); slider->setRange(-5, 10); slider->setStep(0.5); slider->setValue(2);
    auto progress = std::make_shared<ProgressBar>(); progress->setValue(0.25);
    password->setPasswordMode(true); password->setText(L"do-not-disclose");
    password->setAccessibleValue(L"do-not-disclose");
    root->add(button); root->add(password); root->add(editor); root->add(slider); root->add(progress);
    window->setContent(root); desktop->render();
    button->setFrame({10, 10, 120, 30}); password->setFrame({10, 50, 120, 30});
    editor->setFrame({10, 90, 280, 100}); slider->setFrame({10, 200, 200, 30}); progress->setFrame({10, 240, 200, 12});
    std::atomic<int> clicks{0}, failures{0}; std::atomic<bool> done{false};
    button->setOnClick([&] { ++clicks; });
    std::thread client([&] {
        GDBusConnection* bus = nullptr;
        guint subscription = 0;
        struct Signals { std::atomic<int> focus{0}, children{0}, text{0}, caret{0}, selection{0}, value{0}; };
        auto signals = std::make_shared<Signals>();
        auto check = [&](bool ok, const char* message) { if (!ok) { ++failures; std::cerr << message << '\n'; } };
        auto onUi = [&](std::function<void()> fn) {
            auto promise = std::make_shared<std::promise<void>>(); auto future = promise->get_future();
            if (!window->post([fn, promise] { fn(); promise->set_value(); })) throw std::runtime_error("UI post rejected");
            if (future.wait_for(std::chrono::seconds(3)) != std::future_status::ready) throw std::runtime_error("UI post timed out");
        };
        try {
            GDBusConnection* session = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);
            if (!session) throw std::runtime_error("No isolated session bus");
            auto addressReply = call(session, {"org.a11y.Bus", "/org/a11y/bus"}, "org.a11y.Bus", "GetAddress");
            g_object_unref(session);
            const char* address; g_variant_get(addressReply.get(), "(&s)", &address);
            bus = g_dbus_connection_new_for_address_sync(address,
                GDBusConnectionFlags(G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT | G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION),
                nullptr, nullptr, nullptr);
            if (!bus) throw std::runtime_error("No accessibility bus");
            g_dbus_connection_set_exit_on_close(bus, FALSE);
            Ref app;
            for (int attempt = 0; attempt < 100 && app.path.empty(); ++attempt) {
                for (const auto& candidate : children(bus, {"org.a11y.atspi.Registry", rootPath}))
                    if (name(bus, candidate) == "OneUI") { app = candidate; break; }
                if (app.path.empty()) std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            if (app.path.empty()) throw std::runtime_error("OneUI was not embedded in the real AT-SPI registry");
            const auto windows = children(bus, app);
            check(windows.size() == 1, "One native window is exposed under the application");
            if (windows.empty()) throw std::runtime_error("Missing native window");
            check(name(bus, windows[0]) == "OneUI AT-SPI integration", "Window title crosses D-Bus");
            const auto controls = children(bus, windows[0]);
            check(controls.size() == 5, "Widget hierarchy crosses D-Bus");
            if (controls.size() < 5) throw std::runtime_error("Missing controls");
            const auto action = controls[0], secret = controls[1];
            const auto edit = controls[2], number = controls[3], meter = controls[4];
            subscription = g_dbus_connection_signal_subscribe(bus, app.bus.c_str(), "org.a11y.atspi.Event.Object",
                nullptr, nullptr, nullptr, G_DBUS_SIGNAL_FLAGS_NONE,
                [](GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar* signal, GVariant* args, gpointer data) {
                    auto& log = **static_cast<std::shared_ptr<Signals>*>(data);
                    if (!std::strcmp(signal, "ChildrenChanged")) ++log.children;
                    if (!std::strcmp(signal, "TextChanged")) ++log.text;
                    if (!std::strcmp(signal, "TextCaretMoved")) ++log.caret;
                    if (!std::strcmp(signal, "TextSelectionChanged")) ++log.selection;
                    if (!std::strcmp(signal, "PropertyChange")) ++log.value;
                    if (!std::strcmp(signal, "StateChanged")) {
                        const char* detail; gint32 enabled, unused; GVariant *value, *properties;
                        g_variant_get(args, "(&siiv@a{sv})", &detail, &enabled, &unused, &value, &properties);
                        if (!std::strcmp(detail, "focused") && enabled) ++log.focus;
                        g_variant_unref(value); g_variant_unref(properties);
                    }
                }, new std::shared_ptr<Signals>(signals), [](gpointer p) { delete static_cast<std::shared_ptr<Signals>*>(p); });
            onUi([&] { window->requestFocus(password.get()); });
            check(name(bus, action) == "Unicode \xe4\xb8\xad\xe6\x96\x87", "Unicode name round trips");
            check(press(bus, action) && clicks == 1, "Native AT-SPI action invokes the real button");
            auto states = call(bus, action, accessible, "GetState");
            Variant array(g_variant_get_child_value(states.get(), 0), g_variant_unref);
            gsize count; const auto* bits = static_cast<const guint32*>(g_variant_get_fixed_array(array.get(), &count, sizeof(guint32)));
            check(count == 2 && (bits[0] & (1u << ATSPI_STATE_FOCUSED)), "AT-SPI exposes the actual focused leaf");
            auto role = call(bus, secret, accessible, "GetRole"); guint32 value;
            g_variant_get(role.get(), "(u)", &value); check(value == ATSPI_ROLE_PASSWORD_TEXT, "Password role is explicit");
            auto attributes = call(bus, secret, accessible, "GetAttributes");
            gchar* serialized = g_variant_print(attributes.get(), TRUE);
            check(!std::strstr(serialized, "do-not-disclose"), "Password values never cross the accessibility bus"); g_free(serialized);
            auto bounds = call(bus, action, "org.a11y.atspi.Component", "GetExtents", g_variant_new("(u)", ATSPI_COORD_TYPE_WINDOW));
            int x, y, w, h; g_variant_get(bounds.get(), "((iiii))", &x, &y, &w, &h);
            check(w > 0 && h > 0, "Window-relative bounds work on X11 and Wayland");
            constexpr auto textApi = "org.a11y.atspi.Text", editApi = "org.a11y.atspi.EditableText", valueApi = "org.a11y.atspi.Value";
            auto property = [&](const Ref& ref, const char* api, const char* key) {
                auto result = call(bus, ref, "org.freedesktop.DBus.Properties", "Get", g_variant_new("(ss)", api, key));
                GVariant* value; g_variant_get(result.get(), "(v)", &value); return Variant(value, g_variant_unref);
            };
            auto booleanCall = [&](const Ref& ref, const char* api, const char* method, GVariant* args) {
                auto result = call(bus, ref, api, method, args); gboolean value; g_variant_get(result.get(), "(b)", &value); return bool(value);
            };
            auto readText = [&](const Ref& ref, int start, int end) {
                auto result = call(bus, ref, textApi, "GetText", g_variant_new("(ii)", start, end));
                const char* value; g_variant_get(result.get(), "(&s)", &value); return std::string(value);
            };
            auto setNumber = [&](const Ref& ref, double value) {
                return call(bus, ref, "org.freedesktop.DBus.Properties", "Set", g_variant_new("(ssv)", valueApi, "CurrentValue", g_variant_new_double(value)));
            };
            check(g_variant_get_int32(property(edit, textApi, "CharacterCount").get()) == int(sample.size()), "AT-SPI counts Unicode scalars, including emoji and combining marks");
            check(readText(edit, 1, 3) == "\xf0\x9f\x98\x80\xe4\xb8\xad", "Text ranges use character offsets rather than UTF-8 bytes");
            auto character = call(bus, edit, textApi, "GetCharacterAtOffset", g_variant_new("(i)", 1));
            gint32 scalar; g_variant_get(character.get(), "(i)", &scalar); check(scalar == 0x1f600, "Character query returns the emoji scalar");
            check(booleanCall(edit, textApi, "AddSelection", g_variant_new("(ii)", 1, 3)), "Native text selection succeeds");
            auto selection = call(bus, edit, textApi, "GetSelection", g_variant_new("(i)", 0));
            int a, b; g_variant_get(selection.get(), "(ii)", &a, &b); check(a == 1 && b == 3, "Native selection round trips");
            check(!booleanCall(edit, textApi, "AddSelection", g_variant_new("(ii)", 3, 4)), "Single-selection editor rejects a second selection");
            check(booleanCall(edit, textApi, "SetCaretOffset", g_variant_new("(i)", 4)) &&
                g_variant_get_int32(property(edit, textApi, "CaretOffset").get()) == 3, "Caret snaps to the combining grapheme boundary");
            auto line = call(bus, edit, textApi, "GetStringAtOffset", g_variant_new("(iu)", 1, ATSPI_TEXT_GRANULARITY_LINE));
            const char* part; g_variant_get(line.get(), "(&sii)", &part, &a, &b); check(a == 0 && b == 6, "Line queries use real shaped line boundaries");
            auto word = call(bus, edit, textApi, "GetStringAtOffset", g_variant_new("(iu)", 8, ATSPI_TEXT_GRANULARITY_WORD));
            g_variant_get(word.get(), "(&sii)", &part, &a, &b); check(std::string(part) == "Second " && a == 6, "Word queries retain the protocol's following whitespace");
            auto glyph = call(bus, edit, textApi, "GetCharacterExtents", g_variant_new("(iu)", 1, ATSPI_COORD_TYPE_WINDOW));
            g_variant_get(glyph.get(), "(iiii)", &x, &y, &w, &h); check(w > 0 && h > 0, "Character bounds come from the real layout");
            auto hit = call(bus, edit, textApi, "GetOffsetAtPoint", g_variant_new("(iiu)", x + w / 4, y + h / 2, ATSPI_COORD_TYPE_WINDOW));
            g_variant_get(hit.get(), "(i)", &a); check(a == 1 || a == 2, "Character geometry hit test lands at an emoji edge");
            check(booleanCall(edit, editApi, "InsertText", g_variant_new("(isi)", 1, "\xe4\xb8\xadX", 3)) && readText(edit, 1, 2) == "\xe4\xb8\xad", "Insert length is UTF-8 bytes, position is characters");
            check(!booleanCall(edit, editApi, "InsertText", g_variant_new("(isi)", 1, "\xe4\xb8\xadX", 2)), "Truncated UTF-8 insertion is rejected");
            onUi([&] { check(editor->undo() && editor->text() == sample, "Accessible insertion participates in native undo"); });
            check(booleanCall(edit, editApi, "InsertText", g_variant_new("(isi)", 0, "Z", 100)), "Insert byte limit larger than the payload inserts the entire payload");
            onUi([&] { check(editor->undo() && editor->text() == sample, "Oversized byte limit does not read beyond the payload"); });
            check(booleanCall(edit, editApi, "DeleteText", g_variant_new("(ii)", 3, 5)), "Delete removes the requested complete combining sequence");
            onUi([&] { check(editor->text() == L"A\U0001f600\u4e2d\nSecond line" && editor->undo(), "Native deletion has a reversible editor transaction"); });
            check(booleanCall(edit, editApi, "SetTextContents", g_variant_new("(s)", "replacement")), "Whole-text replacement is implemented");
            onUi([&] { check(editor->undo() && editor->text() == sample, "Whole-text replacement preserves undo"); });
            call(bus, edit, editApi, "CopyText", g_variant_new("(ii)", 1, 3));
            onUi([&] { check(clipboard->text() == L"\U0001f600\u4e2d", "Copy uses requested Unicode range"); });
            check(booleanCall(edit, editApi, "CutText", g_variant_new("(ii)", 1, 3)), "Cut reaches the real editor");
            check(booleanCall(edit, editApi, "PasteText", g_variant_new("(i)", 1)), "Paste restores the Unicode range");
            onUi([&] { check(editor->text() == sample, "Cut/paste preserves remaining text"); editor->setReadOnly(true); });
            check(!booleanCall(edit, editApi, "DeleteText", g_variant_new("(ii)", 0, 1)), "Read-only native edits fail");
            check(!readText(edit, 0, -1).empty(), "Read-only text remains readable");
            onUi([&] { editor->setReadOnly(false); editor->setPasswordMode(true); });
            bool secretRejected = false;
            try { readText(edit, 0, -1); } catch (...) { secretRejected = true; }
            check(secretRejected, "Switching to password mode revokes previously cached text interfaces");
            onUi([&] { editor->setPasswordMode(false); });
            check(g_variant_get_double(property(number, valueApi, "MinimumValue").get()) == -5 &&
                g_variant_get_double(property(number, valueApi, "MaximumValue").get()) == 10 &&
                g_variant_get_double(property(number, valueApi, "MinimumIncrement").get()) == 0.5, "Numeric range and step are real control values");
            setNumber(number, 4.5);
            check(g_variant_get_double(property(number, valueApi, "CurrentValue").get()) == 4.5, "Native numeric write reaches slider");
            onUi([&] { slider->setOnInteraction([](SliderInteraction phase, double) {
                if (phase == SliderInteraction::Begin) throw std::runtime_error("native input exception");
            }); });
            bool numericRejected = false; try { setNumber(number, 5); } catch (...) { numericRejected = true; }
            check(numericRejected, "Native numeric setter contains application callback exceptions");
            onUi([&] { check(!slider->dragging(), "Native callback error releases the interaction state"); slider->setOnInteraction({}); });
            setNumber(number, 4.5);
            for (double invalid : {11.0, std::numeric_limits<double>::quiet_NaN()}) {
                bool rejected = false; try { setNumber(number, invalid); } catch (...) { rejected = true; }
                check(rejected, "Numeric setter rejects non-finite and out-of-range values");
            }
            bool progressRejected = false; try { setNumber(meter, 0.5); } catch (...) { progressRejected = true; }
            check(progressRejected && g_variant_get_double(property(meter, valueApi, "CurrentValue").get()) == 0.25, "Progress value is read-only");
            onUi([&] { progress->setIndeterminate(true); });
            progressRejected = false; try { property(meter, valueApi, "CurrentValue"); } catch (...) { progressRejected = true; }
            check(progressRejected, "Indeterminate progress does not fabricate a numeric percentage");
            for (int i = 0; i < 100 && (!signals->text || !signals->caret || !signals->selection || !signals->value); ++i)
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            check(signals->text && signals->caret && signals->selection && signals->value, "Text, caret, selection and value notifications reach native readers");
            onUi([&] { button->setDisabled(true); });
            check(!press(bus, action) && clicks == 1, "Native callers cannot invoke disabled controls");
            onUi([&] { desktop->enterNativeModal(); });
            check(children(bus, app).empty(), "Native modal hides background window from readers");
            bool rejected = false;
            try { press(bus, action); } catch (const std::exception&) { rejected = true; }
            check(rejected && clicks == 1, "Previously retained D-Bus paths cannot bypass modal isolation");
            onUi([&] { desktop->leaveNativeModal(); button->setDisabled(false); });
            check(children(bus, children(bus, app)[0])[0].path == action.path, "Identity survives modal dismissal");
            onUi([&] { root->reconcileChildren({password, button}); });
            check(children(bus, windows[0])[1].path == action.path, "Native tree order follows retained child reordering");
            onUi([&] { button->setOnClick([] { throw std::runtime_error("application callback failure"); }); });
            rejected = false;
            try { press(bus, action); } catch (const std::exception&) { rejected = true; }
            check(rejected, "Callback exceptions are contained at the native D-Bus boundary");
            onUi([&] { root->reconcileChildren({password}); });
            rejected = false;
            try { press(bus, action); } catch (const std::exception&) { rejected = true; }
            check(rejected && clicks == 1, "Detached widgets with retained C++ ownership reject stale native actions");
            for (int i = 0; i < 100 && (!signals->focus || !signals->children); ++i)
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            check(signals->focus > 0 && signals->children > 0, "Native readers receive focus and hierarchy change signals");
            onUi([&] {
                root->reconcileChildren(argc > 1 ? std::vector<std::shared_ptr<Widget>>{slider} : std::vector<std::shared_ptr<Widget>>{button});
                button->setOnClick([&] { ++clicks; window.reset(); });
                slider->setOnChanged([&](double) { window.reset(); });
            });
            // The method handler must keep its service/context alive until the
            // reply has been queued, even when this destroys the last window.
            if (argc > 1) setNumber(number, 5.0);
            else check(press(bus, action) && clicks == 2, "Native action can destroy its own window and still reply");
        } catch (const std::exception& e) { ++failures; std::cerr << e.what() << '\n'; }
        if (subscription) g_dbus_connection_signal_unsubscribe(bus, subscription);
        if (bus) { g_dbus_connection_close_sync(bus, nullptr, nullptr); g_object_unref(bus); }
        done = true;
    });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    while (!done && std::chrono::steady_clock::now() < deadline) {
        if (window) desktop->dispatchWork();
        if (window) desktop->pumpNativeEvents(5);
        while (g_main_context_iteration(nullptr, FALSE)) {}
    }
    if (!done) { std::cerr << "AT-SPI integration timed out\n"; std::_Exit(1); }
    client.join(); if (window) window->close();
    return failures ? 1 : 0;
}
