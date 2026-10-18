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
    return failures == 0 ? 0 : 1;
}
