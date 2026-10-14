#pragma once
#include "oneui/layout/dock_workspace_view.h"
#include <map>
namespace oneui {
struct DockWorkspaceView::State {
    static constexpr size_t none = size_t(-1);
    struct Node {
        size_t first=none, second=none, item=none;
        Size minimum;
        Rect bounds, divider;
        float ratio=.5f;
    };
    struct Item {
        std::shared_ptr<DockItemView> view;
        DockPlacement placement;
        DockId identity=0;
        bool shown=false;
    };
    struct Work {size_t node;Rect rect;};
    enum class Gesture {None,Move,Resize,Divider};
    DockWorkspaceSnapshot snapshot;
    DockWorkspacePresentation presentation;
    DockTreeIndex index;
    std::vector<Node> nodes;
    std::vector<Item> items;
    std::map<std::string,size_t> itemIds,nodeIds;
    std::map<std::string,DockId> identities;
    DockId nextIdentity=1;
    std::vector<Work> work;
    std::vector<DockHitTarget> targets;
    size_t root=none, source=none, divider=none, focusedDivider=none;
    Gesture gesture=Gesture::None;
    Point origin;
    Rect start,bounds;
    DockResizeEdge resizeEdge=DockResizeEdge::North;
    float beginRatio=.5f,grab=0;
    double idleAt=0;
    bool keyboardDirty=false,previewActive=false;
    Rect preview,previewTarget;
    double previewTime=0;
    std::optional<DockHit> hit;
    int64_t pendingRequest=0;
    std::shared_ptr<StyleSheet> sheet;
    StyleNode node;
    // Cached `dock-workspace-float` box, keyed by the sheet it came from.
    mutable std::shared_ptr<const StyleBox> floatSurface;
    mutable const StyleSheet* floatSurfaceSheet=nullptr;
};
} // namespace oneui
