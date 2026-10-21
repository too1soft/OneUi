#pragma once
#include "demo_page.hpp"
#include "oneui/mobile/appearance.h"
#include <map>

namespace lab::mobile {
// Shared mobile shell; native hosts only supply lifecycle, input and surfaces.
// Commands drain after WidgetHost::pumpInput, never while a widget callback runs.
class Workbench final {
  public:
    Workbench();
    ~Workbench();
    std::shared_ptr<oneui::Widget> widget() const;
    void viewport(float width);
    void setActive(bool active);
    bool back();
    bool flush();
    bool hasPending() const { return !pending_.empty(); }
    DemoPage *page() const;
    std::string diagnostics() const;
    void describeControls(std::ostream &out) const;
    bool dark() const { return dark_; }
    oneui::mobile::Appearance &appearance() { return appearance_; }
    void syncAppearance();
    void typography(float scale, float bodySize);

  private:
    oneui::mobile::Appearance appearance_;
    float fontScale_ = 1, bodySize_ = 16;
    uint64_t appearanceRevision_ = 0, appliedRevision_ = 0;
    std::shared_ptr<oneui::View> root_;
    std::shared_ptr<oneui::Panel> content_;
    std::shared_ptr<oneui::ScrollView> navigation_, code_;
    std::shared_ptr<oneui::Stack> entries_;
    std::shared_ptr<oneui::Label> title_, codeText_;
    std::shared_ptr<oneui::Button> menu_, theme_, source_;
    std::map<std::string, std::unique_ptr<DemoPage>> pages_;
    std::map<std::string, std::shared_ptr<oneui::Button>> links_;
    std::map<Category, std::shared_ptr<oneui::Button>> groups_;
    std::string current_, pending_;
    Category expanded_ = Category::Layout;
    bool dark_ = false, wide_ = false, drawer_ = false, codeOpen_ = false;
    bool active_ = false, pageActive_ = false;
    void navigate(const std::string &id);
    void applyTheme();
    void updateVisibility();
    void updateActivation();
    void arrange(oneui::Rect frame);
    void loadSource();
};
} // namespace lab::mobile
