r.at("DockWorkspace").version=2;
r.at("DockItem").version=2;
// Additive activation contract; existing drag event identity remains frozen.
++r.at("DragHandle").version;
r.at("DragHandle").events.push_back({2,"activated",{{"source",4},{"x",3},{"y",3},{"count",2},{"button",2}}});
P("DockWorkspace", 11, "nodes", 5, 2, "@workspace.node")
P("DockWorkspace", 12, "root", 4, 3, "")
P("DockWorkspace", 13, "placements", 5, 2, "@workspace.placement")
P("DockWorkspace", 14, "revision", 2, 3, "")
P("DockWorkspace", 15, "maximized", 4, 3, "")
P("DockWorkspace", 16, "compact-active", 4, 3, "")
P("DockWorkspace", 17, "ack-request", 2, 3, "")
P("DockWorkspace", 18, "ack-accepted", 1, 3, "")
P("DockWorkspace", 19, "gap", 3, 1, "")
P("DockWorkspace", 20, "divider-hit", 3, 1, "")
P("DockWorkspace", 21, "preview-dock-text", 4, 3, "")
P("DockWorkspace", 22, "preview-float-text", 4, 3, "")
P("DockWorkspace", 23, "preview-keep-text", 4, 3, "")
P("DockWorkspace", 24, "raise-on-press", 1, 3, "")
P("DockWorkspace", 25, "float-on-drop", 4, 3, "")
P("DockWorkspace", 26, "float-modifier", 4, 3, "")
P("DockWorkspace", 27, "dock-hit-mode", 4, 3, "")
P("DockItem", 12, "dock-min-width", 3, 1, "")
P("DockItem", 13, "dock-min-height", 3, 1, "")
P("DockItem", 14, "float-min-width", 3, 1, "")
P("DockItem", 15, "float-min-height", 3, 1, "")
P("DockItem", 16, "hideable", 1, 3, "")
r.at("DockWorkspace").events={
    {1,"dockRequested",{{"source",4},{"target",4},{"edge",4},{"request",2},{"revision",2}}},
    {2,"ratioRequested",{{"split",4},{"ratio",3},{"request",2},{"revision",2}}},
    {3,"floatRequested",{{"pane",4},{"x",3},{"y",3},{"width",3},{"height",3},{"request",2},{"revision",2}}},
    {4,"boundsRequested",{{"pane",4},{"x",3},{"y",3},{"width",3},{"height",3},{"request",2},{"revision",2}}},
    {5,"pinRequested",{{"pane",4},{"pinned",1},{"request",2},{"revision",2}}},
    {6,"raiseRequested",{{"pane",4},{"request",2},{"revision",2}}},
    {7,"maximizeRequested",{{"pane",4},{"request",2},{"revision",2}}},
    {8,"visibilityRequested",{{"pane",4},{"visible",1},{"request",2},{"revision",2}}},
    {9,"redockRequested",{{"pane",4},{"request",2},{"revision",2}}}
};
