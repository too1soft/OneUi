#include "demo_shell.hpp"
#include "code_examples.hpp"
#include <oneui/style_adapter.h>

namespace lab {
using namespace oneui;
namespace {
std::shared_ptr<Stack> stack(StackDirection direction = StackDirection::Column) {
    auto value = std::make_shared<Stack>(direction);
    value->setEngine(StackEngine::Yoga);
    value->setGap(8);
    return value;
}
std::shared_ptr<Button> button(Stack &parent, const wchar_t *title,
                               std::function<void()> callback) {
    auto value = std::make_shared<Button>(title);
    value->setOnClick(std::move(callback));
    parent.add(value);
    parent.setFlex(value, {0, 0, 40, 40});
    return value;
}
} // namespace
DemoShell::DemoShell(Window &window)
    : window_(window), row_(stack(StackDirection::Row)), main_(stack()), navigation_(stack()),
      toolbar_(stack(StackDirection::Row)), codeToolbar_(stack(StackDirection::Row)),
      pageHost_(std::make_shared<Panel>()), navScroll_(std::make_shared<ScrollView>()),
      heading_(std::make_shared<Label>()), description_(std::make_shared<Label>()),
      code_(std::make_shared<TextField>()) {
    row_->setGap(1);
    main_->setPadding(Insets{24});
    main_->setGap(12);
    navigation_->setPadding(Insets{16});
    navigation_->setGap(4);
    navScroll_->setContent(navigation_);
    row_->add(navScroll_);
    row_->setFlex(navScroll_, {0, 0, 232, 232});
    row_->add(main_);
    row_->setFlex(main_, {1, 1, 0, 0});
    setContent(row_);
    drawerButton_ = button(*toolbar_, L"导航", [this] {
        drawer_ = !drawer_;
        updateDrawer();
    });
    themeButton_ = button(*toolbar_, L"浅色主题", [this] { setTheme(!dark_, compact_); });
    densityButton_ = button(*toolbar_, L"紧凑间距", [this] { setTheme(dark_, !compact_); });
    codeButton_ = button(*toolbar_, L"接入代码", [this] {
        codeOpen_ = !codeOpen_;
        updateCode();
    });
    for (auto &item : toolbar_->children())
        toolbar_->setFlex(item, {0, 0, 110, 72});
    main_->add(toolbar_);
    main_->setFlex(toolbar_, {0, 0, 40, 40});
    heading_->setFontSize(26);
    heading_->setFontWeight(600);
    main_->add(heading_);
    main_->setFlex(heading_, {0, 0, 40, 40});
    description_->setFontSize(13);
    main_->add(description_);
    main_->setFlex(description_, {0, 0, 28, 28});
    pageHost_->setPadding(Insets{0});
    pageHost_->setBorderWidth(0);
    pageHost_->setRadius(6);
    main_->add(pageHost_);
    main_->setFlex(pageHost_, {1, 1, 0, 100});
    main_->add(codeToolbar_);
    main_->setFlex(codeToolbar_, {0, 0, 36, 36});
    code_->setMultiline(true);
    code_->setReadOnly(true);
    code_->setClipboard(std::make_shared<SystemClipboard>());
    code_->setFontSize(13);
    main_->add(code_);
    main_->setFlex(code_, {0, 0, 200, 100});
    code_->setVisible(false);
    codeToolbar_->setVisible(false);
    buildNavigation();
    setTheme(true, false);
    installMenus();
}
DemoShell::~DemoShell() {
    window_.setOnMenuAction({});
    if (auto page = pages_.find(current_); page != pages_.end())
        page->second->deactivate();
    pageHost_->setContent({});
}
void DemoShell::buildNavigation() {
    auto brand = std::make_shared<Label>(L"OneUI 试验台");
    brand->setFontSize(22);
    brand->setFontWeight(600);
    navigation_->add(brand);
    navigation_->setFlex(brand, {0, 0, 60, 60});
    for (int group = 0; group < 3; ++group) {
        button(*navigation_, categoryName(Category(group)), [this, group] {
            expanded_[group] = !expanded_[group];
            updateNavigation();
        });
        for (auto &page : pageCatalog())
            if (page.category == Category(group)) {
                const std::string id = page.id;
                auto link = button(*navigation_, page.title, [this, id] { navigate(id); });
                link->setAccessibleName(page.title);
                links_.push_back({id, link});
            }
    }
    updateNavigation();
}
void DemoShell::updateNavigation() {
    auto sheet = std::make_shared<StyleSheet>();
    sheet->addRulesFromCss(ui::declarativeTheme(dark_));
    for (auto &item : links_) {
        const auto *page = findPage(item.first);
        item.second->setVisible(expanded_[int(page->category)]);
        applyDemoButton(*item.second, *sheet, item.first == current_);
        AccessibilityState state;
        state.selected = item.first == current_;
        item.second->setAccessibilityState(state);
    }
    invalidate();
}
void DemoShell::applicationActivated(const oneui::ApplicationActivation &value) {
    window_.show();
    window_.activate();
    if (!value.notificationId.empty()) {
        navigate("features/notifications");
        pages_.at(current_)->applicationActivated(value);
    }
}
bool DemoShell::navigate(const std::string &id) {
    const auto *descriptor = findPage(id);
    if (!descriptor)
        return false;
    if (current_ == id)
        return true;
    if (!pages_.count(id)) {
        auto page = createDemoPage(*descriptor, window_);
        page->setTheme(dark_, compact_);
        pages_.emplace(id, std::move(page));
    }
    if (auto old = pages_.find(current_); old != pages_.end())
        old->second->deactivate();
    pageHost_->setContent({});
    current_ = id;
    auto &page = pages_.at(id);
    pageHost_->setContent(page->widget());
    heading_->setText(descriptor->title);
    description_->setText(descriptor->description);
    expanded_[int(descriptor->category)] = true;
    drawer_ = false;
    updateDrawer();
    updateNavigation();
    updateCode();
    page->activate();
    window_.setTitle(std::wstring(L"OneUI 试验台 · ") + descriptor->title);
    return true;
}
void DemoShell::updateCode() {
    codeToolbar_->clearChildren();
    auto found = pages_.find(current_);
    if (found == pages_.end())
        return;
    auto examples = compiledExamples(current_);
    code_->setVisible(codeOpen_);
    codeToolbar_->setVisible(codeOpen_);
    codeButton_->setText(codeOpen_ ? L"收起代码" : L"接入代码");
    if (examples.empty()) {
        code_->setText(L"本页接入示例正在整理。");
        return;
    }
    code_->setText(examples.front().code);
    for (auto example : examples) {
        auto language = button(*codeToolbar_, example.language.c_str(), [this, example] {
            code_->setText(example.code);
            code_->scrollToTop();
        });
        codeToolbar_->setFlex(language, {0, 0, 100, 70});
    }
    auto copy =
        button(*codeToolbar_, L"复制", [this] { SystemClipboard{}.setText(code_->text()); });
    codeToolbar_->setFlex(copy, {0, 0, 80, 80});
}
void DemoShell::setTheme(bool dark, bool compact) {
    dark_ = dark;
    compact_ = compact;
    StyleSheet sheet;
    sheet.addRulesFromCss(
        ui::declarativeTheme(dark, compact ? ui::Density::Compact : ui::Density::Comfortable));
    std::function<void(Widget &)> apply = [&](Widget &w) {
        if (auto label = dynamic_cast<Label *>(&w))
            label->setColor(dark ? Color{229, 236, 232} : Color{28, 43, 35});
        if (auto b = dynamic_cast<Button *>(&w))
            applyDemoButton(*b, sheet);
        if (auto view = dynamic_cast<View *>(&w))
            for (auto &child : view->children())
                apply(*child);
    };
    setStyleScope(std::make_shared<StyleSheet>(sheet));
    apply(*row_);
    apply(*navigation_);
    code_->setStyleOverride(
        textFieldStyleOverrideFromStyleSheet(sheet, {"input", {"one-Input"}, 0}));
    StyleBox surface;
    surface.background.color = dark ? Color{23, 30, 33} : Color{255, 255, 255};
    navigation_->setStyleBox(surface);
    navScroll_->setStyleBox(surface);
    navScroll_->setScrollbarStyle(dark ? Color{102, 137, 115} : Color{112, 145, 123}, 4);
    pageHost_->setBackground(dark ? Color{23, 30, 33} : Color{255, 255, 255});
    main_->setPadding(Insets{compact ? 16.f : 24.f});
    themeButton_->setText(dark ? L"浅色主题" : L"深色主题");
    densityButton_->setText(compact ? L"舒适间距" : L"紧凑间距");
    for (auto &entry : pages_)
        entry.second->setTheme(dark, compact);
    updateNavigation();
    invalidate();
}
void DemoShell::installMenus() {
    std::vector<NativeMenuItem> menus;
    for (int category = 0; category < 3; ++category) {
        NativeMenuItem group{0, categoryName(Category(category))};
        for (std::size_t i = 0; i < pageCatalog().size(); ++i)
            if (pageCatalog()[i].category == Category(category))
                group.children.push_back({1000 + int(i), pageCatalog()[i].title});
        menus.push_back(std::move(group));
    }
    window_.setMenuBar(menus);
    window_.setOnMenuAction([this, weak = lifetimeToken()](int id) {
        if (!weak.expired() && id >= 1000 && std::size_t(id - 1000) < pageCatalog().size())
            navigate(pageCatalog()[id - 1000].id);
    });
}
void DemoShell::updateDrawer() {
    removeOverlay(navScroll_.get());
    row_->reconcileChildren(narrow_ ? std::vector<std::shared_ptr<Widget>>{main_}
                                    : std::vector<std::shared_ptr<Widget>>{navScroll_, main_});
    if (narrow_ && drawer_)
        addAnchoredOverlay(navScroll_, OverlayOptions::modal(100),
                           {260, std::max(0.f, frame().height - 24)}, Insets{12}, 0, 0);
    drawerButton_->setVisible(narrow_);
    invalidate();
}
void DemoShell::layoutChildren() {
    const bool narrow = frame().width < 960;
    if (narrow != narrow_) {
        narrow_ = narrow;
        updateDrawer();
    }
    if (narrow_ && drawer_)
        updateAnchoredOverlay(navScroll_.get(), {260, std::max(0.f, frame().height - 24)},
                              Insets{12}, 0, 0);
    OverlayHost::layoutChildren();
}
bool DemoShell::key(const KeyEvent &event) {
    if (drawer_ && event.key == Key::Escape) {
        drawer_ = false;
        updateDrawer();
        window_.requestFocus(drawerButton_.get());
        return true;
    }
    auto page = pages_.find(current_);
    return page != pages_.end() && page->second->key(event);
}
void DemoShell::paint(Canvas &canvas) {
    canvas.fillRect(frame(), dark_ ? Color{16, 21, 23} : Color{245, 247, 244});
    OverlayHost::paint(canvas);
}
} // namespace lab
