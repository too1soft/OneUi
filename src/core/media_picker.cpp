#include "oneui/controls/media_picker.h"
#include "picker_styles.h"
#include <codecvt>
#include <locale>
#include <stdexcept>
namespace oneui {
MediaPicker::MediaPicker():preview_(std::make_shared<MediaPreview>()),choose_(std::make_shared<Button>(L"Choose image")),
    clear_(std::make_shared<Button>(L"Clear")),cancel_(std::make_shared<Button>(L"Cancel")),label_(std::make_shared<Label>()) {
    add(preview_);add(label_);add(choose_);add(clear_);add(cancel_);
    choose_->setOnClick([this]{if(!config_.loading&&callback_)callback_(MediaPickerAction::Choose);});
    clear_->setOnClick([this]{if(!config_.loading&&!config_.asset.empty()&&callback_)callback_(MediaPickerAction::Clear);});
    cancel_->setOnClick([this]{if(config_.loading&&callback_)callback_(MediaPickerAction::Cancel);});
    setConfig({});
}
void MediaPicker::styleScopeChanged() {
    View::styleScopeChanged();auto sheet=styleScope();if(!sheet)return;
    for(auto button:{choose_,clear_,cancel_})if(button)applyPickerButton(*button,*sheet);
    if(label_)label_->setColor(pickerColors(*sheet).foreground.value_or(Color{28,43,35}));
}
void MediaPicker::setConfig(MediaPickerConfig value) {
    config_=std::move(value);preview_->setSource(config_.asset);
    auto filename=config_.asset.substr(config_.asset.find_last_of("/\\")==std::string::npos?0:config_.asset.find_last_of("/\\")+1);
    auto fallback=config_.asset.empty()?std::wstring(L"No image selected"):std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>{}.from_bytes(filename);
    auto text=config_.loading?L"Loading image…":!config_.error.empty()?config_.error:config_.label.empty()?fallback:config_.label;
    label_->setText(text);preview_->setAccessibleDescription(config_.label);
    auto path=std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>{}.from_bytes(config_.asset);label_->setTooltip(path);
    choose_->setDisabled(config_.loading);clear_->setDisabled(config_.loading||config_.asset.empty());cancel_->setVisible(config_.loading);
    layoutChildren();invalidate();
}
void MediaPicker::setSlot(std::string name,std::shared_ptr<Widget> child) {
    if(name!="empty"&&name!="preview"&&name!="error")throw std::invalid_argument("Invalid media picker slot");
    slots_[std::move(name)]=std::move(child);
    std::vector<std::shared_ptr<Widget>> children{preview_,label_,choose_,clear_,cancel_};
    for(auto& item:slots_)children.push_back(item.second);reconcileChildren(std::move(children));layoutChildren();
}
void MediaPicker::layoutChildren() {
    auto r=frame();Rect preview{r.x,r.y,r.width,std::max(0.f,r.height-70)};
    preview_->setFrame(preview);bool custom=false;
    for(auto& [key,w]:slots_) {
        bool show=key=="error"?!config_.error.empty():key=="empty"?config_.asset.empty()&&config_.error.empty():!config_.asset.empty()&&config_.error.empty();
        w->setLayoutVisible(show);w->setFrame(preview);custom|=show;
    }
    preview_->setLayoutVisible(!custom);label_->setFrame({r.x,r.y+r.height-66,r.width,26});
    float gap=6,width=std::max(0.f,(r.width-gap*2)/3);
    choose_->setFrame({r.x,r.y+r.height-36,width,36});clear_->setFrame({r.x+width+gap,r.y+r.height-36,width,36});cancel_->setFrame({r.x+2*(width+gap),r.y+r.height-36,width,36});
}
} // namespace oneui
