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
    NativeHostView() = default;
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
    void attach(std::shared_ptr<Widget> content, std::shared_ptr<void> owner) {
        if (!content || !children().empty() || content->hasCompositionOwner())
            throw std::invalid_argument("NativeHost requires empty host and detached content");
        add(std::move(content)); owner_ = std::move(owner);
    }
    void detach() { clearChildren(); owner_.reset(); }
    Size naturalSize() const override { return children().empty() ? Size{} : children().front()->naturalSize(); }
    Size measure(Size available) const override { return children().empty() ? Size{} : children().front()->measure(available); }
protected:
    void layoutChildren() override { if (!children().empty()) children().front()->setFrame(frame()); }
};
} // namespace oneui::ui
