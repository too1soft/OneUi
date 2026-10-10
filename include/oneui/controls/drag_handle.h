#pragma once
#include "oneui/view.h"
namespace oneui {
// A capture-owning gesture surface. Domain coordination is supplied by views;
// this control never owns or mutates a workspace model.
class ONEUI_API DragHandle final : public View {
public:
    std::string source,domain,kind="pane";
    float threshold=4;
    bool isFocusable() const override {return interactive();}
    void setOnDrag(std::function<void(int,Point)> callback){callback_=std::move(callback);}
    Size naturalSize() const override {return children().empty()?Size{36,26}:children()[0]->naturalSize();}
    bool onMouseDown(const MouseEvent&) override;
    bool onMouseMove(const MouseEvent&) override;
    bool onMouseUp(const MouseEvent&) override;
    bool onKeyDown(const KeyEvent&) override;
protected:
    void layoutChildren() override {if(!children().empty())children()[0]->setFrame(frame());}
    bool hasInteractionState() const override {return armed_||View::hasInteractionState();}
    void resetInteractionState() override;
private:
    bool armed_=false,dragging_=false;
    Point start_,point_;
    std::function<void(int,Point)> callback_;
};
class ONEUI_API DropSurface final : public View {
public:
    std::string domain,role="tab-strip";
    Size naturalSize() const override {return children().empty()?Size{}:children()[0]->naturalSize();}
protected:
    void layoutChildren() override {if(!children().empty())children()[0]->setFrame(frame());}
};
} // namespace oneui
