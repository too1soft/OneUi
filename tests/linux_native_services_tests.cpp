#include "oneui/oneui_c_api.h"
#include "capi/oneui_c_api_window_internal.h"
#include "platform/shared/desktop_window.h"
#include <gtk/gtk.h>
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <cstring>
#include <unistd.h>
#include <glib/gstdio.h>

namespace {
int failures = 0;
void check(bool value, const char* message) { if (!value) { std::cerr << message << '\n'; ++failures; } }
GtkWidget* findEntry(GtkWidget* widget) {
    if (GTK_IS_ENTRY(widget)) return widget;
    if (!GTK_IS_CONTAINER(widget)) return nullptr;
    GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
    GtkWidget* result = nullptr;
    for (auto* i = children; i && !result; i = i->next) result = findEntry(GTK_WIDGET(i->data));
    g_list_free(children);
    return result;
}
struct Response { int code; const char* text = nullptr; bool password = false; int attempts = 0; const char* file = nullptr; bool selected = false; bool overwrite = false, overwriteSeen = false; };
gboolean respond(gpointer data) {
    auto& action = *static_cast<Response*>(data);
    GList* windows = gtk_window_list_toplevels();
    bool found = false;
    for (auto* item = windows; item; item = item->next) {
        auto* widget = GTK_WIDGET(item->data);
        if (!GTK_IS_DIALOG(widget) || !gtk_widget_get_visible(widget)) continue;
        found = true;
        if (action.overwrite && GTK_IS_MESSAGE_DIALOG(widget)) {
            action.overwriteSeen = true;
            gtk_dialog_response(GTK_DIALOG(widget), GTK_RESPONSE_YES);
            break;
        }
        if (action.file && GTK_IS_FILE_CHOOSER(widget) && !action.selected) {
            check(gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(widget), action.file), "File chooser rejected fixture");
            action.selected = true;
            g_list_free(windows);
            return G_SOURCE_CONTINUE;
        }
        if (action.text) {
            auto* entry = findEntry(widget);
            check(entry != nullptr, "Native prompt entry missing");
            if (entry) {
                check(bool(gtk_entry_get_visibility(GTK_ENTRY(entry))) == !action.password, "Password field visibility incorrect");
                gtk_entry_set_text(GTK_ENTRY(entry), action.text);
            }
        }
        gtk_dialog_response(GTK_DIALOG(widget), action.code);
        break;
    }
    g_list_free(windows);
    if (action.overwrite && !action.overwriteSeen) return G_SOURCE_CONTINUE;
    return !found && ++action.attempts < 100 ? G_SOURCE_CONTINUE : G_SOURCE_REMOVE;
}
// An isolated, protocol-level peer. This proves DBus registration/lifecycle,
// not that a particular desktop shell displays its tray UI.
class NotificationPeer {
public:
    std::atomic<bool> ready{false}, stop{false}, host{true};
    std::atomic<int> registrations{0}, notifications{0};
    std::string itemSender, itemPath;
    std::thread thread;
    NotificationPeer() : thread([this] { run(); }) { while (!ready) std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
    ~NotificationPeer() { stop = true; thread.join(); }
private:
    void run() {
        auto* context = g_main_context_new();
        g_main_context_push_thread_default(context);
        auto* bus = g_dbus_connection_new_for_address_sync(g_getenv("DBUS_SESSION_BUS_ADDRESS"),
            GDBusConnectionFlags(G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT | G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION), nullptr, nullptr, nullptr);
        if (!bus) { ready = true; return; }
        const char* xml = "<node><interface name='org.kde.StatusNotifierWatcher'>"
            "<method name='RegisterStatusNotifierItem'><arg type='s' direction='in'/></method>"
            "<property name='IsStatusNotifierHostRegistered' type='b' access='read'/>"
            "<property name='ProtocolVersion' type='i' access='read'/>"
            "<property name='RegisteredStatusNotifierItems' type='as' access='read'/>"
            "</interface><interface name='org.freedesktop.Notifications'>"
            "<method name='Notify'><arg type='s' direction='in'/><arg type='u' direction='in'/><arg type='s' direction='in'/>"
            "<arg type='s' direction='in'/><arg type='s' direction='in'/><arg type='as' direction='in'/><arg type='a{sv}' direction='in'/>"
            "<arg type='i' direction='in'/><arg type='u' direction='out'/></method></interface></node>";
        auto* node = g_dbus_node_info_new_for_xml(xml, nullptr);
        static const GDBusInterfaceVTable methods{
            +[](GDBusConnection*, const gchar* sender, const gchar*, const gchar*, const gchar* method, GVariant* args, GDBusMethodInvocation* invocation, gpointer data) {
                auto* self = static_cast<NotificationPeer*>(data);
                if (std::string(method) == "Notify") { ++self->notifications; g_dbus_method_invocation_return_value(invocation, g_variant_new("(u)", 1u)); }
                else { const char* value=nullptr;g_variant_get(args,"(&s)",&value);self->itemSender=value[0]=='/'?sender:value;self->itemPath=value[0]=='/'?value:"/StatusNotifierItem"; ++self->registrations; g_dbus_method_invocation_return_value(invocation, nullptr); }
            },
            +[](GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar* property, GError**, gpointer data) -> GVariant* {
                if (std::string(property) == "IsStatusNotifierHostRegistered") return g_variant_new_boolean(static_cast<NotificationPeer*>(data)->host);
                if (std::string(property) == "ProtocolVersion") return g_variant_new_int32(0);
                return g_variant_new_strv(nullptr, 0);
            }, nullptr, {nullptr}};
        const auto watcher = g_dbus_connection_register_object(bus, "/StatusNotifierWatcher", node->interfaces[0], &methods, this, nullptr, nullptr);
        const auto notify = g_dbus_connection_register_object(bus, "/org/freedesktop/Notifications", node->interfaces[1], &methods, this, nullptr, nullptr);
        for (const char* name : {"org.kde.StatusNotifierWatcher", "org.freedesktop.Notifications"}) {
            auto* reply = g_dbus_connection_call_sync(bus, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "RequestName",
                g_variant_new("(su)", name, 0u), G_VARIANT_TYPE("(u)"), G_DBUS_CALL_FLAGS_NONE, 1000, nullptr, nullptr);
            if (reply) g_variant_unref(reply);
        }
        ready = true;
        while (!stop) { while (g_main_context_iteration(context, FALSE)) {} std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
        g_dbus_connection_unregister_object(bus, watcher); g_dbus_connection_unregister_object(bus, notify);
        g_dbus_connection_close_sync(bus, nullptr, nullptr); g_object_unref(bus); g_dbus_node_info_unref(node);
        g_main_context_pop_thread_default(context); g_main_context_unref(context);
    }
};
struct TrayInspection { std::string layout, icon; bool clicked=false; };
TrayInspection inspectTray(NotificationPeer& peer,oneui::platform::DesktopWindow& desktop,bool click) {
    TrayInspection result;std::atomic<bool> done{false};
    const auto sender=peer.itemSender,path=peer.itemPath;
    std::thread worker([&]{
        auto* bus=g_bus_get_sync(G_BUS_TYPE_SESSION,nullptr,nullptr);
        if(!bus){done=true;return;}
        auto property=[&](const char* name)->std::string{
            auto* reply=g_dbus_connection_call_sync(bus,sender.c_str(),path.c_str(),"org.freedesktop.DBus.Properties","Get",
                g_variant_new("(ss)","org.kde.StatusNotifierItem",name),G_VARIANT_TYPE("(v)"),G_DBUS_CALL_FLAGS_NONE,1500,nullptr,nullptr);
            if(!reply)return {};
            auto* wrapped=g_variant_get_child_value(reply,0);auto* value=g_variant_get_variant(wrapped);
            std::string text=g_variant_get_string(value,nullptr);g_variant_unref(value);g_variant_unref(wrapped);g_variant_unref(reply);return text;
        };
        result.icon=property("IconName");const auto menu=property("Menu");
        if(!menu.empty()){
            auto* reply=g_dbus_connection_call_sync(bus,sender.c_str(),menu.c_str(),"com.canonical.dbusmenu","GetLayout",
                g_variant_new("(ii@as)",0,-1,g_variant_new_strv(nullptr,0)),nullptr,G_DBUS_CALL_FLAGS_NONE,1500,nullptr,nullptr);
            if(reply){
                char* dump=g_variant_print(reply,TRUE);result.layout=dump;g_free(dump);
                auto* root=g_variant_get_child_value(reply,1);int activate=-1;
                const auto find=[&](const auto& self,GVariant* node)->void{
                    gint32 id=0;GVariant *props=nullptr,*children=nullptr;g_variant_get(node,"(i@a{sv}@av)",&id,&props,&children);
                    const char* label=nullptr;if(g_variant_lookup(props,"label","&s",&label) && std::string(label)=="Nested action")activate=id;
                    for(gsize i=0;i<g_variant_n_children(children);++i){auto* wrapped=g_variant_get_child_value(children,i);auto* child=g_variant_get_variant(wrapped);self(self,child);g_variant_unref(child);g_variant_unref(wrapped);}
                    g_variant_unref(props);g_variant_unref(children);
                };find(find,root);g_variant_unref(root);g_variant_unref(reply);
                if(click && activate>=0){
                    auto* response=g_dbus_connection_call_sync(bus,sender.c_str(),menu.c_str(),"com.canonical.dbusmenu","Event",
                        g_variant_new("(isvu)",activate,"clicked",g_variant_new_int32(0),0u),nullptr,G_DBUS_CALL_FLAGS_NONE,1500,nullptr,nullptr);
                    result.clicked=response!=nullptr;if(response)g_variant_unref(response);
                }
            }
        }
        g_object_unref(bus);done=true;
    });
    while(!done)desktop.pumpNativeEvents(10);
    worker.join();return result;
}

}
int main() {
    OneUiWindowOptions options{};
    options.title = L"Native services regression"; options.width = 320; options.height = 200;
    auto* handle = oneui_window_create(&options);
    if (!handle) return 1;
    auto& window = *handle->window;
    window.initialize(); window.show();
    auto* desktop = dynamic_cast<oneui::platform::DesktopWindow*>(&window);
    check(window.capabilities() & oneui::WindowCapabilityNativeDialogs, "GTK native dialogs unavailable");
    bool posted = false;
    window.post([&] { posted = true; });
    Response accept{GTK_RESPONSE_ACCEPT, "\xe4\xb8\xad\xe6\x96\x87\xf0\x9f\x98\x80", true};
    g_timeout_add(50, respond, &accept);
    wchar_t output[64]{};
    check(oneui_window_prompt_text(handle, L"Prompt regression", L"Password", L"", L"", 1, output, 64) == 1, "Prompt acceptance failed");
    check(std::wstring(output) == L"\u4e2d\u6587\U0001f600", "Prompt Unicode corrupted");
    check(posted && !desktop->nativeModal(), "Modal dialog starved posted work or failed to restore input");
    Response cancel{GTK_RESPONSE_CANCEL};
    g_timeout_add(50, respond, &cancel);
    check(oneui_window_prompt_text(handle, L"Cancel regression", L"", L"secret", L"", 1, output, 64) == 0 && !output[0], "Cancelled prompt leaked a value");
    Response yes{GTK_RESPONSE_YES};
    g_timeout_add(50, respond, &yes);
    check(oneui_window_confirm(handle, L"Confirm regression", L"Continue?") == 1, "Native confirmation failed");
    char* fixture = nullptr;
    const int fd = g_file_open_tmp("oneui-dialog-XXXXXX", &fixture, nullptr);
    check(fd >= 0, "Could not create dialog fixture");
    if (fd >= 0) {
        ::close(fd);
        OneUiFileDialogOptionsUtf8 fileOptions{};
        fileOptions.mode = OneUiFileDialogOpenFile;
        char path[1024]{}; size_t required = 0;
        Response choose{GTK_RESPONSE_ACCEPT}; choose.file = fixture;
        g_timeout_add(200, respond, &choose);
        check(oneui_window_file_dialog_utf8(handle, &fileOptions, path, sizeof(path), &required) == 1 &&
            std::string(path) == fixture && required == std::strlen(fixture) + 1, "File chooser result encoding/length mismatch");
        choose.selected = false;
        g_timeout_add(200, respond, &choose);
        check(oneui_window_file_dialog_multiple_utf8(handle, &fileOptions, path, sizeof(path), &required) == 1 &&
            required == std::strlen(fixture) + 2 && path[required - 1] == 0 && path[required - 2] == 0,
            "Multiple file chooser did not return double NUL");
        choose.selected = false;
        g_timeout_add(200, respond, &choose);
        path[0] = 'x';
        check(oneui_window_file_dialog_utf8(handle, &fileOptions, path, 1, &required) == -2 && !path[0] &&
            required == std::strlen(fixture) + 1, "Small file buffer was truncated or reported the wrong length");
        const std::string suffixed = std::string(fixture) + ".txt";
        g_file_set_contents(suffixed.c_str(), "existing", -1, nullptr);
        fileOptions.mode = OneUiFileDialogSaveFile;
        fileOptions.default_extension = {"txt", 3};
        fileOptions.confirm_overwrite = 1;
        choose.selected = false; choose.overwrite = true;
        g_timeout_add(200, respond, &choose);
        check(oneui_window_file_dialog_utf8(handle, &fileOptions, path, sizeof(path), &required) == 1 &&
            std::string(path) == suffixed && choose.overwriteSeen, "Save default extension bypassed confirmation of the actual target");
        g_unlink(suffixed.c_str());
        g_unlink(fixture); g_free(fixture);
    }
    check(!window.setTrayEnabled(true), "Missing tray host must not report success");
    {
        NotificationPeer peer;
        check(window.setTrayEnabled(true), "Tray host registration failed");
        window.setTrayMenu({{42, L"Test action"}, {43, L"Disabled", false, false}});
        window.setTrayTooltip(L"OneUI regression");
        const auto deadline = oneui::platform::DesktopWindow::nowMs() + 2000;
        while (!peer.registrations && oneui::platform::DesktopWindow::nowMs() < deadline) desktop->pumpNativeEvents(10);
        check(peer.registrations > 0, "StatusNotifier item never reached the watcher");
        oneui::NativeMenuItem selected{44,L"Checked"};selected.check=oneui::MenuCheck::Check;selected.checked=true;
        oneui::NativeMenuItem radio{45,L"Radio"};radio.check=oneui::MenuCheck::Radio;radio.checked=true;radio.radioGroup=1;
        oneui::NativeMenuItem branch{0,L"More"};branch.children={{46,L"Nested action"}};
        check(window.setTrayMenu({selected,radio,{43,L"Disabled",false},branch}),"Recursive tray model accepted");
        int selectedId=0;window.setOnTrayAction([&](int id){selectedId=id;});
        check(window.setTrayIcon({2,1,{255,0,0,255,0,255,0,128}}),"Linux RGBA tray icon accepted");
        const auto inspection=inspectTray(peer,*desktop,true);
        check(inspection.layout.find("Checked")!=std::string::npos && inspection.layout.find("Nested action")!=std::string::npos,
            "DBus exposes recursive custom menu");
        check(inspection.layout.find("checkmark")!=std::string::npos && inspection.layout.find("radio")!=std::string::npos && inspection.layout.find("toggle-state")!=std::string::npos,
            "DBus exposes checked and radio states");
        check(inspection.layout.find("Quit")==std::string::npos && inspection.layout.find("Show")==std::string::npos,
            "Custom tray does not append default commands");
        check(inspection.clicked && selectedId==46,"DBus menu event reaches nested application ID");
        check(!inspection.icon.empty() && g_file_test(inspection.icon.c_str(),G_FILE_TEST_IS_REGULAR),"DBus publishes generated icon path");
        if(!inspection.icon.empty()){
            auto* pixels=gdk_pixbuf_new_from_file(inspection.icon.c_str(),nullptr);
            check(pixels && gdk_pixbuf_get_width(pixels)==2 && gdk_pixbuf_get_height(pixels)==1 && gdk_pixbuf_get_pixels(pixels)[7]==128,"Generated PNG preserves RGBA alpha");
            if(pixels)g_object_unref(pixels);
        }
        check(window.setTrayMenu({}),"Empty tray restores defaults");
        const auto defaults=inspectTray(peer,*desktop,false);
        check(defaults.layout.find("Checked")==std::string::npos && defaults.layout.find("Quit")!=std::string::npos,"Default tray replaces previous custom rows");
        check(window.setTrayEnabled(false),"Tray disabled after custom icon test");
        check(!g_file_test(inspection.icon.c_str(),G_FILE_TEST_EXISTS),"Tray destruction removes generated icon files");
        check(window.setTrayEnabled(true),"Tray reenabled after icon cleanup");

        check(window.notifyTray(L"Test notification", L"Body") && peer.notifications == 1, "Notification service did not accept the request");
        window.setCloseToTray(true); window.close();
        check(!desktop->isClosed() && window.trayIconVisible(), "Close-to-tray destroyed the live window");
        window.activate();
        int force = 0;
        window.setOnCloseRequested([&](bool value) { if (value) ++force; });
        desktop->requestTrayExit(); check(force == 1, "Tray exit bypassed application policy");
        peer.host = false;
        check(!window.trayIconVisible(), "Lost tray host still advertised as visible");
        window.setOnCloseRequested({}); window.close();
        check(desktop->isClosed(), "Missing tray host stranded a hidden window");
    }
    oneui_window_destroy(handle);
    return failures ? 1 : 0;
}
