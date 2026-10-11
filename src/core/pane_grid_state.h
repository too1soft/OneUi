#pragma once
#include "oneui/layout/pane_grid.h"
#include "oneui/content_viewport.h"
#include <limits>

namespace oneui {
struct PaneGrid::State {
    static constexpr size_t none = size_t(-1);
    struct Node {
        size_t first = none, second = none, pane = none, parent = none;
        Size minimum;
        float from = .5f, to = .5f, current = .5f;
        Rect bounds, divider;
    };
    struct Pane {
        std::shared_ptr<PaneView> view;
        bool shown = false;
        Size minimum;
        bool closing = false;
        int64_t closeRequest = 0;
    };
    struct Work { size_t node; Rect rect; };
    struct Terminal {
        std::shared_ptr<ContentViewportSource> view;
        std::string pane;
        std::function<void(ContentViewport)> observer;
    };
    struct Viewport { std::string pane; ContentViewport size; uint64_t generation; };
    struct Navigation { std::string edge; bool swap; };
    std::vector<Navigation> navigation;
    int64_t navigationSerial=0;
    uint64_t navigationOverflow=0;
    bool previewActive=false,previewValid=false;
    Rect preview,previewTarget;
    double previewTime=0;
    int64_t previewRequest=0;
    Color accent{40,97,61};
    PaneGridSnapshot snapshot;
    DockTreeIndex index;
    std::vector<Node> nodes;
    std::vector<Pane> panes;
    std::map<std::string,size_t> nodeIds, paneIds;
    std::vector<Work> work;
    std::vector<size_t> visibleOrder;
    std::vector<Terminal> terminals;
    std::vector<Terminal> retiredTerminals;
    std::vector<Viewport> pendingViewports;
    std::vector<std::pair<std::string,int64_t>> closing;
    std::map<std::string,int64_t> completedClosing;
    size_t root = none, divider = none, focusedDivider = none, hoveredDivider = none;
    PaneMotionPhase phase = PaneMotionPhase::Idle;
    bool held = false, dirtyDivider = false, dragging = false;
    bool ready = false;
    bool warmPainted = false;
    double started = 0, idleAt = 0;
    float grab = 0, beginRatio = .5f;
    float fromGap = 0, toGap = 0, currentGap = 0;
};
} // namespace oneui
