#pragma once
#include "oneui/application.h"
#include "oneui/layout/dock_model.h"
#include "oneui/layout/panel.h"

namespace oneui {
enum class DockPosition { Tab, Left, Right, Top, Bottom };
enum class DockTransferPhase { Prepare, Commit, Rollback };
enum class DockSessionResult {
    Applied,
    InvalidPane,
    InvalidHost,
    InvalidTarget,
    Busy,
    StaleTransaction,
    ResourceRejected,
    Failed
};
struct DockPane {
    DockId id = 0;
    std::wstring title;
    std::shared_ptr<Widget> content;
    // Retain the controller/session together with its original widget.
    std::shared_ptr<void> owner;
    Size minimum{120, 80};
    // Custom native/GPU views can stage target resources before reparenting.
    // Return false from Prepare/Commit to roll back. Rollback must not throw.
    std::function<bool(Window &, Window &, DockTransferPhase)> transfer;
};
struct DockDestination {
    DockId host = 0;
    DockId pane = 0; // Zero selects the first group, or an empty host.
    DockPosition position = DockPosition::Tab;
    int index = -1; // Tab insertion index; -1 appends.
};
struct DockPaneLocation {
    DockId host = 0, group = 0;
    int index = -1;
    bool selected = false;
};
// UI-thread only. Application must outlive the session. External windows may
// close independently; their panes remain owned until moved or removed.
class ONEUI_API DockSession final {
  public:
    explicit DockSession(Application &);
    ~DockSession();
    DockSession(const DockSession &) = delete;
    DockSession &operator=(const DockSession &) = delete;
    // The surface must be empty and belongs exclusively to this session.
    // It may be nested inside other window content (e.g. an application shell).
    DockId attachHost(Window &, std::shared_ptr<Panel> surface, std::shared_ptr<void> owner = {});
    DockSessionResult addPane(DockPane, DockDestination);
    DockSessionResult removePane(DockId);
    DockSessionResult movePane(DockId, DockDestination);
    DockSessionResult selectPane(DockId);
    // Retains the sheet and refreshes native styling on every host/pane,
    // including already detached windows. Call again after editing the sheet.
    // Null is rejected; no widget identity, focus or layout model is replaced.
    DockSessionResult setStyleSheet(std::shared_ptr<StyleSheet>);
    DockSessionResult detachPane(DockId, WindowOptions, DockId *newHost = nullptr);
    // A drag leaves the tree untouched until commit. Any intervening edit
    // invalidates the token. Tokens are session-local and single-use.
    std::uint64_t beginDrag(DockId);
    DockSessionResult commitDrag(std::uint64_t, DockDestination);
    bool cancelDrag(std::uint64_t);
    // Requires a current native pointer grab. This starts the OS operation;
    // onChanged reports a later successful transfer. False means not started.
    bool startNativeDrag(DockId);
    bool nativeDragSupported(DockId host) const;
    std::string nativeDragLimitation(DockId host) const;
    DockPaneLocation location(DockId) const;
    std::vector<DockId> panes(DockId host = 0) const;
    std::vector<DockId> hosts() const;
    std::shared_ptr<Widget> content(DockId) const;
    Window *hostWindow(DockId) const;
    void setOnChanged(std::function<void()>);

  private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
};
} // namespace oneui
