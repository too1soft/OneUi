#pragma once
#import <Cocoa/Cocoa.h>
namespace oneui::platform {
class DesktopWindow;
}
namespace oneui::macos {
void recordDockMouseDown(NSView *, NSEvent *);
void updateDraggedTypes(NSView *, bool files);
NSDragOperation draggingOffer(NSView *, platform::DesktopWindow *, id<NSDraggingInfo>);
bool performDrop(NSView *, platform::DesktopWindow *, id<NSDraggingInfo>);
void leaveDockDrag(NSView *);
} // namespace oneui::macos
