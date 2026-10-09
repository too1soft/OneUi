#pragma once
#include "demo_page.hpp"
#include <oneui/controls/dialog.h>
#include <oneui/controls/popup.h>

namespace lab {
// Local popup state and focus examples; no system notifications or native windows.
class PopupShowcase final : public oneui::OverlayHost {
  public:
    PopupShowcase();
    ~PopupShowcase() override;
    void setTheme(std::shared_ptr<oneui::StyleSheet> sheet, bool dark);
    void setActive(bool active);
    void paint(oneui::Canvas &canvas) override;

  private:
    void showChanged(bool open);
    void showConfirmation();
    void updateGeometry();
    void closeConfirmation();
    oneui::State<bool> open_{false};
    oneui::Subscription openSubscription_;
    std::shared_ptr<oneui::Stack> body_, panel_;
    std::shared_ptr<oneui::Popup> popup_;
    std::shared_ptr<oneui::Dialog> confirmation_;
    std::shared_ptr<oneui::TextField> editor_;
    std::shared_ptr<oneui::Button> trigger_;
    std::shared_ptr<oneui::Label> feedback_;
    std::shared_ptr<oneui::StyleSheet> styles_;
    oneui::PopupInteractionMode mode_ = oneui::PopupInteractionMode::LightDismiss;
    oneui::PopupPreferredPlacement placement_ = oneui::PopupPreferredPlacement::BottomStart;
    oneui::Rect anchor_{};
    bool active_ = false;
    int backgroundClicks_ = 0;
};
} // namespace lab
