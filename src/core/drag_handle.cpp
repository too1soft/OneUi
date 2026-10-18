#include "oneui/controls/drag_handle.h"
#include <cmath>
#include <unordered_map>
namespace oneui {
namespace {
// Out-of-line extension preserves the frozen DragHandle object layout.
thread_local std::unordered_map<const DragHandle*,std::function<void(const MouseEvent&)>> activations;
void activate(const DragHandle* owner,const MouseEvent& event) {
    auto found=activations.find(owner);
    if(found!=activations.end()) {auto callback=found->second;callback(event);}
}
}
DragHandle::~DragHandle(){activations.erase(this);}
void DragHandle::setOnActivated(std::function<void(const MouseEvent&)> callback) {
    if(callback)activations[this]=std::move(callback);else activations.erase(this);
}
bool DragHandle::onMouseDown(const MouseEvent& event) {
    if(!interactive())return false;
    if(View::onMouseDown(event))return true;
    if(event.button==MouseButton::Right){activate(this,event);return true;}
    if(event.button!=MouseButton::Left)return false;
    start_=point_=event.position;armed_=true;return true;
}
bool DragHandle::onMouseMove(const MouseEvent& event) {
    if(!armed_)return View::onMouseMove(event);
    point_=event.position;
    if(!dragging_ && std::hypot(point_.x-start_.x,point_.y-start_.y)>=threshold) {
        dragging_=true;if(callback_)callback_(0,point_);
    }
    if(dragging_ && callback_)callback_(1,point_);
    return true;
}
bool DragHandle::onMouseUp(const MouseEvent& event) {
    if(!armed_)return View::onMouseUp(event);
    auto dragging=dragging_;armed_=dragging_=false;
    if(dragging && callback_)callback_(2,event.position);
    if(!dragging)activate(this,event);
    return true;
}
bool DragHandle::onKeyDown(const KeyEvent& event) {
    if(armed_ && event.pressed && event.key==Key::Escape){resetInteractionState();return true;}
    if(!armed_ && event.pressed && (event.key==Key::Enter||event.key==Key::Space)) {
        auto r=frame();activate(this,{{r.x,r.y},MouseButton::Left,false,false,false,1});return true;
    }
    return View::onKeyDown(event);
}
void DragHandle::resetInteractionState() {
    auto dragging=dragging_;armed_=dragging_=false;View::resetInteractionState();
    if(dragging && callback_)callback_(3,point_);
}
} // namespace oneui
