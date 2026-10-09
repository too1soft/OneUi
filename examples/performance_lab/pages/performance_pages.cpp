#include "../performance_page.hpp"
#include "page_factories.hpp"

namespace lab {
class PerformancePage final : public DemoPage {
    std::shared_ptr<Lab> lab_;

  public:
    PerformancePage(oneui::Window &window, int variant) {
        Options options;
        options.tab = variant;
        options.embedded = true;
        lab_ = std::make_shared<Lab>(window, options);
    }
    std::shared_ptr<oneui::Widget> widget() const override { return lab_; }
    void activate() override { lab_->setPageActive(true); }
    void deactivate() override { lab_->setPageActive(false); }
    bool key(const oneui::KeyEvent &event) override { return lab_->key(event); }

};
std::unique_ptr<DemoPage> createPerformancePage(const PageDescriptor &page, oneui::Window &window) {
    if (page.variant >= 4) return createPerformanceValidationPage(page.variant == 5);
    return std::make_unique<PerformancePage>(window, page.variant);
}
} // namespace lab
