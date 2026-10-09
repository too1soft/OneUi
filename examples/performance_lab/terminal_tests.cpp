#include "terminal/bridge.hpp"
#include "terminal/workspace.hpp"
#include <chrono>
#include <iostream>
#include <oneui/layout/split_view.h>
#include <oneui/native_interop.h>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#endif

using namespace oneui;
using lab::terminal::Workspace;
void rustTerminalDockTransfers(oneui::Application &);
namespace {
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
void interactions(Workspace &work, Window &window) {
    auto &vm = work.model();
    auto &mount = work.mount();
    auto &sessions = work.sessions();
    auto first = sessions.terminals()[0], second = sessions.secondary()[0];
    auto input = sessions.input();
    vm.commandVisible.set(true);
    mount.flush();
    window.prepareLayoutSnapshot();
    require(first->frame().width > 400 && first->frame().height > 80, "Terminal layout collapsed");
#ifdef _WIN32
    require(window.requestFocus(first.get(), false), "Cannot focus Rust terminal");
    auto handle = static_cast<HWND>(window.nativeHandle());
    SendMessageW(handle, WM_CHAR, L'中', 0);
    SendMessageW(handle, WM_CHAR, L'A', 0);
#else
    first->onTextInputText(L"中A🌿"); // Native input-method acceptance is tracked separately.
#endif
    first->selectAll();
    const auto original = first->selectedText();
    require(original.find(L"中A") != std::wstring::npos, "Rust input callback lost text");
    input->setText(L"输入状态保留 🌿");
    input->setSelectionRange(1, 4);
    vm.commandVisible.set(true);
    mount.flush();
    window.prepareLayoutSnapshot();
    require(window.requestFocus(input.get(), true), "Cannot focus command input");
    input->setTextComposition(L"中文", 1);
    work.theme(false, false);
    mount.flush();
    require(input->focused() && input->hasTextComposition(),
            "Theme change discarded focus/composition state");
    input->setTextComposition({}, 0);
    input->setSelectionRange(1, 4);
#ifdef _WIN32
    auto tabs = std::dynamic_pointer_cast<Tabs>(mount.find("tabs"));
    window.prepareLayoutSnapshot();
    auto tab = tabs->itemFrame(1);
    const auto click = MAKELPARAM(int(tab.x + 30), int(tab.y + tab.height / 2));
    SendMessageW(handle, WM_LBUTTONDOWN, MK_LBUTTON, click);
    SendMessageW(handle, WM_LBUTTONUP, 0, click);
    require(sessions.selected() == 1, "Native tab click did not select stable session");
    SendMessageW(handle, WM_CHAR, L'B', 0);
    sessions.terminals()[1]->selectAll();
    require(sessions.terminals()[1]->selectedText().find(L"> B") != std::wstring::npos,
            "Native tab selection did not route keyboard input into the active terminal");
    sessions.open(0);
#endif
    const auto diagnostics = mount.diagnostics();
    const auto styleCount = mount.styles()->size();
    for (int i = 0; i < 100; ++i) {
        sessions.open(1);
        work.theme(i % 2, false);
        sessions.open(0);
        sessions.close(0);
        vm.restore.execute();
        mount.flush();
        window.prepareLayoutSnapshot();
    }
    require(sessions.terminals()[0] == first, "Reopening replaced terminal identity");
    require(first->selectedText() == original, "Reopening or theme changed terminal state");
    require(input->text() == L"输入状态保留 🌿" && input->selectionStart() == 1 &&
                input->selectionEnd() == 4,
            "Command input state lost");
    require(mount.diagnostics().subscriptions == diagnostics.subscriptions &&
                mount.styles()->size() == styleCount,
            "Bindings or styles grew across cycles");
    sessions.close(1);
    sessions.close(0);
    require(vm.empty.get(), "Empty state missing");
    vm.restore.execute();
    mount.flush();
    require(!vm.empty.get(), "Restore did not reopen session");
    sessions.open(2);
    sessions.open(3);
    require(vm.tabs.get().size() == 4, "Four mock sessions missing");
    vm.closeSession(L"build");
    require(sessions.selected() == 3 && vm.tabs.get().size() == 3,
            "Closing another tab changed selection");
    vm.connectionQuery.set(L"postgres");
    require(sessions.connections()->items().size() == 1, "Connection search broken");
    vm.connectionQuery.set(L"");
    require(sessions.connections()->items().size() == 4, "Connection search reset broken");
    sessions.open(0);
    input->setText(L"help");
    vm.send.execute();
    first->selectAll();
    require(input->text().empty() &&
                first->selectedText().find(L"No system shell") != std::wstring::npos,
            "Local command result missing");
    vm.newFolder.execute();
    require(work.files().table()->selectedIndex() == 0 && vm.removeFile.canExecute.get(),
            "New folder selection missing");
    vm.fileQuery.set(L"new-folder");
    require(work.files().table()->rows().size() == 1, "File filter broken");
    vm.removeFile.execute();
    require(work.files().table()->rows().empty(), "Mock deletion failed");
    vm.fileQuery.set(L"");
    require(work.files().table()->rows().size() == 18, "Mock directory contents changed");
    work.files().table()->setSelectedIndices({0, 1});
    vm.fileQuery.set(L"config");
    require(work.files().table()->selectedIndices().size() == 1,
            "Filtered selection not retained by name");
    vm.fileQuery.set(L"");
    vm.upload.execute();
    require(work.files().table()->rows().size() == 19, "Mock upload missing");
    vm.removeFile.execute();
    require(work.files().table()->rows().size() == 18, "Uploaded item deletion failed");
    vm.toggleSplit.execute();
    mount.flush();
    window.prepareLayoutSnapshot();
    require(vm.pane0.get() && second->frame().width > 150, "Secondary terminal missing");
    second->onTextInputText(L"PANE-RETAINED");
    vm.toggleSplit.execute();
    vm.toggleSplit.execute();
    mount.flush();
    second->selectAll();
    require(second->selectedText().find(L"PANE-RETAINED") != std::wstring::npos,
            "Split hid or replaced session state");
    vm.toggleSplit.execute();
    vm.terminalQuery.set(L"healthy");
    vm.searchNext.execute();
    require(first->hasSelection() && vm.searchResult.get() != L"无匹配", "Terminal search missing");
    vm.showProcesses.execute();
    require(vm.processVisible.get() && !vm.resourceVisible.get(), "Process tab routing failed");
    vm.monitorTab.set(L"connection");
    require(vm.connectionVisible.get(), "Connection details missing");
    vm.showLogs.execute();
    mount.flush();
    require(vm.logVisible.get() && !vm.workspaceVisible.get(), "Log region missing");
    vm.appendLog.execute();
    vm.clearLog.execute();
    vm.showTerminal.execute();
    vm.collapseFiles.execute();
    mount.flush();
    const auto ratio = vm.fileRatio.get();
    require(vm.filesCollapsed.get(), "File collapse failed");
    vm.collapseFiles.execute();
    mount.flush();
    require(vm.fileRatio.get() == ratio, "File collapse discarded split ratio");
}
void narrowLayout(Workspace &work, Window &window) {
    auto &vm = work.model();
    auto &mount = work.mount();
    window.prepareLayoutSnapshot();
    mount.flush();
    window.prepareLayoutSnapshot();
    require(vm.narrow.get(), "Embedded page width did not trigger adaptive layout");
    vm.showFiles.execute();
    mount.flush();
    window.prepareLayoutSnapshot();
    require(vm.filesVisible.get() && !vm.terminalVisible.get() &&
                work.files().table()->frame().width > 480,
            "Narrow files page collapsed");
    vm.showMonitor.execute();
    mount.flush();
    require(vm.monitorVisible.get() && !vm.mainVisible.get(), "Narrow monitor page missing");
    vm.showTerminal.execute();
    mount.flush();
    require(vm.terminalVisible.get() && !vm.monitorVisible.get(), "Narrow terminal page missing");
}
void ownerLifetime() {
    std::shared_ptr<TerminalView> retained;
    std::shared_ptr<Widget> host;
    std::weak_ptr<void> life;
    {
        ui::Mount mount;
        auto owner = std::shared_ptr<void>(terminal_demo_create_v1(), terminal_demo_destroy_v1);
        require(bool(owner), "Rust owner creation failed");
        life = owner;
        retained = std::dynamic_pointer_cast<TerminalView>(
            retainNativeWidget(terminal_demo_widget_v1(owner.get(), 0)));
        mount.registerNative("terminal", retained, owner);
        host = mount.nativeHost("terminal").widget;
    }
    require(!life.expired(), "Native host released Rust owner too early");
    retained->onTextInput(L'B');
    host.reset();
    require(life.expired() && !retained->hasCompositionOwner(), "Native host leaked Rust owner");
    retained->onTextInput(L'C'); // Rust Drop removed callbacks even with C++ pixels still retained.
}
} // namespace
int main() {
    try {
        Application app({"org.oneui.tests.lab-terminal", L"OneUI terminal regression"});
        auto window = Window::create(L"OneUI terminal regression", 1360, 900);
        app.addWindow(*window, true);
        Workspace work(*window, app.dispatcher());
        window->setContent(work.root());
        window->initialize();
        window->show();
        work.activate();
        std::thread timer;
        std::string failure;
        std::vector<TimeSeriesChartSeries> sampled;
        std::uint64_t idlePaints = 0;
        bool passed = false;
        auto guard = [&](auto action) {
            try {
                action();
            } catch (const std::exception &error) {
                failure = error.what();
                app.requestQuit();
            }
        };
        app.post([&] {
            guard([&] {
                interactions(work, *window);
                ownerLifetime();
                rustTerminalDockTransfers(app);
                window->setClientSize({640, 720});
                timer = std::thread([&] {
                    // X11/Wayland resize requests complete after compositor configure events.
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    app.post([&] {
                        guard([&] {
                            narrowLayout(work, *window);
                            window->setClientSize({1360, 900});
                        });
                    });
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    app.post([&] {
                        guard([&] {
                            window->prepareLayoutSnapshot();
                            require(!work.model().narrow.get(),
                                    "Native width restore did not reach embedded page");
                            work.model().restore.execute();
                            work.mount().flush();
                            sampled = work.model().cpuSeries.get();
                            work.model().toggleSampling.execute();
                        });
                    });
                    std::this_thread::sleep_for(std::chrono::milliseconds(1400));
                    app.post([&] {
                        guard([&] {
                            require(sampled != work.model().cpuSeries.get(),
                                    "Explicit sampling did not update chart");
                            sampled = work.model().cpuSeries.get();
                            work.files().table()->setSelectedIndices({0});
                            work.model().download.execute();
                            work.deactivate();
                            window->setContent(std::make_shared<Panel>());
                            require(!work.model().sampling.get() &&
                                        !work.model().transferring.get(),
                                    "Leaving page did not stop activity");
                        });
                    });
                    std::this_thread::sleep_for(std::chrono::milliseconds(450));
                    app.post([&] { idlePaints = window->rendererInfo().paints; });
                    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
                    app.post([&] {
                        guard([&] {
                            require(sampled == work.model().cpuSeries.get(),
                                    "Hidden page continued sampling");
                            require(idlePaints > 0 && window->rendererInfo().paints == idlePaints,
                                    "Hidden page caused continuous redraw");
                            window->setContent(work.root());
                            work.activate();
                            work.mount().flush();
                            window->prepareLayoutSnapshot();
                            require(!work.model().sampling.get(),
                                    "Returning restarted sampling without request");
                            passed = true;
                            app.requestQuit();
                        });
                    });
                });
            });
        });
        app.run();
        if (timer.joinable())
            timer.join();
        work.deactivate();
        window->setContent({});
        if (!passed)
            throw std::runtime_error(failure.empty() ? "Terminal regression did not finish"
                                                     : failure);
        std::cout << "PASS: four Rust sessions, 100 reopen/theme cycles, focus/composition state, "
                     "30 cross-window Rust owner transfers, rollback/cancel/detach/merge, "
                     "state/owner retention, "
                     "file actions/filter/selection, split/search/log/adaptive layout, explicit "
                     "sampling, hidden page cancellation and idle redraw=0.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
