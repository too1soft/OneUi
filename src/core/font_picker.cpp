#include "font_picker_state.h"
#include "picker_styles.h"
#include "internal/unicode.h"
#include "oneui/ui_native_theme.h"
#include <cwctype>
#include <set>
#include <cmath>
#include <stdexcept>
namespace oneui {
FontPicker::FontPicker():state_(std::make_shared<State>()) {
    auto& s=*state_;add(s.selected);add(s.search);add(s.status);add(s.retry);add(s.rows);
    add(s.mono);add(s.apply);add(s.cancel);
    s.mono->setOnChanged([this](bool){filter();});
    s.cancel->setOnClick([this]{setOpen(false);});
    s.apply->setOnClick([this]{
        auto key=state_->pending;if(key.empty())return;
        setOpen(false);if(state_->selectionRequested)state_->selectionRequested(std::move(key));
    });
    s.selected->setOnClick([this]{setOpen(!open());});
    s.search->setAccessibleName(L"Search fonts");s.search->setOnChanged([this](const auto&){filter();});
    s.rows->activate=[this](int row){select(row);};s.retry->setOnClick([this]{if(state_->retryRequested)state_->retryRequested();});
    setConfig({});setOpen(false);
}
FontPicker::~FontPicker()=default;
void FontPicker::styleScopeChanged() {
    View::styleScopeChanged();if(!state_)return;
    auto sheet=styleScope();if(!sheet)return;
    state_->selected->setStyleOverride(buttonStyleOverrideFromStyleSheet(*sheet,{"button",{"one-Button","font-picker-trigger"},0}));
    const auto colors=pickerColors(*sheet);
    auto secondary=buttonStyleOverrideFromStyleSheet(*sheet,{"button",{"one-Button","font-picker-secondary"},0});
    for(auto* slot:{&secondary.normal,&secondary.hovered,&secondary.pressed}) {
        if(!*slot)slot->emplace();auto& style=**slot;
        if(!style.background)style.background=colors.background.color;
        if(!style.foreground)style.foreground=colors.foreground;
        if(!style.border)style.border=colors.borderColor;
    }
    state_->retry->setStyleOverride(secondary);state_->cancel->setStyleOverride(secondary);
    applyPickerButton(*state_->apply,*sheet);
    ui::applyNativeTheme(*state_->mono,*sheet,false);
    auto input=textFieldStyleOverrideFromStyleSheet(*sheet,{"input",{"one-Input","font-picker-search"},0});
    for(auto* slot:{&input.normal,&input.hovered,&input.focusVisible,&input.readOnly}) {
        if(!*slot)slot->emplace();auto& style=**slot;
        if(!style.background)style.background=colors.background.color;
        if(!style.foreground)style.foreground=colors.foreground;
        if(!style.placeholderForeground)style.placeholderForeground=colors.foreground;
        if(!style.caretColor)style.caretColor=colors.foreground;
        if(!style.border)style.border=colors.borderColor;
        if(!style.selectionBackground)style.selectionBackground=colors.selectionColor;
    }
    state_->search->setStyleOverride(input);
    state_->status->setColor(pickerColors(*sheet).foreground.value_or(Color{28,43,35}));
}
const FontPickerConfig& FontPicker::config() const{return state_->config;}
void FontPicker::setConfig(FontPickerConfig c) {
    if(!std::isfinite(c.size)||c.size<=0)throw std::invalid_argument("Invalid font preview size");
    std::set<std::string> keys;
    for(auto& item:c.items)if(item.id.empty()||!keys.insert(item.id).second)throw std::invalid_argument("Invalid font key");
    auto& s=*state_;if(c.selected!=s.config.selected)s.rememberedLabel.clear();
    s.config=std::move(c);auto label=s.rememberedLabel;
    for(auto& item:s.config.items)if(item.id==s.config.selected){label=item.label;s.rememberedLabel=label;break;}
    if(label.empty())label=s.config.selected.empty()?L"Select font":unicode::fromUtf8(s.config.selected);
    s.selected->setText(s.triggerLabel.empty()?label:s.triggerLabel);s.selected->setAccessibleValue(label);
    s.rows->items=s.config.items;s.rows->preview=s.config.previewText;s.rows->size=s.config.size;
    filter();setOpen(s.open);
}
void FontPicker::setTriggerLabel(std::wstring label) {
    if(state_->triggerLabel==label)return;
    state_->triggerLabel=std::move(label);
    if(state_->triggerLabel.empty())state_->selected->clearIcon();
    else state_->selected->setIcon(IconSymbol::ChevronDown);
    setConfig(state_->config);
}
void FontPicker::setOnSelectionRequested(std::function<void(std::string)> cb){state_->selectionRequested=std::move(cb);}
void FontPicker::setConfirmation(bool enabled,std::wstring apply,std::wstring cancel) {
    state_->confirmation=enabled;
    state_->apply->setText(apply.empty()?L"Apply":std::move(apply));
    state_->cancel->setText(cancel.empty()?L"Cancel":std::move(cancel));setOpen(open());
}
void FontPicker::setMonospaceFilter(std::vector<std::string> keys,bool initial,std::wstring label) {
    auto& s=*state_;bool visible=!label.empty();
    if(initial!=s.initialMono||visible!=s.filterVisible)s.mono->setChecked(initial);
    s.initialMono=initial;s.filterVisible=visible;s.mono->setText(std::move(label));
    s.monospaceKeys={keys.begin(),keys.end()};filter();setOpen(s.open);
}
void FontPicker::setSearchLabel(std::wstring label){state_->search->setPlaceholder(label);state_->search->setAccessibleName(std::move(label));}
void FontPicker::setRetryLabel(std::wstring label){state_->retry->setText(label.empty()?L"Retry":std::move(label));}
void FontPicker::setPopupViewportProvider(std::function<Rect()> provider){state_->viewport=std::move(provider);layoutChildren();}
void FontPicker::setOnRetryRequested(std::function<void()> cb){state_->retryRequested=std::move(cb);}
bool FontPicker::open() const{return state_->open;}
void FontPicker::setOpen(bool value) {
    auto& s=*state_;bool changed=s.open!=value;s.open=value;
    if(changed){s.pending.clear();s.apply->setDisabled(true);}
    s.mono->setVisible(value&&s.filterVisible);
    s.apply->setVisible(value&&s.confirmation);s.cancel->setVisible(value&&s.confirmation);
    s.search->setVisible(value);s.rows->setVisible(value&&!s.config.loading&&s.config.error.empty());
    s.status->setVisible(value&&(s.config.loading||!s.config.error.empty()||s.rows->filtered.empty()));
    s.retry->setVisible(value&&!s.config.loading&&(s.confirmation||!s.config.error.empty()));
    if(changed){if(value)requestFocus(s.search.get());else requestFocus(s.selected.get());}
    layoutChildren();invalidate();
}
void FontPicker::setQuery(std::wstring query){state_->search->setText(std::move(query));filter();}
const std::wstring& FontPicker::query() const{return state_->search->text();}
size_t FontPicker::filteredCount() const{return state_->rows->filtered.size();}
size_t FontPicker::paintedRows() const{return state_->rows->painted;}
void FontPicker::filter() {
    auto& s=*state_;auto lower=[](std::wstring t){for(auto& c:t)c=wchar_t(std::towlower(c));return t;};
    s.pending.clear();s.apply->setDisabled(true);
    auto query=lower(s.search->text());auto& rows=*s.rows;rows.filtered.clear();
    for(size_t i=0;i<rows.items.size();++i)
        if((!s.filterVisible||!s.mono->checked()||s.monospaceKeys.count(rows.items[i].id))&&
           (query.empty()||lower(rows.items[i].label+L" "+rows.items[i].family).find(query)!=std::wstring::npos))rows.filtered.push_back(i);
    rows.highlight=rows.filtered.empty()?-1:0;rows.offset=0;rows.refresh();
    s.status->setText(s.config.loading?L"Loading fonts…":!s.config.error.empty()?s.config.error:rows.filtered.empty()?L"No matching fonts":L"");
    s.status->setVisible(s.open&&(s.config.loading||!s.config.error.empty()||rows.filtered.empty()));invalidate();
}
void FontPicker::select(int row) {
    auto& s=*state_;if(row<0||size_t(row)>=s.rows->filtered.size()||s.config.loading)return;
    const auto& item=s.rows->items[s.rows->filtered[row]];if(!item.available)return;
    auto key=item.id;
    if(s.confirmation){s.pending=key;s.apply->setDisabled(false);return;}
    setOpen(false);if(s.selectionRequested){auto callback=s.selectionRequested;callback(std::move(key));}
}
void FontPicker::layoutChildren() {
    auto& s=*state_;auto r=frame();s.selected->setFrame(r);
    const auto popup=popupRect();r=popup;r.height=36;s.search->setFrame(r);
    if(s.filterVisible){r.y+=40;r.height=28;s.mono->setFrame(r);}
    r.y+=40;r.height=std::max(48.f,popup.height-40-(s.filterVisible?40:0)-(s.confirmation?36:0));
    s.rows->setFrame(r);s.rows->refresh();auto rowHeight=r.height;
    r.height=40;s.status->setFrame(r);
    r.y+=s.confirmation?rowHeight+4:44;
    s.retry->setFrame({r.x,r.y,96,32});
    s.cancel->setFrame({r.x+r.width-196,r.y,92,32});s.apply->setFrame({r.x+r.width-96,r.y,96,32});
}
Rect FontPicker::paintBounds() const {
    auto r=frame();if(!open())return r;auto p=popupRect();
    auto x=std::min(r.x,p.x),y=std::min(r.y,p.y);
    return {x,y,std::max(r.x+r.width,p.x+p.width)-x,std::max(r.y+r.height,p.y+p.height)-y};
}
Rect FontPicker::popupRect() const {
    const auto& s=*state_;auto anchor=frame();
    auto height=280.f+(s.filterVisible?40:0)+(s.confirmation?36:0);
    Rect r{anchor.x+anchor.width-std::max(anchor.width,300.f),
           anchor.y>=height+4?anchor.y-height-4:anchor.y+anchor.height+4,
           std::max(anchor.width,300.f),height};
    if(s.viewport) {
        auto v=s.viewport();
        if(v.width>0&&v.height>0){
            r.width=std::min(r.width,v.width);r.height=std::min(r.height,v.height);
            r.x=std::clamp(r.x,v.x,v.x+v.width-r.width);
            r.y=std::clamp(r.y,v.y,v.y+v.height-r.height);
        }
    }
    return r;
}
bool FontPicker::paintsAboveSiblings() const{return open()||View::paintsAboveSiblings();}
bool FontPicker::hitTest(Point p) const{return visible()&&paintBounds().contains(p);}
void FontPicker::paint(Canvas& c){
    layoutChildren();
    if(open()) {
        auto sheet=styleScope();auto colors=sheet?pickerColors(*sheet):StyleBox{};
        c.fillRect(popupRect(),colors.background.color.value_or(Color{255,255,255}),6);
    }
    View::paint(c);
}
bool FontPicker::onKeyDown(const KeyEvent& e) {
    if(e.pressed&&open()&&!state_->search->hasTextComposition()) {
        if(e.key==Key::Escape){setOpen(false);return true;}
        if(e.key==Key::Up||e.key==Key::Down||e.key==Key::Enter)return state_->rows->onKeyDown(e);
    }
    return View::onKeyDown(e);
}
bool FontPicker::onFocusChanged(bool value){if(!value&&open())setOpen(false);return View::onFocusChanged(value);}
AccessibilityInfo FontPicker::accessibilityInfo() const {auto info=View::accessibilityInfo();info.value=state_->selected->accessibilityInfo().value;return info;}
} // namespace oneui
