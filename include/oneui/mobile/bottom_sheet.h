#pragma once
#include <oneui/controls/reveal.h>
#include <oneui/layout/overlay_host.h>
#include <oneui/layout/scroll_view.h>
#include <oneui/mobile/presentation.h>

namespace oneui::mobile {
// A bounded native sheet. The owner supplies content and dismissal policy;
// layout, focus trapping, scrolling, close affordance and motion are shared.
class BottomSheet final : public OverlayHost {
  public:
    static float contentWidth(float viewportWidth) {
        return std::max(1.f, std::min(480.f, viewportWidth - 32.f) - 48.f);
    }

    BottomSheet(std::shared_ptr<Widget> background, std::wstring title,
                std::shared_ptr<Stack> body, ControlPalette palette, float scale,
                std::function<void()> close, bool enter = true, std::shared_ptr<Widget> footer = {},bool modal=true)
        : body_(std::move(body)), footer_(std::move(footer)), scale_(scale), pendingEntrance_(enter) {
        setContent(std::move(background));
        auto header = std::make_shared<Stack>(StackDirection::Row);header_=header;
        header->setAlign(StackAlign::Center);
        header->setPreferredSize({0, std::max(48.f, 48 * scale)});
        auto heading = pageText(title, 16 * scale, palette.ink, 650);
        header->add(heading);
        StackFlex fill;
        fill.grow = 1;
        fill.basis = 0;
        header->setFlex(heading, fill);
        auto dismiss = actionButton(L"", std::move(close), palette, ActionKind::Text, scale);
        dismiss->setIcon(IconSymbol::Close);
        dismiss->setAccessibleName(L"关闭" + title);
        dismiss->setFocusOnPointer(false);
        dismiss->setPreferredSize({std::max(48.f, 48 * scale), std::max(48.f, 48 * scale)});
        header->add(dismiss);

        scroll_ = std::make_shared<ScrollView>();
        scroll_->setContent(body_);
        scroll_->setContentHeight(body_->contentHeight());
        scroll_->setChromeVisible(false);
        StyleBox scrollStyle;
        scrollStyle.background.color = palette.surface;
        scrollStyle.borderWidth = 0;
        scroll_->setStyleBox(scrollStyle);
        auto column = std::make_shared<Stack>();column_=column;
        column->setGap(14);
        column->setPadding({24, 24, 24, 24});
        column->add(header);
        column->add(scroll_);
        if (footer_) column->add(footer_);
        pane_ = std::make_shared<Panel>();
        pane_->setAccessibleName(title);
        pane_->setBackground(palette.surface);
        pane_->setBorderWidth(0);
        pane_->setRadius(20);
        pane_->setContent(column);
        reveal_ = std::make_shared<Reveal>(pane_);
        // OverlayHost may clamp the requested size to its actual viewport.
        // Keep the retained surface and body within that allocated rectangle.
        reveal_->setOnSizeChanged([this](Size size) { fitSurface(size); });
        reveal_->setPreset(RevealPreset::Fade);
        reveal_->setTransition({180, EasingCurve::EaseOutCubic});
        if (enter) reveal_->setOpen(false);
        addAnchoredOverlay(reveal_, modal?OverlayOptions::modal():OverlayOptions::modeless(), {}, {}, 1, 2);
        setOnSizeChanged([this](Size size) { resize(size); });
    }

    void setAnimationScheduler(std::function<void()> scheduler) override {
        OverlayHost::setAnimationScheduler(std::move(scheduler));
        if (pendingEntrance_) {
            pendingEntrance_ = false;
            reveal_->setOpen(true);
        }
    }

    void resize(Size viewport) {
        compact_=compactThreshold_>0&&viewport.height<compactThreshold_;
        header_->setVisible(!compact_);
        column_->setGap(compact_?8:14);column_->setPadding(compact_?Insets{8,16,8,16}:Insets{24,24,24,24});
        scroll_->setContentHeight(body_->contentHeight());
        const float width = std::max(1.f, std::min(480.f, viewport.width - 32.f));
        const float chrome = chromeHeight();
        const float height = std::max(0.f, std::min(viewport.height * (compact_?1.f:.88f),
            body_->contentHeight() + chrome));
        fitSurface({width, height});
        updateAnchoredOverlay(reveal_.get(), {width, height}, {}, 1, 2);
    }
    Rect surfaceRect() const { return pane_->frame(); }
    void setCompactWhenHeightBelow(float threshold){compactThreshold_=std::max(0.f,threshold);}

  private:
    float chromeHeight() const {
        if(compact_)return 16+(footer_?footer_->preferredSize().height+8:0);
        return 48 + std::max(48.f, 48 * scale_) + 14 +
            (footer_ ? footer_->preferredSize().height + 14 : 0);
    }
    void fitSurface(Size size) {
        scroll_->setContentWidth(std::max(1.f, size.width - (compact_?32:48)));
        scroll_->setPreferredSize({0, std::max(0.f, size.height - chromeHeight())});
        pane_->setPreferredSize(size);
    }
    std::shared_ptr<Stack> body_;
    std::shared_ptr<Stack> header_,column_;
    std::shared_ptr<Widget> footer_;
    std::shared_ptr<ScrollView> scroll_;
    std::shared_ptr<Panel> pane_;
    std::shared_ptr<Reveal> reveal_;
    float scale_;
    bool pendingEntrance_;
    bool compact_=false;
    float compactThreshold_=0;
};
} // namespace oneui::mobile
