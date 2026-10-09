#include "internal/application_menu.h"
#include "oneui/controls/button.h"
#include "oneui/controls/dialog.h"
#include "oneui/controls/popup.h"
#include "oneui/layout/scroll_view.h"
#include "oneui/native_menu.h"
#include "support/platform_keys.h"
#include <iostream>
using namespace oneui;
namespace {
int failures = 0;
void check(bool value, const char *text) {
    if (!value) {
        ++failures;
        std::cerr << text << "\n";
    }
}
class NullCanvas final : public oneui::Canvas {
  public:
    void save() override {}
    void restore() override {}
    void clipRect(oneui::Rect) override {}
    void clear(oneui::Color) override {}
    void fillRect(oneui::Rect, oneui::Color, float = 0.0f) override {}
    void strokeRect(oneui::Rect, oneui::Color, float = 0.0f, float = 1.0f) override {}
    void fillEllipse(oneui::Rect, oneui::Color) override {}
    void strokeEllipse(oneui::Rect, oneui::Color, float = 1.0f) override {}
    void drawLine(oneui::Point, oneui::Point, oneui::Color, float = 1.0f) override {}
    void drawText(const std::wstring &, oneui::Rect, oneui::Color, float,
                  oneui::TextAlign = oneui::TextAlign::Center) override {}
};

void modelTests() {
    NativeMenuItem leaf{17, L"保存"};
    leaf.shortcut = {"s", KeyModifierPrimary};
    NativeMenuItem root{0, L"文件"};
    root.children = {leaf};
    check(validateNativeMenu({root}), "valid nested menu");
    auto duplicate = root;
    duplicate.children.push_back(leaf);
    check(!validateNativeMenu({duplicate}), "duplicate IDs rejected");
    duplicate.children.back().id = 18;
    check(!validateNativeMenu({duplicate}), "duplicate shortcuts rejected");
    duplicate.children.back().shortcut = {"S", platform::primaryUsesMeta ? KeyModifierMeta
                                                                         : KeyModifierControl};
    check(!validateNativeMenu({duplicate}), "primary modifiers and case normalized");
    auto radio = leaf;
    radio.shortcut = {};
    radio.check = MenuCheck::Radio;
    radio.checked = true;
    radio.radioGroup = 1;
    auto other = radio;
    other.id = 18;
    check(!validateNativeMenu({radio, other}), "only one checked radio in a group");
    other.checked = false;
    check(validateNativeMenu({radio, other}), "controlled radio group");
    radio.radioGroup = 0;
    check(!validateNativeMenu({radio}), "radio needs a group");
    auto separator = NativeMenuItem{};
    separator.separator = true;
    separator.shortcut.modifiers = KeyModifierAlt;
    check(!validateNativeMenu({separator}), "separator cannot own modifiers");
    auto nested = leaf;
    for (int depth = 1; depth < 8; ++depth) {
        auto parent = NativeMenuItem{0, L"Nested"};
        parent.children.push_back(nested);
        nested = parent;
    }
    check(validateNativeMenu({nested}), "eight levels accepted");
    root.children = {nested};
    check(!validateNativeMenu({root}), "ninth level rejected");
    std::vector<NativeMenuItem> large(1024);
    for (int i = 0; i < 1024; ++i)
        large[i] = {i, L"Item"};
    check(validateNativeMenu(large), "1024 nodes accepted");
    large.push_back({1024, L"Overflow"});
    check(!validateNativeMenu(large), "1025 nodes rejected");
    check(validateTrayIcon({}) && validateTrayIcon({1, 1, {255, 0, 0, 128}}),
          "default and RGBA icons");
    check(!validateTrayIcon({1, 1, {255, 0, 0}}) && !validateTrayIcon({257, 1, {}}),
          "invalid icon buffers rejected");
}
void hostTests() {
    NullCanvas canvas;
    auto host = std::make_shared<internal::ApplicationMenuHost>();
    host->setFrame({0, 0, 640, 480});
    auto body = std::make_shared<Button>(L"Body");
    host->setApplicationContent(body);
    NativeMenuItem save{1, L"Save"};
    save.shortcut = {"s", KeyModifierPrimary};
    NativeMenuItem disabled{2, L"Disabled", false};
    disabled.shortcut = {"d", KeyModifierPrimary};
    NativeMenuItem nested{0, L"More"};
    nested.children = {{3, L"Nested action"}};
    NativeMenuItem root{0, L"File"};
    root.children = {save, disabled, nested};
    host->setItems({root});
    host->paint(canvas);
    host->requestFocus(body.get());
    check(body->frame().y == 28 && body->frame().height == 452, "menu reserves client area");
    int action = 0;
    host->setAction([&](int id) { action = id; });
    KeyEvent shortcut = test::editKey(Key::Other);
    shortcut.virtualKey = 'S';
    check(host->onKeyDown(shortcut) && action == 1, "portable primary shortcut");
    shortcut.virtualKey = 'D';
    host->onKeyDown(shortcut);
    check(action == 1, "disabled shortcut cannot activate");
    KeyEvent f10;
    f10.virtualKey = 0x79;
    check(host->onKeyDown(f10), "F10 enters menu");
    KeyEvent enter;
    enter.key = Key::Enter;
    host->onKeyDown(enter);
    host->paint(canvas);
    auto popup = std::dynamic_pointer_cast<Popup>(host->activeFocusChild());
    check(popup && popup->isOpen(), "Enter opens menu");
    int invalidations = 0;
    host->setInvalidator([&] { ++invalidations; });
    host->paint(canvas);
    invalidations = 0;
    host->paint(canvas);
    check(invalidations == 0, "stationary menu does not schedule continuous repaint");
    KeyEvent right;
    right.key = Key::Right;
    host->onKeyDown(right);
    check(action == 1, "Right on leaf does not activate");
    KeyEvent down;
    down.key = Key::Down;
    host->onKeyDown(down);
    host->onKeyDown(right);
    host->paint(canvas);
    auto childPopup = std::dynamic_pointer_cast<Popup>(host->activeFocusChild());
    check(childPopup && childPopup != popup, "Down skips disabled item and Right opens submenu");
    host->onKeyDown(enter);
    check(action == 3, "nested selection reports leaf ID");
    host->setItems({});
    host->paint(canvas);
    check(body->frame().y == 0 && body->frame().height == 480, "removing menu restores body area");
    shortcut.virtualKey = 'S';
    action = 0;
    host->onKeyDown(shortcut);
    check(action == 0, "removed shortcut cannot fire");
    host->setItems({root});
    auto modalBody = std::make_shared<OverlayHost>();
    modalBody->setContent(body);
    auto dialog = std::make_shared<Dialog>();
    modalBody->addOverlay(dialog, OverlayOptions::modal(10));
    host->setApplicationContent(modalBody);
    host->paint(canvas);
    host->requestFocus(modalBody.get());
    host->onKeyDown(shortcut);
    host->onKeyDown(f10);
    check(action == 0, "modal blocks menu commands");
    check(!std::dynamic_pointer_cast<Popup>(host->activeFocusChild()), "F10 cannot escape modal");
    host->setApplicationContent(body);
    host->requestFocus(body.get());
    host->setAction([&](int) { host->setItems({}); });
    host->onKeyDown(shortcut);
    host->paint(canvas);
    check(body->frame().y == 0, "action can replace menu during dispatch");
    host->setItems({root});
    host->setAction([&](int) { host.reset(); });
    auto *raw = host.get();
    raw->onKeyDown(shortcut);
    check(!host, "action can destroy host during shortcut dispatch");
}
void longMenuTests() {
    NullCanvas canvas;
    auto host = std::make_shared<internal::ApplicationMenuHost>();
    host->setFrame({0, 0, 300, 180});
    auto body = std::make_shared<Button>(L"Document");
    host->setApplicationContent(body);
    NativeMenuItem root{0, L"Long menu"};
    for (int i = 0; i < 50; ++i)
        root.children.push_back({i, L"Item"});
    host->setItems({root});
    host->paint(canvas);
    host->requestFocus(body.get());
    KeyEvent f10;
    f10.virtualKey = 0x79;
    host->onKeyDown(f10);
    KeyEvent enter;
    enter.key = Key::Enter;
    host->onKeyDown(enter);
    host->paint(canvas);
    auto popup = std::dynamic_pointer_cast<Popup>(host->activeFocusChild());
    auto scroll = popup ? std::dynamic_pointer_cast<ScrollView>(popup->content()) : nullptr;
    check(scroll && scroll->maxScrollOffset() > 0, "long menu has a scrollable viewport");
    KeyEvent end;
    end.key = Key::End;
    host->onKeyDown(end);
    host->paint(canvas);
    check(scroll && scroll->scrollOffset() > 0, "End reveals last menu item");
    int chosen = -1;
    host->setAction([&](int id) { chosen = id; });
    host->onKeyDown(enter);
    check(chosen == 49 && body->focused(), "offscreen action selected and document focus restored");
    host->onKeyDown(f10);
    host->onKeyDown(enter);
    host->paint(canvas);
    KeyEvent escape;
    escape.key = Key::Escape;
    host->onKeyDown(escape);
    host->onKeyDown(escape);
    check(body->focused(), "Escape leaves menu and restores document focus");
}
} // namespace
int main() {
    modelTests();
    hostTests();
    longMenuTests();
    return failures ? 1 : 0;
}
