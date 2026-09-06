#pragma once
#include "oneui/platform/window.h"
#include "oneui/view.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <mutex>
#include <thread>
#include <vector>

namespace oneui::platform {
// Shared controller for Cocoa/X11/Wayland. Native event APIs stay in backends.
class DesktopWindow : public Window {
  public:
    explicit DesktopWindow(WindowOptions options);
    ~DesktopWindow() override;
    void setContent(std::shared_ptr<Widget> widget) override;
    bool requestFocus(Widget *widget, bool focusVisible = true) override;
    void setRawKeyHandler(RawKeyHandler handler) override { rawKey_ = std::move(handler); }
    void setClientSizeChangedHandler(ClientSizeChangedHandler handler) override;
    void setDefaultFontFamily(std::wstring family) override;
    bool post(std::function<void()> callback) override;
    void requestAnimationFrame(std::function<void(double)> callback) override;
    void requestRedraw() override;
    void prepareLayoutSnapshot() override;
#ifdef ONEUI_ENABLE_TEST_FRAME_CAPTURE
    bool captureFramePng(const std::wstring &path) override;
#endif
    Size clientSize() const override {
        return {logicalSize_.width, std::max(1.f, logicalSize_.height - chromeHeight())};
    }
    Size clientPixelSize() const override;
    float dpiScale() const override { return scale_; }
    bool isFullscreen() const override { return options_.fullscreen; }
    void setTitleBarDragMetrics(float height, float reserved) override;
    void setTitleBarInteractiveInsets(float leading, float trailing) override;

    void resize(Size logical, float scale);
    void dispatchWork();
    bool hasWork() const;
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
    Size surfacePixelSize() const {
        return {std::ceil(logicalSize_.width * scale_), std::ceil(logicalSize_.height * scale_)};
    }
    virtual void wake() = 0;
    void assertUiThread() const;
    void finishClose();
    void redrawRect(Rect rect);
    std::shared_ptr<std::atomic<bool>> lifetime_ = std::make_shared<std::atomic<bool>>(true);
    WindowOptions options_;
    std::shared_ptr<Widget> content_;
    Size logicalSize_;
    float scale_ = 1.0f;
    bool shown_ = false;
    std::atomic<bool> closed_{false};
    std::wstring fontFamily_;
    std::wstring preedit_;
    std::size_t preeditCaret_ = 0;
    const void* preeditIdentity_ = nullptr;
    std::uint64_t preeditSession_ = 0;

  private:
    std::thread::id uiThread_;
    mutable std::mutex queueMutex_;
    std::vector<std::function<void()>> posted_;
    std::vector<std::function<void(double)>> frames_;
    bool animateContent_ = false;
    bool sizeChanged_ = false;
    double nextFrameMs_ = 0;
    RawKeyHandler rawKey_;
    bool rawKeyConsumed_ = false;
    ClientSizeChangedHandler sizeChangedHandler_;
    std::optional<Rect> dirty_;
    std::vector<std::uint32_t> pixels_;
    float titleHeight_ = 0, reservedWidth_ = 0, interactiveLeading_ = -1, interactiveTrailing_ = -1;
};
} // namespace oneui::platform
