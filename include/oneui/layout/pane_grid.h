#pragma once
#include "oneui/layout/workspace_tree.h"
#include "oneui/view.h"
#include <functional>
#include <map>

namespace oneui {
enum class WorkspaceIntentKind { Ratio, Drop, Focus, Swap, Resize, Transition };
struct WorkspaceIntent {
    WorkspaceIntentKind kind;
    std::string source, target, edge;
    double value = 0;
    int64_t request = 0, revision = 0, columns = 0, rows = 0;
};
class ONEUI_API PaneView final : public View {
public:
    std::string key;
    Size minimum{220, 130};
    bool closing = false;
    int64_t closeRequest = 0;
    std::weak_ptr<Widget> header;
    Size naturalSize() const override { return minimum; }
protected:
    void layoutChildren() override;
};
struct PaneGridSnapshot {
    std::vector<WorkspaceNode> nodes;
    std::vector<WorkspaceGroup> groups;
    std::string activeGroup, activePane, maximized;
    int64_t revision = 0, ackRequest = 0;
    bool ackAccepted = true, motion = true;
    float gap = 5, dividerHit = 10;
    double motionMs = 200, resizeIdleMs = 120;
};
enum class PaneMotionPhase { Idle, Preparing, WarmFrame, Running, FinalLayout };
class ONEUI_API PaneGrid final : public View {
public:
    PaneGrid();
    ~PaneGrid() override;
    // All allocations and validation precede publication. A rejected candidate
    // leaves geometry, native children and content leases in the previous scene.
    std::function<void()> prepare(PaneGridSnapshot,
                                 std::vector<std::shared_ptr<PaneView>>);
    void setSnapshot(PaneGridSnapshot, std::vector<std::shared_ptr<PaneView>>);
    void setOnIntent(std::function<void(const WorkspaceIntent&)>);
    const PaneGridSnapshot& snapshot() const;
    PaneMotionPhase motionPhase() const;
    uint64_t motionGeneration() const;
    bool resizeHeld() const;
    void setMotionEnabled(bool);
    void navigate(const std::string& edge, bool swap = false);
    void queueNavigation(int64_t serial, std::string edge, bool swap);
    uint64_t navigationDropped() const;
    void requestDrop(std::string source, Point point, bool tabSource = false);
    void requestTabDrop(std::string source,std::string target);
    bool canDrag(const std::string& source,bool tabSource=false) const;
    std::pair<std::string,std::string> dropTarget(Point, bool tabSource = false) const;
    std::vector<WorkspaceRect> visiblePanes() const;
    void showPreview(Rect, bool valid);
    void clearPreview();
    std::optional<Rect> previewRect() const;
    void setAccent(Color color);
    Size naturalSize() const override;
    void paint(Canvas&) override;
    bool tickAnimations(double) override;
    bool onMouseDown(const MouseEvent&) override;
    bool onMouseMove(const MouseEvent&) override;
    bool onMouseUp(const MouseEvent&) override;
    bool onKeyDown(const KeyEvent&) override;
    bool onFocusChanged(bool) override;
    CursorKind cursor(Point) const override;
    bool isFocusable() const override { return interactive(); }
protected:
    void layoutChildren() override;
    bool hasInteractionState() const override;
    void resetInteractionState() override;
private:
    struct State;
    std::shared_ptr<State> state_;
    std::function<void(const WorkspaceIntent&)> intent_;
    int64_t request_ = 0;
    uint64_t generation_ = 0;
    bool motionEnabled_ = true;
    void emit(WorkspaceIntent);
    void arrange(float progress, bool minimums);
    void hold(bool);
    bool contentReady();
    bool scanContent(State&);
    void publishContent(State&);
    void finishMotion();
    void updateDivider(Point);
    void settleDivider();
    void cancelDivider();
    size_t dividerAt(Point) const;
    bool tickPreview(double);
};
} // namespace oneui
