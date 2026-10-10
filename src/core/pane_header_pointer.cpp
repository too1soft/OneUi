#include "oneui/controls/pane_header.h"
#include <cmath>
namespace oneui {
bool PaneHeader::onMouseDown(const MouseEvent& e) {
    if(!interactive())return false;
    grip_=actionFrame(PaneHeaderAction::Menu).contains(e.position);
    if(!grip_ && View::onMouseDown(e))return true;
    if(e.button==MouseButton::Right){emit(PaneHeaderAction::Menu,e.position);return true;}
    if(e.button!=MouseButton::Left)return false;
    if(!grip_)emit(PaneHeaderAction::Activate,e.position);
    if(e.clickCount==2 && !grip_ && config_.canMaximize){emit(PaneHeaderAction::Maximize,e.position);return true;}
    armed_=true;origin_=point_=e.position;return true;
}
bool PaneHeader::onMouseMove(const MouseEvent& e) {
    const bool hover=frame().contains(e.position);if(hover!=hovered_){hovered_=hover;invalidate();}
    if(!armed_)return View::onMouseMove(e);
    point_=e.position;
    if(!dragging_ && std::hypot(point_.x-origin_.x,point_.y-origin_.y)>=(grip_?4.f:8.f)) {
        dragging_=true;if(drag_)drag_(0,point_);
    }
    if(dragging_ && drag_)drag_(1,point_);return true;
}
bool PaneHeader::onMouseUp(const MouseEvent& e) {
    if(!armed_)return View::onMouseUp(e);
    const bool dragged=dragging_;armed_=dragging_=false;point_=e.position;
    if(dragged){if(drag_)drag_(2,point_);}else if(grip_)emit(PaneHeaderAction::Menu,point_);
    return true;
}
bool PaneHeader::onKeyDown(const KeyEvent& e) {
    if(e.key==Key::Escape && armed_){resetInteractionState();return true;}
    return View::onKeyDown(e);
}
bool PaneHeader::hasInteractionState() const {return armed_||dragging_||hovered_||View::hasInteractionState();}
void PaneHeader::resetInteractionState() {
    const bool dragged=dragging_;armed_=dragging_=hovered_=false;
    View::resetInteractionState();if(dragged && drag_)drag_(3,point_);invalidate();
}
} // namespace oneui
