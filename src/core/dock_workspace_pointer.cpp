#include "dock_workspace_state.h"
#include "oneui/controls/drag_handle.h"
#include "internal/split_geometry.h"
#include "internal/ui_clock.h"
#include <cmath>
namespace oneui {
size_t DockWorkspaceView::dividerAt(Point point) const {
    const auto& s=*state_;
    if(!s.snapshot.maximized.empty()||!s.snapshot.compactActive.empty())return State::none;
    // A floating tool consumes input before the docked divider below it.
    for(auto& item:s.items)if(item.shown&&item.placement.mode=="floating"&&item.view->frame().contains(point))return State::none;
    for(size_t i=0;i<s.nodes.size();++i) {
        auto r=s.nodes[i].divider;if(r.width<=0||r.height<=0)continue;
        auto hit=std::max(s.snapshot.gap,s.snapshot.dividerHit);
        if(s.snapshot.nodes[i].axis==DockAxis::Horizontal){r.x+=(r.width-hit)*.5f;r.width=hit;}
        else {r.y+=(r.height-hit)*.5f;r.height=hit;}
        if(r.contains(point))return i;
    }
    return State::none;
}
std::pair<size_t,std::optional<DockResizeEdge>> DockWorkspaceView::resizeAt(Point point) const {
    const auto& s=*state_;if(!s.snapshot.maximized.empty())return {State::none,{}};
    for(auto it=children().rbegin();it!=children().rend();++it) {
        auto item=std::dynamic_pointer_cast<DockItemView>(*it);if(!item||!item->visible())continue;
        auto r=item->frame();if(!r.contains(point))continue;
        auto found=s.itemIds.find(item->key);if(found==s.itemIds.end())continue;
        if(s.items[found->second].placement.mode!="floating")return {State::none,{}};
        bool left=point.x-r.x<6,right=(r.x+r.width)-point.x<=6,top=point.y-r.y<6,bottom=(r.y+r.height)-point.y<=6;
        std::optional<DockResizeEdge> edge;
        if(top)edge=left?DockResizeEdge::NorthWest:right?DockResizeEdge::NorthEast:DockResizeEdge::North;
        else if(bottom)edge=left?DockResizeEdge::SouthWest:right?DockResizeEdge::SouthEast:DockResizeEdge::South;
        else if(left)edge=DockResizeEdge::West;else if(right)edge=DockResizeEdge::East;
        return {found->second,edge};
    }
    return {State::none,{}};
}
void DockWorkspaceView::updateResize(Point point) {
    auto& s=*state_;auto f=frame();const auto& item=s.items[s.source];
    resizeDockFloating(s.start,{point.x-s.origin.x,point.y-s.origin.y},s.resizeEdge,{0,0,f.width,f.height},item.view->floatMinimum,s.bounds);
    layoutChildren();invalidate();
}
void DockWorkspaceView::updateDivider(Point point) {
    auto& s=*state_;auto& n=s.nodes[s.divider];bool h=s.snapshot.nodes[s.divider].axis==DockAxis::Horizontal;
    float available=std::max(0.f,(h?n.bounds.width:n.bounds.height)-s.snapshot.gap);if(available<=0)return;
    float ratio=((h?point.x-n.bounds.x:point.y-n.bounds.y)-s.grab-s.snapshot.gap*.5f)/available;
    auto a=s.nodes[n.first].minimum,b=s.nodes[n.second].minimum;
    n.ratio=detail::splitAmount(available,std::clamp(ratio,.001f,.999f),h?a.width:a.height,h?b.width:b.height)/available;
    layoutChildren();invalidate();
}
bool DockWorkspaceView::onMouseDown(const MouseEvent& e) {
    if(!interactive())return false;
    alt_=e.alt;state_->shift=e.shift;state_->control=e.control;pointerOrigin_=e.position;
    if(e.button!=MouseButton::Left)return View::onMouseDown(e);
    auto [item,edge]=resizeAt(e.position);
    if(item!=State::none && state_->presentation.raiseOnPress) {
        auto key=state_->items[item].view->key;
        requestAction(DockIntentKind::Raise,key);
        item=state_->itemIds.count(key)?state_->itemIds.at(key):State::none;
    }
    if(edge&&item!=State::none) {
        auto& s=*state_;s.source=item;s.gesture=State::Gesture::Resize;s.resizeEdge=*edge;s.origin=e.position;
        s.start=s.items[item].view->frame();auto f=frame();s.start.x-=f.x;s.start.y-=f.y;s.bounds=s.start;return true;
    }
    auto divider=dividerAt(e.position);
    if(divider!=State::none) {
        auto& s=*state_;s.divider=s.focusedDivider=divider;s.beginRatio=s.nodes[divider].ratio;
        if(e.clickCount==2){s.nodes[divider].ratio=.5f;s.divider=State::none;layoutChildren();DockIntent event{DockIntentKind::Ratio,s.snapshot.nodes[divider].id};event.ratio=.5;emit(std::move(event));return true;}
        auto r=s.nodes[divider].divider;bool h=s.snapshot.nodes[divider].axis==DockAxis::Horizontal;
        s.grab=h?e.position.x-r.x-r.width*.5f:e.position.y-r.y-r.height*.5f;
        s.gesture=State::Gesture::Divider;return true;
    }
    // Only an actual title DragHandle owns the double-click, never a toolbar
    // button or the interactive body inside PanelHeader.
    if(e.clickCount==2)for(auto it=children().rbegin();it!=children().rend();++it) {
        auto item=std::dynamic_pointer_cast<DockItemView>(*it);if(!item||!item->visible()||!item->frame().contains(e.position))continue;
        std::function<Widget*(const std::shared_ptr<Widget>&)> hit = [&](const auto& w)->Widget* {
            if(!w||!w->visible()||!w->frame().contains(e.position))return nullptr;
            if(auto v=std::dynamic_pointer_cast<View>(w))
                for(auto i=v->children().rbegin();i!=v->children().rend();++i)
                    if(auto* child=hit(*i))return child;
            return w->isFocusable()?w.get():nullptr;
        };
        if(dynamic_cast<DragHandle*>(hit(item->header.lock()))) {
            requestAction(DockIntentKind::Maximize,item->key);return true;
        }
        break;
    }
    return View::onMouseDown(e);
}
bool DockWorkspaceView::onMouseMove(const MouseEvent& e) {
    alt_=e.alt;state_->shift=e.shift;state_->control=e.control;
    if(state_->gesture==State::Gesture::Resize){updateResize(e.position);return true;}
    if(state_->gesture==State::Gesture::Divider){updateDivider(e.position);return true;}
    return View::onMouseMove(e);
}
bool DockWorkspaceView::onMouseUp(const MouseEvent& e) {
    alt_=e.alt;state_->shift=e.shift;state_->control=e.control;
    if(state_->gesture==State::Gesture::Resize) {
        updateResize(e.position);auto& s=*state_;
        DockIntent event{DockIntentKind::Bounds,s.items[s.source].view->key};event.bounds=s.bounds;
        s.gesture=State::Gesture::None;emit(std::move(event));return true;
    }
    if(state_->gesture==State::Gesture::Divider) {
        updateDivider(e.position);auto& s=*state_;
        DockIntent event{DockIntentKind::Ratio,s.snapshot.nodes[s.divider].id};event.ratio=s.nodes[s.divider].ratio;
        s.gesture=State::Gesture::None;s.divider=State::none;emit(std::move(event));return true;
    }
    return View::onMouseUp(e);
}
bool DockWorkspaceView::onKeyDown(const KeyEvent& e) {
    auto& s=*state_;
    if(e.pressed&&e.key==Key::Escape&&(s.gesture!=State::Gesture::None||s.keyboardDirty||s.pendingRequest)) {resetInteractionState();return true;}
    size_t i=s.focusedDivider;
    if(!e.pressed||e.alt||e.control||e.win||i==State::none||s.gesture!=State::Gesture::None||!s.snapshot.maximized.empty()||!s.snapshot.compactActive.empty())return View::onKeyDown(e);
    auto& n=s.nodes[i];bool h=s.snapshot.nodes[i].axis==DockAxis::Horizontal;float ratio=n.ratio;
    if(e.key==Key::Home)ratio=.001f;else if(e.key==Key::End)ratio=.999f;
    else if(e.key==(h?Key::Left:Key::Up))ratio-=.025f;
    else if(e.key==(h?Key::Right:Key::Down))ratio+=.025f;else return View::onKeyDown(e);
    float extent=std::max(0.f,(h?n.bounds.width:n.bounds.height)-s.snapshot.gap);if(extent<=0)return true;
    auto a=s.nodes[n.first].minimum,b=s.nodes[n.second].minimum;
    if(!s.keyboardDirty)s.beginRatio=n.ratio;
    n.ratio=detail::splitAmount(extent,std::clamp(ratio,.001f,.999f),h?a.width:a.height,h?b.width:b.height)/extent;
    s.divider=i;s.keyboardDirty=true;s.idleAt=internal::uiTimeMs()+120;layoutChildren();invalidate();requestAnimationFrame();return true;
}
CursorKind DockWorkspaceView::cursor(Point p) const {
    auto [item,edge]=resizeAt(p);
    if(edge)switch(*edge) {
    case DockResizeEdge::North:case DockResizeEdge::South:return CursorKind::ResizeVertical;
    case DockResizeEdge::East:case DockResizeEdge::West:return CursorKind::ResizeHorizontal;
    case DockResizeEdge::NorthEast:case DockResizeEdge::SouthWest:return CursorKind::ResizeNorthEastSouthWest;
    default:return CursorKind::ResizeNorthWestSouthEast;
    }
    auto i=dividerAt(p);if(i!=State::none)return state_->snapshot.nodes[i].axis==DockAxis::Horizontal?CursorKind::ResizeHorizontal:CursorKind::ResizeVertical;
    return View::cursor(p);
}
} // namespace oneui
