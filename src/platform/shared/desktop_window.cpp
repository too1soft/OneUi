#include "desktop_window.h"
#include "internal/unicode.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkSurface.h"
#include "oneui/color.h"
#include "skia_canvas.h"
#ifdef ONEUI_ENABLE_TEST_FRAME_CAPTURE
#include "include/core/SkStream.h"
#include "include/encode/SkPngEncoder.h"
#endif
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace oneui::platform {
DesktopWindow::DesktopWindow(WindowOptions options)
    : options_(std::move(options)), logicalSize_{static_cast<float>(std::max(1, options_.width)),
                                                 static_cast<float>(std::max(1, options_.height))},
      uiThread_(std::this_thread::get_id()) {
    dirty_ = Rect{0, 0, logicalSize_.width, logicalSize_.height};
}
DesktopWindow::~DesktopWindow() {
    *lifetime_ = false;
    finishClose();
}
void DesktopWindow::assertUiThread() const {
    if (std::this_thread::get_id() != uiThread_)
        throw std::logic_error("OneUI window operation must run on its UI thread; use post()");
}
void DesktopWindow::finishClose() {
    closed_ = true;
    if (content_)
        content_->detachFromOwner(this);
    std::vector<std::function<void()>> posted;
    std::vector<std::function<void(double)>> frames;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        posted.swap(posted_);
        frames.swap(frames_);
        animateContent_ = false;
    }
    // Captured objects may re-enter post() from their destructors. Release
    // cancelled callbacks outside the queue lock, just like the Win32 path.
}
void DesktopWindow::setContent(std::shared_ptr<Widget> widget) {
    assertUiThread();
    if (content_)
        content_->detachFromOwner(this);
    content_ = std::move(widget);
    setCommandRoot(content_);
    if (content_) {
        content_->setTextEnvironment(fontFamily_, dpiScale());
        content_->attachToOwner(
            this, [this] { requestRedraw(); },
            [this](Rect rect) {
                rect.y += chromeHeight();
                redrawRect(rect);
            },
            [this] {
                {
                    std::lock_guard<std::mutex> lock(queueMutex_);
                    animateContent_ = true;
                }
                wake();
            });
    }
    requestRedraw();
}
bool DesktopWindow::requestFocus(Widget *widget, bool visible) {
    assertUiThread();
    if (!content_ || !widget)
        return false;
    bool focused = false;
    if (widget == content_.get() && widget->isFocusable()) {
        widget->onFocusChanged(true);
        widget->setFocusVisible(visible);
        focused = true;
    } else if (auto *view = dynamic_cast<View *>(content_.get())) {
        focused = view->requestFocus(widget, visible);
    }
    if (focused)
        requestRedraw();
    return focused;
}
void DesktopWindow::setClientSizeChangedHandler(ClientSizeChangedHandler handler) {
    sizeChangedHandler_ = std::move(handler);
    sizeChanged_ = true;
    wake();
}
void DesktopWindow::setDefaultFontFamily(std::wstring family) {
    fontFamily_ = std::move(family);
    if (content_) content_->setTextEnvironment(fontFamily_, dpiScale());
    requestRedraw();
}
bool DesktopWindow::post(std::function<void()> callback) {
    if (!callback)
        return false;
    std::lock_guard<std::mutex> lock(queueMutex_);
    if (closed_)
        return false;
    posted_.push_back(std::move(callback));
    wake();
    return true;
}
void DesktopWindow::requestAnimationFrame(std::function<void(double)> callback) {
    if (!callback)
        return;
    std::lock_guard<std::mutex> lock(queueMutex_);
    if (closed_)
        return;
    frames_.push_back(std::move(callback));
    wake();
}
double DesktopWindow::nowMs() {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
void DesktopWindow::dispatchWork() {
    assertUiThread();
    if (closed_)
        return;
    const auto alive = lifetime_;
    std::vector<std::function<void()>> posted;
    std::vector<std::function<void(double)>> frames;
    bool animate = false;
    const double now = nowMs();
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        posted.swap(posted_);
        if (now >= nextFrameMs_) {
            frames.swap(frames_);
            animate = animateContent_;
            animateContent_ = false;
            nextFrameMs_ = now + 16.0;
        }
    }
    for (auto &callback : posted) {
        if (!*alive || closed_)
            return;
        callback();
    }
    if (!*alive || closed_)
        return;
    if (sizeChanged_) {
        sizeChanged_ = false;
        auto handler = sizeChangedHandler_;
        if (handler)
            handler(clientSize());
    }
    for (auto &callback : frames) {
        if (!*alive || closed_)
            return;
        callback(now);
    }
    if (!*alive || closed_)
        return;
    auto content = content_;
    const bool keepAnimating = animate && content && content->tickAnimations(now);
    if (!*alive || closed_)
        return;
    if (keepAnimating) {
        std::lock_guard<std::mutex> lock(queueMutex_);
        animateContent_ = true;
    }
}
bool DesktopWindow::hasWork() const {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return !posted_.empty() || !frames_.empty() || animateContent_ || sizeChanged_;
}
Size DesktopWindow::clientPixelSize() const {
    const auto client = clientSize();
    return {std::ceil(client.width * scale_), std::ceil(client.height * scale_)};
}
void DesktopWindow::resize(Size size, float scale) {
    if (size.width <= 0 || size.height <= 0 || !std::isfinite(scale) || scale <= 0)
        return;
    if (size.width == logicalSize_.width && size.height == logicalSize_.height && scale == scale_)
        return;
    logicalSize_ = size;
    scale_ = scale;
    if (content_) content_->setTextEnvironment(fontFamily_, scale_);
    sizeChanged_ = true;
    pixels_.clear();
    requestRedraw();
}
void DesktopWindow::redrawRect(Rect rect) {
    if (closed_)
        return;
    if (dirty_) {
        const float right = std::max(dirty_->x + dirty_->width, rect.x + rect.width);
        const float bottom = std::max(dirty_->y + dirty_->height, rect.y + rect.height);
        dirty_->x = std::min(dirty_->x, rect.x);
        dirty_->y = std::min(dirty_->y, rect.y);
        dirty_->width = right - dirty_->x;
        dirty_->height = bottom - dirty_->y;
    } else
        dirty_ = rect;
    wake();
}
void DesktopWindow::requestRedraw() { redrawRect({0, 0, logicalSize_.width, logicalSize_.height}); }
const std::vector<std::uint32_t> &DesktopWindow::render() {
    assertUiThread();
    const auto size = surfacePixelSize();
    if (!std::isfinite(size.width) || !std::isfinite(size.height) || size.width <= 0 || size.height <= 0 ||
        size.width > 32768 || size.height > 32768)
        throw std::runtime_error("OneUI backing surface exceeds supported dimensions");
    const int width = static_cast<int>(size.width), height = static_cast<int>(size.height);
    const auto count = static_cast<std::size_t>(width) * height;
    if (count > 128u * 1024u * 1024u)
        throw std::runtime_error("OneUI backing surface exceeds memory limit");
    if (pixels_.size() != count) {
        pixels_.resize(count);
        dirty_ = Rect{0, 0, logicalSize_.width, logicalSize_.height};
    }
    if (!dirty_)
        return pixels_;
    auto surface =
        SkSurfaces::WrapPixels(SkImageInfo::Make(width, height, kBGRA_8888_SkColorType, kPremul_SkAlphaType),
                               pixels_.data(), static_cast<std::size_t>(width) * 4);
    if (!surface)
        throw std::runtime_error("OneUI could not create Skia raster surface");
    auto *sk = surface->getCanvas();
    sk->scale(scale_, scale_);
    auto canvas =
        rendering::makeSkiaCanvas(*sk, &fontFamily_, Rect{0, 0, logicalSize_.width, logicalSize_.height});
    const Rect dirty = *dirty_;
    dirty_.reset(); // Invalidations raised during layout/paint belong to the next frame.
    canvas->clipRect(dirty);
    canvas->fillRect(dirty, colors::Surface, 0);
    paintChrome(*canvas);
    if (content_ && content_->visible()) {
        const auto client = clientSize();
        sk->save();
        sk->clipRect(SkRect::MakeXYWH(0, chromeHeight(), client.width, client.height));
        sk->translate(0, chromeHeight());
        auto clientCanvas =
            rendering::makeSkiaCanvas(*sk, &fontFamily_, Rect{0, 0, client.width, client.height});
        content_->setFrame({0, 0, client.width, client.height});
        content_->setTextEnvironment(fontFamily_, dpiScale());
        content_->paint(*clientCanvas);
        const auto state = inputState();
        if (!preedit_.empty() && state.editable && !state.drawsPreedit && state.identity == preeditIdentity_) {
            auto rect = content_->textInputCaretRect();
            const float fontSize = std::clamp(rect.height * 0.75f, 12.f, 24.f);
            rect.height = std::max(rect.height, fontSize + 4);
            rect.width = std::min(client.width, clientCanvas->measureTextWidth(preedit_, fontSize) + 8);
            rect.x = std::clamp(rect.x, 0.f, std::max(0.f, client.width - rect.width));
            rect.y = std::clamp(rect.y, 0.f, std::max(0.f, client.height - rect.height));
            clientCanvas->fillRect(rect, colors::Surface, 2);
            clientCanvas->drawText(preedit_, rect, Color{30, 41, 59, 255}, fontSize, TextAlign::Left);
            const float x = rect.x + clientCanvas->measureTextWidth(preedit_.substr(0, preeditCaret_), fontSize);
            clientCanvas->fillRect({x, rect.y + 2, 1, rect.height - 4}, Color{37, 99, 235, 255}, 0);
            clientCanvas->fillRect({rect.x, rect.y + rect.height - 1, rect.width, 1}, Color{37, 99, 235, 255}, 0);
        }
        sk->restore();
    }
    return pixels_;
}
void DesktopWindow::prepareLayoutSnapshot() {
    requestRedraw();
    render();
}
#ifdef ONEUI_ENABLE_TEST_FRAME_CAPTURE
bool DesktopWindow::captureFramePng(const std::wstring &path) {
    if (isClosed())
        return false;
    requestRedraw();
    const auto &pixels = render();
    const auto size = surfacePixelSize();
    SkPixmap pixmap(SkImageInfo::Make(static_cast<int>(size.width), static_cast<int>(size.height),
                                      kBGRA_8888_SkColorType, kPremul_SkAlphaType),
                    pixels.data(), static_cast<std::size_t>(size.width) * 4);
    SkFILEWStream output(unicode::toUtf8(path).c_str());
    return output.isValid() && SkPngEncoder::Encode(&output, pixmap, {});
}
#endif
bool DesktopWindow::key(KeyEvent event) {
    if (event.pressed && preeditIdentity_ && (preeditIdentity_ != inputState().identity || preeditSession_ != inputState().session)) {
        preedit_.clear();
        preeditIdentity_ = nullptr; // A new physical key starts a new input transaction.
    }
    const auto alive = lifetime_;
    const auto raw = rawKey_;
    const bool consumed = raw && raw(event);
    if (!*alive || closed_)
        return true;
    rawKeyConsumed_ = consumed;
    if (rawKeyConsumed_)
        return true;
    if (!content_)
        return false;
    auto content = content_;
    if (event.pressed && dispatchCommandKey(event)) return true;
    if (!*alive || closed_) return true;
    return event.pressed ? content->onKeyDown(event) : content->onKeyUp(event);
}
void DesktopWindow::text(const std::wstring &value) {
    const auto state = inputState();
    if (preeditIdentity_ && (preeditIdentity_ != state.identity || preeditSession_ != state.session)) {
        preedit_.clear();
        return;
    }
    preedit_.clear();
    preeditIdentity_ = nullptr;
    auto content = content_;
    if (content)
        content->onTextCommitted(value);
}
void DesktopWindow::composition(std::wstring value, std::size_t caret) {
    if (!value.empty() && preedit_.empty()) {
        const auto state = inputState();
        preeditIdentity_ = state.identity;
        preeditSession_ = state.session;
    }
    if (!value.empty() && (preeditIdentity_ != inputState().identity || preeditSession_ != inputState().session)) return;
    preedit_ = value;
    preeditCaret_ = unicode::boundary(value, caret);
    requestRedraw();
    auto content = content_;
    if (content) content->setTextComposition(std::move(value), caret);
}
void DesktopWindow::mouse(const MouseEvent &event, int action) {
    if (!content_)
        return;
    auto local = event;
    local.position.y -= chromeHeight();
    auto content = content_;
    if (action == 0)
        content->onMouseMove(local);
    else if (action == 1)
        content->onMouseDown(local);
    else
        content->onMouseUp(local);
}
void DesktopWindow::wheel(const MouseWheelEvent &event) {
    auto local = event;
    local.position.y -= chromeHeight();
    auto content = content_;
    if (content)
        content->onMouseWheel(local);
}
void DesktopWindow::focus(bool value) {
    const auto alive = lifetime_;
    if (!value) composition({}, 0);
    if (!*alive || closed_) return;
    auto content = content_;
    if (content)
        content->onFocusChanged(value);
}
CursorKind DesktopWindow::cursor(Point point) const {
    point.y -= chromeHeight();
    return content_ ? content_->cursor(point) : CursorKind::Default;
}
Rect DesktopWindow::caretRect() const {
    Rect rect = content_ ? content_->textInputCaretRect() : Rect{};
    rect.y += chromeHeight();
    return rect;
}
void DesktopWindow::setTitleBarDragMetrics(float h, float r) {
    titleHeight_ = h;
    reservedWidth_ = r;
}
void DesktopWindow::setTitleBarInteractiveInsets(float l, float r) {
    interactiveLeading_ = l;
    interactiveTrailing_ = r;
}
bool DesktopWindow::dragRegion(Point p) const {
    if (!options_.borderless || p.y < 0 || p.y >= titleHeight_ || p.x >= logicalSize_.width - reservedWidth_)
        return false;
    return interactiveLeading_ < 0 || interactiveTrailing_ < 0 || p.x < interactiveLeading_ ||
           p.x >= logicalSize_.width - interactiveTrailing_;
}
} // namespace oneui::platform
