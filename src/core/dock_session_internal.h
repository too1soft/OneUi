#pragma once
#include "oneui/controls/tabs.h"
#include "../platform/shared/native_dock_drag.h"
#include "oneui/layout/dock_session.h"
#include <set>
#include <thread>

namespace oneui {
struct DockSession::Impl : std::enable_shared_from_this<Impl> {
    struct Group {
        std::vector<DockId> tabs;
        DockId selected = 0;
    };
    struct Layout {
        std::optional<DockNode> root;
        std::map<DockId, Group> groups;
    };
    struct Presentation {
        std::uint64_t generation = 0;
        std::shared_ptr<Widget> root;
        std::map<DockId, std::shared_ptr<Panel>> slots;
        std::map<DockId, std::shared_ptr<Tabs>> tabs;
    };
    struct Host {
        Window *window = nullptr;
        std::shared_ptr<Panel> surface;
        Window::EventSubscription closed;
        Presentation view;
        std::shared_ptr<void> owner;
        std::shared_ptr<platform::NativeDockDrag> nativeDrag;
    };
    Application *application;
    Application::Dispatcher dispatch;
    std::thread::id thread = std::this_thread::get_id();
    std::map<DockId, Host> hosts;
    std::map<DockId, Layout> layouts;
    std::map<DockId, DockPane> panes;
    std::map<DockId, std::unique_ptr<Window>> owned;
    std::shared_ptr<StyleSheet> styleSheet;
    DockId nextHost = 1, nextGroup = 1;
    std::uint64_t revision = 0, nextToken = 1, dragToken = 0, dragRevision = 0;
    DockId dragPane = 0;
    std::string nativePayload;
    bool nativeDropAttempted = false;
    bool busy = false, stopped = false, changedQueued = false;
    std::function<void()> changed;
    explicit Impl(Application &app) : application(&app), dispatch(app.dispatcher()) {}
    void check() const;
    void applyTheme(const std::shared_ptr<Widget> &);
    DockId attach(Window &, std::shared_ptr<Panel>, std::shared_ptr<void> owner = {});
    DockPaneLocation locate(DockId, const std::map<DockId, Layout> &) const;
    static void erasePane(Layout &, DockId);
    DockSessionResult insert(std::map<DockId, Layout> &, DockId, DockDestination);
    DockSessionResult move(DockId, DockDestination);
    DockSessionResult select(DockId);
    DockSessionResult apply(std::map<DockId, Layout>, const std::set<DockId> &, DockId moving = 0,
                            Window *source = nullptr, Window *target = nullptr);
    Presentation build(DockId, const Layout &);
    std::shared_ptr<Widget> buildNode(DockId, const DockNode &, const Layout &, Presentation &,
                                      std::vector<bool> path = {});
    void connect(const std::map<DockId, Layout> &, std::map<DockId, Presentation> &);
    void hostClosed(DockId);
    void notify();
    void attachNativeDrag(DockId);
    bool startNativeDrag(DockId);
    bool nativeDrop(DockId host, const std::string&, Point, bool commit);
    DockSessionResult detach(DockId, WindowOptions, DockId*);
};
} // namespace oneui
