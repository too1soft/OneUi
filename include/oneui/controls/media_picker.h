#pragma once
#include "oneui/controls/media_preview.h"
#include "oneui/controls/button.h"
#include "oneui/controls/label.h"
#include <map>
namespace oneui {
struct MediaPickerConfig {std::string asset;std::wstring label,error;bool loading=false;};
enum class MediaPickerAction {Choose,Clear,Cancel};
class ONEUI_API MediaPicker final : public View {
public:
    MediaPicker();
    void setConfig(MediaPickerConfig);
    void setChooseLabel(std::wstring);
    void setClearLabel(std::wstring);
    void setCancelLabel(std::wstring);
    const MediaPickerConfig& config() const{return config_;}
    void setOnIntent(std::function<void(MediaPickerAction)> callback){callback_=std::move(callback);}
    void setSlot(std::string,std::shared_ptr<Widget>);
    std::shared_ptr<MediaPreview> preview() const{return preview_;}
    Size naturalSize() const override{return {300,240};}
protected:
    void layoutChildren() override;
    void styleScopeChanged() override;
private:
    MediaPickerConfig config_;
    std::shared_ptr<MediaPreview> preview_;
    std::shared_ptr<Button> choose_,clear_,cancel_;
    std::shared_ptr<Label> label_;
    std::map<std::string,std::shared_ptr<Widget>> slots_;
    std::function<void(MediaPickerAction)> callback_;
};
} // namespace oneui
