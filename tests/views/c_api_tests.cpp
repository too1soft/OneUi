#include "capi/oneui_c_api_view_internal.h"
#include "fixture.h"
#include "oneui/layout/overlay_host.h"
#include "oneui/controls/table.h"
#include <thread>
using namespace viewtest;
namespace {
struct Window final : oneui::Window {
    oneui::Widget *focused = nullptr;
    bool requestFocus(oneui::Widget *widget, bool) override {
        if (!widget || widget->disabled()) return false;
        focused = widget; return true;
    }
    std::shared_ptr<oneui::Widget> content;
    std::vector<std::function<void()>> queued;
    void setContent(std::shared_ptr<oneui::Widget> v) override { content = std::move(v); }
    void initialize() override {}
    void show() override {}
    int run() override {
        auto q = std::move(queued);
        queued.clear();
        for (auto &f : q)
            f();
        return 0;
    }
    void close() override {
        publishClosed();
        content.reset();
    }
    void minimize() override {}
    void requestRedraw() override {}
    bool post(std::function<void()> f) override {
        if (closed())
            return false;
        queued.push_back(std::move(f));
        return true;
    }
    void requestAnimationFrame(std::function<void(double)>) override {}
    oneui::NativeWindowHandle nativeHandle() const override { return {}; }
    oneui::Size clientSize() const override { return {800, 600}; }
    oneui::Size clientPixelSize() const override { return clientSize(); }
    float dpiScale() const override { return 1; }
    void setTitle(std::wstring) override {}
    void setFullscreen(bool) override {}
    bool isFullscreen() const override { return false; }
    void setBorderless(bool) override {}
    void toggleMaximize() override {}
};
struct ApiCase : Case {
    OneUiWindow window;
    OneUiViewMount *mount = nullptr;
    OneUiView *view = nullptr;
    oneview::Bytes bytes;
    explicit ApiCase(const char *path) : Case(path), bytes(oneview::writeImage(image)) {
        window.window = std::make_unique<Window>();
        expect(oneui_view_mount_create(&window, nullptr, &mount) == 0, "mount create");
    }
    void start() {
        expect(oneui_view_instantiate(mount, bytes.data(), bytes.size(), &vm.table, &vm, &view) ==
                   0,
               "instantiate");
    }
    ~ApiCase() {
        oneui_view_destroy(view);
        oneui_view_mount_destroy(mount);
    }
};
} // namespace
int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    try {
        expect(oneui_utf8_abi_version() == 45 && oneui_view_protocol_version() == 1,
               "ABI and protocol versions");
        {
            ApiCase c(argv[1]);
            c.start();
            auto label = c.view->value->root->children[0]->element.widget;
            label->setAccessibleName(L"metrics-unique");
            label->setFrame({12, 24, 160, 28});
            OneUiViewElementMetrics metrics{};
            metrics.struct_size = sizeof(metrics);
            auto str = [](const char *text) { return OneUiUtf8String{text, std::char_traits<char>::length(text)}; };
            auto name = str("metrics-unique");
            expect(oneui_view_element_metrics(c.view, name, &metrics) == 0 &&
                metrics.x == 12 && metrics.y == 24 && metrics.width == 160 &&
                metrics.height == 28 && !metrics.scrollable, "read-only element metrics");
            int focused = -1;
            expect(oneui_view_element_focused(c.view, name, &focused) == 0 && !focused,
                "unfocused named element");
            expect(oneui_view_element_focused(c.view, str("missing"), &focused) == ONEUI_VIEW_NOT_FOUND && !focused,
                "focus missing name resets output");
            expect(oneui_view_element_focused(c.view, name, nullptr) == ONEUI_VIEW_INVALID_ARGUMENT,
                "focus requires output");
            int visible = -1;
            expect(oneui_view_element_visible(c.view, name, &visible) == 0 && visible,
                "named element visible");
            label->setVisible(false);
            expect(oneui_view_element_visible(c.view, name, &visible) == 0 && !visible,
                "named element hidden");
            label->setVisible(true);
            expect(oneui_view_element_visible(c.view, str("missing"), &visible) ==
                ONEUI_VIEW_NOT_FOUND && !visible, "visibility missing name resets output");
            expect(oneui_view_element_visible(c.view, name, nullptr) ==
                ONEUI_VIEW_INVALID_ARGUMENT, "visibility requires output");
            int visibilityForeign = 0;
            std::thread visibilityThread([&] {
                int result = -1;
                visibilityForeign = oneui_view_element_visible(c.view, name, &result);
            });
            visibilityThread.join();
            expect(visibilityForeign == ONEUI_VIEW_WRONG_THREAD, "visibility UI thread ownership");
            expect(oneui_view_element_metrics(c.view, str("missing"), &metrics) ==
                ONEUI_VIEW_NOT_FOUND, "metrics missing name");
            expect(oneui_view_element_metrics(c.view, str(""), &metrics) ==
                ONEUI_VIEW_INVALID_ARGUMENT, "metrics empty name");
            expect(oneui_view_element_metrics(c.view, name, nullptr) ==
                ONEUI_VIEW_INVALID_ARGUMENT, "metrics missing output");
            int foreign = 0;
            std::thread t([&] { foreign = oneui_view_element_metrics(c.view, name, &metrics); });
            t.join();
            expect(foreign == ONEUI_VIEW_WRONG_THREAD, "metrics UI thread ownership");
            auto other = c.view->value->root->children[1]->element.widget;
            other->setAccessibleName(L"focus-input");
            focused = 0;
            expect(oneui_view_mount_focus_element(c.mount, str("focus-input"), 1, &focused) == 0 && focused &&
                static_cast<Window*>(c.window.window.get())->focused == other.get(), "focus named native input");
            other->setDisabled(true);
            expect(oneui_view_mount_focus_element(c.mount, str("focus-input"), 1, &focused) == 0 && !focused,
                "focus reports native rejection");
            other->setDisabled(false);
            expect(oneui_view_mount_focus_element(c.mount, str("missing"), 1, &focused) == ONEUI_VIEW_NOT_FOUND,
                "focus rejects missing name");
            expect(oneui_view_mount_focus_element(c.mount, str(""), 1, &focused) == ONEUI_VIEW_INVALID_ARGUMENT &&
                oneui_view_mount_focus_element(c.mount, name, 1, nullptr) == ONEUI_VIEW_INVALID_ARGUMENT,
                "focus validates name and output");
            std::thread ft([&] { foreign = oneui_view_mount_focus_element(c.mount, name, 1, &focused); });
            ft.join(); expect(foreign == ONEUI_VIEW_WRONG_THREAD, "focus requires UI thread");
            { Callback callback(*c.view->value, Phase::Mutate);
              expect(oneui_view_mount_focus_element(c.mount, name, 1, &focused) == ONEUI_VIEW_REENTRANT,
                     "focus mutation is not permitted during VM callback"); }
            other->setAccessibleName(L"metrics-unique");
            expect(oneui_view_mount_focus_element(c.mount, name, 1, &focused) == ONEUI_VIEW_INVALID_ARGUMENT,
                "focus rejects ambiguous name");
            expect(oneui_view_element_metrics(c.view, name, &metrics) ==
                ONEUI_VIEW_INVALID_ARGUMENT, "metrics ambiguous name rejected");
            other->setAccessibleName(L"overlay-base-input");
            auto badge=std::make_shared<oneui::Label>();badge->setAccessibleName(L"overlay-badge");
            auto overlay=std::make_shared<oneui::TemplateOverlay>(c.view->value->root->element.widget,badge);
            c.view->value->root->element.widget=overlay;
            expect(oneui_view_element_metrics(c.view,str("overlay-badge"),&metrics)==0 &&
                oneui_view_mount_focus_element(c.mount,str("overlay-base-input"),1,&focused)==0 && focused,
                "named lookup includes native overlay base and foreground");
            other->onFocusChanged(true);
            expect(oneui_view_element_focused(c.view,str("overlay-base-input"),&focused)==0 && focused,
                "focus query reads native state");
            auto table=std::make_shared<oneui::Table>();
            table->setAccessibleName(L"metrics-table");
            table->setFrame({0,0,400,200}); table->setRowHeight(30);
            table->setRows(std::vector<std::vector<std::wstring>>(100,{L"row"}));
            table->setScrollOffset(180);
            c.view->value->root->element.widget=table;
            expect(oneui_view_element_metrics(c.view,str("metrics-table"),&metrics)==0 &&
                metrics.scrollable && metrics.scroll_y==180 && metrics.max_scroll_y>2500,
                "rich Table exposes actual scroll metrics");
            int focusForeign=0;
            std::thread focusReadThread([&] { focusForeign=oneui_view_element_focused(c.view,str("metrics-table"),&focused); });
            focusReadThread.join(); expect(focusForeign==ONEUI_VIEW_WRONG_THREAD,"focus query owns UI thread");
            oneui_view_disconnect(c.view);
            expect(oneui_view_element_focused(c.view, name, &focused) == ONEUI_VIEW_CLOSED,"closed focus query rejected");
            expect(oneui_view_element_visible(c.view, name, &visible) == ONEUI_VIEW_CLOSED,
                "visibility closed view rejected");
            expect(oneui_view_mount_focus_element(c.mount, name, 1, &focused) == ONEUI_VIEW_CLOSED,
                "focus rejects closed view");
            expect(oneui_view_element_metrics(c.view, name, &metrics) ==
                ONEUI_VIEW_CLOSED, "metrics closed view rejected");
        }
        {
            OneUiWindow window;
            window.window = std::make_unique<Window>();
            auto host = std::make_shared<oneui::OverlayHost>();
            auto destination = std::make_shared<oneui::Panel>();
            host->setContent(destination);
            window.rootContent = host;
            window.window->setContent(host);
            OneUiWidget handle;
            handle.widget = destination;
            OneUiViewMount *mount = nullptr;
            expect(oneui_view_mount_create(&window, &handle, &mount) == ONEUI_VIEW_OK,
                   "mount panel under overlay host content");
            oneui_view_mount_destroy(mount);
            handle.widget = std::make_shared<oneui::Panel>();
            expect(oneui_view_mount_create(&window, &handle, &mount) == ONEUI_VIEW_INVALID_ARGUMENT,
                   "reject panel outside this window");
            host->addOverlay(handle.widget);
            expect(oneui_view_mount_create(&window, &handle, &mount) == ONEUI_VIEW_OK,
                   "mount panel under overlay entry");
            oneui_view_mount_destroy(mount);
        }
        {
            auto path = std::filesystem::path(argv[1]).parent_path() / "ExternalTheme.oneview";
            ApiCase c(path.string().c_str());
            // Scoped declarations may reference application tokens that are absent
            // from the SDK default theme, including on the very first mount.
            const std::string applicationCss = ":root { --application-ink:#123456; }";
            expect(oneui_view_mount_set_styles(c.mount,
                       {applicationCss.data(), applicationCss.size()}, 0) == 0,
                   "application token stylesheet");
            c.start();
            expect(oneui_view_root(c.view) != nullptr,
                   "initial scoped CSS resolves application tokens");
        }
        {
            ApiCase c(argv[1]);
            c.start();
            expect(oneui_view_root(c.view) != nullptr, "borrowed root");
            OneUiViewNotifier *n = nullptr;
            expect(oneui_view_notifier(c.view, &n) == 0, "notifier acquire");
            int wrong = 0;
            std::thread worker([&] {
                oneui_view_notifier_retain(n);
                wrong = oneui_view_notifier_notify(n, c.title);
                oneui_view_notifier_release(n);
            });
            worker.join();
            expect(wrong == ONEUI_VIEW_WRONG_THREAD, "wrong thread");
            c.vm.text(c.title, "queued");
            expect(oneui_view_notifier_notify(n, c.title) == 0, "notify");
            c.window.window->run();
            auto label = c.view->value->root->children[0]->element.as<oneui::Label>();
            expect(label->text() == L"queued", "dispatcher flush");
            expect(oneui_view_disconnect(c.view) == 0, "disconnect");
            expect(oneui_view_root(c.view) == nullptr, "closed root");
            expect(oneui_view_notifier_notify(n, c.title) == ONEUI_VIEW_CLOSED, "late notifier");
            oneui_view_destroy(c.view);
            c.view = nullptr;
            expect(c.vm.retains == c.vm.releases, "destroy balanced");
            oneui_view_notifier_release(n);
            c.window.window->run();
        }
        {
            ApiCase c(argv[1]);
            c.start();
            c.vm.onInvoke = [&] {
                expect(oneui_view_flush(c.view) == ONEUI_VIEW_REENTRANT, "nested flush");
                oneui_view_destroy(c.view);
                c.view = nullptr;
                expect(c.vm.releases == 0, "self destroy defers release");
            };
            auto button = c.view->value->root->children[2]->element.as<oneui::Button>();
            button->setFrame({0, 0, 100, 30});
            auto click = [&] {
                button->onMouseDown({{10, 10}, oneui::MouseButton::Left});
                button->onMouseUp({{10, 10}, oneui::MouseButton::Left});
            };
            click();
            expect(c.vm.releases == 1, "self destroy release at return");
            click();
            expect(c.vm.invokes == 1, "stale widget event ignored");
        }
        {
            ApiCase c(argv[1]);
            c.start();
            c.vm.onGet = [&] {
                expect(oneui_view_notifier_notify(c.vm.notifier, c.title) == ONEUI_VIEW_REENTRANT,
                       "get notify rejected");
            };
            oneui_view_notify(c.view, c.title);
            expect(oneui_view_flush(c.view) == ONEUI_VIEW_REENTRANT, "get reentrancy reported");
            c.vm.onGet = {};
        }
        {
            ApiCase c(argv[1]);
            c.start();
            OneUiViewNotifier *n = nullptr;
            oneui_view_notifier(c.view, &n);
            c.window.window->close();
            expect(oneui_view_notify(c.view, c.title) == ONEUI_VIEW_CLOSED && c.vm.releases == 1,
                   "window closes view");
            OneUiViewError error{};
            error.struct_size = sizeof(error);
            expect(oneui_view_get_error(c.view, &error) == 0, "closed view diagnostics");
            oneui_view_notifier_release(n);
        }
        {
            ApiCase c(argv[1]);
            c.start();
            OneUiWidget *host = nullptr;
            expect(oneui_view_find_host(c.view, nullptr, {"terminal", 8}, &host) == 0,
                   "host lookup");
            OneUiWidget content;
            content.widget = std::make_shared<oneui::Label>();
            int leases = 0;
            expect(oneui_view_host_attach(
                       host, &content, &leases, [](void *p) { ++*static_cast<int *>(p); },
                       [](void *p) { --*static_cast<int *>(p); }) == 0,
                   "host attach");
            expect(leases == 1, "host owner retained");
            oneui_view_disconnect(c.view);
            expect(leases == 0 && !content.widget->hasCompositionOwner(),
                   "host detached before release");
        }
        {
            ApiCase c(argv[1]);
            c.start();
            OneUiView *second = reinterpret_cast<OneUiView *>(1);
            expect(oneui_view_instantiate(c.mount, c.bytes.data(), c.bytes.size(), &c.vm.table,
                                          &c.vm, &second) == ONEUI_VIEW_BUSY &&
                       !second,
                   "busy clears output");
            oneui_view_mount_destroy(c.mount);
            c.mount = nullptr;
            expect(oneui_view_notify(c.view, 0) == ONEUI_VIEW_CLOSED, "mount closes view");
        }
        {
            ApiCase c(argv[1]);
            c.start();
            c.vm.onGet = [&] {
                oneui_view_destroy(c.view);
                c.view = nullptr;
                expect(c.vm.releases == 0, "get self-destroy keeps current lease");
            };
            oneui_view_notify(c.view, c.title);
            expect(oneui_view_flush(c.view) == ONEUI_VIEW_CLOSED, "get self-destroy closes flush");
            expect(c.vm.releases == 1, "get self-destroy final release");
            c.vm.onGet = {};
        }
        {
            ApiCase c(argv[1]);
            c.vm.onConnect = [&] {
                oneui_view_mount_destroy(c.mount);
                c.mount = nullptr;
                expect(c.vm.releases == 0, "connect close keeps current lease");
            };
            expect(oneui_view_instantiate(c.mount, c.bytes.data(), c.bytes.size(), &c.vm.table,
                                          &c.vm, &c.view) == ONEUI_VIEW_CLOSED,
                   "mount destroyed inside connect rolls back");
            expect(!c.view && c.vm.connects == 1 && c.vm.disconnects == 1 && c.vm.releases == 1,
                   "connect self-close balanced");
        }
        {
            ApiCase c(argv[1]);
            c.start();
            c.vm.onInvoke = [&] {
                c.vm.text(c.title, "after-modal");
                expect(oneui_view_notify(c.view, c.title) == 0, "invoke queues notify");
                c.window.window->run();
                expect(c.view->value->root->children[0]->element.as<oneui::Label>()->text() ==
                           L"initial",
                       "nested dispatcher cannot flush mutable callback");
            };
            auto instance = c.view->value->root->children[2];
            c.view->value->invoke(instance, instance->events.front(), {});
            c.window.window->run();
            expect(c.view->value->root->children[0]->element.as<oneui::Label>()->text() ==
                       L"after-modal",
                   "outer callback reschedules deferred flush");
        }
        {
            Case c(argv[1]);
            auto *window = new OneUiWindow;
            window->window = std::make_unique<Window>();
            OneUiViewMount *mount = nullptr;
            OneUiView *view = nullptr;
            auto bytes = oneview::writeImage(c.image);
            expect(oneui_view_mount_create(window, nullptr, &mount) == 0, "heap window mount");
            expect(oneui_view_instantiate(mount, bytes.data(), bytes.size(), &c.vm.table, &c.vm,
                                          &view) == 0,
                   "heap window view");
            oneui_window_destroy(window);
            expect(c.vm.releases == 1 && oneui_view_notify(view, 0) == ONEUI_VIEW_CLOSED,
                   "window destroy closes mounts before native destructor");
            oneui_view_destroy(view);
            oneui_view_mount_destroy(mount);
        }
        {
            ApiCase c(argv[1]);
            c.start();
            auto input = c.view->value->root->children[1]->element.as<oneui::TextField>();
            expect(std::dynamic_pointer_cast<oneui::SystemClipboard>(input->clipboard()) != nullptr,
                   "C ABI host supplies system clipboard");
            oneui::ui::Mount legacy;
            expect(std::dynamic_pointer_cast<oneui::SystemClipboard>(
                       legacy.make("Input").as<oneui::TextField>()->clipboard()) != nullptr,
                   "legacy default mount retains system clipboard behavior");
        }
        std::cout << "11 C ABI lifetime scenarios passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
