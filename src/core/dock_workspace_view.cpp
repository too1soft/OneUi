#include "dock_workspace_state.h"
#include "internal/split_geometry.h"
#include <algorithm>
namespace oneui {
void DockItemView::layoutChildren() {
    auto rect=frame();auto h=header.lock(),f=footer.lock(),b=body.lock();
    if(h&&h->visible()) {auto r=rect;r.height=std::min(rect.height,std::max(29.f,h->naturalSize().height));h->setFrame(r);rect.y+=r.height;rect.height-=r.height;}
    if(f&&f->visible()) {auto r=rect;r.height=std::min(rect.height,f->naturalSize().height);r.y=(rect.y+rect.height)-r.height;f->setFrame(r);rect.height-=r.height;}
    if(b)b->setFrame(rect);
    for(auto& child:children())if(child!=h&&child!=f&&child!=b)child->setFrame(rect);
}
DockWorkspaceView::DockWorkspaceView():state_(std::make_shared<State>()){}
DockWorkspaceView::~DockWorkspaceView()=default;
const DockWorkspaceSnapshot& DockWorkspaceView::snapshot() const{return state_->snapshot;}
void DockWorkspaceView::setOnIntent(std::function<void(const DockIntent&)> callback){intent_=std::move(callback);}
void DockWorkspaceView::setSnapshot(DockWorkspaceSnapshot s,std::vector<std::shared_ptr<DockItemView>> items){prepare(std::move(s),std::move(items))();}
void DockWorkspaceView::emit(DockIntent e) {
    e.revision=state_->snapshot.revision;e.request=++request_;
    if(e.kind==DockIntentKind::Dock||e.kind==DockIntentKind::Float||e.kind==DockIntentKind::Bounds)state_->pendingRequest=e.request;
    if(intent_){auto callback=intent_;callback(e);}
}
Size DockWorkspaceView::naturalSize() const{return state_->index.minimum("root");}
void DockWorkspaceView::layoutChildren() {
    auto& s=*state_;const auto& m=s.snapshot;auto viewport=frame();
    auto active=m.maximized.empty()?m.compactActive:m.maximized;
    for(auto& item:s.items) {
        item.shown=item.placement.mode!="hidden" && (m.maximized.empty()?active.empty()||item.placement.mode=="floating"||item.view->key==active:item.view->key==active);
        item.view->setLayoutVisible(item.shown);
    }
    if(!active.empty()&&s.itemIds.count(active))s.items[s.itemIds.at(active)].view->setFrame(viewport);
    for(auto& n:s.nodes)n.divider={};
    size_t count=0;
    if(active.empty()&&s.root!=State::none)s.work[count++]={s.root,viewport};
    while(count) {
        auto work=s.work[--count];auto& n=s.nodes[work.node];n.bounds=work.rect;
        if(n.item!=State::none){s.items[n.item].view->setFrame(work.rect);continue;}
        bool h=m.nodes[work.node].axis==DockAxis::Horizontal;
        float gap=std::min(m.gap,h?work.rect.width:work.rect.height),extent=std::max(0.f,(h?work.rect.width:work.rect.height)-gap);
        auto a=s.nodes[n.first].minimum,b=s.nodes[n.second].minimum;
        auto amount=detail::splitAmount(extent,n.ratio,h?a.width:a.height,h?b.width:b.height);
        auto rects=detail::splitRects(work.rect,gap,amount,h);n.divider=rects.divider;
        s.work[count++]={n.second,rects.second};s.work[count++]={n.first,rects.first};
    }
    if(m.maximized.empty())for(size_t i=0;i<s.items.size();++i) {
        auto& item=s.items[i];if(!item.shown||item.placement.mode!="floating")continue;
        Rect local,out;local=item.placement.bounds;
        if(i==s.source&&(s.gesture==State::Gesture::Move||s.gesture==State::Gesture::Resize||s.pendingRequest))local=s.bounds;
        if(clampDockFloating(local,{0,0,viewport.width,viewport.height},item.view->floatMinimum,out)==DockError::None) {
            out.x+=viewport.x;out.y+=viewport.y;item.view->setFrame(out);
        }
    }
}
std::optional<Rect> DockWorkspaceView::previewRect() const{return state_->previewActive?std::optional<Rect>(state_->preview):std::nullopt;}
bool DockWorkspaceView::paintPending() const{return state_->pendingRequest!=0;}
void DockWorkspaceView::setMotionEnabled(bool value){motion_=value;if(!value){state_->preview=state_->previewTarget;invalidate();}}
void DockWorkspaceView::paint(Canvas& c) {
    layoutChildren();auto& s=*state_;Color accent{40,97,61};
    for(auto& n:s.nodes)if(n.divider.width>0&&n.divider.height>0)c.fillRect(n.divider,{90,100,110,70});
    View::paint(c); // Floating content covers dividers beneath it.
    if(s.previewActive){c.fillRect(s.preview,{40,97,61,40},8);c.strokeRect(s.preview,accent,8,2);}
    if(s.hit&&s.gesture==State::Gesture::Move) {
        auto r=s.hit->bounds;float x=r.x+r.width*.5f,y=r.y+r.height*.5f;
        Rect guides[]={{x-68,y-18,42,36},{x+26,y-18,42,36},{x-21,y-58,42,36},{x-21,y+22,42,36}};
        const wchar_t* labels[]={L"←",L"→",L"↑",L"↓"};
        for(int i=0;i<4;++i){bool active=int(s.hit->placement)==i+1;c.fillRect(guides[i],active?accent:Color{90,100,110,220},5);c.drawText(labels[i],guides[i],{255,255,255},16);}
    }
}
void DockWorkspaceView::requestAction(DockIntentKind kind,const std::string& source,bool flag) {
    auto found=state_->itemIds.find(source);if(found==state_->itemIds.end())return;
    const auto& item=state_->items[found->second];
    if(kind==DockIntentKind::Visibility&&!flag&&!item.view->hideable)return;
    DockIntent e{kind,source};e.flag=flag;
    if(kind==DockIntentKind::Float) {
        auto r=item.view->frame();auto host=frame();r.x-=host.x;r.y-=host.y;
        if(r.width<=0||r.height<=0)r={0,0,400,360};
        r.width=std::min(r.width,560.f);r.height=std::min(r.height,620.f);
        clampDockFloating(r,{0,0,host.width,host.height},item.view->floatMinimum,e.bounds);
    }
    emit(std::move(e));
}
} // namespace oneui
