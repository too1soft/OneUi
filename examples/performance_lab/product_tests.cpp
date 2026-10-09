#include "pages/page_factories.hpp"
#include "support/recording_canvas.h"
#include <iostream>
#include <oneui/controls/icon_button.h>
#include <oneui/controls/nav_item.h>
#include <oneui/controls/status_strip.h>
#include <oneui/layout/app_shell.h>
#include <oneui/layout/top_bar.h>
#include <stdexcept>

using namespace oneui;
namespace {
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class T, class Match>
std::shared_ptr<T> find(const std::shared_ptr<Widget> &widget, Match match) {
    if (auto item = std::dynamic_pointer_cast<T>(widget); item && match(*item))
        return item;
    if (auto view = std::dynamic_pointer_cast<View>(widget))
        for (const auto &child : view->children())
            if (auto result = find<T>(child, match))
                return result;
    return {};
}
template <class T> std::shared_ptr<T> find(const std::shared_ptr<Widget> &widget) {
    return find<T>(widget, [](auto &) { return true; });
}
class Probe final : public Widget {
  public:
    int paints = 0, clicks = 0;
    void paint(Canvas &) override { ++paints; }
    bool onMouseDown(const MouseEvent &) override {
        ++clicks;
        return true;
    }
};
void sidebarLifecycle() {
    auto shell = std::make_shared<AppShell>();
    auto sidebar = std::make_shared<Probe>();
    auto content = std::make_shared<TextField>();
    shell->setSidebar(sidebar);
    shell->setContent(content);
    shell->setFrame({10, 10, 900, 600});
    shell->setSidebarWidth(180);
    test_support::RecordingCanvas canvas;
    shell->paint(canvas);
    require(sidebar->paints == 1, "Visible sidebar was not painted");
    require(shell->requestFocus(content.get()), "Content focus failed");
    content->setText(L"Retained draft");
    content->setSelectionRange(0, 8);
    const float oldX = content->frame().x;
    shell->setSidebarVisible(false);
    shell->paint(canvas);
    require(sidebar->paints == 1 && shell->children().size() == 1,
            "Hidden sidebar still participates in drawing/input traversal");
    require(content->frame().x < oldX && content->focused(),
            "Sidebar collapse lost content layout or keyboard focus");
    require(content->selectedText() == L"Retained", "Collapse lost the editor selection");
    auto replacement = std::make_shared<Probe>();
    shell->setSidebar(replacement);
    shell->paint(canvas);
    require(replacement->paints == 0 && shell->children().size() == 1,
            "Replacing a hidden sidebar accidentally showed it");
    for (int i = 0; i < 30; ++i) {
        shell->setSidebarVisible(true);
        shell->paint(canvas);
        shell->setSidebarVisible(false);
        shell->paint(canvas);
    }
    require(replacement->paints == 30 && content->focused() &&
                content->selectedText() == L"Retained",
            "Repeated sidebar changes lost content identity/focus");
    shell->setSidebarVisible(true);
    shell->paint(canvas);
    MouseEvent event;
    event.position = {20, 20};
    shell->onMouseDown(event);
    require(replacement->clicks == 1, "Restored sidebar did not receive input");
}
void productInteractions() {
    auto page = lab::createProductPage();
    page->setTheme(true, false);
    page->activate();
    auto root = page->widget();
    test_support::RecordingCanvas canvas;
    root->setFrame({0, 0, 1100, 800});
    root->paint(canvas);
    auto shell = find<AppShell>(root);
    auto feedback = find<Label>(
        root, [](auto &label) { return label.accessibleName() == L"组合页面事件结果"; });
    auto field = [&](const wchar_t *name) {
        return find<TextField>(root, [name](auto &item) { return item.accessibleName() == name; });
    };
    auto partner = field(L"对方设备 ID"), code = field(L"临时识别码");
    auto search = field(L"筛选本地模拟设备");
    auto recents = find<VirtualList>(root);
    auto connect = find<Button>(root, [](auto &button) { return button.text() == L"模拟连接"; });
    auto generate =
        find<Button>(root, [](auto &button) { return button.text() == L"生成临时识别码"; });
    require(shell && feedback && partner && code && search && recents && connect && generate,
            "Product composition is missing its interactive controls");
    KeyEvent enter;
    enter.key = Key::Enter;
    connect->onKeyDown(enter);
    require(feedback->text().find(L"请填写") != std::wstring::npos, "Empty partner was accepted");
    generate->onKeyDown(enter);
    require(code->text() == L"DEMO-1", "Generate command did not update the field");
    partner->setText(L"draft-id");
    partner->setSelectionRange(0, 5);
    connect->onKeyDown(enter);
    require(feedback->text().find(L"draft-id") != std::wstring::npos,
            "Connection result lost its ID");
    search->onTextCommitted(L"构建");
    require(recents->items().size() == 1 && recents->items()[0].title == L"构建工作站",
            "Search did not filter the real rich list");
    search->setText(L"");
    search->onTextCommitted(L"missing-device");
    require(recents->items().empty(), "No-match search did not expose empty state");
    for (int i = 0; i < 30; ++i) {
        page->deactivate();
        generate->onKeyDown(enter);
        require(code->text() == L"DEMO-1", "Inactive page accepted a stale callback");
        root->setFrame({0, 0, i % 2 ? 1100.f : 560.f, 700});
        page->setTheme(i % 2, i % 2);
        root->paint(canvas);
        require(shell->sidebarVisible() == bool(i % 2), "Responsive sidebar did not change");
        page->activate();
    }
    require(field(L"对方设备 ID") == partner && partner->text() == L"draft-id" &&
                partner->selectedText() == L"draft",
            "Theme/width changes recreated the editor or lost its draft/selection");
    page.reset();
    generate->onKeyDown(
        enter); // A caller may retain native widgets after the page owner goes away.
    require(code->text() == L"DEMO-1", "Destroyed page callback still executed");
}
void toolbarLifecycle() {
    auto bar = std::make_shared<TopBar>();
    auto search = std::make_shared<TextField>();
    bar->setLeading(search);
    search->setText(L"Retained search");
    search->setSelectionRange(0, 8);
    require(bar->requestFocus(search.get()), "Toolbar search focus failed");
    auto action = std::make_shared<Probe>();
    test_support::RecordingCanvas canvas;
    bar->setFrame({0, 0, 800, 48});
    for (int i = 0; i < 30; ++i) {
        bar->addAction(action);
        bar->paint(canvas);
        bar->clearActions();
        bar->paint(canvas);
        require(search->focused() && search->selectedText() == L"Retained",
                "Toolbar action changes lost search focus or selection");
    }
    require(action->paints == 30, "Removed toolbar actions are still painted");
    bar->setLeading({});
    require(!search->focused(), "Detached toolbar search kept focus");
}
void composedPageLifecycle() {
    auto window = Window::create(L"Product composition regression", 1100, 800);
    require(bool(window), "Composition test window creation failed");
    const lab::PageDescriptor descriptor{
        "layout/pages", lab::Category::Layout, L"", L"", lab::PageKind::Recipes, 4, ""};
    auto page = lab::createLayoutPage(descriptor, *window);
    auto root = page->widget();
    page->setTheme(true, false);
    page->activate();
    window->setContent(root);
    window->initialize();
    window->prepareLayoutSnapshot();
    auto chooser = find<Tabs>(root);
    auto partner =
        find<TextField>(root, [](auto &field) { return field.accessibleName() == L"对方设备 ID"; });
    require(chooser && partner, "Composition did not default to the product shell");
    partner->setText(L"persistent draft");
    partner->setSelectionRange(0, 10);
    KeyEvent next, previous;
    next.key = Key::Right;
    previous.key = Key::Left;
    for (int i = 0; i < 30; ++i) {
        chooser->onKeyDown(next);
        require(chooser->selectedIndex() == 1 && !find<AppShell>(root),
                "List/detail choice did not detach product content");
        page->deactivate();
        page->setTheme(i % 2, i % 2);
        page->activate();
        chooser->onKeyDown(previous);
        window->prepareLayoutSnapshot();
        auto returned = find<TextField>(
            root, [](auto &field) { return field.accessibleName() == L"对方设备 ID"; });
        require(returned == partner && returned->text() == L"persistent draft" &&
                    returned->selectedText() == L"persistent",
                "Composition switching recreated an editor or lost its selection");
    }
    page->deactivate();
    window->setContent({});
    page.reset();
    chooser->onKeyDown(next); // A retained chooser cannot access the destroyed page owner.
}
template <class T> void buttonKeyboard() {
    auto button = std::make_unique<T>();
    int clicked = 0;
    button->setOnClick([&] { ++clicked; });
    KeyEvent event;
    for (auto key : {Key::Enter, Key::Space}) {
        event.key = key;
        require(button->onKeyDown(event), "Keyboard activation was not handled");
    }
    require(clicked == 2, "Keyboard activation did not reach the callback");
    button->setDisabled(true);
    require(!button->onKeyDown(event) && clicked == 2, "Disabled button accepted a key");
    button->setDisabled(false);
    button->setVisible(false);
    require(!button->onKeyDown(event), "Hidden button accepted a key");
    button->setVisible(true);
    auto token = std::make_shared<int>();
    std::weak_ptr<int> weak = token;
    button->setOnClick([&, token] {
        button.reset();
        require(!weak.expired(), "Callback storage was destroyed while still executing");
        ++clicked;
    });
    token.reset();
    button->onKeyDown(event);
    require(!button && weak.expired() && clicked == 3, "Button destruction leaked callback state");
}
void statusKeyboard() {
    auto strip = std::make_unique<StatusStrip>(L"Status", L"Local");
    KeyEvent event;
    event.key = Key::Enter;
    require(!strip->isFocusable() && !strip->onKeyDown(event), "Empty status trapped focus");
    int primary = 0, secondary = 0;
    strip->setPrimaryAction(L"Details");
    strip->setSecondaryAction(L"Copy");
    strip->setOnPrimaryAction([&] { ++primary; });
    strip->setOnSecondaryAction([&] { ++secondary; });
    require(strip->isFocusable() && strip->onKeyDown(event) && primary == 1,
            "Status primary action is not keyboard accessible");
    event.key = Key::Right;
    strip->onKeyDown(event);
    event.key = Key::Space;
    strip->onKeyDown(event);
    require(secondary == 1 && primary == 1, "Arrow did not choose secondary action");
    event.key = Key::Tab;
    require(!strip->onKeyDown(event), "Status strip trapped Tab");
    strip->setSecondaryAction({});
    event.key = Key::Enter;
    strip->onKeyDown(event);
    require(primary == 2, "Removed action did not fall back to remaining action");
    strip->setDisabled(true);
    require(!strip->onKeyDown(event), "Disabled status accepted activation");
    strip->setDisabled(false);
    strip->setOnPrimaryAction([&] {
        strip.reset();
        ++primary;
    });
    strip->onKeyDown(event);
    require(!strip && primary == 3, "Status callback could not destroy its owner");
}
} // namespace
int main() {
    try {
        sidebarLifecycle();
        toolbarLifecycle();
        buttonKeyboard<NavItem>();
        buttonKeyboard<IconButton>();
        statusKeyboard();
        productInteractions();
        composedPageLifecycle();
        std::cout << "PASS: hidden sidebar drawing/input, retained focus and draft, product "
                     "validation/filter/events, 30 theme/width/lifecycle cycles.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
