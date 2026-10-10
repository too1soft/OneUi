// Extends S2's identity-only contracts. C ABI remains 44; COMP hashes include
// these properties and semantic event signatures in full.
r.at("PaneGrid").version=2;
r.at("Pane").version=2;
r.at("NativeHost").version=2;
P("PaneGrid", 11, "nodes", 5, 2, "@workspace.node")
P("PaneGrid", 12, "groups", 5, 2, "@workspace.group")
P("PaneGrid", 13, "active-group", 4, 3, "")
P("PaneGrid", 14, "active-pane", 4, 3, "")
P("PaneGrid", 15, "maximized", 4, 3, "")
P("PaneGrid", 16, "revision", 2, 3, "")
P("PaneGrid", 17, "ack-request", 2, 3, "")
P("PaneGrid", 18, "ack-accepted", 1, 3, "")
P("PaneGrid", 19, "navigation-serial", 2, 3, "")
P("PaneGrid", 20, "navigation-edge", 4, 3, "")
P("PaneGrid", 21, "navigation-mode", 4, 3, "")
P("PaneGrid", 22, "drag-domain", 4, 1, "")
P("PaneGrid", 23, "gap", 3, 1, "")
P("PaneGrid", 24, "divider-hit", 3, 1, "")
P("PaneGrid", 25, "motion-ms", 3, 1, "")
P("PaneGrid", 26, "resize-idle-ms", 3, 1, "")
P("PaneGrid", 27, "motion", 1, 3, "")
P("Pane", 12, "closing", 1, 3, "")
P("Pane", 13, "close-request", 2, 3, "")
P("Pane", 14, "min-width", 3, 1, "")
P("Pane", 15, "min-height", 3, 1, "")
P("NativeHost", 13, "resize-policy", 4, 1, "")
r.at("PaneGrid").events={
    {1,"dropRequested",{{"source",4},{"target",4},{"edge",4},{"request",2},{"revision",2}}},
    {2,"ratioRequested",{{"split",4},{"ratio",3},{"request",2},{"revision",2}}},
    {3,"focusRequested",{{"from",4},{"target",4},{"request",2},{"revision",2}}},
    {4,"swapRequested",{{"source",4},{"target",4},{"request",2},{"revision",2}}},
    {5,"resizeSettled",{{"pane",4},{"cols",2},{"rows",2},{"generation",2}}},
    {6,"transitionSettled",{{"pane",4},{"request",2},{"revision",2}}}
};
