#include "oneui/controls/button.h"
#include "oneui/controls/label.h"
#include "oneui/layout/panel.h"
#include "platform/shared/title_bar_hit.h"

#include <iostream>
#include <memory>

namespace {
int failures = 0;
void expectTrue(const char* name, bool value) {
    if (!value) {
        std::cerr << name << '\n';
        ++failures;
    }
}
} // namespace

int main() {
    auto bar = std::make_shared<oneui::Panel>();
    bar->setFrame(oneui::Rect{0.0f, 0.0f, 800.0f, 40.0f});
    auto title = std::make_shared<oneui::Label>(L"iShell Pro");
    title->setFrame(oneui::Rect{10.0f, 10.0f, 120.0f, 20.0f});
    auto search = std::make_shared<oneui::Button>(L"Search");
    search->setFrame(oneui::Rect{300.0f, 6.0f, 200.0f, 28.0f});
    bar->add(title);
    bar->add(search);
    using oneui::platform::titleBarControlAt;
    expectTrue("a button in the title bar takes clicks", titleBarControlAt(bar, {400.0f, 20.0f}));
    expectTrue("the title text drags the window", !titleBarControlAt(bar, {50.0f, 20.0f}));
    expectTrue("empty title bar drags the window", !titleBarControlAt(bar, {650.0f, 20.0f}));
    search->setVisible(false);
    expectTrue("a hidden control does not take clicks", !titleBarControlAt(bar, {400.0f, 20.0f}));
    search->setVisible(true);
    search->setDisabled(true);
    expectTrue("a disabled control does not take clicks", !titleBarControlAt(bar, {400.0f, 20.0f}));
    search->setDisabled(false);
    auto root = std::make_shared<oneui::OverlayHost>();
    root->setFrame(bar->frame());
    root->setContent(bar);
    expectTrue("root overlay content exposes caption buttons", titleBarControlAt(root, {400.0f, 20.0f}));
    expectTrue("blank root overlay content still drags", !titleBarControlAt(root, {650.0f, 20.0f}));
    auto account = std::make_shared<oneui::Button>(L"Account");
    account->setFrame({720, 4, 32, 32});
    root->addOverlay(account, 2);
    expectTrue("interactive overlay takes caption input", titleBarControlAt(root, {730, 20}));
    auto modal = std::make_shared<oneui::Panel>();
    modal->setFrame({20, 60, 400, 200});
    root->addOverlay(modal, oneui::OverlayOptions::modal(3));
    expectTrue("modal blocks dragging the underlying caption", titleBarControlAt(root, {650, 20}));
    root->removeOverlay(modal.get());
    expectTrue("dismissed modal restores blank drag lane", !titleBarControlAt(root, {650, 20}));
    auto decorated = std::make_shared<oneui::OverlayHost>();
    decorated->setFrame(search->frame());
    decorated->setContent(search);
    auto shortcut = std::make_shared<oneui::Panel>();
    shortcut->setFrame({450, 10, 42, 18});
    shortcut->setDisabled(true);
    decorated->addOverlay(shortcut);
    auto nested = std::make_shared<oneui::OverlayHost>();
    nested->setFrame(bar->frame());
    nested->setContent(decorated);
    expectTrue("decorative shortcut keeps underlying search clickable", titleBarControlAt(nested, {470, 20}));
    return failures == 0 ? 0 : 1;
}
