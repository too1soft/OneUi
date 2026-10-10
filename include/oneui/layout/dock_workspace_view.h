#pragma once
#include "oneui/layout/workspace_tree.h"
#include "oneui/view.h"

namespace oneui {
class ONEUI_API DockItemView final : public View {
public:
    std::string key;
    Size dockMinimum{280,150}, floatMinimum{240,160};
    bool hideable = true;
    std::weak_ptr<Widget> header, body, footer;
    Size naturalSize() const override { return dockMinimum; }
protected:
    void layoutChildren() override;
};
struct DockPlacement {
    std::string id, mode = "docked";
    Rect bounds;
    bool pinned = false;
    int64_t order = 0;
};
struct DockWorkspaceSnapshot {
    std::vector<WorkspaceNode> nodes;
    std::vector<DockPlacement> placements;
    std::string root, maximized, compactActive;
    int64_t revision = 0, ackRequest = 0;
    bool ackAccepted = true;
    float gap = 8, dividerHit = 8;
};
enum class DockIntentKind { Dock, Ratio, Float, Bounds, Pin, Raise, Maximize, Visibility, Redock };
struct DockIntent {
    DockIntentKind kind;
    std::string source, target, edge;
    Rect bounds;
    double ratio = .5;
    bool flag = false;
    int64_t request = 0, revision = 0;
};
// Placement is always VM-owned. Gestures alter only preview geometry until an
// accepted snapshot arrives; all children keep their native identity and owner.
class ONEUI_API DockWorkspaceView final : public View {
public:
    DockWorkspaceView();
    ~DockWorkspaceView() override;
    std::function<void()> prepare(DockWorkspaceSnapshot, std::vector<std::shared_ptr<DockItemView>>);
    void setSnapshot(DockWorkspaceSnapshot, std::vector<std::shared_ptr<DockItemView>>);
    const DockWorkspaceSnapshot& snapshot() const;
    void setOnIntent(std::function<void(const DockIntent&)>);
    void drag(const std::string& source, int phase, Point windowPoint);
    void requestAction(DockIntentKind, const std::string& source, bool flag = false);
    void setMotionEnabled(bool);
    std::optional<Rect> previewRect() const;
    bool paintPending() const;
    Size naturalSize() const override;
    void paint(Canvas&) override;
    bool tickAnimations(double) override;
    bool onMouseDown(const MouseEvent&) override;
    bool onMouseMove(const MouseEvent&) override;
    bool onMouseUp(const MouseEvent&) override;
    bool onKeyDown(const KeyEvent&) override;
    CursorKind cursor(Point) const override;
    bool isFocusable() const override { return interactive(); }
protected:
    void layoutChildren() override;
    bool hasInteractionState() const override;
    void resetInteractionState() override;
private:
    struct State;
    std::shared_ptr<State> state_;
    std::function<void(const DockIntent&)> intent_;
    int64_t request_ = 0;
    bool alt_ = false, motion_ = true;
    Point pointerOrigin_;
    void emit(DockIntent);
    void cancel();
    void updateMove(Point);
    void updateResize(Point);
    void updateDivider(Point);
    void showPreview(Rect);
    size_t dividerAt(Point) const;
    std::pair<size_t,std::optional<DockResizeEdge>> resizeAt(Point) const;
};
} // namespace oneui
