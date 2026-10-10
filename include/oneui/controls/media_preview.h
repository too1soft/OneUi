#pragma once
#include "oneui/controls/image_view.h"
#include "oneui/view.h"
#include <atomic>
namespace oneui {
class MediaSlotView final : public View {
public:
    std::string name;
protected:
    void layoutChildren() override{if(!children().empty())children()[0]->setFrame(frame());}
};
struct MediaPixels {
    int width=0,height=0;
    std::vector<uint8_t> rgba;
    std::string code,message;
};
class ONEUI_API MediaPreview final : public View {
public:
    using Completion=std::function<void(std::shared_ptr<const MediaPixels>)>;
    using Loader=std::function<void(std::string,std::shared_ptr<std::atomic<bool>>,Completion)>;
    MediaPreview();
    ~MediaPreview() override;
    void setSource(std::string);
    const std::string& source() const;
    bool loading() const;
    const std::string& errorCode() const;
    uint64_t generation() const;
    std::shared_ptr<ImageView> image() const;
    // Optional resource service; default only reads bounded local static files.
    // Completion may arrive from any thread and after cancellation/destruction.
    void setLoader(Loader);
    void setDispatcher(std::function<bool(std::function<void()>)>);
    void setFit(const std::string&);
    void setPosition(const std::string&);
    void setOpacity(float);
    void setOnLoadFailed(std::function<void(std::string,std::string,std::string)>);
    void setSlot(const std::string&,std::shared_ptr<Widget>);
    Size naturalSize() const override {return {240,160};}
    bool tickAnimations(double) override;
    void paint(Canvas&) override;
protected:
    void layoutChildren() override;
private:
    struct State;
    std::shared_ptr<State> state_;
    void complete();
};
} // namespace oneui
