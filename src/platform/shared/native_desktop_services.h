#pragma once
#include "oneui/platform/window.h"
#include <memory>
#include <string>
#include <vector>
#ifdef __linux__
#include <poll.h>
#endif

namespace oneui::platform {
struct NativeFileFilter { std::wstring name, pattern; };
struct NativeFileRequest {
    int mode = 0; // open, save, folder, matching the public C ABI
    std::wstring title, directory, name, extension;
    std::vector<NativeFileFilter> filters;
    bool multiple = false, confirmOverwrite = true;
};
struct NativeFileResult {
    int status = -1; // -1 failure, 0 cancelled, 1 accepted
    std::vector<std::wstring> paths;
};
class NativeTray {
public:
    virtual ~NativeTray() = default;
    virtual bool show() = 0;
    virtual void hide() = 0;
    virtual bool visible() const = 0;
    virtual void setTooltip(const std::wstring&) = 0;
    virtual void setMenu(const std::vector<TrayMenuItem>&) = 0;
    virtual bool setIcon(const TrayIcon&) = 0;
    virtual bool notify(const std::wstring&, const std::wstring&) = 0;
};
class NativeMenuBar {
public:
    virtual ~NativeMenuBar() = default;
    virtual void setMenu(const std::vector<NativeMenuItem>&) = 0;
    virtual void activate() = 0;
};
#ifdef __APPLE__
std::unique_ptr<NativeMenuBar> makeNativeMenuBar(Window*, std::function<void(int)>, std::function<void()> quit);
#endif
NativeFileResult nativeFileDialog(Window*, const NativeFileRequest&);
bool nativeConfirm(Window*, const std::wstring& title, const std::wstring& message);
bool nativePrompt(Window*, const std::wstring& title, const std::wstring& message,
                  const std::wstring& initial, const std::wstring& placeholder, bool password,
                  std::wstring& result);
std::shared_ptr<NativeTray> makeNativeTray(std::function<void(int)> action);
bool nativeDialogsAvailable();
bool nativeTrayAvailable();
unsigned int nativeDoubleClickInterval();
bool nativeAnimationsEnabled();
void dispatchNativeServices();
#ifdef __linux__
// Integrate GLib/DBus sources into the existing event-loop poll, without an
// idle polling timer. Dispatch only after cancelling Wayland's prepared read.
int pollNativeServices(pollfd* descriptors, nfds_t count, int timeout);
#endif
} // namespace oneui::platform
