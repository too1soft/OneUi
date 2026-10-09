#include "connection_workspace.hpp"
#include <iostream>
#include <oneui/application.h>
#include <oneui/layout/panel.h>

using namespace oneui;
namespace {
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
} // namespace
int main(int argc, char **argv) {
    try {
        Application app(
            {"org.oneui.tests.workspace", L"Workspace lifecycle", QuitPolicy::Explicit});
        auto window = Window::create(L"Workspace lifecycle", 1100, 800);
        require(bool(window), "Window creation failed");
        app.addWindow(*window, true);
        connection_demo::Workspace work(
            *window, [] {}, argc > 1 && std::string(argv[1]) == "--template");
        window->setContent(work.page.root.widget);
        auto identity = work.page.fields.at("name").get();
        const auto original = work.flow.find(L"20")->config;
        std::string failure;
        bool passed = false;
        std::uint64_t idlePaints = 0;
        auto guard = [&](auto action) {
            if (!failure.empty())
                return;
            try {
                action();
            } catch (const std::exception &error) {
                failure = error.what();
            }
        };
        window->initialize();
        window->show();
        app.post([&] {
            guard([&] {
                work.flow.open(L"20");
                work.vm.name.set(L"cancelled draft");
                work.vm.save.execute();
                require(work.vm.save.running.get(), "Save did not start");
                work.suspend();
                require(!work.vm.save.running.get(), "Hidden page kept save running");
                work.resume();
                work.vm.name.set(L"retained draft");
                work.vm.save.execute();
            });
        });
        auto post = app.dispatcher();
        std::thread timer([&] {
            auto after = [&](int milliseconds, std::function<void()> action) {
                std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
                post([&, action = std::move(action)] { guard(action); });
            };
            after(160, [&] {
                require(work.vm.save.running.get(), "Old completion cancelled a newer save");
                work.suspend();
                window->setContent(std::make_shared<Panel>());
            });
            after(450, [&] { idlePaints = window->rendererInfo().paints; });
            after(900, [&] {
                require(idlePaints > 0 && window->rendererInfo().paints == idlePaints,
                        "Hidden workspace continued redrawing");
                require(work.flow.find(L"20")->config == original, "Cancelled save was committed");
                require(work.vm.name.get() == L"retained draft", "Suspension lost draft");
                require(!work.vm.save.running.get(), "Cancelled task completed into hidden page");
                window->setContent(work.page.root.widget);
                work.resume();
                require(work.page.fields.at("name").get() == identity, "Return rebuilt controls");
                work.vm.save.execute();
            });
            after(900, [&] {
                require(!work.vm.save.running.get(), "Save failed after resume");
                require(work.flow.find(L"20")->config.name == L"retained draft",
                        "Fresh sender did not deliver saved draft");
                work.flow.gallery.set(true);
                work.mount.flush();
                work.samples->loading.set(true);
                work.suspend();
                work.resume();
                require(!work.samples->loading.get(), "Returning restarted loading animation");
                // Exercise command and pending UI callback cancellation across generations.
                work.flow.gallery.set(false);
                work.flow.open(L"20");
                for (int i = 0; i < 30; ++i) {
                    work.vm.note.set(std::to_wstring(i));
                    work.vm.save.execute();
                    work.suspend();
                    work.resume();
                }
                work.close();
                work.resume(); // close is final
                work.vm.save.execute();
                require(!work.vm.save.running.get(), "Closed workspace accepted a new task");
                window->setContent({});
                passed = true;
            });
            // Give cooperative workers time to observe cancellation before shutdown.
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            post([&] { app.requestQuit(); });
        });
        app.run();
        timer.join();
        if (!failure.empty() || !passed)
            throw std::runtime_error(failure.empty() ? "Test did not finish" : failure);
        std::cout << "PASS: cancellation, draft/control retention, fresh save, 30 rapid returns, "
                     "hidden redraw=0, loading stopped, final close.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
