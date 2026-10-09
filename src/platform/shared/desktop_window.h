#pragma once
#include "oneui/platform/window.h"
#include "internal/build_features.h"
#include "internal/runtime_config.h"
#include "oneui/view.h"
#include "native_desktop_services.h"
#include "gpu_renderer.h"
#include "accessibility.h"
#include "internal/application_menu.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <mutex>
#include <thread>
#include <vector>

namespace oneui::internal { class DeveloperRecorder; }
namespace oneui::platform {
// Shared controller for Cocoa/X11/Wayland. Native event APIs stay in backends.
class DesktopWindow : public Window {
  public:
    explicit DesktopWindow(WindowOptions options);
    ~DesktopWindow() override;
    void setContent(std::shared_ptr<Widget> widget) override;
    void setContentScale(float scale) override;
    bool clientAreaAnimationsEnabled() const override { assertUiThread(); return nativeAnimationsEnabled(); }
    void setFileDropHandler(FileDropHandler handler) override { assertUiThread(); fileDrop_ = std::move(handler); fileDropChanged(bool(fileDrop_)); }
    bool acceptsFileDrop() const { return !isClosed() && !nativeModal() && bool(fileDrop_); }
    void deliverFileDrop(std::vector<std::wstring> files, Point point) {
        if (!acceptsFileDrop() || files.empty()) return;
        deliverFileDropClient(std::move(files), clientPoint(point));
    }
    void deliverFileDropClient(std::vector<std::wstring> files, Point point) {
        if (!acceptsFileDrop() || files.empty()) return;
        const auto handler = fileDrop_; handler(std::move(files), point);
    }
    bool requestFocus(Widget *widget, bool focusVisible = true) override;
    void setRawKeyHandler(RawKeyHandler handler) override { rawKey_ = std::move(handler); }
    void setClientSizeChangedHandler(ClientSizeChangedHandler handler) override;
    void setDefaultFontFamily(std::wstring family) override;
    void setActivationChangedHandler(ActivationChangedHandler handler) override { assertUiThread(); activation_ = std::move(handler); }
    void setOnCloseRequested(std::function<void(bool)> handler) override { assertUiThread(); closeRequested_ = std::move(handler); }
    void requestClose() override;
    void setCloseToTray(bool enabled) override;
    bool setTrayEnabled(bool enabled) override;
    bool trayIconVisible() const override;
    bool setTrayMenu(const std::vector<TrayMenuItem>& items) override;
    bool setTrayIcon(const TrayIcon& icon) override;
    bool setMenuBar(const std::vector<NativeMenuItem>& items) override;
    void setOnMenuAction(std::function<void(int)> action) override { assertUiThread(); menuAction_ = std::move(action); }
    bool setTrayTooltip(const std::wstring& text) override;
    void setOnTrayAction(std::function<void(int)> handler) override { assertUiThread(); trayAction_ = std::move(handler); }
    bool notifyTray(const std::wstring& title, const std::wstring& message) override;
    bool hide() override { assertUiThread();if(isClosed())return false;hideNative();queueStateChange();return true; }
    bool setInputEnabled(bool enabled) override { assertUiThread();if(isClosed())return false;inputEnabled_=enabled;return true; }
    virtual void hideNative() = 0;
    virtual void pumpNativeEvents(int timeout) = 0;
    virtual std::string exportDialogParent() { return {}; }
    void requestTrayExit();
    std::weak_ptr<std::atomic<bool>> nativeLifetime() const { return lifetime_; }
    void trackTray(const std::shared_ptr<NativeTray>& tray) { externalTrays_.push_back(tray); }
    bool nativeModal() const { return modalDepth_ != 0 || !inputEnabled_; }
    std::shared_ptr<AccessibilityContext> accessibilityContext();
    virtual std::optional<Rect> accessibilityScreenBounds(Rect) const { return std::nullopt; }
    void enterNativeModal() { assertUiThread(); ++modalDepth_; }
    void leaveNativeModal() { assertUiThread(); if (modalDepth_) --modalDepth_; }
    bool post(std::function<void()> callback) override;
    void requestAnimationFrame(std::function<void(double)> callback) override;
    void requestRedraw() override;
    RendererInfo rendererInfo() const override { assertUiThread(); return rendererInfo_; }
    RendererMemoryInfo rendererMemoryInfo() const override;
    bool setDeveloperTools(DeveloperOptions options) override;
    DeveloperSnapshot developerSnapshot() const override;
    bool exportDeveloperReport(const std::wstring& path) const override;
    void prepareLayoutSnapshot() override;
#ifdef ONEUI_ENABLE_TEST_FRAME_CAPTURE
    bool captureFramePng(const std::wstring &path) override;
#endif
    Size clientSize() const override {
        return {logicalSize_.width / contentScale_, std::max(1.f, logicalSize_.height - chromeHeight()) / contentScale_};
    }
    Size clientPixelSize() const override;
    float dpiScale() const override { return scale_ * contentScale_; }
    bool isFullscreen() const override { return options_.fullscreen; }
    void setTitleBarDragMetrics(float height, float reserved) override;
    void setTitleBarInteractiveInsets(float leading, float trailing) override;

    void resize(Size logical, float scale);
    void dispatchWork();
    bool hasWork() const;
    // Time until queued animation work is due. Empty means the native loop can
    // sleep until an event; do not add a full frame delay after painting.
    std::optional<double> workDelayMs() const;
    bool isClosed() const { return closed_; }
    bool key(KeyEvent event);
    bool rawKeyConsumed() const { return rawKeyConsumed_; }
    void text(const std::wstring &value);
    virtual void mouse(const MouseEvent &event, int action);
    void wheel(const MouseWheelEvent &event);
    void focus(bool focused);
    CursorKind cursor(Point point) const;
    bool dragRegion(Point point) const;
    Rect caretRect() const;
    TextInputState inputState() const { return content_ ? content_->textInputState() : TextInputState{}; }
    void composition(std::wstring value, std::size_t caret);
    void replaceText(std::size_t start, std::size_t end, const std::wstring &value) {
        if (isClosed() || nativeModal()) return;
        auto content = content_;
        if (content)
            content->replaceTextRange(start, end, value);
    }
    const std::vector<std::uint32_t> &render();
    bool needsPaint() const { return dirty_.has_value(); }
    static double nowMs();

  protected:
    virtual float chromeHeight() const { return 0; }
    virtual void paintChrome(Canvas &) {}
    virtual void fileDropChanged(bool) {}
    virtual void contentScaleChanged() {}
    void setFrameIntervalMs(double interval);
    Point clientPoint(Point point) const { return {point.x / contentScale_, (point.y - chromeHeight()) / contentScale_}; }
    Size surfacePixelSize() const {
        return {std::ceil(logicalSize_.width * scale_), std::ceil(logicalSize_.height * scale_)};
    }
    virtual void wake() = 0;
    void initializeGpu(void* display, void* window);
    void initializeAccessibility(void* nativeView = nullptr);
    bool paintGpu();
    void releaseGpu();
    bool hasGpu() const { return gpu_ != nullptr; }
    void assertUiThread() const;
    void finishClose();
    void redrawRect(Rect rect);
    std::shared_ptr<std::atomic<bool>> lifetime_ = std::make_shared<std::atomic<bool>>(true);
    WindowOptions options_;
    std::shared_ptr<Widget> content_;
    Size logicalSize_;
    float scale_ = 1.0f;
    float contentScale_ = 1.0f;
    bool shown_ = false;
    bool inputEnabled_=true,active_=false;
    std::atomic<bool> closed_{false};
    std::wstring fontFamily_;
    unsigned int serviceCapabilities() const;
    bool hideForTray();
    std::wstring preedit_;
    std::size_t preeditCaret_ = 0;
    const void* preeditIdentity_ = nullptr;
    std::uint64_t preeditSession_ = 0;

  private:
    DeveloperFrame paintScene(SkCanvas& canvas, bool fullFrame);
    std::unique_ptr<GpuRenderer> gpu_;
    std::shared_ptr<AccessibilityContext> accessibilityContext_;
    std::unique_ptr<NativeAccessibility> accessibility_;
    RendererInfo rendererInfo_;
    const internal::RuntimeConfig runtimeConfig_ = internal::RuntimeConfig::fromEnvironment();
#if ONEUI_DEVELOPER_TOOLS
    std::shared_ptr<internal::DeveloperRecorder> developerTools_;
#endif
    std::thread::id uiThread_;
    mutable std::mutex queueMutex_;
    std::vector<std::function<void()>> posted_;
    std::vector<std::function<void(double)>> frames_;
    bool animateContent_ = false;
    bool sizeChanged_ = false;
    double nextFrameMs_ = 0;
    double frameIntervalMs_ = 16.0;
    RawKeyHandler rawKey_;
    FileDropHandler fileDrop_;
    bool rawKeyConsumed_ = false;
    ClientSizeChangedHandler sizeChangedHandler_;
    ActivationChangedHandler activation_;
    std::function<void(bool)> closeRequested_;
    std::function<void(int)> trayAction_;
    std::shared_ptr<NativeTray> tray_;
    std::vector<std::weak_ptr<NativeTray>> externalTrays_;
    std::vector<TrayMenuItem> trayMenu_;
    std::wstring trayTooltip_;
    TrayIcon trayIcon_;
    std::shared_ptr<internal::ApplicationMenuHost> menuHost_;
    std::shared_ptr<Widget> applicationContent_;
    std::unique_ptr<NativeMenuBar> nativeMenuBar_;
    std::function<void(int)> menuAction_;
    bool closeToTray_ = false, explicitTray_ = false;
    unsigned int modalDepth_ = 0;
    std::optional<Rect> dirty_;
    std::vector<std::uint32_t> pixels_;
    float titleHeight_ = 0, reservedWidth_ = 0, interactiveLeading_ = -1, interactiveTrailing_ = -1;
};
} // namespace oneui::platform
