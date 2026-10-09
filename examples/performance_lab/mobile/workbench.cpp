#include "workbench.hpp"
#include "internal/unicode.h"
#include "mobile_snippets.hpp"
#include "pages/page_factories.hpp"
#include "performance_page.hpp"
#include <algorithm>
#include <sstream>

namespace lab::mobile {
using namespace oneui;
namespace {
class ShellView final : public View {
  public:
    Color background{245, 247, 244};
    void refresh() { invalidate(); }
    std::function<void(Rect)> arrange;
    void paint(Canvas &canvas) override {
        canvas.fillRect(frame(), background);
        View::paint(canvas);
    }
    void layoutChildren() override {
        if (arrange)
            arrange(frame());
    }
};
bool available(const std::string &id) {
    return id == "layout/basic" || id == "features/controls" || id == "performance/charts" ||
           id == "performance/particles";
}
void touchTargets(const std::shared_ptr<Widget> &widget) {
    if (auto view = std::dynamic_pointer_cast<View>(widget)) {
        for (const auto &child : view->children()) {
            auto stack = dynamic_cast<Stack *>(view.get());
            const bool control =
                dynamic_cast<Button *>(child.get()) || dynamic_cast<Slider *>(child.get()) ||
                dynamic_cast<Checkbox *>(child.get()) || dynamic_cast<Switch *>(child.get()) ||
                dynamic_cast<RadioGroup *>(child.get()) || dynamic_cast<Select *>(child.get());
            if (stack && stack->direction() == StackDirection::Column && control) {
                auto flex = stack->explicitFlex(child).value_or(StackFlex{});
                flex.basis = std::max(48.f, flex.basis.value_or(0));
                flex.min = std::max(48.f, flex.min);
                flex.max = std::max(48.f, flex.max);
                stack->setFlex(child, flex);
            }
            touchTargets(child);
        }
    }
}
void rect(std::ostream &out, const std::string &name, const std::shared_ptr<Widget> &w) {
    if (!w->visible())
        return;
    const auto b = w->frame();
    out << ' ' << name << '=' << b.x << ',' << b.y << ',' << b.width << ',' << b.height;
}
} // namespace
Workbench::Workbench() {
    auto shell = std::make_shared<ShellView>();
    root_ = shell;
    shell->arrange = [this](Rect frame) { arrange(frame); };
    title_ = std::make_shared<Label>(L"OneUI 试验台");
    title_->setFontSize(18);
    menu_ = std::make_shared<Button>(L"目录");
    theme_ = std::make_shared<Button>(L"深色");
    source_ = std::make_shared<Button>(L"查看接入代码 · C++");
    menu_->setOnClick([this] { pending_ = "@menu"; });
    theme_->setOnClick([this] { pending_ = "@theme"; });
    source_->setOnClick([this] { pending_ = "@code"; });
    content_ = std::make_shared<Panel>();
    content_->setBorderWidth(0);
    code_ = std::make_shared<ScrollView>();
    codeText_ = std::make_shared<Label>();
    codeText_->setFontFamily(L"monospace");
    codeText_->setFontSize(14);
    codeText_->setTextWrapping(true);
    code_->setContent(codeText_);
    navigation_ = std::make_shared<ScrollView>();
    entries_ = std::make_shared<Stack>();
    entries_->setEngine(StackEngine::Yoga);
    entries_->setPadding(Insets{12});
    entries_->setGap(8);
    navigation_->setContent(entries_);
    for (auto category : {Category::Layout, Category::Performance, Category::Features}) {
        auto group = std::make_shared<Button>(categoryName(category));
        group->setOnClick(
            [this, category] { pending_ = "@group" + std::to_string(int(category)); });
        entries_->add(group);
        entries_->setFlex(group, {0, 0, 48, 48});
        groups_[category] = group;
        for (const auto &entry : pageCatalog())
            if (entry.category == category) {
                auto link = std::make_shared<Button>(std::wstring(entry.title) +
                                                     (available(entry.id) ? L"" : L" · 待接入"));
                link->setDisabled(!available(entry.id));
                link->setOnClick([this, id = std::string(entry.id)] { pending_ = id; });
                entries_->add(link);
                entries_->setFlex(link, {0, 0, 48, 48});
                links_[entry.id] = link;
            }
    }
    for (const auto &w : std::vector<std::shared_ptr<Widget>>{title_, menu_, theme_, content_,
                                                              code_, source_, navigation_})
        root_->add(w);
    root_->setTextEnvironment(L"sans-serif", 1);
    navigate("layout/basic");
    applyTheme();
}
Workbench::~Workbench() {
    setActive(false);
    // Detach before page owners are destroyed; callbacks capture those owners.
    content_->setContent({});
    root_->clearChildren();
}
std::shared_ptr<Widget> Workbench::widget() const { return root_; }
DemoPage *Workbench::page() const {
    auto found = pages_.find(current_);
    return found == pages_.end() ? nullptr : found->second.get();
}
void Workbench::navigate(const std::string &id) {
    const auto *entry = findPage(id);
    if (!entry || !available(id))
        return;
    if (!pages_.count(id)) {
        std::unique_ptr<DemoPage> created;
        if (id == "layout/basic")
            created = createBasicLayoutPage();
        else if (id == "features/controls")
            created = createBasicControlsPage();
        else
            created = createPlotPage(id == "performance/particles");
        created->setTheme(dark_, false);
        touchTargets(created->widget());
        pages_.emplace(id, std::move(created));
    }
    if (pageActive_ && page())
        page()->deactivate();
    pageActive_ = false;
    if (page())
        page()->widget()->clearInteractionState();
    content_->setContent({});
    current_ = id;
    expanded_ = entry->category;
    drawer_ = false;
    codeOpen_ = false;
    content_->setContent(page()->widget());
    title_->setText(entry->title);
    loadSource();
    updateVisibility();
    applyTheme();
}
bool Workbench::flush() {
    if (pending_.empty())
        return false;
    auto command = std::move(pending_);
    pending_.clear();
    if (command == "@menu") {
        drawer_ = !drawer_;
        updateVisibility();
    } else if (command == "@theme") {
        dark_ = !dark_;
        applyTheme();
    } else if (command == "@code") {
        codeOpen_ = !codeOpen_;
        updateVisibility();
    } else if (command.rfind("@group", 0) == 0) {
        expanded_ = Category(command.back() - '0');
        navigation_->setScrollOffset(0);
        updateVisibility();
        applyTheme();
    } else
        navigate(command);
    return true;
}
bool Workbench::back() {
    if (drawer_) {
        drawer_ = false;
        updateVisibility();
        return true;
    }
    if (codeOpen_) {
        codeOpen_ = false;
        updateVisibility();
        return true;
    }
    if (current_ != "layout/basic") {
        navigate("layout/basic");
        return true;
    }
    return false;
}
void Workbench::viewport(float width) {
    const bool wide = width >= 840;
    if (wide != wide_) {
        wide_ = wide;
        drawer_ = false;
        updateVisibility();
    }
}
void Workbench::setActive(bool active) {
    active_ = active;
    updateActivation();
}
void Workbench::updateActivation() {
    const bool next = active_ && (wide_ || !drawer_) && !codeOpen_;
    if (page() && next != pageActive_) {
        if (next)
            page()->activate();
        else
            page()->deactivate();
    }
    pageActive_ = next;
}
void Workbench::updateVisibility() {
    navigation_->setVisible(wide_ || drawer_);
    const bool pageVisible = wide_ || !drawer_;
    content_->setVisible(pageVisible && !codeOpen_);
    code_->setVisible(pageVisible && codeOpen_);
    source_->setVisible(pageVisible);
    menu_->setVisible(!wide_);
    menu_->setText(drawer_ ? L"返回" : L"目录");
    source_->setText(codeOpen_ ? L"收起代码 · 返回体验" : L"查看接入代码 · C++");
    for (const auto &entry : pageCatalog())
        links_.at(entry.id)->setVisible(entry.category == expanded_);
    updateActivation();
    std::static_pointer_cast<ShellView>(root_)->refresh();
}
void Workbench::arrange(Rect b) {
    const float header = std::min(64.f, b.height), footer = std::min(64.f, b.height - header);
    const float sidebar = wide_ ? 272.f : 0.f;
    menu_->setFrame({b.x + 12, b.y + 8, 64, 48});
    title_->setFrame(
        {b.x + (wide_ ? 20 : 88), b.y + 8, std::max(0.f, b.width - (wide_ ? 120 : 188)), 48});
    theme_->setFrame({b.x + b.width - 84, b.y + 8, 72, 48});
    const Rect body{b.x + sidebar, b.y + header, b.width - sidebar,
                    std::max(0.f, b.height - header - footer)};
    content_->setFrame(body);
    code_->setFrame({body.x + 16, body.y, std::max(0.f, body.width - 32), body.height});
    source_->setFrame(
        {body.x + 16, b.y + b.height - footer + 8, std::max(0.f, body.width - 32), 48});
    navigation_->setFrame({b.x, b.y + header, wide_ ? sidebar : b.width, b.height - header});
}
void Workbench::applyTheme() {
    StyleSheet styles;
    styles.addRulesFromCss(ui::declarativeTheme(dark_, ui::Density::Comfortable));
    for (auto &entry : pages_)
        entry.second->setTheme(dark_, false);
    for (auto button : {menu_, theme_, source_})
        applyDemoButton(*button, styles);
    for (auto &entry : groups_)
        applyDemoButton(*entry.second, styles, entry.first == expanded_);
    for (auto &entry : links_)
        applyDemoButton(*entry.second, styles, entry.first == current_);
    const Color ink = dark_ ? Color{229, 236, 232} : Color{28, 43, 35};
    const Color surface = dark_ ? Color{23, 30, 33} : Color{245, 247, 244};
    title_->setColor(ink);
    codeText_->setColor(ink);
    StyleBox box;
    box.background.color = surface;
    std::static_pointer_cast<ShellView>(root_)->background = surface;
    std::static_pointer_cast<ShellView>(root_)->refresh();
    entries_->setStyleBox(box);
    navigation_->setStyleBox(box);
    code_->setStyleBox(box);
    content_->setBackground(surface);
    theme_->setText(dark_ ? L"浅色" : L"深色");
}
void Workbench::loadSource() {
    const char *source = current_ == "layout/basic"         ? snippets::basic_layout
                         : current_ == "features/controls"  ? snippets::controls
                         : current_ == "performance/charts" ? snippets::charts
                                                            : snippets::particles;
    codeText_->setText(L"C++ · 与桌面同源，已随 Android 目标编译检查。\n"
                       L"移动宿主使用 WidgetHost::setContent；C / Rust 控件接入仍待补齐。\n\n" +
                       unicode::fromUtf8(source));
    code_->setContentHeight(1400);
    code_->setScrollOffset(0);
}
std::string Workbench::diagnostics() const {
    std::ostringstream out;
    out << " pageId=" << current_ << " pages=" << pages_.size()
        << " catalog=" << pageCatalog().size() << " drawer=" << drawer_ << " wide=" << wide_
        << " code=" << codeOpen_ << " activePage=" << pageActive_;
    for (const auto &entry : pages_) {
        const auto detail = plotDiagnostics(entry.second.get());
        if (!detail.empty())
            out << " retained[" << entry.first << "]{" << detail << " }";
    }
    return out.str();
}
void Workbench::describeControls(std::ostream &out) const {
    rect(out, "menu", menu_);
    rect(out, "theme", theme_);
    rect(out, "source", source_);
    if (navigation_->visible()) {
        for (const auto &entry : groups_)
            rect(out, "group" + std::to_string(int(entry.first)), entry.second);
        for (const auto &entry : links_)
            if (available(entry.first))
                rect(out, "nav:" + entry.first, entry.second);
    }
}
} // namespace lab::mobile
