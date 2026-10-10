#pragma once
#include "oneui/view.h"
namespace oneui {
class ONEUI_API ContainerPage final : public View {
public:
    ContainerPage(){setLayoutVisible(false);}
    void setSelected(bool value){selected_=value;setLayoutVisible(display_.value_or(selected_));}
    bool selected() const{return selected_;}
    void setDisplay(std::optional<bool> value){display_=value;setLayoutVisible(display_.value_or(selected_));}
    Size naturalSize() const override {return children().empty()?Size{}:children()[0]->naturalSize();}
protected:
    void layoutChildren() override {for(auto& child:children())if(child->visible())child->setFrame(frame());}
private:
    bool selected_=false;
    std::optional<bool> display_;
};
} // namespace oneui
