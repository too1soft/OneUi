#include "dock_workspace_state.h"
#include "oneui/style_sheet.h"
#include "internal/split_geometry.h"
#include "internal/unicode.h"
#include <algorithm>
namespace oneui {
void DockItemView::setFloatingSurface(std::shared_ptr<const StyleBox> surface) {
    if(floatingSurface_==surface)return;
    floatingSurface_=std::move(surface);invalidate();
}
void DockItemView::paint(Canvas& canvas) {
    if(floatingSurface_)paintStyleBox(canvas,frame(),*floatingSurface_);
    View::paint(canvas);
}
void DockItemView::layoutChildren() {
    auto rect=frame();auto h=header.lock(),f=footer.lock(),b=body.lock();
    if(floatingSurface_)if(const float border=floatingSurface_->borderWidth.value_or(0);border>0)
        rect=rect.inset({border,border,border,border});
    if(h&&h->visible()) {
        const float preferred=h->preferredSize().height;
        const float height=preferred>0?preferred:h->naturalSize().height;
        auto r=rect;r.height=std::min(rect.height,std::max(29.f,height));
        h->setFrame(r);rect.y+=r.height;rect.height-=r.height;
    }
    if(f&&f->visible()) {auto r=rect;r.height=std::min(rect.height,f->naturalSize().height);r.y=(rect.y+rect.height)-r.height;f->setFrame(r);rect.height-=r.height;}
    if(b)b->setFrame(rect);
    for(auto& child:children())if(child!=h&&child!=f&&child!=b)child->setFrame(rect);
}
DockWorkspaceView::DockWorkspaceView():state_(std::make_shared<State>()){}
void DockWorkspaceView::setStyleSheet(std::shared_ptr<StyleSheet> sheet,StyleNode node) {
    state_->sheet=std::move(sheet);state_->node=std::move(node);
    state_->floatSurface.reset();state_->floatSurfaceSheet=nullptr;invalidate();
}
void DockWorkspaceView::styleScopeChanged() {
    View::styleScopeChanged();
    state_->floatSurface.reset();state_->floatSurfaceSheet=nullptr;invalidate();
}
StyleBox DockWorkspaceView::partStyle(const char* part,unsigned flags) const {
    const auto& s=*state_;
    const StyleSheet* sheet=s.sheet?s.sheet.get():styleScope().get();
    if(!sheet)return {};
    StyleNode node{"dock-workspace-part",{},flags};
    for(const auto& klass:s.node.classes)if(klass!=s.node.tag)node.classes.push_back(klass);
    node.classes.push_back(part);
    return sheet->resolve(node);
}
std::shared_ptr<const StyleBox> DockWorkspaceView::floatingSurface() const {
    const auto& s=*state_;
    const StyleSheet* sheet=s.sheet?s.sheet.get():styleScope().get();
    if(sheet!=s.floatSurfaceSheet) {
        s.floatSurfaceSheet=sheet;s.floatSurface.reset();
        auto box=partStyle("dock-workspace-float",0);
        if(box.background.color||box.borderColor||!box.shadows.empty())
            s.floatSurface=std::make_shared<const StyleBox>(std::move(box));
    }
    return s.floatSurface;
}
DockWorkspaceView::~DockWorkspaceView()=default;
const DockWorkspaceSnapshot& DockWorkspaceView::snapshot() const{return state_->snapshot;}
const DockWorkspacePresentation& DockWorkspaceView::presentation() const{return state_->presentation;}
void DockWorkspaceView::setPresentation(DockWorkspacePresentation value){state_->presentation=std::move(value);invalidate();}
void DockWorkspaceView::setOnIntent(std::function<void(const DockIntent&)> callback){intent_=std::move(callback);}
void DockWorkspaceView::setSnapshot(DockWorkspaceSnapshot s,std::vector<std::shared_ptr<DockItemView>> items){prepare(std::move(s),std::move(items))();}
void DockWorkspaceView::setSnapshot(DockWorkspaceSnapshot s,std::vector<std::shared_ptr<DockItemView>> items,DockWorkspacePresentation presentation){prepare(std::move(s),std::move(items),std::move(presentation))();}
void DockWorkspaceView::emit(DockIntent e) {
    e.revision=state_->snapshot.revision;e.request=++request_;
    if(e.kind==DockIntentKind::Dock||e.kind==DockIntentKind::Float||e.kind==DockIntentKind::Bounds)state_->pendingRequest=e.request;
    if(intent_){auto callback=intent_;callback(e);}
}
Size DockWorkspaceView::naturalSize() const{return state_->index.minimum("root");}
void DockWorkspaceView::layoutChildren() {
    auto& s=*state_;const auto& m=s.snapshot;auto viewport=frame();
    auto active=m.maximized.empty()?m.compactActive:m.maximized;
    const auto surface=floatingSurface();
    for(auto& item:s.items) {
        item.shown=item.placement.mode!="hidden" && (m.maximized.empty()?active.empty()||item.placement.mode=="floating"||item.view->key==active:item.view->key==active);
        item.view->setLayoutVisible(item.shown);
        const bool floating=m.maximized.empty()&&item.placement.mode=="floating";
        item.view->setFloatingSurface(floating?surface:nullptr);
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
    for(size_t i=0;i<s.nodes.size();++i) {
        auto& n=s.nodes[i];if(n.divider.width<=0||n.divider.height<=0)continue;
        const auto flags=(s.gesture==State::Gesture::Divider&&s.divider==i?StyleStateActive:0u)
            |(focusVisible()&&s.focusedDivider==i?StyleStateFocus:0u);
        const Color color=partStyle("dock-workspace-divider",flags).background.color.value_or(Color{90,100,110,70});
        c.fillRect(n.divider,color);
    }
    View::paint(c); // Floating content covers dividers beneath it.
    if(s.previewActive){
        const auto preview=partStyle("dock-workspace-preview",0),caption=partStyle("dock-workspace-caption",0);
        const Color fill=preview.background.color.value_or(Color{54,153,255,24}),border=preview.borderColor.value_or(Color{54,153,255,128});
        const Color ink=caption.foreground.value_or(Color{32,33,36});Color backing=caption.background.color.value_or(Color{255,255,255});
        c.fillRect(s.preview,fill,12);c.strokeRect(s.preview,border,12,1);
        std::wstring label;
        auto title=[](const State::Item& item){return item.view->accessibleName().empty()?unicode::fromUtf8(item.view->key):item.view->accessibleName();};
        if(s.source!=State::none) {
            if(s.hit&&s.hit->placement!=DockHitPlacement::None) {
                label=title(s.items[s.source])+L" → ";
                for(auto& item:s.items)if(item.identity==s.hit->target)label+=title(item);
                label+=L" · "+unicode::fromUtf8(s.presentation.previewDockText);
            } else if(alt_)label=title(s.items[s.source])+L" · "+unicode::fromUtf8(s.presentation.previewFloatText);
            else label=unicode::fromUtf8(s.presentation.previewKeepText);
        }
        auto host=frame();float width=std::min(std::max(0.f,host.width-24),c.measureTextWidth(label,12,500)+16);
        Rect chip{std::clamp(s.preview.x+12,host.x+12,std::max(host.x+12,host.x+host.width-width-12)),
                  std::clamp(s.preview.y+12,host.y+12,std::max(host.y+12,host.y+host.height-40)),width,28};
        backing.a=255;c.fillRect(chip,backing,6);
        c.drawTextStyled(label,{chip.x+8,chip.y,chip.width-16,chip.height},ink,12,TextAlign::Center,500);
    }
    if(s.hit&&s.gesture==State::Gesture::Move) {
        auto r=s.hit->bounds;float x=r.x+r.width*.5f,y=r.y+r.height*.5f;
        Rect guides[]={{x-68,y-18,42,36},{x+26,y-18,42,36},{x-21,y-58,42,36},{x-21,y+22,42,36}};
        const wchar_t* labels[]={L"←",L"→",L"↑",L"↓"};
        for(int i=0;i<4;++i) {
            const bool active=int(s.hit->placement)==i+1;
            const auto guide=partStyle("dock-workspace-guide",active?StyleStateSelected:0u);
            const Color fill=guide.background.color.value_or(active?accent:Color{90,100,110,220});
            const float radius=guide.radius.value_or(5);
            c.fillRect(guides[i],fill,radius);
            if(guide.borderColor&&guide.borderWidth.value_or(0)>0)c.strokeRect(guides[i],*guide.borderColor,radius,*guide.borderWidth);
            c.drawText(labels[i],guides[i],guide.foreground.value_or(Color{255,255,255}),16);
        }
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
