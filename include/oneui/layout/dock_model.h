#pragma once
#include "oneui/export.h"
#include "oneui/geometry.h"
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <vector>

namespace oneui {
using DockId = std::uint64_t;
enum class DockAxis { Horizontal, Vertical };
enum class DockEdge { Left, Right, Top, Bottom };
enum class DockHitMode { PaneSwap, GuideStrict, GuideNearest, TabMerge };
enum class DockHitPlacement { None, Left, Right, Top, Bottom, Swap };
struct DockHitTarget {
    DockId pane = 0;
    Rect bounds;
    bool floating = false;
};
struct DockHit {
    DockId target = 0;
    Rect bounds, preview;
    DockHitPlacement placement = DockHitPlacement::None;
};
// First eligible target wins; callers provide their paint/hit-test order.
// Invalid points/rectangles and the source are ignored. Guide modes exclude
// floating targets. Strict guides may identify a target without an active drop.
ONEUI_API std::optional<DockHit> dockHitTest(DockId source, Point,
                                             const std::vector<DockHitTarget> &, DockHitMode);
enum class DockError { None, InvalidGeometry, DuplicateId, MissingPane, InvalidTarget };
struct DockNode {
    DockId pane = 0; // Nonzero for a leaf; zero for a split.
    DockAxis axis = DockAxis::Horizontal;
    float ratio = 0.5f;
    std::unique_ptr<DockNode> first, second;
    ONEUI_API DockNode();
    ONEUI_API explicit DockNode(DockId);
    ONEUI_API DockNode(DockAxis, float, DockNode, DockNode);
    ONEUI_API DockNode(const DockNode &);
    ONEUI_API DockNode &operator=(const DockNode &);
    DockNode(DockNode &&) noexcept = default;
    DockNode &operator=(DockNode &&) noexcept = default;
    ONEUI_API bool operator==(const DockNode &) const;
    ONEUI_API bool contains(DockId) const;
    ONEUI_API std::vector<DockId> ids() const;
    ONEUI_API std::optional<DockNode> without(DockId) const;
    ONEUI_API bool replace(DockId, DockNode);
    ONEUI_API DockError swap(DockId, DockId);
    ONEUI_API void balance();
    ONEUI_API DockError setRatio(const std::vector<bool> &path, float);
    ONEUI_API DockError validate() const;
};
struct DockFloating {
    Rect bounds;
    bool pinned = false;
    std::uint64_t order = 0;
};
struct DockRestore {
    std::optional<DockNode> before, after;
    DockId target = 0;
    DockEdge edge = DockEdge::Right;
    float ratio = 0.5f;
    std::optional<DockFloating> floating;
};
struct DockSnapshot {
    std::optional<DockNode> root;
    std::map<DockId, DockFloating> floating;
    std::map<DockId, DockRestore> hidden;
    DockId maximized = 0;
};
struct DockDivider {
    std::vector<bool> path;
    DockAxis axis;
    Rect bounds;
};
struct DockGeometry {
    std::map<DockId, Rect> panes;
    std::vector<DockDivider> dividers;
};
using DockMinimum = std::function<Size(DockId)>;
// Shared, renderer-independent workspace model. Callers serialize access; it
// carries IDs only and may be moved between threads. GUI owners use DockSession.
class ONEUI_API DockWorkspace final {
  public:
    DockWorkspace() = default;
    DockError restore(DockSnapshot);
    DockSnapshot snapshot() const { return state_; }
    std::uint64_t sequence() const { return sequence_; }
    void preserveSequence(std::uint64_t value) {
        if (value > sequence_)
            sequence_ = value;
    }
    bool visible(DockId) const;
    DockError hide(DockId);
    DockError show(DockId);
    DockError dock(DockId, DockId target, DockEdge);
    DockError floatPane(DockId, Rect);
    DockError redock(DockId);
    DockError maximize(DockId);
    DockError setPinned(DockId, bool);
    DockError raise(DockId);
    DockError setFloatBounds(DockId, Rect);
    DockError swap(DockId, DockId);
    DockError setRatio(const std::vector<bool> &path, float);
    void balance();
    std::vector<DockId> paintOrder() const;

  private:
    void remember(DockId);
    DockError insert(DockId, DockId target, DockEdge, float ratio);
    DockSnapshot state_;
    std::uint64_t sequence_ = 0;
};
ONEUI_API Size dockMinimum(const DockNode &, float gap, const DockMinimum &);
ONEUI_API DockError dockGeometry(const DockNode &, Rect bounds, float gap, const DockMinimum &,
                                 DockGeometry &);
ONEUI_API DockError clampDockFloating(Rect, Rect workspace, Size minimum, Rect &);
enum class DockResizeEdge { North, South, East, West, NorthEast, NorthWest, SouthEast, SouthWest };
ONEUI_API DockError resizeDockFloating(Rect start, Point delta, DockResizeEdge, Rect workspace,
                                       Size minimum, Rect &);
} // namespace oneui
