#include "oneui/controls/media_preview.h"
#include "internal/media_loader.h"
#include "picker_styles.h"
#include <mutex>
#include <map>
#include <cmath>
#include <stdexcept>
namespace oneui {
struct MediaPreview::State {
    std::string source,code,message;
    uint64_t generation=0;
    bool loading=false,dispatcher=false;
    float opacity=1;
    std::shared_ptr<ImageView> image=std::make_shared<ImageView>();
    Loader loader=internal::loadLocalMedia;
    std::function<bool(std::function<void()>)> dispatch;
    std::function<void(std::string,std::string,std::string)> failed;
    std::map<std::string,std::shared_ptr<Widget>> slots;
    std::shared_ptr<std::atomic<bool>> cancelled;
    std::mutex mutex;
    std::shared_ptr<const MediaPixels> result;
};
MediaPreview::MediaPreview():state_(std::make_shared<State>()){add(state_->image);setAccessibleName(L"Media preview");}
MediaPreview::~MediaPreview(){if(state_->cancelled)*state_->cancelled=true;}
const std::string& MediaPreview::source() const{return state_->source;}
bool MediaPreview::loading() const{return state_->loading;}
const std::string& MediaPreview::errorCode() const{return state_->code;}
uint64_t MediaPreview::generation() const{return state_->generation;}
std::shared_ptr<ImageView> MediaPreview::image() const{return state_->image;}
void MediaPreview::setDispatcher(std::function<bool(std::function<void()>)> dispatch){state_->dispatch=std::move(dispatch);}
void MediaPreview::setLoader(Loader loader){if(!loader)throw std::invalid_argument("Media loader missing");state_->loader=std::move(loader);}
void MediaPreview::setSource(std::string value) {
    auto& s=*state_;if(value==s.source)return;
    if(s.cancelled)*s.cancelled=true;
    s.cancelled=std::make_shared<std::atomic<bool>>(false);++s.generation;s.source=std::move(value);s.code.clear();s.message.clear();
    {std::lock_guard<std::mutex> lock(s.mutex);s.result.reset();}
    s.image->clearImage();s.loading=!s.source.empty();layoutChildren();invalidate();
    if(!s.loading)return;
    auto weak=std::weak_ptr<State>(state_);auto token=s.cancelled;auto alive=lifetimeToken();
    auto dispatch=s.dispatch;s.dispatcher=bool(dispatch);
    s.loader(s.source,token,[this,alive,weak,token,dispatch](std::shared_ptr<const MediaPixels> pixels) {
        if(*token)return;auto state=weak.lock();if(!state)return;
        {std::lock_guard<std::mutex> lock(state->mutex);if(*token)return;state->result=std::move(pixels);}
        if(dispatch)dispatch([this,alive,token]{if(!alive.expired()&&!*token)complete();});
    });
    if(!dispatch)requestAnimationFrame();
}
void MediaPreview::complete() {
    auto& s=*state_;std::shared_ptr<const MediaPixels> result;
    {std::lock_guard<std::mutex> lock(s.mutex);result=std::move(s.result);}
    if(!result)return;s.loading=false;s.code=result->code;s.message=result->message;
    if(s.code.empty()&&!s.image->setRgbaPixels(result->rgba.data(),result->rgba.size(),result->width,result->height,result->width*4)) {
        s.code="decode";s.message="Invalid image pixels";
    }
    setAccessibleValue(s.code.empty()?L"Image loaded":L"Preview failed");
    layoutChildren();invalidate();
    if(!s.code.empty()&&s.failed){auto callback=s.failed;callback(s.source,s.code,s.message);}
}
void MediaPreview::setFit(const std::string& value) {
    if(value!="contain"&&value!="cover"&&value!="stretch")throw std::invalid_argument("Invalid media fit");
    state_->image->setContentMode(value=="contain"?ImageContentMode::Contain:value=="cover"?ImageContentMode::Cover:ImageContentMode::Stretch);
}
void MediaPreview::setPosition(const std::string& value) {
    static const std::map<std::string,std::pair<ImageContentAlignment,ImageContentAlignment>> positions={
        {"top-left",{ImageContentAlignment::Start,ImageContentAlignment::Start}}, {"top",{ImageContentAlignment::Center,ImageContentAlignment::Start}},
        {"top-right",{ImageContentAlignment::End,ImageContentAlignment::Start}}, {"left",{ImageContentAlignment::Start,ImageContentAlignment::Center}},
        {"center",{ImageContentAlignment::Center,ImageContentAlignment::Center}}, {"right",{ImageContentAlignment::End,ImageContentAlignment::Center}},
        {"bottom-left",{ImageContentAlignment::Start,ImageContentAlignment::End}}, {"bottom",{ImageContentAlignment::Center,ImageContentAlignment::End}},
        {"bottom-right",{ImageContentAlignment::End,ImageContentAlignment::End}}};
    auto found=positions.find(value);if(found==positions.end())throw std::invalid_argument("Invalid media position");
    state_->image->setContentAlignment(found->second.first,found->second.second);
}
void MediaPreview::setOpacity(float value){if(!std::isfinite(value)||value<0||value>1)throw std::invalid_argument("Invalid media opacity");state_->opacity=value;invalidate();}
void MediaPreview::setOnLoadFailed(std::function<void(std::string,std::string,std::string)> callback){state_->failed=std::move(callback);}
void MediaPreview::setSlot(const std::string& name,std::shared_ptr<Widget> child) {
    if(name!="empty"&&name!="error")throw std::invalid_argument("Invalid media preview slot");
    state_->slots[name]=std::move(child);std::vector<std::shared_ptr<Widget>> children{state_->image};
    for(auto& pair:state_->slots)children.push_back(pair.second);reconcileChildren(std::move(children));layoutChildren();
}
void MediaPreview::layoutChildren() {
    auto& s=*state_;s.image->setFrame(frame());
    for(auto& [key,widget]:s.slots){widget->setLayoutVisible(key=="empty"?s.source.empty():!s.code.empty());widget->setFrame(frame());}
}
bool MediaPreview::tickAnimations(double now){complete();bool active=View::tickAnimations(now)||(state_->loading&&!state_->dispatcher);if(active)requestAnimationFrame();return active;}
void MediaPreview::paint(Canvas& c) {
    layoutChildren();auto& s=*state_;
    if(s.loading&&!s.dispatcher)requestAnimationFrame();
    bool slotVisible=false;for(auto& [_,slot]:s.slots)slotVisible|=slot->visible();
    if(!s.image->hasImage()&&!slotVisible) {
        auto box=styleScope()?pickerColors(*styleScope()):StyleBox{};
        c.fillRect(frame(),box.background.color.value_or(Color{245,247,244}),6);
        c.drawText(s.loading?L"Loading image?":s.source.empty()?L"No image":L"Preview unavailable",frame(),box.foreground.value_or(Color{84,103,91}),13);
    }
    c.saveOpacity(frame(),s.opacity);View::paint(c);c.restore();
}
} // namespace oneui
