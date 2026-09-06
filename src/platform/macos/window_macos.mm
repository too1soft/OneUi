#include "include/core/SkFontMgr.h"
#include "include/ports/SkFontMgr_mac_ct.h"
#include "internal/unicode.h"
#include "oneui/platform/monitor.h"
#include "platform/shared/desktop_window.h"
#include "platform/shared/key_mapping.h"
#include "platform/shared/skia_canvas.h"
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
@interface OneUiNativeView : NSView <NSTextInputClient>
@property(nonatomic, assign) oneui::macos::CocoaWindow *owner;
@property(nonatomic, copy) NSString *preedit;
@property(nonatomic) NSRange compositionReplacement;
@property(nonatomic) NSRange compositionSelection;
@property(nonatomic, assign) const void* compositionIdentity;
@property(nonatomic) uint64_t compositionSession;
@property(nonatomic, strong) NSTrackingArea *tracking;
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
    ~CocoaWindow() override { close(); }
    WindowBackend backend() const override { return WindowBackend::Cocoa; }
    unsigned int capabilities() const override {
        return WindowCapabilityClipboard | WindowCapabilityIme | WindowCapabilityPlacement |
               WindowCapabilityActivation | WindowCapabilityTopmost;
    }
    void initialize() override {
        assertUiThread();
        if (native_ || isClosed())
            return;
        static bool launched = false;
        if (!launched) {
            [NSApplication sharedApplication];
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
        native_.acceptsMouseMovedEvents = YES;
        [native_ center];
        [native_ makeFirstResponder:view_];
        windows.push_back(this);
        resized();
        if (options_.visible)
            show();
    }
    void show() override {
        initialize();
        if (!native_)
            return;
        shown_ = true;
        [native_ makeKeyAndOrderFront:nil];
        if (options_.fullscreen && !(native_.styleMask & NSWindowStyleMaskFullScreen))
            [native_ toggleFullScreen:nil];
        requestRedraw();
    }
    void activate() override {
        show();
        [NSApp activateIgnoringOtherApps:YES];
    }
    int run() override {
        initialize();
        while (!windows.empty()) {
            @autoreleasepool {
                bool pending = false;
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
                    pending |= item->hasWork();
                }
                if (windows.empty())
                    break;
                [NSApp updateWindows];
                NSDate *deadline =
                    pending ? [NSDate dateWithTimeIntervalSinceNow:0.016] : [NSDate distantFuture];
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
    }
    void minimize() override {
        initialize();
        [native_ miniaturize:nil];
    }
    NativeWindowHandle nativeHandle() const override { return (__bridge void *)native_; }
    void setTitle(std::wstring title) override {
        options_.title = std::move(title);
        native_.title = nativeText(options_.title);
    }
    void setFullscreen(bool value) override {
        options_.fullscreen = value;
        if (native_ && value != bool(native_.styleMask & NSWindowStyleMaskFullScreen))
            [native_ toggleFullScreen:nil];
    }
    void setBorderless(bool value) override {
        options_.borderless = value;
        if (native_)
            native_.styleMask = styleMask();
    }
    void toggleMaximize() override {
        initialize();
        [native_ zoom:nil];
    }
    void setMinimumClientSize(Size size) override {
        initialize();
        native_.contentMinSize = NSMakeSize(std::max(0.f, size.width), std::max(0.f, size.height));
    }
    bool getWindowPlacement(WindowPlacement &p) const override {
        if (!native_)
            return false;
        NSRect r = native_.frame;
        p = {static_cast<int>(r.origin.x), static_cast<int>(r.origin.y), static_cast<int>(r.size.width),
             static_cast<int>(r.size.height), bool(native_.zoomed)};
        return true;
    }
    bool setWindowPlacement(const WindowPlacement &p) override {
        if (p.width <= 0 || p.height <= 0)
            return false;
        initialize();
        NSRect frame = NSMakeRect(p.x, p.y, p.width, p.height);
        bool intersects = false;
        for (NSScreen *screen in NSScreen.screens)
            intersects |= NSIntersectsRect(frame, screen.visibleFrame);
        if (!intersects)
            return false;
        [native_ setFrame:frame display:YES];
        if (bool(native_.zoomed) != p.maximized)
            [native_ zoom:nil];
        return true;
    }
    void resized() {
        const NSSize size = view_.bounds.size;
        resize({static_cast<float>(size.width), static_cast<float>(size.height)}, native_.backingScaleFactor);
        [view_.inputContext invalidateCharacterCoordinates];
    }
    void nativeFocus(bool value) {
        const auto alive = lifetime_;
        if (!value) [view_ cancelComposition];
        if (*alive && !isClosed()) focus(value);
    }
    void draw() {
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
        e.repeat = event.isARepeat;
        e.shift = flags & NSEventModifierFlagShift;
        e.control = flags & NSEventModifierFlagControl;
        e.alt = flags & NSEventModifierFlagOption;
        e.win = flags & NSEventModifierFlagCommand;
        return key(e);
    }
    void pointer(NSEvent *event, int action) {
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
    NSWindowStyleMask styleMask() const {
        if (options_.borderless)
            return NSWindowStyleMaskBorderless | (options_.resizable ? NSWindowStyleMaskResizable : 0);
        return NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable |
               (options_.resizable ? NSWindowStyleMaskResizable : 0);
    }
    NSWindow *native_ = nil;
    OneUiNativeView *view_ = nil;
    OneUiNativeDelegate *delegate_ = nil;
};
} // namespace oneui::macos

@implementation OneUiNativeDelegate
- (BOOL)windowShouldClose:(NSWindow *)sender {
    (void)sender;
    if (_owner)
        _owner->close();
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
- (void)keyDown:(NSEvent *)e {
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
- (void)otherMouseDown:(NSEvent *)e {
    if (_owner)
        _owner->pointer(e, 1);
}
- (void)otherMouseUp:(NSEvent *)e {
    if (_owner)
        _owner->pointer(e, 2);
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
    NSUInteger start = oneui::macos::nativeText(s.text.substr(0, std::min(s.anchor, s.caret))).length;
    NSUInteger end = oneui::macos::nativeText(s.text.substr(0, std::max(s.anchor, s.caret))).length;
    return NSMakeRange(start, end - start);
}
- (NSArray<NSAttributedStringKey> *)validAttributesForMarkedText {
    return @[];
}
- (void)setMarkedText:(id)value selectedRange:(NSRange)selection replacementRange:(NSRange)replacement {
    if (!_owner || !_owner->inputState().editable)
        return;
    [self syncTextInput];
    if (!_owner) return;
    self.compositionIdentity = _owner->inputState().identity;
    self.compositionSession = _owner->inputState().session;
    if (![self hasMarkedText])
        self.compositionReplacement = replacement.location == NSNotFound ? [self selectedRange] : replacement;
    self.preedit = [value isKindOfClass:NSAttributedString.class] ? [value string] : value;
    self.compositionSelection = NSIntersectionRange(selection, NSMakeRange(0, self.preedit.length));
    if (_owner)
        _owner->composition(
            oneui::macos::wideText(self.preedit),
            oneui::macos::wideText(
                [self.preedit substringToIndex:std::min(selection.location, self.preedit.length)])
                .size());
}
- (void)unmarkText {
    self.preedit = @"";
    self.compositionIdentity = nullptr;
    self.compositionReplacement = NSMakeRange(NSNotFound, 0);
    if (_owner)
        _owner->composition({}, 0);
}
- (void)cancelComposition {
    [self unmarkText];
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
    if ([self hasMarkedText] && (!_owner || self.compositionIdentity != _owner->inputState().identity ||
                                self.compositionSession != _owner->inputState().session)) {
        [self cancelComposition];
        return;
    }
    NSString *string = [value isKindOfClass:NSAttributedString.class] ? [value string] : value;
    if ([self hasMarkedText] &&
        (replacement.location == NSNotFound || NSEqualRanges(replacement, [self markedRange])))
        replacement = self.compositionReplacement;
    [self unmarkText];
    if (!_owner)
        return;
    if (replacement.location == NSNotFound || NSEqualRanges(replacement, [self selectedRange]))
        _owner->text(oneui::macos::wideText(string));
    else {
        NSString *existing = oneui::macos::nativeText(_owner->inputState().text);
        NSUInteger start = std::min(replacement.location, existing.length);
        NSUInteger end = std::min(NSMaxRange(replacement), existing.length);
        _owner->replaceText(oneui::macos::wideText([existing substringToIndex:start]).size(),
                            oneui::macos::wideText([existing substringToIndex:end]).size(),
                            oneui::macos::wideText(string));
    }
}
- (NSAttributedString *)attributedSubstringForProposedRange:(NSRange)range
                                                actualRange:(NSRangePointer)actual {
    if (!_owner || _owner->inputState().sensitive)
        return nil;
    NSMutableString *text = [oneui::macos::nativeText(_owner->inputState().text) mutableCopy];
    if ([self hasMarkedText] && NSMaxRange(self.compositionReplacement) <= text.length)
        [text replaceCharactersInRange:self.compositionReplacement withString:self.preedit];
    NSRange valid = NSIntersectionRange(range, NSMakeRange(0, text.length));
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
