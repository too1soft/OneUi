#include "oneui/controls/text_field.h"
#include "platform/shared/desktop_window.h"
#include <iostream>

class RemoteEditor final : public oneui::Widget {
public:
    std::wstring committed;
    int compositions = 0, replacements = 0;
    bool isFocusable() const override { return true; }
    oneui::TextInputState textInputState() const override { return {{}, 0, 0, true, false, this}; }
    oneui::Rect textInputCaretRect() const override { return {12, 12, 1, 24}; }
    bool onTextCommitted(const std::wstring& value) override { committed += value; return true; }
    void setTextComposition(std::wstring, std::size_t) override { ++compositions; }
    bool replaceTextRange(std::size_t, std::size_t, const std::wstring&) override { ++replacements; return true; }
    void paint(oneui::Canvas& canvas) override { canvas.fillRect(frame(), {240, 240, 240, 255}, 0); }
};

int main() {
    int failures = 0;
    auto check = [&](bool ok, const char* name) { if (!ok) { std::cerr << name << '\n'; ++failures; } };
    oneui::WindowOptions options;
    options.visible = false;
    options.width = 320;
    options.height = 200;
    auto window = oneui::Window::create(options);
    window->initialize();
    auto* desktop = dynamic_cast<oneui::platform::DesktopWindow*>(window.get());
    if (!desktop) return 1;
    auto remote = std::make_shared<RemoteEditor>();
    window->setContent(remote);
    window->requestFocus(remote.get());
    const auto before = desktop->render();
    desktop->composition(L"\u4e2d\u6587", 1);
    const auto during = desktop->render();
    check(before != during, "Remote-editor preedit overlay is rendered");
    check(remote->committed.empty(), "Preedit is not committed remotely");
    desktop->text(L"\u4e2d\u6587");
    check(remote->committed == L"\u4e2d\u6587", "Remote IME commit is atomic");
    const auto compositions = remote->compositions;
    desktop->enterNativeModal();
    desktop->enterNativeModal();
    desktop->text(L"blocked");
    desktop->replaceText(0, 1, L"blocked");
    desktop->composition(L"blocked", 1);
    desktop->leaveNativeModal();
    desktop->replaceText(0, 1, L"still blocked");
    check(remote->committed == L"\u4e2d\u6587" && remote->replacements == 0 &&
          remote->compositions == compositions, "Nested native modals suppress direct IME callbacks");
    desktop->composition({}, 0);
    check(remote->compositions == compositions + 1, "Modal focus loss can cancel existing preedit");
    desktop->leaveNativeModal();
    desktop->replaceText(0, 1, L"allowed");
    check(remote->replacements == 1, "IME replacement resumes after the last modal closes");
    auto field = std::make_shared<oneui::TextField>();
    window->setContent(field);
    window->requestFocus(field.get());
    desktop->composition(L"x", 1);
    desktop->focus(false);
    check(field->text().empty(), "Focus loss cancels preedit without committing");
    window->post([&] { window.reset(); });
    check(desktop->run() == 0, "A callback may destroy its own window safely");
    check(!window, "Self-destroying callback ran");
    return failures ? 1 : 0;
}
