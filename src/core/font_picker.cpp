#include "font_picker_state.h"
#include "picker_styles.h"
#include "internal/unicode.h"
#include <cwctype>
#include <set>
#include <cmath>
#include <stdexcept>
namespace oneui {
FontPicker::FontPicker():state_(std::make_shared<State>()) {
    auto& s=*state_;add(s.selected);add(s.search);add(s.status);add(s.retry);add(s.rows);
    s.selected->setOnClick([this]{setOpen(!open());});
    s.search->setAccessibleName(L"Search fonts");s.search->setOnChanged([this](const auto&){filter();});
    s.rows->activate=[this](int row){select(row);};s.retry->setOnClick([this]{if(state_->retryRequested)state_->retryRequested();});
    setConfig({});setOpen(false);
}
FontPicker::~FontPicker()=default;
void FontPicker::styleScopeChanged() {
    View::styleScopeChanged();if(!state_)return;
    auto sheet=styleScope();if(!sheet)return;
    applyPickerButton(*state_->selected,*sheet);applyPickerButton(*state_->retry,*sheet);
    state_->search->setStyleOverride(textFieldStyleOverrideFromStyleSheet(*sheet,{"input",{"one-Input"},0}));
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
    s.selected->setText(label);s.selected->setAccessibleValue(label);
    s.rows->items=s.config.items;s.rows->preview=s.config.previewText;s.rows->size=s.config.size;
    filter();setOpen(s.open);
}
void FontPicker::setOnSelectionRequested(std::function<void(std::string)> cb){state_->selectionRequested=std::move(cb);}
void FontPicker::setOnRetryRequested(std::function<void()> cb){state_->retryRequested=std::move(cb);}
bool FontPicker::open() const{return state_->open;}
void FontPicker::setOpen(bool value) {
    auto& s=*state_;bool changed=s.open!=value;s.open=value;
    s.search->setVisible(value);s.rows->setVisible(value&&!s.config.loading&&s.config.error.empty());
    s.status->setVisible(value&&(s.config.loading||!s.config.error.empty()||s.rows->filtered.empty()));
    s.retry->setVisible(value&&!s.config.loading&&!s.config.error.empty());
    if(changed){if(value)requestFocus(s.search.get());else requestFocus(s.selected.get());}
    layoutChildren();invalidate();
}
void FontPicker::setQuery(std::wstring query){state_->search->setText(std::move(query));filter();}
const std::wstring& FontPicker::query() const{return state_->search->text();}
size_t FontPicker::filteredCount() const{return state_->rows->filtered.size();}
size_t FontPicker::paintedRows() const{return state_->rows->painted;}
void FontPicker::filter() {
    auto& s=*state_;auto lower=[](std::wstring t){for(auto& c:t)c=wchar_t(std::towlower(c));return t;};
    auto query=lower(s.search->text());auto& rows=*s.rows;rows.filtered.clear();
    for(size_t i=0;i<rows.items.size();++i)if(query.empty()||lower(rows.items[i].label+L" "+rows.items[i].family).find(query)!=std::wstring::npos)rows.filtered.push_back(i);
    rows.highlight=rows.filtered.empty()?-1:0;rows.offset=0;rows.refresh();
    s.status->setText(s.config.loading?L"Loading fonts…":!s.config.error.empty()?s.config.error:rows.filtered.empty()?L"No matching fonts":L"");
    s.status->setVisible(s.open&&(s.config.loading||!s.config.error.empty()||rows.filtered.empty()));invalidate();
}
void FontPicker::select(int row) {
    auto& s=*state_;if(row<0||size_t(row)>=s.rows->filtered.size()||s.config.loading)return;
    const auto& item=s.rows->items[s.rows->filtered[row]];if(!item.available)return;
    auto key=item.id;setOpen(false);if(s.selectionRequested){auto callback=s.selectionRequested;callback(std::move(key));}
}
void FontPicker::layoutChildren() {
    auto& s=*state_;auto r=frame();s.selected->setFrame(r);
    r.y+=r.height+4;r.height=36;s.search->setFrame(r);
    r.y+=40;r.height=240;s.rows->setFrame(r);s.rows->refresh();
    r.height=40;s.status->setFrame(r);r.y+=44;s.retry->setFrame({r.x,r.y,std::min(100.f,r.width),32});
}
Rect FontPicker::paintBounds() const {auto r=frame();if(open())r.height+=284;return r;}
bool FontPicker::paintsAboveSiblings() const{return open()||View::paintsAboveSiblings();}
bool FontPicker::hitTest(Point p) const{return visible()&&paintBounds().contains(p);}
void FontPicker::paint(Canvas& c){layoutChildren();View::paint(c);}
bool FontPicker::onKeyDown(const KeyEvent& e) {
    if(e.pressed&&open()&&!state_->search->hasTextComposition()) {
        if(e.key==Key::Escape){setOpen(false);return true;}
        if(e.key==Key::Up||e.key==Key::Down||e.key==Key::Enter)return state_->rows->onKeyDown(e);
    }
    return View::onKeyDown(e);
}
bool FontPicker::onFocusChanged(bool value){if(!value&&open())setOpen(false);return View::onFocusChanged(value);}
AccessibilityInfo FontPicker::accessibilityInfo() const {auto info=View::accessibilityInfo();info.value=state_->selected->text();return info;}
} // namespace oneui
