#pragma once
#include "oneui/view.h"
#include <stdexcept>

namespace oneui::ui {
// An undecorated native container. Layout, focus, IME and accessibility continue
// through the original widget tree. The optional owner retains foreign callbacks.
class NativeHostView final : public View {
    std::shared_ptr<void> owner_;
    bool themed_=false;
public:
    NativeHostView(std::shared_ptr<Widget> content, std::shared_ptr<void> owner = {})
        : owner_(std::move(owner)) {
        if (!content) throw std::invalid_argument("NativeHost requires nonnull content");
        if (content->hasCompositionOwner()) throw std::invalid_argument("NativeHost content is already mounted; detach it before mounting again");
        add(std::move(content));
    }
    ~NativeHostView() override {
        Widget::setInvalidator({}); Widget::setRectInvalidator({}); Widget::setAnimationScheduler({});
        clearChildren();
    } // detach before releasing foreign callbacks
    void setThemed(bool value){themed_=value;}
    bool themed() const{return themed_;}
    Size naturalSize() const override { return children().front()->naturalSize(); }
    Size measure(Size available) const override { return children().front()->measure(available); }
protected:
    void layoutChildren() override { children().front()->setFrame(frame()); }
};
} // namespace oneui::ui
