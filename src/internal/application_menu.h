#pragma once
#include "oneui/layout/overlay_host.h"
#include "oneui/native_menu.h"

namespace oneui::internal {
// Retained wrapper: menu updates preserve the application's content/focus tree.
class ONEUI_API ApplicationMenuHost : public OverlayHost {
  public:
    ApplicationMenuHost();
    ~ApplicationMenuHost() override;
    void setApplicationContent(std::shared_ptr<Widget> content);
    void setItems(std::vector<NativeMenuItem> items);
    void setAction(std::function<void(int)> action);
    bool onKeyDown(const KeyEvent &) override;
    bool onFocusChanged(bool) override;
    void paint(Canvas &) override;

  private:
    struct State;
    std::unique_ptr<State> state_;
};
} // namespace oneui::internal
