#include "internal/unicode.h"
#include "platform/shared/notification_backend.h"
#include <filesystem>
#include <fstream>
#include <glib.h>

namespace oneui::platform {
namespace {
std::filesystem::path dataRoot() { return g_get_user_data_dir(); }
std::string read(const std::filesystem::path &path) {
    std::ifstream file(path);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
std::string quoteExec(const std::string &value, bool desktop) {
    std::string out = "\"";
    for (char c : value) {
        if (c == '\n' || c == '\r' || c == '\0')
            throw std::invalid_argument("Invalid executable path");
        if (c == '\\' || c == '"' || c == '`' || c == '$')
            out += '\\';
        if (desktop && c == '%')
            out += '%';
        out += c;
    }
    return out + "\"";
}
std::string marker(const ApplicationOptions &options) {
    return "X-OneUI-Identity=" + options.id + "\n";
}
void checkOwned(const std::filesystem::path &path, const ApplicationOptions &options) {
    if (std::filesystem::is_symlink(path))
        throw std::runtime_error("Refusing a symbolic-link registration target");
    if (std::filesystem::exists(path) && read(path).find(marker(options)) == std::string::npos)
        throw std::runtime_error("Existing desktop registration is owned by another installer");
}
void write(const std::filesystem::path &path, const std::string &value) {
    GError *error = nullptr;
    if (!g_file_set_contents(path.c_str(), value.data(), value.size(), &error)) {
        const std::string message = error ? error->message : "Writing desktop registration failed";
        g_clear_error(&error);
        throw std::runtime_error(message);
    }
}
} // namespace
NotificationResult registerDesktopApplication(const ApplicationOptions &options,
                                              const std::wstring &executable) {
    try {
        const auto binary = std::filesystem::path(unicode::toUtf8(executable));
        if (!binary.is_absolute() || !std::filesystem::is_regular_file(binary))
            return {NotificationStatus::Failed, {}, "Executable must be an existing absolute path"};
        const auto desktop = dataRoot() / "applications" / (options.id + ".desktop");
        const auto service = dataRoot() / "dbus-1/services" / (options.id + ".service");
        checkOwned(desktop, options);
        checkOwned(service, options);
        const bool previousService = std::filesystem::exists(service);
        const auto oldService = previousService ? read(service) : std::string{};
        std::filesystem::create_directories(desktop.parent_path());
        std::filesystem::create_directories(service.parent_path());
        auto name = unicode::toUtf8(options.name);
        if (name.find_first_of("\n\r") != std::string::npos)
            throw std::invalid_argument("Invalid display name");
        GKeyFile *entry = g_key_file_new();
        g_key_file_set_string(entry, "Desktop Entry", "Type", "Application");
        g_key_file_set_string(entry, "Desktop Entry", "Name", name.c_str());
        g_key_file_set_string(entry, "Desktop Entry", "Exec",
                              quoteExec(binary.string(), true).c_str());
        g_key_file_set_boolean(entry, "Desktop Entry", "DBusActivatable", TRUE);
        g_key_file_set_string(entry, "Desktop Entry", "Icon", "application-x-executable");
        g_key_file_set_string(entry, "Desktop Entry", "X-OneUI-Identity", options.id.c_str());
        gsize size = 0;
        gchar *content = g_key_file_to_data(entry, &size, nullptr);
        const std::string desktopText(content, size);
        g_free(content);
        g_key_file_unref(entry);
        write(service, "[D-BUS Service]\nName=" + options.id +
                           "\nExec=" + quoteExec(binary.string(), false) +
                           " --oneui-notification-server\n" + marker(options));
        try {
            write(desktop, desktopText);
        } catch (...) {
            if (previousService)
                write(service, oldService);
            else
                std::filesystem::remove(service);
            throw;
        }
        return {NotificationStatus::Accepted,
                {},
                "Desktop and D-Bus activation entries installed for this user"};
    } catch (const std::exception &error) {
        return {NotificationStatus::Failed, {}, error.what()};
    }
}
NotificationResult unregisterDesktopApplication(const ApplicationOptions &options) {
    try {
        const auto desktop = dataRoot() / "applications" / (options.id + ".desktop");
        const auto service = dataRoot() / "dbus-1/services" / (options.id + ".service");
        checkOwned(desktop, options);
        checkOwned(service, options);
        std::filesystem::remove(desktop);
        std::filesystem::remove(service);
        return {NotificationStatus::Accepted, {}, "Desktop and D-Bus activation entries removed"};
    } catch (const std::exception &error) {
        return {NotificationStatus::Failed, {}, error.what()};
    }
}
} // namespace oneui::platform
