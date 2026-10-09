#include "oneui/application.h"
#include "oneui/controls/text_field.h"
#include "oneui/layout/stack.h"
#include "oneui/modal_session.h"
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif

namespace {
using namespace oneui;
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
void modalFocusAndOwnership() {
    Application app({"org.oneui.tests.modal-focus", L"Modal focus", QuitPolicy::Explicit});
    auto parent = Window::create(L"Parent", 600, 400);
    auto a = Window::create(L"Modal A", 300, 200), b = Window::create(L"Modal B", 300, 200);
    app.addWindow(*parent, true);
    app.addWindow(*a);
    app.addWindow(*b);
    auto root = std::make_shared<Stack>();
    auto first = std::make_shared<TextField>(), second = std::make_shared<TextField>();
    root->add(first);
    root->add(second);
    first->setPreferredSize({400, 40});
    second->setPreferredSize({400, 40});
    parent->setContent(root);
    parent->initialize();
    parent->show();
    parent->prepareLayoutSnapshot();
    require(parent->requestFocus(second.get()), "Could not select the original input");
    second->setText(L"Retain selection");
    second->setSelectionRange(1, 4);
    int completed = 0;
    auto one = ModalSession::show(*a, *parent, [&](int) { ++completed; });
    require(bool(one), "Modal A rejected");
    require(!ModalSession::show(*a, *parent, {}), "Same child acquired two modal sessions");
    require(!ModalSession::show(*parent, *a, {}), "Modal ownership cycle accepted");
    auto two = ModalSession::show(*b, *parent, [&](int) { ++completed; });
    require(bool(two), "Sibling modal rejected");
    one.reset();
#ifdef _WIN32
    require(!IsWindowEnabled(static_cast<HWND>(parent->nativeHandle())),
            "First sibling released parent input too early");
    require(GetWindowLongPtrW(static_cast<HWND>(a->nativeHandle()), GWLP_HWNDPARENT) == 0,
            "Destroyed modal session retained its temporary native owner");
#endif
    two.reset();
    require(completed == 0 && !a->closed() && !b->closed(),
            "Session destruction closed caller-owned windows or delivered a result");
    require(second->focused() && second->hasSelection() && second->selectedText() == L"eta",
            "Final sibling did not restore the original focused input/selection");
#ifdef _WIN32
    require(IsWindowEnabled(static_cast<HWND>(parent->nativeHandle())),
            "Final sibling did not release parent input");
#endif
    // A stale bookmark is ignored when its widget is removed while a modal is open.
    one = ModalSession::show(*a, *parent, {});
    root->clearChildren();
    second.reset();
    first.reset();
    one.reset();
    require(!parent->closed(), "Removed focus target invalidated the modal parent");
    app.requestQuit();
}
void quitRemovalReentrancy() {
    auto app = std::make_unique<Application>(
        ApplicationOptions{"org.oneui.tests.remove-reentrant", L"Removal", QuitPolicy::Explicit});
    auto first = Window::create(L"First", 400, 300), last = Window::create(L"Last", 400, 300);
    first->initialize();
    last->initialize();
    app->addWindow(*first);
    app->addWindow(*last);
    std::function<void(bool)> answer;
    app->setCloseGuard(*last, [&](auto reply) { answer = std::move(reply); });
    auto closing = first->observeClosed([&] { app.reset(); });
    app->requestQuit();
    require(bool(answer) && bool(app), "Quit preflight did not wait for last window");
    auto raw = app.get();
    require(raw->removeWindow(*last), "Reentrant removal did not report removed window");
    require(!app && first->closed() && !last->closed(),
            "Removal closed the caller-owned detached window");
    answer(false); // An answer retained past Application destruction must be harmless.
    last->close();
}
} // namespace
void applicationModalRegressions() {
    modalFocusAndOwnership();
    quitRemovalReentrancy();
}
