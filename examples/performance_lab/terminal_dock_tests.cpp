#include "terminal/bridge.hpp"
#include <oneui/controls/terminal_view.h>
#include <oneui/layout/dock_session.h>
#include <oneui/native_interop.h>
#include <stdexcept>

void rustTerminalDockTransfers(oneui::Application &app) {
    using namespace oneui;
    auto require = [](bool ok, const char *message) {
        if (!ok)
            throw std::runtime_error(message);
    };
    auto first = Window::create(L"Rust terminal host A", 800, 600);
    auto second = Window::create(L"Rust terminal host B", 800, 600);
    auto left = std::make_shared<Panel>(), right = std::make_shared<Panel>();
    first->setContent(left);
    second->setContent(right);
    app.addWindow(*first);
    app.addWindow(*second);
    first->initialize();
    second->initialize();
    DockSession dock(app);
    auto a = dock.attachHost(*first, left), b = dock.attachHost(*second, right);
    auto owner = std::shared_ptr<void>(terminal_demo_create_v1(), terminal_demo_destroy_v1);
    require(bool(owner), "Rust owner unavailable for dock transfer");
    std::weak_ptr<void> life = owner;
    auto terminal = std::dynamic_pointer_cast<TerminalView>(
        retainNativeWidget(terminal_demo_widget_v1(owner.get(), 0)));
    require(bool(terminal), "Rust terminal unavailable for dock transfer");
    bool reject = false;
    DockPane pane{1, L"Rust session", terminal, owner};
    pane.transfer = [&](Window &, Window &, DockTransferPhase phase) {
        return !reject || phase != DockTransferPhase::Commit;
    };
    require(dock.addPane(std::move(pane), {a}) == DockSessionResult::Applied,
            "Rust pane registration failed");
    owner.reset();
    first->prepareLayoutSnapshot();
    second->prepareLayoutSnapshot();
    terminal->onTextInputText(L"SESSION-IDENTITY-🌿");
    terminal->selectAll();
    const auto selection = terminal->selectedText();
    require(selection.find(L"SESSION-IDENTITY") != std::wstring::npos,
            "Session text missing before move");
    for (int i = 0; i < 30; ++i) {
        auto target = i % 2 ? a : b;
        require(dock.movePane(1, {target}) == DockSessionResult::Applied,
                "Rust cross-window move failed");
        first->prepareLayoutSnapshot();
        second->prepareLayoutSnapshot();
        require(dock.content(1) == terminal && !life.expired(),
                "Move replaced terminal or released Rust owner");
        require(terminal->selectedText() == selection,
                "Cross-window move lost terminal selection or contents");
    }
    const auto before = dock.location(1).host;
    reject = true;
    require(dock.movePane(1, {before == a ? b : a}) == DockSessionResult::ResourceRejected,
            "Rejected resource commit did not fail");
    require(dock.location(1).host == before && dock.content(1) == terminal && !life.expired(),
            "Resource rejection did not roll back owner and host");
    reject = false;
    auto token = dock.beginDrag(1);
    require(dock.cancelDrag(token), "Rust pane drag cancellation failed");
    require(dock.location(1).host == before, "Cancellation changed Rust pane host");
    DockId floating = 0;
    require(dock.detachPane(1, {L"Rust floating session", 800, 600}, &floating) ==
                    DockSessionResult::Applied &&
                floating,
            "Rust pane detach failed");
    dock.hostWindow(floating)->prepareLayoutSnapshot();
    require(dock.content(1) == terminal && !life.expired(), "Detached window lost Rust owner");
    require(dock.movePane(1, {a}) == DockSessionResult::Applied, "Rust floating merge failed");
    terminal->onTextInputText(L"AFTER-MERGE");
    terminal->selectAll();
    require(terminal->selectedText().find(L"AFTER-MERGE") != std::wstring::npos,
            "Rust callbacks stopped after merge");
    require(dock.removePane(1) == DockSessionResult::Applied && life.expired(),
            "Rust owner leaked after final pane removal");
    require(!terminal->hasCompositionOwner(), "Rust callback owner survived final removal");
    terminal->onTextInput(L'X'); // C++ widget can outlive the removed Rust controller safely.
    first->close();
    second->close();
}
