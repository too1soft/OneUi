#include "oneui/controls/pane_header.h"
#include "oneui/controls/icon_button.h"
#include "oneui/style.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace oneui {
namespace {
constexpr PaneHeaderAction actions[]={PaneHeaderAction::Split,PaneHeaderAction::Maximize,PaneHeaderAction::Broadcast,
                                     PaneHeaderAction::Live,PaneHeaderAction::Close,PaneHeaderAction::Menu};
constexpr IconSymbol icons[]={IconSymbol::OutlineWorkbenchSplit,IconSymbol::OutlineWorkbenchMaximize,IconSymbol::Share,
                              IconSymbol::Play,IconSymbol::OutlineClose,IconSymbol::OutlineWorkbenchGrip};
constexpr const wchar_t* labels[]={L"Split pane right",L"Maximize or restore pane",L"Broadcast",L"Return to live output",L"Close pane",L"Move pane or open menu"};
}
PaneHeader::PaneHeader() {
    setPreferredSize({220,29});setAccessibleName(L"Pane header");
    for(size_t i=0;i<buttons_.size();++i) {
        buttons_[i]=std::make_shared<IconButton>(icons[i]);buttons_[i]->setAccessibleName(labels[i]);
        buttons_[i]->setTooltip(labels[i]);buttons_[i]->setOnClick([this,i]{
            auto b=buttons_[i]->frame();emit(actions[i],{b.x+b.width,b.y+b.height});
        });add(buttons_[i]);
    }
}
void PaneHeader::setConfig(PaneHeaderConfig config) {
    if(config.broadcastRole!="none" && config.broadcastRole!="source" && config.broadcastRole!="receiver")
        throw std::invalid_argument("Invalid broadcast role");
    if(config.number<1)throw std::invalid_argument("Pane number must be positive");
    if(config.pane!=config_.pane)resetInteractionState();
    config_=std::move(config);buttons_[2]->setTooltip(config_.broadcastHint);
    setTooltip(config_.title+(config_.path.empty()?L"":L"\n"+config_.path));
    layoutChildren();invalidate();
}
void PaneHeader::setStyleBox(StyleBox style){
    style_=std::move(style);
    auto sheet=std::make_shared<StyleSheet>();StyleBox button;
    button.foreground=style_.foreground;button.padding=Insets{4};button.borderWidth=1.5f;
    sheet->addRule({"button",button});
    for(auto& b:buttons_)b->setStyleSheet(sheet,{"button",{},0});
    invalidate();
}
void PaneHeader::setOnIntent(std::function<void(PaneHeaderAction,Point)> callback){intent_=std::move(callback);}
void PaneHeader::setOnDrag(std::function<void(int,Point)> callback){drag_=std::move(callback);}
void PaneHeader::emit(PaneHeaderAction action,Point point){if(intent_ && interactive()){auto callback=intent_;callback(action,point);}}
void PaneHeader::setSlot(const std::string& name,std::shared_ptr<Widget> widget) {
    auto* slot=name=="identity"?&identity_:name=="actions"?&actions_:name=="menu"?&menu_:nullptr;
    if(!slot)throw std::invalid_argument("Invalid PaneHeader slot");
    std::vector<std::shared_ptr<Widget>> next;
    for(auto& child:children())if(child!=*slot)next.push_back(child);
    if(widget)next.push_back(widget);auto update=prepareChildren(std::move(next));update->commit();
    *slot=std::move(widget);layoutChildren();invalidate();
}
Rect PaneHeader::actionFrame(PaneHeaderAction action) const {
    for(size_t i=0;i<buttons_.size();++i)if(actions[i]==action)return buttons_[i]->frame();return {};
}
void PaneHeader::layoutChildren() {
    auto r=frame();float right=r.x+r.width-4;
    buttons_[0]->setVisible(!compact() && config_.canSplit);buttons_[1]->setVisible(!compact() && config_.canMaximize);
    buttons_[3]->setVisible(config_.viewingHistory);buttons_[4]->setVisible(config_.canClose);
    for(int i=4;i>=0;--i)if(buttons_[i]->visible()){right-=24;buttons_[i]->setFrame({right,r.y+2,24,24});}
    buttons_[5]->setFrame({r.x,r.y+1,36,26});
    if(actions_ && actions_->visible()){right-=actions_->naturalSize().width;actions_->setFrame({right,r.y,actions_->naturalSize().width,29});}
    if(identity_)identity_->setFrame({r.x+39,r.y,std::max(0.f,right-r.x-43),29});
    if(menu_)menu_->setFrame({r.x,r.y+r.height,r.width,menu_->naturalSize().height});
}
void PaneHeader::paint(Canvas& canvas) {
    layoutChildren();auto r=frame();const auto& t=theme();
    Color background=style_.background.color.value_or(t.surface),foreground=style_.foreground.value_or(t.text);
    canvas.fillRect(r,background);
    if(config_.active)canvas.fillRect({r.x,r.y,r.width,1},style_.borderColor.value_or(t.primary));
    const bool reveal=hovered_||config_.active||focusVisible()||std::any_of(buttons_.begin(),buttons_.end(),[](auto& b){return b->focusVisible();});
    for(size_t i=0;i<buttons_.size();++i)if(buttons_[i]->visible() && (reveal || (i==2 && config_.broadcastRole!="none")))buttons_[i]->paint(canvas);
    canvas.fillEllipse({r.x+11,r.y+6,17,17},config_.active?style_.borderColor.value_or(t.primary):background);
    canvas.drawText(std::to_wstring(config_.number),{r.x+10,r.y+4,19,20},config_.active?background:foreground,11);
    if(identity_) {if(identity_->visible())identity_->paint(canvas);}
    else {
        auto title=config_.title;
        if(config_.broadcastRole!="none")title=!config_.broadcastLabel.empty()?config_.broadcastLabel:config_.broadcastRole=="source"?L"Source":L"Receiver";
        float available=std::max(r.width-(compact()?128.f:184.f),36.f);
        float width=std::min(available,std::clamp(std::ceil(canvas.measureTextWidth(title,12)),36.f,120.f));
        TextBlockStyle text;text.fontSize=12;text.ellipsis=true;text.maxLines=1;text.align=TextAlign::Left;
        canvas.drawTextBlock(title,{r.x+39,r.y+6,width,18},foreground,text);
        float end=r.x+r.width-4;
        for(size_t i=0;i<5;++i)if(buttons_[i]->visible())end=std::min(end,buttons_[i]->frame().x);
        canvas.drawTextBlock(config_.path,{r.x+43+width,r.y+6,std::max(0.f,end-r.x-width-47),18},foreground,text);
    }
    for(auto& slot:{actions_,menu_})if(slot&&slot->visible())slot->paint(canvas);
}
} // namespace oneui
