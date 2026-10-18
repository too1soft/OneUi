#pragma once
#include "oneui/layout/overlay_host.h"
#include <stdexcept>
#include <string>

namespace oneui {
// Two-child non-modal composition: a filling base and one anchored foreground.
// Input routing, clipping, focus and theme propagation remain OverlayHost's.
class TemplateOverlay final : public OverlayHost {
public:
    TemplateOverlay(std::shared_ptr<Widget> base, std::shared_ptr<Widget> foreground)
        : foreground_(std::move(foreground)) {
        setContent(std::move(base));
        addAnchoredOverlay(foreground_, OverlayOptions::modeless(1), {-1,-1}, {}, 0, 0);
    }
    void setNumber(const std::string& key, float value) {
        if (key == "overlay-width") size_.width = value > 0 ? value : -1;
        else if (key == "overlay-height") size_.height = value > 0 ? value : -1;
        else if (key == "inset-left") margin_.left = value;
        else if (key == "inset-right") margin_.right = value;
        else if (key == "inset-top") margin_.top = value;
        else if (key == "inset-bottom") margin_.bottom = value;
        else throw std::invalid_argument("Unknown Overlay dimension");
        apply();
    }
    void setAlignment(const std::string& key, const std::wstring& value) {
        if (value != L"start" && value != L"center" && value != L"end")
            throw std::invalid_argument("Overlay alignment must be start, center or end");
        const int align = value == L"start" ? 0 : value == L"center" ? 1 : 2;
        if (key == "horizontal-align") horizontal_ = align;
        else vertical_ = align;
        apply();
    }
    void setOverlayVisible(bool visible) { foreground_->setVisible(visible); }
private:
    void apply() { updateAnchoredOverlay(foreground_.get(), size_, margin_, horizontal_, vertical_); }
    std::shared_ptr<Widget> foreground_;
    Size size_{-1,-1};
    Insets margin_{};
    int horizontal_ = 0, vertical_ = 0;
};
}
