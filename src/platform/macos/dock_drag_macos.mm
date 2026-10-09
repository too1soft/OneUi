#include "dock_drag_macos.h"
#include "internal/unicode.h"
#include "platform/shared/desktop_window.h"
#include "platform/shared/native_dock_drag.h"
#include <map>

namespace oneui::macos {
class CocoaDockDrag;
}
@interface OneUiDockDragSource : NSObject <NSDraggingSource> {
  @public
    std::weak_ptr<oneui::macos::CocoaDockDrag> adapter;
}
@end

namespace oneui::macos {
// NSPasteboardType requires a UTI, unlike the MIME string used by Xdnd/Wayland.
// AppKit rejects MIME names containing '/' before a drag can carry any data.
static NSPasteboardType dockPasteboardType() { return @"org.oneui.dock-v1"; }
static std::map<NSView *, std::weak_ptr<CocoaDockDrag>> adapters;
class CocoaDockDrag final : public platform::NativeDockDrag,
                            public std::enable_shared_from_this<CocoaDockDrag> {
  public:
    Window *window;
    __weak NSView *view;
    platform::DockDragCallbacks callbacks;
    Window::EventSubscription closed;
    NSEvent *mouseDown = nil;
    OneUiDockDragSource *source = nil;
    id monitor = nil;
    bool cancelled = false;
    std::function<void(platform::NativeDragResult, Point)> completion;
    std::shared_ptr<CocoaDockDrag> active;
    CocoaDockDrag(Window &w, NSView *v, platform::DockDragCallbacks c)
        : window(&w), view(v), callbacks(std::move(c)) {}
    ~CocoaDockDrag() override {
        if (monitor)
            [NSEvent removeMonitor:monitor];
        auto native = view;
        adapters.erase(native);
        if (window)
            updateDraggedTypes(native,
                               static_cast<platform::DesktopWindow *>(window)->acceptsFileDrop());
    }
    bool start(std::string payload,
               std::function<void(platform::NativeDragResult, Point)> done) override {
        auto native = view;
        if (!window || !native || !mouseDown || completion || payload.size() > 1024 ||
            !([NSEvent pressedMouseButtons] & 1))
            return false;
        NSPasteboardItem *item = [NSPasteboardItem new];
        if (![item setData:[NSData dataWithBytes:payload.data() length:payload.size()]
                   forType:dockPasteboardType()])
            return false;
        NSDraggingItem *drag = [[NSDraggingItem alloc] initWithPasteboardWriter:item];
        auto point = [native convertPoint:mouseDown.locationInWindow fromView:nil];
        NSImage *image = [[NSImage alloc] initWithSize:NSMakeSize(120, 30)];
        [image lockFocus];
        [[NSColor colorWithCalibratedRed:.10 green:.47 blue:.31 alpha:.85] setFill];
        [[NSBezierPath bezierPathWithRoundedRect:NSMakeRect(0, 0, 120, 30) xRadius:5
                                         yRadius:5] fill];
        [@"OneUI" drawAtPoint:NSMakePoint(12, 7)
               withAttributes:@{NSForegroundColorAttributeName : NSColor.whiteColor}];
        [image unlockFocus];
        [drag setDraggingFrame:NSMakeRect(point.x, point.y, 120, 30) contents:image];
        cancelled = false;
        completion = std::move(done);
        active = shared_from_this();
        source = [OneUiDockDragSource new];
        source->adapter = weak_from_this();
        auto weak = weak_from_this();
        monitor = [NSEvent
            addLocalMonitorForEventsMatchingMask:NSEventMaskKeyDown | NSEventMaskRightMouseDown
                                         handler:^NSEvent *(NSEvent *event) {
                                           if (event.type == NSEventTypeRightMouseDown ||
                                               event.keyCode == 53)
                                               if (auto self = weak.lock())
                                                   self->cancelled = true;
                                           return event;
                                         }];
        @try {
            auto session = [native beginDraggingSessionWithItems:@[ drag ]
                                                           event:mouseDown
                                                          source:source];
            session.animatesToStartingPositionsOnCancelOrFail = NO;
            return true;
        } @catch (NSException *) {
            finish(platform::NativeDragResult::Failed, NSZeroPoint);
            return false;
        }
    }
    void finish(platform::NativeDragResult result, NSPoint point) {
        auto keepAlive = shared_from_this();
        if (monitor) {
            [NSEvent removeMonitor:monitor];
            monitor = nil;
        }
        auto done = std::move(completion);
        completion = {};
        source = nil;
        active.reset();
        if (done) {
            try {
                done(result, {float(point.x), float(point.y)});
            } catch (...) { /* No C++ exception may cross AppKit. */
            }
        }
    }
    bool offer(id<NSDraggingInfo> sender, bool commit) {
        auto native = view;
        if (!window || !native || !callbacks.drop ||
            !([sender draggingSourceOperationMask] & NSDragOperationMove))
            return false;
        auto data = [[sender draggingPasteboard] dataForType:dockPasteboardType()];
        if (!data.length || data.length > 1024)
            return false;
        auto point = [native convertPoint:[sender draggingLocation] fromView:nil];
        const auto scale = native.window.backingScaleFactor / window->dpiScale();
        try {
            return callbacks.drop(std::string(static_cast<const char *>(data.bytes), data.length),
                                  {float(point.x * scale), float(point.y * scale)}, commit);
        } catch (...) {
            return false;
        }
    }
};
static std::shared_ptr<CocoaDockDrag> adapter(NSView *view) {
    auto found = adapters.find(view);
    return found == adapters.end() ? nullptr : found->second.lock();
}
void recordDockMouseDown(NSView *view, NSEvent *event) {
    if (auto current = adapter(view))
        current->mouseDown = event;
}
void updateDraggedTypes(NSView *view, bool files) {
    [view unregisterDraggedTypes];
    NSMutableArray<NSPasteboardType> *types = [NSMutableArray new];
    if (files)
        [types addObject:NSPasteboardTypeFileURL];
    if (adapter(view))
        [types addObject:dockPasteboardType()];
    if (types.count)
        [view registerForDraggedTypes:types];
}
NSDragOperation draggingOffer(NSView *view, platform::DesktopWindow *window,
                              id<NSDraggingInfo> sender) {
    if (!window || window->nativeModal())
        return NSDragOperationNone;
    if ([[sender draggingPasteboard].types containsObject:dockPasteboardType()]) {
        auto current = adapter(view);
        return current && current->offer(sender, false) ? NSDragOperationMove : NSDragOperationNone;
    }
    return static_cast<platform::DesktopWindow *>(window)->acceptsFileDrop() &&
                   ([sender draggingSourceOperationMask] & NSDragOperationCopy) &&
                   [[sender draggingPasteboard].types containsObject:NSPasteboardTypeFileURL]
               ? NSDragOperationCopy
               : NSDragOperationNone;
}
bool performDrop(NSView *view, platform::DesktopWindow *window, id<NSDraggingInfo> sender) {
    if (!window || window->nativeModal())
        return false;
    if ([[sender draggingPasteboard].types containsObject:dockPasteboardType()]) {
        auto current = adapter(view);
        return current && current->offer(sender, true);
    }
    if (!static_cast<platform::DesktopWindow *>(window)->acceptsFileDrop())
        return false;
    auto urls =
        [[sender draggingPasteboard] readObjectsForClasses:@[ NSURL.class ]
                                                   options:@{
                                                       NSPasteboardURLReadingFileURLsOnlyKey : @YES
                                                   }];
    std::vector<std::wstring> files;
    for (NSURL *url in urls)
        if (url.isFileURL) {
            auto data = [url.path dataUsingEncoding:NSUTF8StringEncoding];
            files.push_back(
                unicode::fromUtf8({static_cast<const char *>(data.bytes), data.length}));
        }
    if (files.empty())
        return false;
    auto point = [view convertPoint:[sender draggingLocation] fromView:nil];
    window->deliverFileDrop(std::move(files), {float(point.x), float(point.y)});
    return true;
}
void leaveDockDrag(NSView *view) {
    if (auto current = adapter(view))
        if (current->callbacks.leave)
            current->callbacks.leave();
}
} // namespace oneui::macos

@implementation OneUiDockDragSource
- (NSDragOperation)draggingSession:(NSDraggingSession *)session
    sourceOperationMaskForDraggingContext:(NSDraggingContext)context {
    return context == NSDraggingContextWithinApplication ? NSDragOperationMove
                                                         : NSDragOperationNone;
}
- (BOOL)ignoreModifierKeysForDraggingSession:(NSDraggingSession *)session {
    return YES;
}
- (void)draggingSession:(NSDraggingSession *)session
           endedAtPoint:(NSPoint)point
              operation:(NSDragOperation)operation {
    if (auto current = adapter.lock()) {
        using oneui::platform::NativeDragResult;
        const auto result =
            operation & NSDragOperationMove ? NativeDragResult::Dropped
            : current->cancelled || !current->window || ([NSEvent pressedMouseButtons] & 1)
                ? NativeDragResult::Cancelled
                : NativeDragResult::Unhandled;
        current->finish(result, point);
    }
}
@end

namespace oneui::platform {
std::shared_ptr<NativeDockDrag> makeCocoaDockDrag(Window &window, DockDragCallbacks callbacks) {
    window.initialize();
    auto native = (__bridge NSWindow *)window.nativeHandle();
    if (!native.contentView)
        return {};
    auto result =
        std::make_shared<macos::CocoaDockDrag>(window, native.contentView, std::move(callbacks));
    macos::adapters[native.contentView] = result;
    result->closed = window.observeClosed([weak = std::weak_ptr<macos::CocoaDockDrag>(result)] {
        if (auto self = weak.lock()) {
            macos::adapters.erase(self->view);
            self->window = nullptr;
            self->finish(NativeDragResult::Cancelled, NSZeroPoint);
        }
    });
    macos::updateDraggedTypes(native.contentView,
                              static_cast<DesktopWindow &>(window).acceptsFileDrop());
    return result;
}
} // namespace oneui::platform
