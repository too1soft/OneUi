#pragma once

#include "oneui/clipboard.h"
#include "oneui/export.h"
#include "oneui/geometry.h"
#include "oneui/widget.h"

#include <functional>
#include <memory>
#include <string>

namespace oneui {

using NativeWindowHandle = void*;

struct WindowOptions {
    std::wstring title;
    int width = 1280;
    int height = 800;
    bool visible = false;
    bool borderless = false;
    bool fullscreen = false;
    bool topmost = false;
    bool resizable = true;
};

// Round-trippable native window state for application persistence. Bounds are
// the restored outer frame in platform screen coordinates, not client layout
// units. Callers should treat them as opaque persistence values and let OneUI
// validate visibility when restoring them.
struct WindowPlacement {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    bool maximized = false;
};

class ONEUI_API Window {
public:
    using RawKeyHandler = std::function<bool(const KeyEvent&)>;
    using ClientSizeChangedHandler = std::function<void(Size)>;

    virtual ~Window() = default;

    static std::unique_ptr<Window> create(std::wstring title, int width, int height);
    static std::unique_ptr<Window> create(WindowOptions options);

    virtual void setContent(std::shared_ptr<Widget> widget) = 0;
    // Establishes the focus path from the window root to a descendant widget.
    // Returns false when the widget is not part of the active tree.
    virtual bool requestFocus(Widget* widget, bool focusVisible = true) {
        (void)widget;
        (void)focusVisible;
        return false;
    }
    // Runs before focused-widget key routing. Returning true consumes the key
    // event, which lets applications implement global shortcuts without
    // duplicating platform message hooks or intercepting IME text input.
    virtual void setRawKeyHandler(RawKeyHandler handler) { (void)handler; }
    // Delivers coalesced logical client-size updates on the UI thread. Backends
    // should avoid invoking this once per native resize message when several
    // messages can be represented by the latest size in one frame.
    virtual void setClientSizeChangedHandler(ClientSizeChangedHandler handler) {
        (void)handler;
    }
    // Sets the application font used whenever controls request the default
    // UI family. An empty value restores the platform default. The value is
    // owned by the window and takes effect on the next paint.
    virtual void setDefaultFontFamily(std::wstring family) { (void)family; }
    // Creates the native handle on the calling UI thread without showing it.
    virtual void initialize() = 0;
    virtual void show() = 0;
    // Restores a minimized window when needed and requests foreground attention.
    // Platforms without a foreground concept retain the regular show behavior.
    virtual void activate() { show(); }
    virtual int run() = 0;
    virtual void close() = 0;
    virtual void minimize() = 0;
    virtual void requestRedraw() = 0;
    // Commits pending layout before diagnostics inspect widget geometry.
    // Backends should perform the same synchronous layout/paint pass used by
    // presentation so snapshots never expose stale frames after tree changes.
    virtual void prepareLayoutSnapshot() { requestRedraw(); }
    // Returns false when the window no longer accepts UI work.
    virtual bool post(std::function<void()> callback) = 0;
    virtual void requestAnimationFrame(std::function<void(double nowMs)> callback) = 0;
    virtual NativeWindowHandle nativeHandle() const = 0;
    // Logical client size in OneUI device-independent pixels.
    virtual Size clientSize() const = 0;
    // Physical backing/presentation size in platform pixels.
    virtual Size clientPixelSize() const = 0;
    // Physical pixels per OneUI logical pixel for the current window.
    virtual float dpiScale() const = 0;
    virtual void setTitle(std::wstring title) = 0;
    virtual void setFullscreen(bool enabled) = 0;
    virtual void setBorderless(bool enabled) = 0;
    virtual void toggleMaximize() = 0;
    virtual bool getWindowPlacement(WindowPlacement& placement) const {
        (void)placement;
        return false;
    }
    virtual bool setWindowPlacement(const WindowPlacement& placement) {
        (void)placement;
        return false;
    }
    // 配置无边框窗口的拖拽命中区（逻辑像素）：标题栏高度 + 右侧不可拖拽预留宽（留给窗口按钮/账号按钮）。
    // 默认空实现，仅需拖拽命中的后端（Win32）覆盖。
    virtual void setTitleBarDragMetrics(float titleBarHeight, float reservedButtonWidth) {
        (void)titleBarHeight;
        (void)reservedButtonWidth;
    }
    // 配置标题栏中间的客户区命中范围（逻辑像素）。该范围优先于 caption 拖拽，
    // 用于承载搜索框、标签页等可交互附件；任一参数为负数时恢复整段标题栏拖拽。
    virtual void setTitleBarInteractiveInsets(float leadingWidth, float trailingWidth) {
        (void)leadingWidth;
        (void)trailingWidth;
    }
    // 设置窗口整体圆角半径（逻辑像素，0 = 直角）。默认空实现，Win32 覆盖。
    virtual void setCornerRadius(float radiusLogical) { (void)radiusLogical; }
    // 开启后，点击关闭改为隐藏到托盘（而非退出）。默认空实现，Win32 覆盖。
    virtual void setCloseToTray(bool closeToTray) { (void)closeToTray; }
};

// 托盘图标回调消息（Shell_NotifyIcon 的 uCallbackMessage）：托盘在窗口 HWND 上注册，
// 事件会投递到窗口过程，故 capi 与 Win32Window 需共用同一常量。WM_APP = 0x8000。
inline constexpr unsigned int kTrayCallbackMessage = 0x8000u + 0x21u;
// 托盘右键菜单命令 ID。
inline constexpr unsigned int kTrayCommandShow = 0xE001u;
inline constexpr unsigned int kTrayCommandExit = 0xE002u;

class ONEUI_API SystemClipboard final : public Clipboard {
public:
    void setText(std::wstring text) override;
    std::wstring text() const override;
};

} // namespace oneui
