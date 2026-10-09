#include "platform/shared/native_desktop_services.h"
#include "platform/shared/desktop_window.h"
#include "internal/unicode.h"
#include "oneui/localization.h"
#include <glib/gstdio.h>
#include <cstring>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <gdk/gdkwayland.h>
#include <libayatana-appindicator/app-indicator.h>
#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <thread>

namespace oneui::platform {
namespace {
bool gtkReady() {
    static const auto servicesThread = std::this_thread::get_id();
    if (std::this_thread::get_id() != servicesThread) return false;
    static const bool ready = [] {
        const char* backend = std::getenv("ONEUI_LINUX_BACKEND");
        if (backend && (std::string(backend) == "x11" || std::string(backend) == "wayland"))
            gdk_set_allowed_backends(backend);
        return gtk_init_check(nullptr, nullptr) != FALSE;
    }();
    return ready;
}
GDBusConnection* sessionBus() {
    static GDBusConnection* bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);
    return bus;
}
bool watcherPresent() {
    if (!gtkReady() || !sessionBus()) return false;
    GVariant* reply = g_dbus_connection_call_sync(sessionBus(), "org.kde.StatusNotifierWatcher", "/StatusNotifierWatcher",
        "org.freedesktop.DBus.Properties", "Get", g_variant_new("(ss)", "org.kde.StatusNotifierWatcher", "IsStatusNotifierHostRegistered"),
        G_VARIANT_TYPE("(v)"), G_DBUS_CALL_FLAGS_NO_AUTO_START, 500, nullptr, nullptr);
    if (!reply) return false;
    gboolean present = FALSE;
    GVariant* value = nullptr;
    g_variant_get(reply, "(v)", &value);
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_BOOLEAN)) present = g_variant_get_boolean(value);
    g_variant_unref(value);
    g_variant_unref(reply);
    return present;
}
std::string utf8(const std::wstring& text) { return unicode::toUtf8(text); }
int runDialog(GtkWidget* dialog, Window* owner) {
    auto* desktop = dynamic_cast<DesktopWindow*>(owner);
    if (desktop && desktop->isClosed()) return GTK_RESPONSE_CANCEL;
    auto weak = desktop ? desktop->nativeLifetime() : std::weak_ptr<std::atomic<bool>>{};
    int response = GTK_RESPONSE_NONE;
    const auto signal = g_signal_connect(dialog, "response", G_CALLBACK(+[](GtkDialog*, gint code, gpointer data) {
        *static_cast<int*>(data) = code;
    }), &response);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_widget_realize(dialog);
    if (owner && owner->backend() == WindowBackend::X11 && GDK_IS_X11_DISPLAY(gdk_display_get_default())) {
        GdkWindow* foreign = gdk_x11_window_foreign_new_for_display(gdk_display_get_default(),
            reinterpret_cast<std::uintptr_t>(owner->nativeHandle()));
        if (foreign) { gdk_window_set_transient_for(gtk_widget_get_window(dialog), foreign); g_object_unref(foreign); }
    }
    if (desktop) desktop->enterNativeModal();
    struct ModalScope {
        GtkWidget* dialog;
        gulong signal;
        DesktopWindow* owner;
        std::weak_ptr<std::atomic<bool>> weak;
        ~ModalScope() {
            g_signal_handler_disconnect(dialog, signal);
            gtk_widget_hide(dialog);
            if (const auto alive = weak.lock(); alive && *alive) owner->leaveNativeModal();
        }
    } scope{dialog, signal, desktop, weak};
    gtk_widget_show_all(dialog);
    bool waylandParentSet = false;
    while (response == GTK_RESPONSE_NONE) {
        if (desktop) {
            const auto alive = weak.lock();
            if (!alive || !*alive || desktop->isClosed()) break;
            if (!waylandParentSet && desktop->backend() == WindowBackend::Wayland && GDK_IS_WAYLAND_DISPLAY(gdk_display_get_default())) {
                auto handle = desktop->exportDialogParent();
                if (!handle.empty()) waylandParentSet = gdk_wayland_window_set_transient_for_exported(gtk_widget_get_window(dialog), handle.data());
            }
            desktop->pumpNativeEvents(20);
        } else {
            g_main_context_iteration(nullptr, TRUE);
        }
        dispatchNativeServices();
    }
    if (desktop) {
        const auto alive = weak.lock();
        if (!alive || !*alive) response = GTK_RESPONSE_CANCEL;
    }
    return response;
}
class GtkTray final : public NativeTray {
public:
    explicit GtkTray(std::function<void(int)> callback) : action_(std::move(callback)) {
        static std::atomic<unsigned int> next{0};
        const auto name = "oneui-" + std::to_string(++next);
        indicator_ = app_indicator_new(name.c_str(), "application-x-executable", APP_INDICATOR_CATEGORY_APPLICATION_STATUS);
        setMenu({});
    }
    ~GtkTray() override {
        hide();
        if (indicator_) g_object_unref(indicator_);
        if (menu_) g_object_unref(menu_);
        if(!iconDirectory_.empty()){g_remove((iconDirectory_+"/icon0.png").c_str());g_remove((iconDirectory_+"/icon1.png").c_str());g_rmdir(iconDirectory_.c_str());}
    }
    bool show() override {
        if (!indicator_ || !watcherPresent()) return false;
        app_indicator_set_status(indicator_, APP_INDICATOR_STATUS_ACTIVE);
        shown_ = true;
        return true;
    }
    void hide() override {
        if (indicator_) app_indicator_set_status(indicator_, APP_INDICATOR_STATUS_PASSIVE);
        shown_ = false;
    }
    bool visible() const override { return shown_ && watcherPresent(); }
    void setTooltip(const std::wstring& value) override {
        if (indicator_) app_indicator_set_title(indicator_, utf8(value).c_str());
    }
    void setMenu(const std::vector<TrayMenuItem>& items) override {
        const std::vector<NativeMenuItem> defaults={{int(kTrayCommandShow),builtinText(BuiltinText::ShowMainWindow)},{0,{},true,true},{int(kTrayCommandExit),builtinText(BuiltinText::QuitApplication)}};
        const auto build=[&](const auto& self,const std::vector<NativeMenuItem>& values,bool parentEnabled)->GtkWidget* {
            GtkWidget* menu=gtk_menu_new();
            for(const auto& item:values){
                GtkWidget* entry=item.separator?gtk_separator_menu_item_new():item.check==MenuCheck::Normal?gtk_menu_item_new_with_label(utf8(item.text).c_str()):gtk_check_menu_item_new_with_label(utf8(item.text).c_str());
                gtk_widget_set_sensitive(entry,parentEnabled&&item.enabled);
                if(item.check!=MenuCheck::Normal){gtk_check_menu_item_set_draw_as_radio(GTK_CHECK_MENU_ITEM(entry),item.check==MenuCheck::Radio);gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(entry),item.checked);}
                if(!item.children.empty())gtk_menu_item_set_submenu(GTK_MENU_ITEM(entry),self(self,item.children,parentEnabled&&item.enabled));
                else if(!item.separator){
                    auto* callback=new std::function<void()>([action=action_,id=item.id,active=parentEnabled&&item.enabled,entry,check=item.check,checked=item.checked]{
                        // Controlled state, matching AppKit and Win32 menus.
                        if(check!=MenuCheck::Normal)gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(entry),checked);
                        if(active&&action)action(id);
                    });
                    g_signal_connect_data(entry,"activate",G_CALLBACK(+[](GtkMenuItem*,gpointer data){const auto callback=*static_cast<std::function<void()>*>(data);callback();}),callback,+[](gpointer data,GClosure*){delete static_cast<std::function<void()>*>(data);},GConnectFlags(0));
                }
                auto label=menuShortcutLabel(item.shortcut);
                if(!label.empty()){
                    GtkWidget* child=gtk_bin_get_child(GTK_BIN(entry));
                    if(GTK_IS_LABEL(child))gtk_label_set_text(GTK_LABEL(child),(utf8(item.text)+"    "+utf8(label)).c_str());
                }
                gtk_menu_shell_append(GTK_MENU_SHELL(menu),entry);
            }
            return menu;
        };
        auto* menu=build(build,items.empty()?defaults:items,true);g_object_ref_sink(menu);gtk_widget_show_all(menu);
        app_indicator_set_menu(indicator_,GTK_MENU(menu));if(menu_)g_object_unref(menu_);menu_=menu;
    }
    bool setIcon(const TrayIcon& icon) override {
        if(!validateTrayIcon(icon) || !indicator_)return false;
        if(icon.rgba.empty()){app_indicator_set_icon_full(indicator_,"application-x-executable","OneUI");return true;}
        if(iconDirectory_.empty()){
            gchar* directory=g_dir_make_tmp("oneui-tray-XXXXXX",nullptr);if(!directory)return false;iconDirectory_=directory;g_free(directory);
        }
        const auto path=iconDirectory_+"/icon"+std::to_string(nextIcon_)+".png";
        auto* pixels=gdk_pixbuf_new(GDK_COLORSPACE_RGB,TRUE,8,icon.width,icon.height);if(!pixels)return false;
        for(unsigned y=0;y<icon.height;++y)std::memcpy(gdk_pixbuf_get_pixels(pixels)+y*gdk_pixbuf_get_rowstride(pixels),icon.rgba.data()+y*icon.width*4,icon.width*4);
        const bool saved=gdk_pixbuf_save(pixels,path.c_str(),"png",nullptr,nullptr);g_object_unref(pixels);if(!saved)return false;
        app_indicator_set_icon_full(indicator_,path.c_str(),"OneUI");nextIcon_=1-nextIcon_;return true;
    }
    bool notify(const std::wstring& title, const std::wstring& message) override {
        if (!sessionBus()) return false;
        GVariantBuilder actions, hints;
        g_variant_builder_init(&actions, G_VARIANT_TYPE("as"));
        g_variant_builder_init(&hints, G_VARIANT_TYPE("a{sv}"));
        auto* reply = g_dbus_connection_call_sync(sessionBus(), "org.freedesktop.Notifications", "/org/freedesktop/Notifications",
            "org.freedesktop.Notifications", "Notify", g_variant_new("(susss@as@a{sv}i)", "OneUI", 0u,
            "application-x-executable", utf8(title).c_str(), utf8(message).c_str(),
            g_variant_builder_end(&actions), g_variant_builder_end(&hints), -1), G_VARIANT_TYPE("(u)"),
            G_DBUS_CALL_FLAGS_NONE, 1500, nullptr, nullptr);
        if (!reply) return false;
        g_variant_unref(reply);
        return true;
    }
private:
    AppIndicator* indicator_ = nullptr;
    GtkWidget* menu_ = nullptr;
    std::function<void(int)> action_;
    bool shown_ = false;
    std::string iconDirectory_;
    unsigned int nextIcon_ = 0;
};
} // namespace
bool nativeDialogsAvailable() { return gtkReady(); }
bool nativeTrayAvailable() { return watcherPresent(); }
unsigned int nativeDoubleClickInterval() {
    if (!gtkReady()) return 500;
    gint interval = 500;
    g_object_get(gtk_settings_get_default(), "gtk-double-click-time", &interval, nullptr);
    return static_cast<unsigned int>(std::max(1, interval));
}
bool nativeAnimationsEnabled() {
    if (!gtkReady()) return true;
    gboolean enabled = TRUE;
    g_object_get(gtk_settings_get_default(), "gtk-enable-animations", &enabled, nullptr);
    return enabled;
}
std::shared_ptr<NativeTray> makeNativeTray(std::function<void(int)> action) {
    if (!gtkReady()) return {};
    return std::make_shared<GtkTray>(std::move(action));
}
NativeFileResult nativeFileDialog(Window* owner, const NativeFileRequest& request) {
    if (!gtkReady() || request.mode < 0 || request.mode > 2 || (request.multiple && request.mode == 1)) return {};
    const auto mode = request.mode == 1 ? GTK_FILE_CHOOSER_ACTION_SAVE : request.mode == 2 ? GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER : GTK_FILE_CHOOSER_ACTION_OPEN;
    GtkWidget* dialog = gtk_file_chooser_dialog_new(utf8(request.title).c_str(), nullptr, mode,
        "_Cancel", GTK_RESPONSE_CANCEL, request.mode == 1 ? "_Save" : "_Open", GTK_RESPONSE_ACCEPT, nullptr);
    std::unique_ptr<GtkWidget, decltype(&gtk_widget_destroy)> cleanup(dialog, gtk_widget_destroy);
    auto* chooser = GTK_FILE_CHOOSER(dialog);
    gtk_file_chooser_set_local_only(chooser, TRUE);
    gtk_file_chooser_set_select_multiple(chooser, request.multiple);
    // Confirm the final path ourselves: GTK does not append default_extension,
    // and confirming the unsuffixed name would allow overwriting a different file.
    gtk_file_chooser_set_do_overwrite_confirmation(chooser, FALSE);
    if (!request.directory.empty() && !gtk_file_chooser_set_current_folder(chooser, utf8(request.directory).c_str())) {
        return {};
    }
    if (!request.name.empty()) {
        if (request.mode == 1) gtk_file_chooser_set_current_name(chooser, utf8(request.name).c_str());
        else gtk_file_chooser_set_filename(chooser, utf8(request.name).c_str());
    }
    for (const auto& filter : request.filters) {
        auto* native = gtk_file_filter_new();
        gtk_file_filter_set_name(native, utf8(filter.name).c_str());
        const auto patterns = utf8(filter.pattern);
        for (std::size_t start = 0; start < patterns.size();) {
            const auto end = patterns.find(';', start);
            auto pattern = patterns.substr(start, end - start);
            if (pattern == "*.*") pattern = "*";
            if (!pattern.empty()) gtk_file_filter_add_pattern(native, pattern.c_str());
            if (end == std::string::npos) break;
            start = end + 1;
        }
        gtk_file_chooser_add_filter(chooser, native);
    }
    NativeFileResult result{0, {}};
    while (runDialog(dialog, owner) == GTK_RESPONSE_ACCEPT) {
        result.paths.clear();
        GSList* files = gtk_file_chooser_get_filenames(chooser);
        for (auto* item = files; item; item = item->next) result.paths.push_back(unicode::fromUtf8(static_cast<const char*>(item->data)));
        g_slist_free_full(files, g_free);
        if (request.mode == 1 && !result.paths.empty()) {
            auto& path = result.paths.front();
            const auto nativePath = std::filesystem::u8path(utf8(path));
            if (!request.extension.empty() && !nativePath.has_extension())
                path += (request.extension.front() == L'.' ? L"" : L".") + request.extension;
            std::error_code error;
            if (request.confirmOverwrite && std::filesystem::exists(std::filesystem::u8path(utf8(path)), error) &&
                !nativeConfirm(owner, L"Replace file?", L"The following file already exists. Replace it?\n" + path)) {
                result.paths.clear();
                continue;
            }
            if (error) { result = {}; break; }
        }
        result.status = result.paths.empty() ? -1 : 1;
        break;
    }
    return result;
}
bool nativeConfirm(Window* owner, const std::wstring& title, const std::wstring& message) {
    if (!gtkReady()) return false;
    auto* dialog = gtk_message_dialog_new(nullptr, GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, "%s", utf8(message).c_str());
    std::unique_ptr<GtkWidget, decltype(&gtk_widget_destroy)> cleanup(dialog, gtk_widget_destroy);
    gtk_window_set_title(GTK_WINDOW(dialog), utf8(title).c_str());
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_NO);
    const bool accepted = runDialog(dialog, owner) == GTK_RESPONSE_YES;
    return accepted;
}
bool nativePrompt(Window* owner, const std::wstring& title, const std::wstring& message, const std::wstring& initial,
                  const std::wstring& placeholder, bool password, std::wstring& result) {
    if (!gtkReady()) return false;
    auto* dialog = gtk_dialog_new_with_buttons(utf8(title).c_str(), nullptr, GTK_DIALOG_MODAL,
        "_Cancel", GTK_RESPONSE_CANCEL, "_OK", GTK_RESPONSE_ACCEPT, nullptr);
    std::unique_ptr<GtkWidget, decltype(&gtk_widget_destroy)> cleanup(dialog, gtk_widget_destroy);
    auto* area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(area), 16);
    gtk_box_set_spacing(GTK_BOX(area), 12);
    auto* label = gtk_label_new(utf8(message).c_str());
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    gtk_box_pack_start(GTK_BOX(area), label, FALSE, FALSE, 0);
    auto* entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), utf8(initial).c_str());
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), utf8(placeholder).c_str());
    gtk_entry_set_visibility(GTK_ENTRY(entry), !password);
    gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
    gtk_box_pack_start(GTK_BOX(area), entry, FALSE, FALSE, 0);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 420, -1);
    gtk_widget_grab_focus(entry);
    const bool accepted = runDialog(dialog, owner) == GTK_RESPONSE_ACCEPT;
    if (accepted) result = unicode::fromUtf8(gtk_entry_get_text(GTK_ENTRY(entry)));
    gtk_entry_set_text(GTK_ENTRY(entry), "");
    return accepted;
}
void dispatchNativeServices() {
    // Bound dispatch prevents a noisy DBus peer from starving native input.
    for (int i = 0; i < 64 && g_main_context_pending(nullptr); ++i) g_main_context_iteration(nullptr, FALSE);
}
int pollNativeServices(pollfd* descriptors, nfds_t count, int timeout) {
    auto* context = g_main_context_default();
    if (!g_main_context_acquire(context)) return ::poll(descriptors, count, timeout);
    gint priority = 0, wait = -1;
    if (g_main_context_prepare(context, &priority)) timeout = 0;
    int needed = g_main_context_query(context, priority, &wait, nullptr, 0);
    std::vector<GPollFD> fds(needed + count);
    int actual = g_main_context_query(context, priority, &wait, fds.data(), needed);
    if (actual > needed) {
        needed = actual;
        fds.resize(needed + count);
        g_main_context_query(context, priority, &wait, fds.data(), needed);
    }
    if (wait >= 0) timeout = timeout < 0 ? wait : std::min(timeout, wait);
    for (nfds_t i = 0; i < count; ++i) fds[needed + i] = {descriptors[i].fd, static_cast<gushort>(descriptors[i].events), 0};
    const int result = g_poll(fds.data(), fds.size(), timeout);
    for (nfds_t i = 0; i < count; ++i) descriptors[i].revents = fds[needed + i].revents;
    g_main_context_release(context);
    return result;
}
} // namespace oneui::platform
