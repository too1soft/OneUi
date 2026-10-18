#include "oneui/controls/text_field.h"
#include "oneui/controls/select.h"
#include "oneui/controls/interactive_surface.h"
#include <type_traits>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

void interactiveSurfaceDestroyedDuringTabContract() {
    class TabCallback final : public oneui::Widget {
    public:
        std::function<void()> callback;
        void paint(oneui::Canvas&) override {}
        bool isFocusable() const override { return true; }
        bool onKeyDown(const oneui::KeyEvent& event) override {
            if (event.key == oneui::Key::Tab) callback();
            return false;
        }
    };
    auto row = std::make_shared<oneui::InteractiveSurface>();
    row->setOnClick([] {});
    auto child = std::make_shared<TabCallback>();
    row->setContent(child);
    row->onFocusChanged(true);
    expect(row->focusFirstLeaf(), "destruction fixture enters the nested action");
    std::weak_ptr<oneui::InteractiveSurface> lifetime = row;
    child->callback = [&] { row.reset(); };
    oneui::KeyEvent reverse{oneui::Key::Tab};
    reverse.shift = true;
    auto* dispatch = row.get();
    expect(dispatch->onKeyDown(reverse), "destroyed surface consumes the in-flight Tab safely");
    expect(lifetime.expired(), "Tab callback really destroys the surface before dispatch returns");
}

void interactiveSurfaceNativeWindowContract() {
#ifdef _WIN32
    struct FocusAccess : oneui::View {
        static constexpr auto reader() { return &FocusAccess::focusedChild; }
    };
    constexpr auto focusedChild = FocusAccess::reader();
    static_assert(std::is_same_v<decltype(FocusAccess::reader()),
                  oneui::Widget* (oneui::View::*)() const>);
    oneui::WindowOptions options;
    options.visible = false;
    auto window = oneui::Window::create(options);
    window->initialize();
    auto root = std::make_shared<oneui::OverlayHost>();
    auto content = std::make_shared<oneui::View>();
    auto before = std::make_shared<oneui::Button>(L"Before rows");
    auto after = std::make_shared<oneui::Button>(L"After rows");
    content->add(before);
    std::vector<std::shared_ptr<oneui::InteractiveSurface>> rows;
    std::vector<std::shared_ptr<oneui::Button>> actions;
    std::vector<oneui::Widget*> order{before.get()};
    int activations = 0;
    for (int i = 0; i < 2; ++i) {
        auto row = std::make_shared<oneui::InteractiveSurface>();
        row->setOnClick([&] { ++activations; });
        auto nested = std::make_shared<oneui::View>();
        auto first = std::make_shared<oneui::Button>(L"First action");
        auto last = std::make_shared<oneui::Button>(L"Last action");
        nested->add(first); nested->add(last); row->setContent(nested);
        content->add(row);
        order.insert(order.end(), {row.get(), first.get(), last.get()});
        rows.push_back(row);
        actions.push_back(first); actions.push_back(last);
    }
    content->add(after); order.push_back(after.get()); root->setContent(content);
    window->setContent(root);
    expect(window->requestFocus(before.get(), true), "native row traversal starts before rows");
    const auto hwnd = static_cast<HWND>(window->nativeHandle());
    const auto check = [&](oneui::Widget* expected) {
        int count = int(before->focused()) + int(after->focused());
        for (const auto& action : actions) count += int(action->focused());
        for (const auto& row : rows) count += int(row->focused() && !(row.get()->*focusedChild)());
        expect(count == 1, "native rows have exactly one semantic focus stop");
        expect(expected->focused(), "native row Tab order matches the complete forward/reverse sequence");
        for (const auto& row : rows) {
            if (expected == row.get()) {
                expect(!(row.get()->*focusedChild)(), "reverse nested boundary stops on the row itself");
            }
        }
    };
    const auto tab = [&](bool reverse) {
        if (reverse) SendMessageW(hwnd, WM_KEYDOWN, VK_SHIFT, 0);
        SendMessageW(hwnd, WM_KEYDOWN, VK_TAB, 0);
        SendMessageW(hwnd, WM_KEYUP, VK_TAB, 0);
        if (reverse) SendMessageW(hwnd, WM_KEYUP, VK_SHIFT, 0);
    };
    for (int cycle = 0; cycle < 2; ++cycle) {
        for (std::size_t i = 1; i < order.size(); ++i) { tab(false); check(order[i]); }
        for (std::size_t i = order.size() - 1; i > 0; --i) { tab(true); check(order[i - 1]); }
    }
    expect(activations == 0, "Tab navigation does not activate row commands");
    window->setContent(nullptr);
    window->close();
#endif
}

void overlayFocusNativeWindowContract() {
    interactiveSurfaceDestroyedDuringTabContract();
    interactiveSurfaceNativeWindowContract();
#ifdef _WIN32
    oneui::WindowOptions options;
    options.visible = false;
    auto window = oneui::Window::create(options);
    window->initialize();
    auto root = std::make_shared<oneui::OverlayHost>();
    auto content = std::make_shared<oneui::View>();
    auto before = std::make_shared<oneui::Button>(L"Before");
    auto outer = std::make_shared<oneui::OverlayHost>();
    auto inner = std::make_shared<oneui::OverlayHost>();
    auto search = std::make_shared<oneui::TextField>();
    auto after = std::make_shared<oneui::Select>();
    after->setItems({L"All hosts", L"SSH"});
    // A non-focusable decoration must not turn a toolbar field into a trap.
    inner->setContent(search);
    inner->addOverlay(std::make_shared<oneui::Panel>());
    outer->setContent(inner);
    content->add(before); content->add(outer); content->add(after);
    root->setContent(content);
    window->setContent(root);
    expect(root->hasCompositionOwner() && outer->hasCompositionOwner() && inner->hasCompositionOwner(),
           "native window root and nested hosts all have real composition owners");
    expect(window->requestFocus(before.get(), true), "native window focuses initial sibling");
    const auto hwnd = static_cast<HWND>(window->nativeHandle());
    const auto tab = [&](bool reverse) {
        if (reverse) SendMessageW(hwnd, WM_KEYDOWN, VK_SHIFT, 0);
        SendMessageW(hwnd, WM_KEYDOWN, VK_TAB, 0);
        SendMessageW(hwnd, WM_KEYUP, VK_TAB, 0);
        if (reverse) SendMessageW(hwnd, WM_KEYUP, VK_SHIFT, 0);
        expect(int(before->focused()) + int(search->focused()) + int(after->focused()) == 1,
               "native Tab leaves exactly one focused leaf");
    };
    tab(false); expect(search->focused(), "Tab enters two nested content hosts");
    tab(false); expect(after->focused(), "nested content boundary advances to adjacent Select");
    tab(false); expect(before->focused(), "real owned root wraps forward");
    tab(true); expect(after->focused(), "real owned root wraps backward");
    tab(true); expect(search->focused(), "Shift Tab enters nested last leaf");
    tab(true); expect(before->focused(), "nested reverse boundary reaches previous sibling");
    window->setContent(nullptr);
    window->close();
#endif
}
