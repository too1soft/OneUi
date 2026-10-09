#include "../connection_workspace.hpp"
#include "page_factories.hpp"
#include <oneui/ui_native_theme.h>

namespace lab {
using namespace oneui;
class RecipePage final : public DemoPage {
    connection_demo::Workspace workspace_;

  public:
    RecipePage(Window &window, int scene) : workspace_(window, [] {}, false) {
        workspace_.flow.gallery.set(true);
        workspace_.mount.flush();
        workspace_.samples->scene.set(scene);
        workspace_.mount.flush();
        if (auto back = workspace_.mount.find("galleryBack"))
            back->setVisible(false);
        workspace_.suspend();
    }
    void activate() override { workspace_.resume(); }
    void deactivate() override { workspace_.suspend(); }
    std::shared_ptr<Widget> widget() const override { return workspace_.page.root.widget; }
    void setTheme(bool dark, bool compact) override {
        workspace_.vm.theme.set(dark ? 1 : 0);
        workspace_.vm.density.set(compact ? 1 : 0);
        workspace_.mount.flush();
    }
};
class ComposedPagesPage final : public DemoPage {
    Window &window_;
    std::shared_ptr<Stack> root_ = std::make_shared<Stack>();
    std::shared_ptr<Tabs> chooser_ = std::make_shared<Tabs>();
    std::shared_ptr<Panel> host_ = std::make_shared<Panel>();
    std::unique_ptr<DemoPage> pages_[2];
    std::shared_ptr<int> lifetime_ = std::make_shared<int>();
    int selected_ = 0;
    bool active_ = false, dark_ = true, compact_ = false;
    void select(int index) {
        if (index < 0 || index > 1)
            return;
        if (pages_[selected_] && active_)
            pages_[selected_]->deactivate();
        selected_ = index;
        if (!pages_[index]) {
            pages_[index] =
                index == 0 ? createProductPage() : std::make_unique<RecipePage>(window_, 4);
            pages_[index]->setTheme(dark_, compact_);
        }
        host_->setContent(pages_[index]->widget());
        if (active_)
            pages_[index]->activate();
    }

  public:
    explicit ComposedPagesPage(Window &window) : window_(window) {
        root_->setEngine(StackEngine::Yoga);
        chooser_->setItems({L"应用外壳", L"列表与详情"});
        chooser_->setOnChanged([this, weak = std::weak_ptr<int>(lifetime_)](int index) {
            if (!weak.expired())
                select(index);
        });
        root_->add(chooser_);
        root_->setFlex(chooser_, {0, 0, 40, 40});
        root_->add(host_);
        root_->setFlex(host_, {1, 1, 0});
        select(0);
    }
    ~ComposedPagesPage() override {
        deactivate();
        lifetime_.reset();
    }
    std::shared_ptr<Widget> widget() const override { return root_; }
    void activate() override {
        active_ = true;
        pages_[selected_]->activate();
    }
    void deactivate() override {
        active_ = false;
        pages_[selected_]->deactivate();
    }
    void setTheme(bool dark, bool compact) override {
        dark_ = dark;
        compact_ = compact;
        StyleSheet sheet;
        sheet.addRulesFromCss(
            ui::declarativeTheme(dark, compact ? ui::Density::Compact : ui::Density::Comfortable));
        ui::applyNativeTheme(*chooser_, sheet, false);
        host_->setBorderWidth(0);
        host_->setBackground(dark ? Color{23, 30, 33} : Color{255, 255, 255});
        for (auto &page : pages_)
            if (page)
                page->setTheme(dark, compact);
    }
    bool key(const KeyEvent &event) override { return pages_[selected_]->key(event); }
};
std::unique_ptr<DemoPage> createLayoutPage(const PageDescriptor &page, Window &window) {
    if (page.kind == PageKind::Layout)
        return createBasicLayoutPage();
    if (std::string(page.id) == "layout/pages")
        return std::make_unique<ComposedPagesPage>(window);
    return std::make_unique<RecipePage>(window, page.variant);
}
} // namespace lab
