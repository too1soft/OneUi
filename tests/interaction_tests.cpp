#include "support/test_ui_context.h"
#include "oneui/controls/text_field.h"
#include "oneui/layout/overlay_host.h"
#include "internal/unicode.h"
#include <iostream>

static void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
static oneui::KeyEvent primary(char key) {
    oneui::KeyEvent event; event.virtualKey = key;
#ifdef __APPLE__
    event.win = true;
#else
    event.control = true;
#endif
    return event;
}
int main() {
    using namespace oneui;
    try {
        auto view = std::make_shared<View>();
        auto field = std::make_shared<TextField>();
        view->add(field); view->requestFocus(field.get());
        test_support::TestUiContext ui(view);
        field->setFrame({0, 0, 300, 36}); field->setClipboard(ui.clipboard);
        int leaf = 0, parent = 0, window = 0, raw = 0;
        auto parentToken = view->commands().registerCommand("sample", [&] { ++parent; }, {}, KeyChord{"k", KeyModifierPrimary});
        auto windowToken = ui.windowCommands.registerCommand("sample", [&] { ++window; }, {}, KeyChord{"k", KeyModifierPrimary});
        auto token = field->commands().registerCommand("sample", [&] { ++leaf; }, [] { return false; }, KeyChord{"k", KeyModifierPrimary});
        ui.rawKey = [&](const KeyEvent&) { ++raw; return false; };
        require(ui.key(primary('K')) && raw == 1 && leaf == 0 && parent == 0 && window == 0, "disabled command blocks ancestors after raw callback");
        token.reset(); require(ui.key(primary('K')) && parent == 1 && window == 0, "unregister restores parent route");
        bool duplicate = false;
        try { auto ignored = view->commands().registerCommand("sample", [] {}); } catch (const std::invalid_argument&) { duplicate = true; }
        require(duplicate, "duplicate ID rejected");
        duplicate = false;
        try { auto ignored = view->commands().registerCommand("other", [] {}, {}, KeyChord{"K", KeyModifierPrimary}); } catch (const std::invalid_argument&) { duplicate = true; }
        require(duplicate, "duplicate normalized shortcut rejected");
        int functionKey = 0;
        auto functionToken = view->commands().registerCommand("help", [&] { ++functionKey; }, {}, KeyChord{"F1", 0});
        KeyEvent f1; f1.virtualKey = 0x70;
        require(ui.key(f1) && functionKey == 1 && logicalKeyName(f1) == "f1", "F1 is not misread as lowercase p");
        ui.rawKey = [](const KeyEvent&) { return true; };
        ui.key(primary('K')); require(parent == 1, "raw callback can consume before commands");
        ui.rawKey = {};
        field->setTextComposition(L"预编辑", 1);
        ui.key(primary('K')); require(parent == 1 && field->text().empty(), "composition suppresses application shortcuts");
        field->onTextCommitted(L"中文");
        require(field->text() == L"中文" && field->undo() && field->text().empty(), "composition commit is one transaction");
        field->setText(unicode::fromUtf8(u8"A👨‍👩‍👧‍👦é"));
        ui.replay({KeyEvent{Key::Backspace}, KeyEvent{Key::Backspace}});
        require(field->text() == L"A", "replay deletes combining and ZWJ graphemes atomically");
        ui.key(primary('A')); ui.key(primary('C'));
        require(ui.clipboard->text() == L"A", "built-in commands share clipboard");
        field->setPasswordMode(true);
        require(field->textInputState().text.empty(), "password surrounding text hidden");
        require(queryCommand(view, nullptr, "edit.copy") == CommandResult::Disabled, "password command disabled");
        auto canvas = ui.paint();
        require(canvas.textBlocks.at(0).text == L"*", "password is masked before canvas");
        const auto caretCount = [](const auto& painted) {
            return std::count_if(painted.fillRects.begin(), painted.fillRects.end(), [](const auto& call) { return call.rect.width == 1 && call.rect.height == 14; });
        };
        require(caretCount(canvas) == 1, "caret initially visible");
        ui.advance(531); require(caretCount(ui.paint()) == 0, "deterministic blink off");
        ui.advance(530); require(caretCount(ui.paint()) == 1, "deterministic blink on");
        auto host = std::make_shared<OverlayHost>(); host->setContent(view);
        host->setFrame({0, 0, 500, 300});
        auto popup = std::make_shared<TextField>();
        host->addOverlay(popup, OverlayOptions::modal());
        require(host->focusFirstLeaf() && host->activeFocusChild() == popup, "modal focusFirstLeaf never enters background");
        require(!dispatchCommandKey(host, &ui.windowCommands, primary('K')) && parent == 1 && window == 0, "modal scope isolates window and background commands");
        host->setTextComposition(L"拼音", 1);
        require(popup->hasTextComposition() && !field->hasTextComposition(), "overlay preedit routes to focused leaf");
        require(host->textInputState().identity == popup.get(), "overlay surrounding text follows active focus");
        host->onTextCommitted(L"中文"); require(popup->text() == L"中文", "overlay atomic commit");
        popup->setVisible(false);
        require(!host->onTextCommitted(L"迟到") && !popup->hasTextComposition(), "hidden editor rejects routed commit");
        auto scope = std::make_unique<CommandScope>();
        Subscription self;
        self = scope->registerCommand("self", [&] { self.reset(); });
        require(scope->execute("self") == CommandResult::Executed && scope->query("self") == CommandResult::NotFound, "callback may unregister itself");
        auto destruction = scope->registerCommand("destroy", [] {}, [&] { scope.reset(); return true; });
        require(scope->execute("destroy") == CommandResult::Disabled, "predicate may destroy scope without calling handler");
        State<std::wstring> state(L"before");
        auto doomed = std::make_unique<TextField>(); doomed->bindText(state);
        auto destroyWidget = state.subscribeScoped([&](const auto&) { doomed.reset(); });
        doomed->setText(L"after");
        require(!doomed && state.get() == L"after", "state notification may destroy editing widget");
        int tasks = 0;
        ui.post([&] { ++tasks; ui.close(); }); ui.post([&] { ++tasks; }); ui.drain();
        require(tasks == 1 && !ui.post([] {}), "close cancels remaining deterministic tasks");
        std::cout << "Interaction tests passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
