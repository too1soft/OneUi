#include "oneui/controls/pane_header.h"
#include "oneui/controls/icon_button.h"
#include "oneui/style.h"
#include "internal/widget_presentation.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace oneui {
namespace {
constexpr PaneHeaderAction actions[]={PaneHeaderAction::Split,PaneHeaderAction::Maximize,PaneHeaderAction::Broadcast,
                                     PaneHeaderAction::Live,PaneHeaderAction::Close,PaneHeaderAction::Menu,PaneHeaderAction::Menu};
constexpr IconSymbol icons[]={IconSymbol::OutlineWorkbenchSplit,IconSymbol::OutlineWorkbenchMaximize,IconSymbol::OutlineWorkbenchBroadcast,
                              IconSymbol::ChevronDown,IconSymbol::OutlineWorkbenchClose,IconSymbol::OutlineWorkbenchGrip,IconSymbol::OutlineWorkbenchMore};
constexpr const wchar_t* labels[]={L"Split pane right",L"Maximize or restore pane",L"Broadcast",L"Return to live output",L"Close pane",L"Move pane or open menu",L"Pane actions"};
}
PaneHeader::PaneHeader() {
    setPreferredSize({220,30});setAccessibleName(L"Pane header");
    for(size_t i=0;i<std::size(actions);++i) {
        auto button=std::make_shared<IconButton>(icons[i]);button->setAccessibleName(labels[i]);
        if(i<buttons_.size())buttons_[i]=button;
        else internal::WidgetPresentationAccess::get(*this).paneHeaderMore=button;
        button->setTooltip(labels[i]);button->setOnClick([this,i]{
            auto b=actionButtons()[i]->frame();emit(actions[i],{b.x+b.width,b.y+b.height});
        });add(button);
    }
}
std::array<std::shared_ptr<IconButton>,7> PaneHeader::actionButtons() const {
    const auto* state=internal::WidgetPresentationAccess::find(*this);
    return {buttons_[0],buttons_[1],buttons_[2],buttons_[3],buttons_[4],buttons_[5],state?state->paneHeaderMore:nullptr};
}
void PaneHeader::setConfig(PaneHeaderConfig config) {
    if(config.broadcastRole!="none" && config.broadcastRole!="source" && config.broadcastRole!="receiver")
        throw std::invalid_argument("Invalid broadcast role");
    if(config.number<1)throw std::invalid_argument("Pane number must be positive");
    if(config.pane!=config_.pane)resetInteractionState();
    const bool roleChanged=config.broadcastRole!=config_.broadcastRole;
    config_=std::move(config);buttons_[2]->setTooltip(config_.broadcastHint);
    if(roleChanged)refreshButtonStyles();
    setTooltip(config_.title+(config_.path.empty()?L"":L"\n"+config_.path));
    layoutChildren();invalidate();
}
void PaneHeader::setStyleBox(StyleBox style) {
    style_=std::move(style);refreshButtonStyles();invalidate();
}
void PaneHeader::styleScopeChanged() {
    View::styleScopeChanged();refreshButtonStyles();invalidate();
}
StyleBox PaneHeader::partStyle(const char* part) const {
    auto sheet=styleScope();if(!sheet)return {};
    StyleNode node{"pane-header-part",{part,config_.active?"pane-header-active":"pane-header-inactive"},0};
    return sheet->resolve(node);
}
void PaneHeader::refreshButtonStyles() {
    auto sheet=std::make_shared<StyleSheet>();
    StyleBox button;button.foreground=style_.foreground;button.padding=Insets{4};
    button.borderWidth=1.5f;button.background.color=Color{0,0,0,0};button.radius=4;
    sheet->addRule({"pane-header-part",button});
    if(auto scope=styleScope()) {
        for(auto& token:scope->customProperties())sheet->setCustomProperty(token.first,token.second);
        for(auto& rule:scope->rules())sheet->addRule(rule);
    }
    const auto buttons=actionButtons();
    for(size_t i=0;i<buttons.size();++i)if(buttons[i]) {
        StyleNode node{"pane-header-part",{"pane-header-button"},0};
        if(i==2 && config_.broadcastRole!="none")node.classes.push_back("pane-header-broadcast-on");
        buttons[i]->setStyleSheet(sheet,std::move(node));
    }
}
void PaneHeader::setActionLabel(PaneHeaderAction action,std::wstring label) {
    bool found=false;
    const auto buttons=actionButtons();
    for(size_t i=0;i<buttons.size();++i)if(actions[i]==action) {
        buttons[i]->setAccessibleName(label);buttons[i]->setTooltip(label);found=true;
    }
    if(!found)throw std::invalid_argument("Pane header action has no button");
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
    const auto buttons=actionButtons();
    for(size_t i=0;i<buttons.size();++i)if(actions[i]==action)return buttons[i]->frame();return {};
}
void PaneHeader::layoutChildren() {
    const auto buttons=actionButtons();
    auto r=frame();float right=r.x+r.width-6;
    buttons_[0]->setVisible(!compact() && config_.canSplit);buttons_[1]->setVisible(!compact() && config_.canMaximize);
    buttons_[3]->setVisible(config_.viewingHistory);buttons_[4]->setVisible(config_.canClose);
    // Product reference order: broadcast, split, maximize, more, close, live.
    for(auto i:{3,4,6,1,0,2})if(buttons[i] && buttons[i]->visible()) {
        right-=24;buttons[i]->setFrame({right,r.y+2.5f,24,24});right-=4;
    }
    buttons_[5]->setFrame({r.x+8,r.y+1.5f,36,26});
    if(actions_ && actions_->visible()){right-=actions_->naturalSize().width;actions_->setFrame({right,r.y,actions_->naturalSize().width,29});}
    if(identity_)identity_->setFrame({r.x+48,r.y,std::max(0.f,right-r.x-48),29});
    if(menu_)menu_->setFrame({r.x,r.y+r.height,r.width,menu_->naturalSize().height});
}
void PaneHeader::paint(Canvas& canvas) {
    const auto buttons=actionButtons();
    layoutChildren();auto r=frame();const auto& t=theme();
    Color background=style_.background.color.value_or(t.surface),foreground=style_.foreground.value_or(t.text);
    canvas.fillRect(r,background);
    auto rule=partStyle("pane-header-rule");
    canvas.fillRect({r.x,r.y+29,r.width,1},rule.background.color.value_or(config_.active?style_.borderColor.value_or(t.primary):Color{0,0,0,0}));
    const bool reveal=hovered_||config_.active||focusVisible()||std::any_of(buttons.begin(),buttons.end(),[](auto& b){return b->focusVisible();});
    for(size_t i=0;i<buttons.size();++i)if(i!=5 && buttons[i]->visible() && (reveal || (i==2 && config_.broadcastRole!="none")))buttons[i]->paint(canvas);
    auto grip=partStyle("pane-header-grip");
    paintIcon(canvas,IconSymbol::OutlineWorkbenchGrip,{r.x+10,r.y+6.5f,12,16},grip.foreground.value_or(foreground),Color{0,0,0,0},1.5f);
    auto badge=partStyle("pane-header-number");
    if(!badge.background.color)badge.background.color=config_.active?style_.borderColor.value_or(t.primary):background;
    if(!badge.radius)badge.radius=5;
    paintStyleBox(canvas,{r.x+25,r.y+6,17,17},badge);
    canvas.drawTextStyledEllipsized(std::to_wstring(config_.number),{r.x+25,r.y+6,17,17},
        badge.foreground.value_or(config_.active?background:foreground),badge.fontSize.value_or(11),TextAlign::Center,badge.fontWeight.value_or(400));
    if(identity_) {if(identity_->visible())identity_->paint(canvas);}
    else {
        auto title=config_.title;
        if(config_.broadcastRole!="none")title=!config_.broadcastLabel.empty()?config_.broadcastLabel:config_.broadcastRole=="source"?L"Source":L"Receiver";
        auto titleStyle=partStyle("pane-header-title"),pathStyle=partStyle("pane-header-path");
        float size=titleStyle.fontSize.value_or(13);
        float available=std::max(r.width-(compact()?128.f:184.f),36.f);
        float width=title.empty()?0:std::min(available,std::clamp(std::ceil(canvas.measureTextWidth(title,size)),36.f,120.f));
        TextBlockStyle text;text.fontSize=size;text.fontWeight=titleStyle.fontWeight.value_or(400);
        text.fontFamily=titleStyle.fontFamily.value_or(L"");text.ellipsis=true;text.maxLines=1;text.align=TextAlign::Left;
        if(!title.empty())canvas.drawTextBlock(title,{r.x+48,r.y+3.5f,width,22},titleStyle.foreground.value_or(foreground),text);
        float end=r.x+r.width-6;
        for(size_t i=0;i<buttons.size();++i)if(i!=5 && buttons[i]->visible())end=std::min(end,buttons[i]->frame().x-4);
        if(actions_ && actions_->visible())end=std::min(end,actions_->frame().x-4);
        const float start=r.x+48+(title.empty()?0:width+6);
        text.fontSize=pathStyle.fontSize.value_or(13);text.fontWeight=pathStyle.fontWeight.value_or(400);
        text.fontFamily=pathStyle.fontFamily.value_or(L"");
        canvas.drawTextBlock(config_.path,{start,r.y+3.5f,std::max(0.f,end-start),22},pathStyle.foreground.value_or(foreground),text);
    }
    for(auto& slot:{actions_,menu_})if(slot&&slot->visible())slot->paint(canvas);
}
} // namespace oneui
