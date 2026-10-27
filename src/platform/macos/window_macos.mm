#include "window_operations_macos.h"
#include "application_events_macos.h"
#include "dock_drag_macos.h"
#include "include/core/SkFontMgr.h"
#include "include/ports/SkFontMgr_mac_ct.h"
#include "internal/unicode.h"
#include "oneui/platform/monitor.h"
#include "platform/shared/desktop_window.h"
#include "platform/shared/key_mapping.h"
#include "platform/shared/skia_canvas.h"
#include "platform/shared/text_input.h"
#include "platform/shared/window_fit.h"
#import <Cocoa/Cocoa.h>
#include <algorithm>
#include <stdexcept>

namespace oneui::macos {
class CocoaWindow;
}
@interface OneUiNativeWindow : NSWindow
@end
@implementation OneUiNativeWindow
- (BOOL)canBecomeKeyWindow {
    return YES;
}
- (BOOL)canBecomeMainWindow {
    return YES;
}
@end
@interface OneUiNativeView : NSView <NSTextInputClient, NSDraggingDestination>
@property(nonatomic, assign) oneui::macos::CocoaWindow *owner;
@property(nonatomic, copy) NSString *preedit;
@property(nonatomic) NSRange compositionReplacement;
@property(nonatomic) NSRange compositionSelection;
@property(nonatomic, assign) const void* compositionIdentity;
@property(nonatomic) uint64_t compositionSession;
@property(nonatomic, strong) NSTrackingArea *tracking;
- (void)clearMarkedText;
- (void)cancelComposition;
- (void)syncTextInput;
@end
@interface OneUiNativeDelegate : NSObject <NSWindowDelegate>
@property(nonatomic, assign) oneui::macos::CocoaWindow *owner;
@end

namespace oneui::macos {
static std::vector<CocoaWindow *> windows;
static NSString *nativeText(std::wstring_view value) {
    const auto utf8 = unicode::toUtf8(value);
    return [[NSString alloc] initWithBytes:utf8.data() length:utf8.size() encoding:NSUTF8StringEncoding];
}
static std::wstring wideText(NSString *value) {
    if (!value)
        return {};
    NSData *data = [value dataUsingEncoding:NSUTF8StringEncoding];
    return unicode::fromUtf8({static_cast<const char *>(data.bytes), data.length});
}
static unsigned int virtualKey(NSEvent *event) {
    switch (event.keyCode) {
    case 36:
    case 76:
        return 13;
    case 48:
        return 9;
    case 49:
        return 32;
    case 51:
        return 8;
    case 53:
        return 27;
    case 117:
        return 46;
    case 123:
        return 37;
    case 124:
        return 39;
    case 125:
        return 40;
    case 126:
        return 38;
    case 115:
        return 36;
    case 119:
        return 35;
    case 116:
        return 33;
    case 121:
        return 34;
    case 122:
        return 112;
    case 120:
        return 113;
    case 56:
    case 60:
        return 16;
    case 59:
    case 62:
        return 17;
    case 58:
    case 61:
        return 18;
    case 55:
        return 91;
    case 54:
        return 92;
    case 57:
        return 20;
    default:
        break;
    }
    // Fn and other flagsChanged events do not provide key-event characters.
    if (event.type != NSEventTypeKeyDown && event.type != NSEventTypeKeyUp)
        return 0;
    NSString *chars = event.charactersIgnoringModifiers.uppercaseString;
    if (!chars.length)
        return 0;
    const auto ch = [chars characterAtIndex:0];
    if (ch >= NSF1FunctionKey && ch <= NSF35FunctionKey)
        return 112 + ch - NSF1FunctionKey;
    return platform::asciiVirtualKey(ch);
}
class CocoaWindow final : public platform::DesktopWindow {
  public:
    explicit CocoaWindow(WindowOptions options) : DesktopWindow(std::move(options)) {
        if (![NSThread isMainThread])
            throw std::logic_error("Cocoa windows require the process main thread");
    }
    ~CocoaWindow() override { setCloseToTray(false); close(); }
    WindowBackend backend() const override { return WindowBackend::Cocoa; }
    unsigned int capabilities() const override {
        return WindowCapabilityClipboard | WindowCapabilityIme | WindowCapabilityPlacement |
               WindowCapabilityActivation | WindowCapabilityTopmost | WindowCapabilityFileDrop | serviceCapabilities();
    }
    void initialize() override {
        assertUiThread();
        if (native_ || isClosed())
            return;
        static bool launched = false;
        if (!launched) {
            [NSApplication sharedApplication];
            installOpenUriEvents();
            [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
            [NSApp finishLaunching];
            launched = true;
        }
        native_ = [[OneUiNativeWindow alloc]
            initWithContentRect:NSMakeRect(0, 0, logicalSize_.width, logicalSize_.height)
                      styleMask:styleMask()
                        backing:NSBackingStoreBuffered
                          defer:NO];
        native_.releasedWhenClosed = NO;
        native_.title = nativeText(options_.title);
        native_.level = options_.topmost ? NSFloatingWindowLevel : NSNormalWindowLevel;
        view_ =
            [[OneUiNativeView alloc] initWithFrame:NSMakeRect(0, 0, logicalSize_.width, logicalSize_.height)];
        view_.owner = this;
        view_.compositionReplacement = NSMakeRange(NSNotFound, 0);
        delegate_ = [OneUiNativeDelegate new];
        delegate_.owner = this;
        native_.delegate = delegate_;
        native_.contentView = view_;
        setMinimumClientSize(minimum_);
        fileDropChanged(acceptsFileDrop());
        native_.acceptsMouseMovedEvents = YES;
        fitInitialFrame();
        [native_ center];
        [native_ makeFirstResponder:view_];
        windows.push_back(this);
        resized();
        initializeGpu(nullptr, (__bridge void*)view_);
        initializeAccessibility((__bridge void*)view_);
        if (options_.visible)
            show();
    }
    void show() override {
        queueStateChange();
        initialize();
        if (!native_)
            return;
        shown_ = true;
        [native_ makeKeyAndOrderFront:nil];
        if (options_.fullscreen && !(native_.styleMask & NSWindowStyleMaskFullScreen))
            [native_ toggleFullScreen:nil];
        requestRedraw();
    }
    std::optional<Rect> accessibilityScreenBounds(Rect rect) const override {
        if (!native_ || !view_ || isClosed()) return std::nullopt;
        NSRect local = NSMakeRect(rect.x * contentScale_, rect.y * contentScale_ + chromeHeight(),
                                  rect.width * contentScale_, rect.height * contentScale_);
        NSRect screen = [native_ convertRectToScreen:[view_ convertRect:local toView:nil]];
        return Rect{static_cast<float>(screen.origin.x), static_cast<float>(screen.origin.y),
                    static_cast<float>(screen.size.width), static_cast<float>(screen.size.height)};
    }
    void activate() override {
        show();
        if (!native_) return;
        if (native_.miniaturized) [native_ deminiaturize:nil];
        [NSApp activateIgnoringOtherApps:YES];
    }
    bool centerOnActiveMonitor() override {
        assertUiThread(); initialize();
        if (!native_ || options_.fullscreen) return false;
        NSScreen* target = native_.screen ?: NSScreen.mainScreen;
        for (NSScreen* screen in NSScreen.screens)
            if (NSPointInRect(NSEvent.mouseLocation, screen.frame)) { target = screen; break; }
        if (!target) return false;
        const auto area = target.visibleFrame;
        [native_ setFrameOrigin:NSMakePoint(NSMidX(area) - native_.frame.size.width / 2, NSMidY(area) - native_.frame.size.height / 2)];
        return true;
    }
    void showWithFade(unsigned int duration) override {
        assertUiThread(); initialize();
        if (!native_ || !duration || !clientAreaAnimationsEnabled()) { show(); return; }
        native_.alphaValue = 0;
        show();
        NSWindow* window = native_;
        [NSAnimationContext runAnimationGroup:^(NSAnimationContext* context) {
            context.duration = std::min(duration, 10000u) / 1000.0;
            [[window animator] setAlphaValue:1];
        } completionHandler:nil];
    }
    int run() override {
        initialize();
        while (!windows.empty()) {
            @autoreleasepool {
                const auto snapshot = windows;
                for (auto *item : snapshot) {
                    if (std::find(windows.begin(), windows.end(), item) == windows.end())
                        continue;
                    item->dispatchWork();
                    if (std::find(windows.begin(), windows.end(), item) == windows.end())
                        continue;
                    [item->view_ syncTextInput];
                    if (std::find(windows.begin(), windows.end(), item) == windows.end())
                        continue;
                    if (item->needsPaint() && item->shown_)
                        [item->view_ setNeedsDisplay:YES];
                }
                if (windows.empty())
                    break;
                [NSApp updateWindows];
                NSDate *deadline = [NSDate distantFuture];
                // Account for time already spent dispatching and rendering.
                // A fresh 16 ms sleep after each iteration loses display ticks.
                for (auto* item : windows) {
                    if (auto delay = item->workDelayMs()) {
                        NSDate* due = [NSDate dateWithTimeIntervalSinceNow:*delay / 1000.0];
                        if ([due compare:deadline] == NSOrderedAscending) deadline = due;
                    }
                }
                NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                                    untilDate:deadline
                                                       inMode:NSDefaultRunLoopMode
                                                      dequeue:YES];
                if (event)
                    [NSApp sendEvent:event];
            }
        }
        return 0;
    }
    void close() override {
        assertUiThread();
        if (isClosed())
            return;
        if (hideForTray()) return;
        finishClose();
        windows.erase(std::remove(windows.begin(), windows.end(), this), windows.end());
        view_.owner = nullptr;
        delegate_.owner = nullptr;
        native_.delegate = nil;
        [native_ close];
        native_ = nil;
        view_ = nil;
        delegate_ = nil;
        wake();
        publishClosed();
    }
    void minimize() override {
        initialize();
        [native_ miniaturize:nil];
    }
    void hideNative() override { assertUiThread(); shown_ = false; [native_ orderOut:nil];queueStateChange(); }
    void pumpNativeEvents(int timeout) override {
        const auto snapshot = windows;
        for (auto* item : snapshot) {
            if (std::find(windows.begin(), windows.end(), item) == windows.end()) continue;
            item->dispatchWork();
            if (std::find(windows.begin(), windows.end(), item) == windows.end()) continue;
            [item->view_ syncTextInput];
            if (std::find(windows.begin(), windows.end(), item) == windows.end()) continue;
            if (item->needsPaint() && item->shown_) [item->view_ setNeedsDisplay:YES];
        }
        [NSApp updateWindows];
        NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
            untilDate:[NSDate dateWithTimeIntervalSinceNow:std::max(0, timeout) / 1000.0]
            inMode:NSDefaultRunLoopMode dequeue:YES];
        if (event) [NSApp sendEvent:event];
    }
    NativeWindowHandle nativeHandle() const override { return (__bridge void *)native_; }
    void setTitle(std::wstring title) override {
        assertUiThread();
        options_.title = std::move(title);
        native_.title = nativeText(options_.title);
    }
    void setFullscreen(bool value) override {
        assertUiThread();
        options_.fullscreen = value;
        if (native_ && value != bool(native_.styleMask & NSWindowStyleMaskFullScreen))
            [native_ toggleFullScreen:nil];
    }
    void setBorderless(bool value) override {
        assertUiThread();
        options_.borderless = value;
        if (native_)
            native_.styleMask = styleMask();
    }
    void toggleMaximize() override {
        initialize();
        [native_ zoom:nil];
    }
    void setMinimumClientSize(Size size) override {
        assertUiThread(); minimum_ = size;
        native_.contentMinSize = NSMakeSize(std::max(0.f, size.width * contentScale_), std::max(0.f, size.height * contentScale_));
    }
    WindowState state() const override { assertUiThread();return nativeWindowState(nativeHandle(),Window::state()); }
    bool restore() override { initialize();return restoreWindow(nativeHandle()); }
    bool maximize() override { initialize();return maximizeWindow(nativeHandle()); }
    bool setTopmost(bool enabled) override { initialize();if(!topmostWindow(nativeHandle(),enabled))return false;options_.topmost=enabled;queueStateChange();return true; }
    bool setResizable(bool enabled) override { assertUiThread();if(isClosed())return false;options_.resizable=enabled;if(native_)native_.styleMask=styleMask();queueStateChange();return true; }
    bool setClientSize(Size size) override { initialize();return resizeWindow(nativeHandle(),size,contentScale_); }
    bool setMaximumClientSize(Size size) override { initialize();return constrainWindow(nativeHandle(),size,contentScale_); }
    bool setOwner(Window* owner) override {
        if(owner&&(owner->backend()!=backend()||owner->closed()))return false;
        initialize();if(owner)owner->initialize();return ownerWindow(nativeHandle(),owner?owner->nativeHandle():nullptr);
    }
    bool getWindowPlacement(WindowPlacement& p) const override { assertUiThread();return windowPlacement(nativeHandle(),p); }
    bool setWindowPlacement(const WindowPlacement& p) override { initialize();return restorePlacement(nativeHandle(),p); }
    void resized() {
        queueStateChange();
        const NSSize size = view_.bounds.size;
        NSScreen* screen = native_.screen ?: NSScreen.mainScreen;
        NSNumber* screenNumber = screen.deviceDescription[@"NSScreenNumber"];
        CGDisplayModeRef mode = screenNumber ? CGDisplayCopyDisplayMode(screenNumber.unsignedIntValue) : nullptr;
        double refresh = mode ? CGDisplayModeGetRefreshRate(mode) : 0.0;
        if (mode) CGDisplayModeRelease(mode);
        if (refresh <= 0) refresh = screen ? screen.maximumFramesPerSecond : 60.0;
        setFrameIntervalMs(1000.0 / std::max(1.0, refresh));
        resize({static_cast<float>(size.width), static_cast<float>(size.height)}, native_.backingScaleFactor);
        [view_.inputContext invalidateCharacterCoordinates];
    }
    void nativeFocus(bool value) {
        const auto alive = lifetime_;
        if (!value) [view_ cancelComposition];
        if (*alive && !isClosed()) focus(value);
    }
    bool prepareContentTransfer(Widget* subtree) override {
        assertUiThread();
        if (subtree && subtree->focused()) [view_ cancelComposition];
        return Window::prepareContentTransfer(subtree);
    }
    void draw() {
        if (paintGpu()) return;
        const auto &data = render();
        const auto size = clientPixelSize();
        CGDataProviderRef provider =
            CGDataProviderCreateWithData(nullptr, data.data(), data.size() * 4, nullptr);
        CGColorSpaceRef color = CGColorSpaceCreateDeviceRGB();
        CGImageRef image = CGImageCreate(size.width, size.height, 8, 32, static_cast<size_t>(size.width) * 4,
                                         color, kCGBitmapByteOrder32Little | kCGImageAlphaPremultipliedFirst,
                                         provider, nullptr, false, kCGRenderingIntentDefault);
        CGContextRef context = NSGraphicsContext.currentContext.CGContext;
        CGContextSaveGState(context);
        CGContextTranslateCTM(context, 0, logicalSize_.height);
        CGContextScaleCTM(context, 1, -1);
        CGContextDrawImage(context, CGRectMake(0, 0, logicalSize_.width, logicalSize_.height), image);
        CGContextRestoreGState(context);
        CGImageRelease(image);
        CGColorSpaceRelease(color);
        CGDataProviderRelease(provider);
    }
    bool keyboard(NSEvent *event, bool pressed) {
        const auto flags = event.modifierFlags;
        KeyEvent e;
        e.virtualKey = virtualKey(event);
        e.scanCode = event.keyCode;
        e.key = platform::logicalKey(e.virtualKey);
        e.pressed = pressed;
        // AppKit raises NSInternalInconsistencyException for flagsChanged.
        e.repeat = event.type == NSEventTypeKeyDown && event.isARepeat;
        e.shift = flags & NSEventModifierFlagShift;
        e.control = flags & NSEventModifierFlagControl;
        e.alt = flags & NSEventModifierFlagOption;
        e.win = flags & NSEventModifierFlagCommand;
        return key(e);
    }
    void pointer(NSEvent *event, int action) {
        if (nativeModal()) return;
        const auto alive = lifetime_;
        NSPoint p = [view_ convertPoint:event.locationInWindow fromView:nil];
        MouseEvent e;
        e.position = {static_cast<float>(p.x), static_cast<float>(p.y)};
        e.button = event.buttonNumber == 1   ? MouseButton::Right
                   : event.buttonNumber == 2 ? MouseButton::Middle
                                             : MouseButton::Left;
        e.shift = event.modifierFlags & NSEventModifierFlagShift;
        e.control = event.modifierFlags & NSEventModifierFlagControl;
        e.alt = event.modifierFlags & NSEventModifierFlagOption;
        e.clickCount = event.clickCount;
        if (action == 1 && e.button == MouseButton::Left && dragRegion(e.position)) {
            [native_ performWindowDragWithEvent:event];
            return;
        }
        mouse(e, action);
        if (!*alive || isClosed())
            return;
        switch (cursor(e.position)) {
        case CursorKind::Pointer:
            [NSCursor.pointingHandCursor set];
            break;
        case CursorKind::Text:
            [NSCursor.IBeamCursor set];
            break;
        case CursorKind::ResizeHorizontal:
            [NSCursor.resizeLeftRightCursor set];
            break;
        case CursorKind::ResizeVertical:
            [NSCursor.resizeUpDownCursor set];
            break;
        case CursorKind::Crosshair:
            [NSCursor.crosshairCursor set];
            break;
        case CursorKind::Grab:
            [NSCursor.openHandCursor set];
            break;
        default:
            [NSCursor.arrowCursor set];
            break;
        }
    }

  protected:
    void contentScaleChanged() override { setMinimumClientSize(minimum_); }
    void fileDropChanged(bool enabled) override {
        updateDraggedTypes(view_, enabled);
    }
    void wake() override {
        @autoreleasepool {
            if (!NSApp)
                return;
            NSEvent *event = [NSEvent otherEventWithType:NSEventTypeApplicationDefined
                                                location:NSZeroPoint
                                           modifierFlags:0
                                               timestamp:0
                                            windowNumber:0
                                                 context:nil
                                                 subtype:0
                                                   data1:0
                                                   data2:0];
            [NSApp postEvent:event atStart:NO];
        }
    }

  private:
    // Requested sizes are logical points; a 1600x1000 default is larger than
    // most MacBook screens, and AppKit does not constrain borderless frames.
    void fitInitialFrame() {
        NSScreen* screen = native_.screen ?: NSScreen.mainScreen;
        if (!screen) return;
        const NSRect frame = native_.frame, area = screen.visibleFrame;
        const platform::WindowFitRect current{frame.origin.x, frame.origin.y, frame.size.width, frame.size.height};
        const platform::WindowFitRect work{area.origin.x, area.origin.y, area.size.width, area.size.height};
        if (!platform::windowExceedsArea(current, work)) return;
        const auto fitted = platform::initialWindowRect(current, work);
        [native_ setFrame:NSMakeRect(fitted.x, fitted.y, fitted.width, fitted.height) display:NO];
    }
    NSWindowStyleMask styleMask() const {
        if (options_.borderless)
            return NSWindowStyleMaskBorderless | (options_.resizable ? NSWindowStyleMaskResizable : 0);
        return NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable |
               (options_.resizable ? NSWindowStyleMaskResizable : 0);
    }
    NSWindow *native_ = nil;
    Size minimum_{};
    OneUiNativeView *view_ = nil;
    OneUiNativeDelegate *delegate_ = nil;
};
} // namespace oneui::macos

@implementation OneUiNativeDelegate
- (BOOL)windowShouldClose:(NSWindow *)sender {
    (void)sender;
    if (_owner)
        _owner->requestClose();
    return NO;
}
- (void)windowDidResize:(NSNotification *)n {
    (void)n;
    if (_owner)
        _owner->resized();
}
- (void)windowDidChangeBackingProperties:(NSNotification *)n {
    (void)n;
    if (_owner)
        _owner->resized();
}
- (void)windowDidChangeScreen:(NSNotification *)n {
    (void)n;
    if (_owner) _owner->resized();
}
- (void)windowDidBecomeKey:(NSNotification *)n {
    (void)n;
    if (_owner)
        _owner->nativeFocus(true);
}
- (void)windowDidResignKey:(NSNotification *)n {
    (void)n;
    if (_owner)
        _owner->nativeFocus(false);
}
@end

@implementation OneUiNativeView
- (id)accessibilityFocusedUIElement {
    for (id child in self.accessibilityChildren) {
        id focused = [child accessibilityFocusedUIElement];
        if (focused) return focused;
    }
    return [super accessibilityFocusedUIElement];
}
- (id)accessibilityHitTest:(NSPoint)point {
    for (id child in [self.accessibilityChildren reverseObjectEnumerator]) {
        id hit = [child accessibilityHitTest:point];
        if (hit) return hit;
    }
    return [super accessibilityHitTest:point];
}
- (BOOL)isFlipped {
    return YES;
}
- (BOOL)acceptsFirstResponder {
    return YES;
}
- (void)updateTrackingAreas {
    if (self.tracking)
        [self removeTrackingArea:self.tracking];
    self.tracking =
        [[NSTrackingArea alloc] initWithRect:NSZeroRect
                                     options:NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved |
                                             NSTrackingActiveInKeyWindow | NSTrackingInVisibleRect
                                       owner:self
                                    userInfo:nil];
    [self addTrackingArea:self.tracking];
    [super updateTrackingAreas];
}
- (void)mouseExited:(NSEvent *)e {
    (void)e;
    if (_owner)
        _owner->mouse({{-1, -1}, oneui::MouseButton::None}, 0);
}
- (void)drawRect:(NSRect)rect {
    (void)rect;
    if (_owner)
        _owner->draw();
}
- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender {
    return oneui::macos::draggingOffer(self, _owner, sender);
}
- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)sender { return [self draggingEntered:sender]; }
- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender {
    return oneui::macos::performDrop(self, _owner, sender);
}
- (void)draggingExited:(id<NSDraggingInfo>)sender { oneui::macos::leaveDockDrag(self); }
- (BOOL)wantsPeriodicDraggingUpdates { return NO; }
- (void)keyDown:(NSEvent *)e {
    if (!_owner || _owner->nativeModal()) return;
    [self syncTextInput];
    if (_owner && ![self hasMarkedText]) {
        const bool consumed = _owner->keyboard(e, true);
        if (!_owner)
            return;
        const unsigned int vk = oneui::macos::virtualKey(e);
        const bool printable = oneui::platform::printableVirtualKey(vk) || (vk >= 128 && vk < 0xF700);
        if (_owner->rawKeyConsumed() || (consumed && !(_owner->inputState().editable && printable)) ||
            (e.modifierFlags & (NSEventModifierFlagCommand | NSEventModifierFlagControl)))
            return;
    }
    [self interpretKeyEvents:@[ e ]];
}
- (void)keyUp:(NSEvent *)e {
    if (_owner)
        _owner->keyboard(e, false);
}
- (void)flagsChanged:(NSEvent *)e {
    if (!_owner)
        return;
    const auto key = oneui::macos::virtualKey(e);
    if (key != 16 && key != 17 && key != 18 && key != 20 && key != 91 && key != 92)
        return;
    const auto mask = key == 16   ? NSEventModifierFlagShift
                      : key == 17 ? NSEventModifierFlagControl
                      : key == 18 ? NSEventModifierFlagOption
                      : key == 20 ? NSEventModifierFlagCapsLock
                                  : NSEventModifierFlagCommand;
    _owner->keyboard(e, (e.modifierFlags & mask) != 0);
}
- (void)doCommandBySelector:(SEL)s {
    (void)s;
}
- (void)mouseDown:(NSEvent *)e {
    oneui::macos::recordDockMouseDown(self, e);
    [self.window makeFirstResponder:self];
    if (_owner)
        _owner->pointer(e, 1);
}
- (void)mouseUp:(NSEvent *)e {
    if (_owner)
        _owner->pointer(e, 2);
}
- (void)mouseMoved:(NSEvent *)e {
    if (_owner)
        _owner->pointer(e, 0);
}
- (void)mouseDragged:(NSEvent *)e {
    [self mouseMoved:e];
}
- (void)rightMouseDown:(NSEvent *)e {
    if (_owner)
        _owner->pointer(e, 1);
}
- (void)rightMouseUp:(NSEvent *)e {
    if (_owner)
        _owner->pointer(e, 2);
}
- (void)rightMouseDragged:(NSEvent *)e {
    [self mouseMoved:e];
}
- (void)otherMouseDown:(NSEvent *)e {
    if (_owner)
        _owner->pointer(e, 1);
}
- (void)otherMouseUp:(NSEvent *)e {
    if (_owner)
        _owner->pointer(e, 2);
}
- (void)otherMouseDragged:(NSEvent *)e {
    [self mouseMoved:e];
}
- (void)scrollWheel:(NSEvent *)e {
    if (!_owner)
        return;
    NSPoint p = [self convertPoint:e.locationInWindow fromView:nil];
    oneui::MouseWheelEvent value;
    value.position = {static_cast<float>(p.x), static_cast<float>(p.y)};
    value.deltaY = e.scrollingDeltaY * (e.hasPreciseScrollingDeltas ? 1.f : 40.f);
    value.shift = e.modifierFlags & NSEventModifierFlagShift;
    value.control = e.modifierFlags & NSEventModifierFlagControl;
    value.alt = e.modifierFlags & NSEventModifierFlagOption;
    value.timestampMs = oneui::platform::DesktopWindow::nowMs();
    _owner->wheel(value);
}
- (BOOL)hasMarkedText {
    return self.preedit.length > 0;
}
- (NSRange)markedRange {
    return [self hasMarkedText] ? NSMakeRange(self.compositionReplacement.location, self.preedit.length)
                                : NSMakeRange(NSNotFound, 0);
}
- (NSRange)selectedRange {
    if (!_owner)
        return NSMakeRange(NSNotFound, 0);
    if ([self hasMarkedText])
        return NSMakeRange(self.compositionReplacement.location + self.compositionSelection.location,
                           self.compositionSelection.length);
    auto s = _owner->inputState();
    NSUInteger start = oneui::platform::wideToUtf16Offset(s.text, std::min(s.anchor, s.caret));
    NSUInteger end = oneui::platform::wideToUtf16Offset(s.text, std::max(s.anchor, s.caret));
    return NSMakeRange(start, end - start);
}
- (NSArray<NSAttributedStringKey> *)validAttributesForMarkedText {
    return @[];
}
- (void)setMarkedText:(id)value selectedRange:(NSRange)selection replacementRange:(NSRange)replacement {
    if (!_owner || _owner->nativeModal() || !_owner->inputState().editable)
        return;
    [self syncTextInput];
    if (!_owner || !_owner->inputState().editable) return;
    self.compositionIdentity = _owner->inputState().identity;
    self.compositionSession = _owner->inputState().session;
    if (![self hasMarkedText]) {
        const auto state = _owner->inputState();
        const NSRange requested = replacement.location == NSNotFound ? [self selectedRange] : replacement;
        const auto range = oneui::platform::utf16ReplacementRange(state.text, requested.location, requested.length);
        const NSUInteger start = oneui::platform::wideToUtf16Offset(state.text, range.first);
        self.compositionReplacement = NSMakeRange(start, oneui::platform::wideToUtf16Offset(state.text, range.second) - start);
    }
    self.preedit = [value isKindOfClass:NSAttributedString.class] ? [value string] : value;
    const auto preedit = oneui::macos::wideText(self.preedit);
    const auto range = oneui::platform::utf16ReplacementRange(preedit, selection.location, selection.length);
    const NSUInteger caret = oneui::platform::wideToUtf16Offset(preedit, range.first);
    self.compositionSelection = NSMakeRange(caret, oneui::platform::wideToUtf16Offset(preedit, range.second) - caret);
    if (_owner)
        _owner->composition(
            preedit, range.first);
}
- (void)unmarkText {
    // AppKit unmark accepts the composition. Focus loss/cancellation use the
    // separate clear path so they never commit text into a different editor.
    if ([self hasMarkedText])
        [self insertText:self.preedit replacementRange:self.compositionReplacement];
}
- (void)clearMarkedText {
    self.preedit = @"";
    self.compositionIdentity = nullptr;
    self.compositionSession = 0;
    self.compositionSelection = NSMakeRange(0, 0);
    self.compositionReplacement = NSMakeRange(NSNotFound, 0);
    if (_owner)
        _owner->composition({}, 0);
}
- (void)cancelComposition {
    [self clearMarkedText];
    [self.inputContext discardMarkedText];
}
- (void)syncTextInput {
    if ([self hasMarkedText] &&
        (!_owner || self.compositionIdentity != _owner->inputState().identity || self.compositionSession != _owner->inputState().session))
        [self cancelComposition];
    if (_owner && [self hasMarkedText])
        [self.inputContext invalidateCharacterCoordinates];
}
- (void)insertText:(id)value replacementRange:(NSRange)replacement {
    if (!_owner || _owner->nativeModal()) return;
    if ([self hasMarkedText] && (!_owner || self.compositionIdentity != _owner->inputState().identity ||
                                self.compositionSession != _owner->inputState().session)) {
        [self cancelComposition];
        return;
    }
    NSString *string = [value isKindOfClass:NSAttributedString.class] ? [value string] : value;
    if ([self hasMarkedText] &&
        (replacement.location == NSNotFound || NSEqualRanges(replacement, [self markedRange])))
        replacement = self.compositionReplacement;
    if (!_owner || !_owner->inputState().editable)
        return;
    const auto state = _owner->inputState();
    [self clearMarkedText];
    if (!_owner || _owner->inputState().identity != state.identity ||
        _owner->inputState().session != state.session || !_owner->inputState().editable)
        return;
    if (replacement.location == NSNotFound || NSEqualRanges(replacement, [self selectedRange]))
        _owner->text(oneui::macos::wideText(string));
    else {
        const auto range = oneui::platform::utf16ReplacementRange(state.text, replacement.location, replacement.length);
        _owner->replaceText(range.first, range.second, oneui::macos::wideText(string));
    }
}
- (NSAttributedString *)attributedSubstringForProposedRange:(NSRange)range
                                                actualRange:(NSRangePointer)actual {
    if (actual)
        *actual = NSMakeRange(NSNotFound, 0);
    if (!_owner || _owner->inputState().sensitive)
        return nil;
    NSMutableString *text = [oneui::macos::nativeText(_owner->inputState().text) mutableCopy];
    if ([self hasMarkedText] && self.compositionReplacement.location <= text.length &&
        self.compositionReplacement.length <= text.length - self.compositionReplacement.location)
        [text replaceCharactersInRange:self.compositionReplacement withString:self.preedit];
    if (range.location == NSNotFound || range.location > text.length)
        return nil;
    const auto wide = oneui::macos::wideText(text);
    const auto span = oneui::platform::utf16ReplacementRange(wide, range.location, range.length);
    const NSUInteger start = oneui::platform::wideToUtf16Offset(wide, span.first);
    NSRange valid = NSMakeRange(start, oneui::platform::wideToUtf16Offset(wide, span.second) - start);
    if (actual)
        *actual = valid;
    return [[NSAttributedString alloc] initWithString:[text substringWithRange:valid]];
}
- (NSUInteger)characterIndexForPoint:(NSPoint)point {
    (void)point;
    return [self selectedRange].location;
}
- (NSRect)firstRectForCharacterRange:(NSRange)range actualRange:(NSRangePointer)actual {
    if (actual)
        *actual = range;
    if (!_owner)
        return NSZeroRect;
    auto r = _owner->caretRect();
    return [self.window convertRectToScreen:[self convertRect:NSMakeRect(r.x, r.y, r.width, r.height)
                                                       toView:nil]];
}
@end

namespace oneui {
std::unique_ptr<Window> Window::create(std::wstring title, int width, int height) {
    WindowOptions o;
    o.title = std::move(title);
    o.width = width;
    o.height = height;
    return create(std::move(o));
}
std::unique_ptr<Window> Window::create(WindowOptions options) {
    return std::make_unique<macos::CocoaWindow>(std::move(options));
}
void SystemClipboard::setText(std::wstring text) {
    @autoreleasepool {
        [NSPasteboard.generalPasteboard clearContents];
        if (![NSPasteboard.generalPasteboard setString:macos::nativeText(text) forType:NSPasteboardTypeString])
            throw std::runtime_error("Cocoa clipboard write failed");
    }
}
std::wstring SystemClipboard::text() const {
    @autoreleasepool {
        return macos::wideText([NSPasteboard.generalPasteboard stringForType:NSPasteboardTypeString]);
    }
}
std::vector<MonitorInfo> enumerateMonitors() {
    std::vector<MonitorInfo> result;
    @autoreleasepool {
        for (NSScreen *screen in NSScreen.screens) {
            const NSRect b = screen.frame, w = screen.visibleFrame;
            result.push_back({static_cast<int>(result.size()),
                              {static_cast<float>(b.origin.x), static_cast<float>(b.origin.y),
                               static_cast<float>(b.size.width), static_cast<float>(b.size.height)},
                              {static_cast<float>(w.origin.x), static_cast<float>(w.origin.y),
                               static_cast<float>(w.size.width), static_cast<float>(w.size.height)},
                              static_cast<float>(screen.backingScaleFactor),
                              result.empty(),
                              macos::wideText(screen.localizedName)});
        }
    }
    return result;
}
namespace rendering {
} // namespace rendering
} // namespace oneui
