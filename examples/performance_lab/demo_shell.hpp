#pragma once
#include "demo_page.hpp"
#include <array>
#include <map>

namespace lab {
class DemoShell final : public oneui::OverlayHost {
  public:
    explicit DemoShell(oneui::Window &window);
    ~DemoShell() override;
    bool navigate(const std::string &id);
    void applicationActivated(const oneui::ApplicationActivation &);
    bool key(const oneui::KeyEvent &event);
    const std::string &pageId() const { return current_; }
    void setTheme(bool dark, bool compact);
    void paint(oneui::Canvas &) override;

  protected:
    void layoutChildren() override;

  private:
    oneui::Window &window_;
    std::shared_ptr<oneui::Stack> row_, main_, navigation_, toolbar_, codeToolbar_;
    std::shared_ptr<oneui::Panel> pageHost_;
    std::shared_ptr<oneui::ScrollView> navScroll_;
    std::shared_ptr<oneui::Label> heading_, description_;
    std::shared_ptr<oneui::TextField> code_;
    std::shared_ptr<oneui::Button> drawerButton_, codeButton_, themeButton_, densityButton_;
    std::vector<std::pair<std::string, std::shared_ptr<oneui::Button>>> links_;
    std::map<std::string, std::unique_ptr<DemoPage>> pages_;
    std::array<bool, 3> expanded_{{true, false, false}};
    std::string current_;
    bool dark_ = true, compact_ = false, narrow_ = false, drawer_ = false, codeOpen_ = false;
    void buildNavigation();
    void updateNavigation();
    void updateDrawer();
    void updateCode();
    void installMenus();
};
} // namespace lab
