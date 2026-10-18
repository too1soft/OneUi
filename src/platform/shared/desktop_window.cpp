#include "desktop_window.h"
#include "title_bar_hit.h"
#include "internal/unicode.h"
#include "internal/frame_profile.h"
#include "internal/developer_tools.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkGraphics.h"
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
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace oneui::platform {
DesktopWindow::DesktopWindow(WindowOptions options)
    : options_(std::move(options)), logicalSize_{static_cast<float>(std::max(1, options_.width)),
                                                 static_cast<float>(std::max(1, options_.height))},
      uiThread_(std::this_thread::get_id()) {
    dirty_ = Rect{0, 0, logicalSize_.width, logicalSize_.height};
#if ONEUI_DEVELOPER_TOOLS
    if (runtimeConfig_.developerTools) developerTools_ = std::make_shared<internal::DeveloperRecorder>(DeveloperOptions{});
#endif
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
    releaseGpu();
    closed_ = true;
    *lifetime_ = false;
    accessibility_.reset();
    nativeMenuBar_.reset();
    if (tray_) tray_->hide();
    tray_.reset();
    for (auto& weak : externalTrays_) if (auto tray = weak.lock()) tray->hide();
    externalTrays_.clear();
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
unsigned int DesktopWindow::serviceCapabilities() const {
    return WindowCapabilityMenuBar |
#ifdef __APPLE__
           WindowCapabilityTrayTooltip |
#endif
           (nativeDialogsAvailable() ? WindowCapabilityNativeDialogs : 0) |
           (nativeTrayAvailable() ? WindowCapabilityTray : 0);
}
bool DesktopWindow::setTrayEnabled(bool enabled) {
    assertUiThread();
    if (isClosed()) return false;
    if (!enabled) {
        explicitTray_ = false;
        if (tray_) tray_->hide();
        tray_.reset();
        return true;
    }
    if (!tray_) {
        const auto weak = nativeLifetime();
        tray_ = makeNativeTray([this, weak](int action) {
            const auto alive = weak.lock();
            if (!alive || !*alive || isClosed()) return;
            if (action == static_cast<int>(kTrayCommandShow)) { activate(); return; }
            if (action == static_cast<int>(kTrayCommandExit)) {
                requestTrayExit();
                return;
            }
            const auto handler = trayAction_;
            if (handler) handler(action);
        });
        if (!tray_) return false;
        tray_->setTooltip(trayTooltip_.empty() ? options_.title : trayTooltip_);
        tray_->setMenu(trayMenu_);
        if (!tray_->setIcon(trayIcon_)) { tray_.reset(); return false; }
    }
    if (!tray_->show()) { tray_.reset(); return false; }
    explicitTray_ = true;
    return true;
}
void DesktopWindow::requestTrayExit() {
    assertUiThread();
    if(dispatchCloseRequest())return;
    const auto handler = closeRequested_;
    if (handler) handler(true); else { closeToTray_ = false; close(); }
}
void DesktopWindow::setCloseToTray(bool enabled) {
    assertUiThread();
    if (enabled) {
        const bool explicitBefore = explicitTray_;
        closeToTray_ = setTrayEnabled(true);
        explicitTray_ = explicitBefore;
    } else {
        closeToTray_ = false;
        if (!explicitTray_) setTrayEnabled(false);
    }
}
bool DesktopWindow::trayIconVisible() const { return tray_ && tray_->visible(); }
bool DesktopWindow::hideForTray() {
    if (!closeToTray_) return false;
    if (!trayIconVisible()) {
        const bool explicitBefore = explicitTray_;
        const bool restored = setTrayEnabled(true);
        explicitTray_ = explicitBefore;
        if (!restored) return false;
    }
    hideNative();
    return true;
}
bool DesktopWindow::setTrayMenu(const std::vector<TrayMenuItem>& items) {
    assertUiThread();
    if (isClosed() || !validateNativeMenu(items)) return false;
    trayMenu_ = items;
    if (tray_) tray_->setMenu(items);
    return true;
}
bool DesktopWindow::setTrayIcon(const TrayIcon& icon) {
    assertUiThread();
    if (isClosed() || !validateTrayIcon(icon)) return false;
    if (tray_ && !tray_->setIcon(icon)) return false;
    trayIcon_ = icon; return true;
}
bool DesktopWindow::setMenuBar(const std::vector<NativeMenuItem>& items) {
    assertUiThread();
    if (isClosed() || !validateNativeMenu(items)) return false;
    const auto weak = nativeLifetime();
    auto action = [this,weak](int id) { const auto alive=weak.lock(); if(alive && *alive && !isClosed() && !nativeModal()) {
        for(auto node=content_;node;node=node->activeFocusChild())if(node->isCommandBoundary())return;
        auto callback=menuAction_; if(callback)callback(id); } };
#ifdef __APPLE__
    if (items.empty()) { nativeMenuBar_.reset(); return true; }
    if (!nativeMenuBar_) nativeMenuBar_ = makeNativeMenuBar(this,action,[this,weak]{const auto alive=weak.lock();if(alive && *alive && !isClosed())requestTrayExit();});
    if (!nativeMenuBar_) return false;
    nativeMenuBar_->setMenu(items);
#else
    if (!menuHost_) {
        if (items.empty()) return true;
        menuHost_=std::make_shared<internal::ApplicationMenuHost>();
        menuHost_->setAction(action);
        setContent(applicationContent_);
    }
    menuHost_->setItems(items);
    requestRedraw();
#endif
    return true;
}
bool DesktopWindow::setTrayTooltip(const std::wstring& text) {
    assertUiThread();
    if (isClosed()) return false;
    trayTooltip_ = text;
    if (tray_) tray_->setTooltip(text);
    return true;
}
bool DesktopWindow::notifyTray(const std::wstring& title, const std::wstring& message) {
    assertUiThread();
    return tray_ && tray_->visible() && tray_->notify(title, message);
}
void DesktopWindow::requestClose() {
    assertUiThread();
    if (isClosed() || nativeModal()) return;
    const auto handler = closeRequested_;
    if (handler) { handler(false); return; }
    if (closeToTray_ && trayIconVisible()) { hideNative(); return; }
    close();
}
void DesktopWindow::setContent(std::shared_ptr<Widget> widget) {
    assertUiThread();
    applicationContent_ = widget;
    if (menuHost_) { menuHost_->setApplicationContent(std::move(widget)); widget = menuHost_; }
    if (content_)
        content_->detachFromOwner(this);
    content_ = std::move(widget);
    setCommandRoot(content_);
    if (content_) {
        content_->setTextEnvironment(fontFamily_, dpiScale());
        content_->attachToOwner(
            this, [this] { requestRedraw(); },
            [this](Rect rect) {
                rect.x *= contentScale_; rect.y = rect.y * contentScale_ + chromeHeight();
                rect.width *= contentScale_; rect.height *= contentScale_;
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
void DesktopWindow::setContentScale(float scale) {
    assertUiThread();
    if (!std::isfinite(scale) || scale < 0.25f || scale > 4.f || scale == contentScale_) return;
    contentScale_ = scale;
    contentScaleChanged();
    if (content_) content_->setTextEnvironment(fontFamily_, dpiScale());
    sizeChanged_ = true;
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
        if (now >= nextFrameMs_ && (!frames_.empty() || animateContent_)) {
            frames.swap(frames_);
            animate = animateContent_;
            animateContent_ = false;
            nextFrameMs_ = nextFrameMs_ + frameIntervalMs_ > now
                ? nextFrameMs_ + frameIntervalMs_ : now + frameIntervalMs_;
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
    if (accessibility_) accessibility_->sync();
}
std::shared_ptr<AccessibilityContext> DesktopWindow::accessibilityContext() {
    assertUiThread();
    if (!accessibilityContext_) {
        auto context = std::make_shared<AccessibilityContext>();
        const auto weak = nativeLifetime();
        context->valid = [weak] { auto alive = weak.lock(); return alive && alive->load(); };
        context->exposed = [this] { return shown_ && !nativeModal(); };
        context->root = [this] { return content_; };
        context->title = [this] { return options_.title; };
        context->focus = [this](Widget* widget) { return requestFocus(widget); };
        context->bounds = [this](Rect r, bool screen) -> std::optional<Rect> {
            if (screen) return accessibilityScreenBounds(r);
            return Rect{r.x * dpiScale(), (r.y * contentScale_ + chromeHeight()) * scale_,
                r.width * dpiScale(), r.height * dpiScale()};
        };
        accessibilityContext_ = std::move(context);
    }
    return accessibilityContext_;
}
void DesktopWindow::initializeAccessibility(void* nativeView) {
    if (!accessibility_ && !isClosed()) accessibility_ = makeNativeAccessibility(accessibilityContext(), nativeView);
}
bool DesktopWindow::hasWork() const {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return !posted_.empty() || !frames_.empty() || animateContent_ || sizeChanged_;
}
std::optional<double> DesktopWindow::workDelayMs() const {
    std::lock_guard<std::mutex> lock(queueMutex_);
    if (!posted_.empty() || sizeChanged_) return 0.0;
    if (!frames_.empty() || animateContent_) return std::max(0.0, nextFrameMs_ - nowMs());
    return std::nullopt;
}
void DesktopWindow::setFrameIntervalMs(double interval) {
    assertUiThread();
    if (!std::isfinite(interval) || interval < 1.0 || interval > 1000.0) return;
    std::lock_guard<std::mutex> lock(queueMutex_);
    if (frameIntervalMs_ == interval) return;
    frameIntervalMs_ = interval;
    nextFrameMs_ = std::min(nextFrameMs_, nowMs() + interval);
}
Size DesktopWindow::clientPixelSize() const {
    const auto client = clientSize();
    return {std::ceil(client.width * dpiScale()), std::ceil(client.height * dpiScale())};
}
void DesktopWindow::resize(Size size, float scale) {
    queueStateChange();
    if (size.width <= 0 || size.height <= 0 || !std::isfinite(scale) || scale <= 0)
        return;
    if (size.width == logicalSize_.width && size.height == logicalSize_.height && scale == scale_)
        return;
    logicalSize_ = size;
    scale_ = scale;
    if (content_) content_->setTextEnvironment(fontFamily_, dpiScale());
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
void DesktopWindow::initializeGpu(void* display, void* window) {
    assertUiThread();
    std::string failure = "gpu-disabled-at-build";
    if (!runtimeConfig_.gpuEnabled) failure = "disabled-by-environment";
    else {
#ifdef ONEUI_ENABLE_GPU
#ifdef __APPLE__
        gpu_ = makeMetalRenderer(window, failure);
#else
        gpu_ = makeEglRenderer(display, window, backend() == WindowBackend::Wayland, failure);
#endif
#endif
    }
    rendererInfo_.backend = gpu_ ? gpu_->backend() : RenderBackend::Software;
    rendererInfo_.device = gpu_ ? gpu_->device() : "Skia CPU raster";
    rendererInfo_.reason = gpu_ ? (gpu_->backend() == RenderBackend::Metal ? "native-metal" : "native-opengl") : failure;
    if (gpu_ && (rendererInfo_.device.find("llvmpipe") != std::string::npos ||
        rendererInfo_.device.find("softpipe") != std::string::npos || rendererInfo_.device.find("SwiftShader") != std::string::npos))
        rendererInfo_.reason = "software-opengl-driver";
    rendererInfo_.vsync = gpu_ && gpu_->vsync();
    ++rendererInfo_.backendChanges;
}
void DesktopWindow::releaseGpu() { gpu_.reset(); }
bool DesktopWindow::setDeveloperTools(DeveloperOptions options) {
    assertUiThread();
#if ONEUI_DEVELOPER_TOOLS
    developerTools_ = options.enabled ? std::make_shared<internal::DeveloperRecorder>(options) : nullptr;
    requestRedraw(); return true;
#else
    (void)options; return false;
#endif
}
DeveloperSnapshot DesktopWindow::developerSnapshot() const {
    assertUiThread();
#if ONEUI_DEVELOPER_TOOLS
    auto snapshot = developerTools_ ? developerTools_->snapshot() : DeveloperSnapshot{};
    snapshot.supported = true;
#else
    DeveloperSnapshot snapshot;
#endif
    snapshot.backend = rendererInfo_.backend == RenderBackend::Metal ? "metal" : rendererInfo_.backend == RenderBackend::OpenGL ? "opengl" : rendererInfo_.backend == RenderBackend::Software ? "software" : "unknown";
    return snapshot;
}
bool DesktopWindow::exportDeveloperReport(const std::wstring& path) const {
    assertUiThread();
#if ONEUI_DEVELOPER_TOOLS
    try {
        std::ofstream file(std::filesystem::u8path(unicode::toUtf8(path)), std::ios::binary);
        file << developerReportJson(developerSnapshot()); file.flush(); return bool(file);
    } catch (...) { return false; }
#else
    (void)path; return false;
#endif
}
bool DesktopWindow::paintGpu() {
    if (!gpu_) return false;
    const auto start = nowMs();
    if (auto* canvas = gpu_->beginFrame(surfacePixelSize(), scale_)) {
#if ONEUI_DEVELOPER_TOOLS
        const auto diagnostics = developerTools_;
#endif
        DeveloperFrame timing;
        try { timing = paintScene(*canvas, true); }
        catch (...) {
            releaseGpu();
            rendererInfo_.backend = RenderBackend::Software;
            rendererInfo_.reason = "paint-exception";
            rendererInfo_.device = "Skia CPU raster";
            rendererInfo_.vsync = false;
            ++rendererInfo_.backendChanges;
            requestRedraw();
            throw;
        }
        const auto painted = nowMs();
        if (gpu_->present()) {
            ++rendererInfo_.paints;
            rendererInfo_.contentMs += painted - start;
            rendererInfo_.submitMs += nowMs() - painted;
            rendererInfo_.paintMs += nowMs() - start;
            timing.submitMs = nowMs() - painted;
#if ONEUI_DEVELOPER_TOOLS
            if (diagnostics) diagnostics->finishFrame(nowMs(), animateContent_, timing);
#endif
            return true;
        }
    }
    gpu_.reset();
    rendererInfo_.backend = RenderBackend::Software;
    rendererInfo_.device = "Skia CPU raster";
    rendererInfo_.reason = "gpu-frame-failed";
    rendererInfo_.vsync = false;
    ++rendererInfo_.backendChanges;
    requestRedraw();
    return false;
}
RendererMemoryInfo DesktopWindow::rendererMemoryInfo() const {
    assertUiThread();
    auto result = gpu_ ? gpu_->memoryInfo() : RendererMemoryInfo{};
    result.cpuCachesAvailable = true;
    result.cpuFontCacheBytes = SkGraphics::GetFontCacheUsed();
    result.cpuResourceCacheBytes = SkGraphics::GetResourceCacheTotalBytesUsed();
    result.retainedSurfaceBytes = pixels_.capacity() * sizeof(std::uint32_t);
    return result;
}
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
    const auto start = nowMs();
#if ONEUI_DEVELOPER_TOOLS
    const auto diagnostics = developerTools_;
#endif
    const auto timing = paintScene(*surface->getCanvas(), false);
#if ONEUI_DEVELOPER_TOOLS
    if (diagnostics) diagnostics->finishFrame(nowMs(), animateContent_, timing);
#endif
    if (!gpu_) {
        ++rendererInfo_.paints;
        rendererInfo_.contentMs += nowMs() - start;
        rendererInfo_.paintMs += nowMs() - start;
    }
    return pixels_;
}
DeveloperFrame DesktopWindow::paintScene(SkCanvas& target, bool fullFrame) {
#if ONEUI_DEVELOPER_TOOLS
    const auto diagnostics = developerTools_;
#endif
#if ONEUI_DEVELOPER_TOOLS
    internal::DiagnosticSession diagnosticSession(diagnostics.get());
    const bool showOverlay = diagnostics && diagnostics->options.overlay;
    internal::FrameProfileSession profile{bool(diagnostics)};
#else
    const bool showOverlay = false;
    internal::FrameProfileSession profile{false};
#endif
    const auto start = nowMs();
#if ONEUI_DEVELOPER_TOOLS
    if (diagnostics) diagnostics->beginFrame(start);
#endif
    auto* sk = &target;
    SkAutoCanvasRestore restore(sk, true);
    sk->scale(scale_, scale_);
    auto canvas =
        rendering::makeSkiaCanvas(*sk, &fontFamily_, Rect{0, 0, logicalSize_.width, logicalSize_.height});
    const Rect dirty = fullFrame || showOverlay ? Rect{0, 0, logicalSize_.width, logicalSize_.height} : *dirty_;
    dirty_.reset(); // Invalidations raised during layout/paint belong to the next frame.
    canvas->clipRect(dirty);
    canvas->fillRect(dirty, colors::Surface, 0);
    paintChrome(*canvas);
    if (content_ && content_->visible()) {
        const auto client = clientSize();
        sk->save();
        sk->clipRect(SkRect::MakeXYWH(0, chromeHeight(), logicalSize_.width, logicalSize_.height - chromeHeight()));
        sk->translate(0, chromeHeight());
        sk->scale(contentScale_, contentScale_);
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
    DeveloperFrame timing;
    timing.contentMs = nowMs() - start;
    timing.layoutMs = profile.data.ms[0]; timing.textLayoutMs = profile.data.ms[1];
    timing.fillMs = profile.data.ms[2]; timing.pathMs = profile.data.ms[3];
#if ONEUI_DEVELOPER_TOOLS
    if (diagnostics) {
        if (diagnostics->options.overlay) {
            const auto& summary = diagnostics->summary();
            const Rect panel{std::max(4.f, logicalSize_.width - 400), std::max(4.f, logicalSize_.height - 142),
                std::max(1.f, std::min(392.f, logicalSize_.width - 8)), 134};
            canvas->fillRect(panel, {20, 29, 25, 250}, 10);
            auto line = [&](std::wstring text, float y) { canvas->drawText(text, {panel.x + 12, panel.y + y, panel.width - 24, 22}, {220, 230, 224}, 12, TextAlign::Left); };
            line(L"OneUI DevTools  F12 / Ctrl+F12", 8);
            std::wostringstream timingText; timingText << std::fixed << std::setprecision(2) << L"CPU paint: " << summary.last.paintMs << L" ms";
            line(timingText.str(), 32);
            line(L"Slow paints: " + std::to_wstring(summary.slowPaints) + L"  Delays: " + std::to_wstring(summary.animationDelays), 56);
            line(unicode::fromUtf8(summary.last.hottest.component), 80);
            line(unicode::fromUtf8(developerSnapshot().backend + " / " + rendererInfo_.device), 104);
        }
    }
#endif
    // Layout happens during paint; publish the resulting tree before the
    // native event loop can go idle after this frame.
    if (accessibility_) accessibility_->sync();
    return timing;
}
void DesktopWindow::prepareLayoutSnapshot() {
    requestRedraw();
    render();
    requestRedraw(); // Layout snapshots must not consume the pending presentation.
}
#ifdef ONEUI_ENABLE_TEST_FRAME_CAPTURE
bool DesktopWindow::captureFramePng(const std::wstring &path) {
    if (isClosed())
        return false;
    requestRedraw();
    const auto &pixels = render();
    requestRedraw();
    const auto size = surfacePixelSize();
    SkPixmap pixmap(SkImageInfo::Make(static_cast<int>(size.width), static_cast<int>(size.height),
                                      kBGRA_8888_SkColorType, kPremul_SkAlphaType),
                    pixels.data(), static_cast<std::size_t>(size.width) * 4);
    SkFILEWStream output(unicode::toUtf8(path).c_str());
    return output.isValid() && SkPngEncoder::Encode(&output, pixmap, {});
}
#endif
bool DesktopWindow::key(KeyEvent event) {
    if (isClosed() || nativeModal()) return true;
#if ONEUI_DEVELOPER_TOOLS
    if (developerTools_ && event.pressed && event.virtualKey == 123 && !event.shift && !event.alt && preedit_.empty()) {
        if (!event.repeat) {
            if (event.control || event.win) exportDeveloperReport(L"oneui-devtools.json");
            else developerTools_->options.overlay = !developerTools_->options.overlay;
            requestRedraw();
        }
        return true;
    }
#endif
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
    if (isClosed() || nativeModal()) return;
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
    // IME replacement/preedit callbacks can arrive independently of key events.
    // Cancellation must still clear an existing composition during a modal.
    if (isClosed() || (nativeModal() && !value.empty())) return;
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
    if (nativeModal()) return;
    if (!content_)
        return;
    auto local = event;
    local.position = clientPoint(local.position);
    auto content = content_;
    if (action == 0)
        content->onMouseMove(local);
    else if (action == 1)
        content->onMouseDown(local);
    else
        content->onMouseUp(local);
}
void DesktopWindow::wheel(const MouseWheelEvent &event) {
    if (nativeModal()) return;
    auto local = event;
    local.position = clientPoint(local.position);
    auto content = content_;
    if (content)
        content->onMouseWheel(local);
}
void DesktopWindow::focus(bool value) {
    active_=value;queueStateChange();
    if (value && nativeMenuBar_) nativeMenuBar_->activate();
    const auto alive = lifetime_;
    if (!value) composition({}, 0);
    if (!*alive || closed_) return;
    auto content = content_;
    if (content)
        content->onFocusChanged(value);
    if (!*alive || closed_) return;
    const auto handler = activation_;
    if (handler) handler(value);
}
CursorKind DesktopWindow::cursor(Point point) const {
    point = clientPoint(point);
    return content_ ? content_->cursor(point) : CursorKind::Default;
}
Rect DesktopWindow::caretRect() const {
    Rect rect = content_ ? content_->textInputCaretRect() : Rect{};
    rect.x *= contentScale_; rect.y = rect.y * contentScale_ + chromeHeight();
    rect.width *= contentScale_; rect.height *= contentScale_;
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
    if (titleBarControlAt(content_, clientPoint(p))) return false;
    return interactiveLeading_ < 0 || interactiveTrailing_ < 0 || p.x < interactiveLeading_ ||
           p.x >= logicalSize_.width - interactiveTrailing_;
}
} // namespace oneui::platform
