#include "oneui/controls/text_field.h"
#include "oneui/layout/dock_session.h"
#include <iostream>
#include <stdexcept>

using namespace oneui;
namespace {
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
void applied(DockSessionResult result) {
    require(result == DockSessionResult::Applied, "Dock operation rejected");
}
void retainedTransactions() {
    Application app({"org.oneui.tests.dock", L"Dock", QuitPolicy::Explicit});
    auto a = Window::create(L"Source", 480, 320), b = Window::create(L"Target", 480, 320);
    auto left = std::make_shared<Panel>(), right = std::make_shared<Panel>();
    a->setContent(left);
    b->setContent(right);
    a->initialize();
    b->initialize();
    app.addWindow(*a);
    app.addWindow(*b);
    DockSession session(app);
    auto first = session.attachHost(*a, left), second = session.attachHost(*b, right);
    require(first && second && !session.attachHost(*a, left), "Host exclusivity failed");
    auto editor = std::make_shared<TextField>();
    editor->setMultiline(true);
    editor->setText(L"Original editor\n保存编辑内容");
    editor->setSelectionRange(2, 7);
    auto owner = std::make_shared<int>(42);
    std::weak_ptr<int> lifetime = owner;
    DockPane pane{1, L"Document", editor, owner};
    std::vector<DockTransferPhase> phases;
    bool rejectPrepare = false, rejectCommit = false;
    pane.transfer = [&](Window &, Window &, DockTransferPhase phase) {
        phases.push_back(phase);
        require(session.movePane(1, {first}) == DockSessionResult::Busy,
                "Transfer allowed reentrant mutation");
        return !(phase == DockTransferPhase::Prepare && rejectPrepare) &&
               !(phase == DockTransferPhase::Commit && rejectCommit);
    };
    applied(session.addPane(std::move(pane), {first}));
    owner.reset();
    applied(session.addPane({2, L"Other", std::make_shared<TextField>()}, {first}));
    require(!lifetime.expired(), "Controller owner was not retained");
    require(session.addPane({3, L"Duplicate", editor}, {first}) == DockSessionResult::InvalidPane,
            "One widget was accepted under two pane IDs");
    auto token = session.beginDrag(1);
    require(session.cancelDrag(token), "Cancel failed");
    require(session.commitDrag(token, {second}) == DockSessionResult::StaleTransaction &&
                session.location(1).host == first,
            "Cancelled drag changed the tree");
    token = session.beginDrag(1);
    applied(session.selectPane(1));
    require(session.commitDrag(token, {second}) == DockSessionResult::StaleTransaction,
            "Intervening edit did not invalidate drag");
    rejectPrepare = true;
    require(session.movePane(1, {second}) == DockSessionResult::ResourceRejected,
            "Prepare rejection ignored");
    require(phases == std::vector<DockTransferPhase>{DockTransferPhase::Prepare,
                                                     DockTransferPhase::Rollback},
            "Prepare failure did not roll back resources");
    phases.clear();
    rejectPrepare = false;
    rejectCommit = true;
    require(session.movePane(1, {second}) == DockSessionResult::ResourceRejected &&
                session.location(1).host == first,
            "Commit failure did not restore source");
    require(editor->hasCompositionOwner() && phases.back() == DockTransferPhase::Rollback,
            "Rollback lost composition owner");
    rejectCommit = false;
    require(a->requestFocus(editor.get()), "Could not establish original editor focus");
    for (int i = 0; i < 100; ++i) {
        applied(session.movePane(1, {second}));
        applied(session.movePane(1, {first, 2, DockPosition::Right}));
        require(session.location(1).group != session.location(2).group, "Edge split failed");
        applied(session.movePane(1, {first, 2, DockPosition::Tab, 0}));
        require(session.location(1).index == 0, "Tab insertion order lost");
        require(session.content(1) == editor && !lifetime.expired(),
                "Transfer recreated widget or controller");
        require(editor->focused(), "Transfer lost editor focus");
        require(editor->text() == L"Original editor\n保存编辑内容" &&
                    editor->selectionStart() == 2 && editor->selectionEnd() == 7,
                "Transfer changed document or selection");
    }
    applied(session.removePane(1));
    require(lifetime.expired() && !editor->hasCompositionOwner(),
            "Removing pane retained its owner or stale parent callbacks");
    app.requestQuit();
}
void emptyDetachedWindow() {
    Application app({"org.oneui.tests.dockempty", L"Empty dock", QuitPolicy::Explicit});
    auto window = Window::create(L"Main", 480, 320);
    auto surface = std::make_shared<Panel>();
    window->setContent(surface);
    window->initialize();
    app.addWindow(*window);
    DockSession session(app);
    auto host = session.attachHost(*window, surface);
    auto editor = std::make_shared<TextField>();
    applied(session.addPane({1, L"Document", editor}, {host}));
    DockId detached = 0;
    applied(session.detachPane(1, {L"Temporary document", 400, 240}, &detached));
    applied(session.movePane(1, {host}));
    // Closing the empty window is deferred until the native drop stack unwinds.
    app.post([&] {
        require(!session.hostWindow(detached), "Empty detached window remained alive after merge");
        require(app.windows().size() == 1 && session.content(1) == editor,
                "Merge lost document or registry ownership");
        app.requestQuit();
    });
    app.run();
}
void detachedLifetime() {
    Application app({"org.oneui.tests.dockwindows", L"Dock windows", QuitPolicy::Explicit});
    auto window = Window::create(L"Main", 480, 320);
    auto surface = std::make_shared<Panel>();
    window->setContent(surface);
    window->initialize();
    app.addWindow(*window);
    DockSession session(app);
    auto main = session.attachHost(*window, surface);
    auto editor = std::make_shared<TextField>();
    editor->setText(L"Retained");
    applied(session.addPane({1, L"Editor", editor}, {main}));
    DockId detached = 0;
    applied(session.detachPane(1, WindowOptions{L"Detached test", 400, 240}, &detached));
    require(detached && session.hostWindow(detached) && app.windows().size() == 2,
            "Detached window not registered");
    session.hostWindow(detached)->close();
    app.post([&] {
        require(session.location(1).host == main && session.content(1) == editor,
                "Closing detached window lost original content");
        require(!session.hostWindow(detached), "Closed host still exposes a Window pointer");
        window->close();
        app.post([&] {
            require(session.location(1).host == 0 && session.content(1) == editor,
                    "Last host close destroyed orphan pane");
            app.requestQuit();
        });
    });
    require(app.run() == 0, "Dock lifecycle event loop failed");
}
void detachedTheme() {
    Application app({"org.oneui.tests.docktheme", L"Dock theme", QuitPolicy::Explicit});
    auto window = Window::create(L"Theme host", 480, 320);
    auto surface = std::make_shared<Panel>();
    window->setContent(surface);
    window->initialize();
    app.addWindow(*window);
    DockSession session(app);
    auto initial = std::make_shared<StyleSheet>();
    initial->addRulesFromCss("Input { color:#ffffff; background-color:#18221b; }");
    applied(session.setStyleSheet(initial));
    auto host = session.attachHost(*window, surface);
    require(surface->styleScope() == initial, "Future host did not inherit session theme");
    auto editor = std::make_shared<TextField>();
    editor->setText(L"Retain theme state");
    editor->setSelectionRange(2, 6);
    applied(session.addPane({1, L"Editor", editor}, {host}));
    DockId detached = 0;
    applied(session.detachPane(1, {L"Theme detached", 480, 320}, &detached));
    require(editor->styleScope() == initial, "Detached content lost initial theme");
    auto next = std::make_shared<StyleSheet>();
    next->addRulesFromCss("Input { color:#102218; background-color:#eeeeee; }");
    std::weak_ptr<StyleSheet> retained = next;
    bool guarded = false;
    editor->setOnStyleScopeChanged(
        [&] { guarded = session.removePane(1) == DockSessionResult::Busy; });
    applied(session.setStyleSheet(next));
    require(guarded && editor->styleScope() == next && surface->styleScope() == next,
            "Existing host theme or reentrancy guard failed");
    require(session.content(1) == editor && editor->text() == L"Retain theme state" &&
                editor->selectionStart() == 2 && editor->selectionEnd() == 6,
            "Theme refresh replaced content or selection");
    editor->setOnStyleScopeChanged({});
    next.reset();
    require(!retained.expired(), "Session did not retain sheet");
    applied(session.movePane(1, {host}));
    require(editor->styleScope() == retained.lock(), "Merge restored stale theme");
    require(session.setStyleSheet({}) == DockSessionResult::Failed, "Null sheet accepted");
    app.requestQuit();
}
} // namespace
int main() {
    try {
        retainedTransactions();
        detachedLifetime();
        emptyDetachedWindow();
        detachedTheme();
        std::cout << "Dock identity, rollback, and window lifetime tests passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
