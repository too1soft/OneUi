#include <oneui/application.h>

int example_application() {
    // Application must outlive the caller-owned windows.
    oneui::Application app({"org.example.oneui", L"示例"});
    if (!app.isPrimaryInstance())
        return 0;
    auto main = oneui::Window::create(L"主窗口", 800, 600);
    auto observer = oneui::Window::create(L"观察窗口", 480, 320);
    app.addWindow(*main, true);
    app.addWindow(*observer);
    app.setOnActivation([](const oneui::ApplicationActivation &event) {
        // Route event.notificationId / actionId / parameter on the UI thread.
        (void)event;
    });
    app.setCloseGuard(*main, [](std::function<void(bool)> answer) {
        // Retain answer for an asynchronous confirmation; call it exactly once.
        answer(true);
    });
    app.setQuitPolicy(oneui::QuitPolicy::LastWindowClosed);
    main->show();
    observer->show();
    return app.run();
}
