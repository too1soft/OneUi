// Pane header identity, actions and semantic intents.
r.at("Tabs").version=2;
P("Tabs", 15, "reorderable", 1, 3, "")
P("Tabs", 16, "drag-domain", 4, 1, "")
P("Tabs", 17, "drag-kind", 4, 1, "")
P("Tabs", 18, "drag-sources", 5, 2, "@workspace.tab-source")
r.at("Tabs").events.push_back({2,"dragOut",{{"key",4},{"phase",2},{"x",3},{"y",3}}});
r.at("Tabs").events.push_back({3,"reorderRequested",{{"source",4},{"before",4}}});
r.at("Tabs").version=3;
P("Tabs", 19, "decorations", 5, 2, "@workspace.tab-decoration")
P("Tabs", 20, "item-min", 3, 3, "")
P("Tabs", 21, "item-max", 3, 3, "")
P("Tabs", 22, "edit-key", 4, 3, "")
r.at("Tabs").version=4;
P("Tabs", 23, "sizing", 4, 3, "")
r.at("Tabs").events.push_back({4,"contextRequested",{{"key",4},{"x",3},{"y",3}}});
r.at("Tabs").events.push_back({5,"overflowChanged",{{"count",2}}});
r.at("Tabs").events.push_back({6,"editFinished",{{"key",4},{"text",4},{"reason",2}}});
C("PaneHeader", "header-slots", 0)
P("PaneHeader", 1, "basis", 3, 1, "")
P("PaneHeader", 2, "class", 4, 1, "")
P("PaneHeader", 3, "description", 4, 3, "")
P("PaneHeader", 4, "disabled", 1, 3, "")
P("PaneHeader", 5, "grow", 3, 1, "")
P("PaneHeader", 6, "max", 3, 1, "")
P("PaneHeader", 7, "min", 3, 1, "")
P("PaneHeader", 8, "name", 4, 3, "")
P("PaneHeader", 9, "shrink", 3, 1, "")
P("PaneHeader", 10, "visible", 1, 3, "")
P("PaneHeader", 11, "pane-key", 4, 3, "")
P("PaneHeader", 12, "number", 2, 3, "")
P("PaneHeader", 13, "title", 4, 3, "")
P("PaneHeader", 14, "path", 4, 3, "")
P("PaneHeader", 15, "active", 1, 3, "")
P("PaneHeader", 16, "broadcast-role", 4, 3, "")
P("PaneHeader", 17, "broadcast-hint", 4, 3, "")
P("PaneHeader", 18, "broadcast-label", 4, 3, "")
P("PaneHeader", 19, "viewing-history", 1, 3, "")
P("PaneHeader", 20, "can-close", 1, 3, "")
P("PaneHeader", 21, "can-split", 1, 3, "")
P("PaneHeader", 22, "can-maximize", 1, 3, "")
C("HeaderSlot", "one", 0)
P("HeaderSlot", 1, "basis", 3, 1, "")
P("HeaderSlot", 2, "class", 4, 1, "")
P("HeaderSlot", 3, "description", 4, 3, "")
P("HeaderSlot", 4, "disabled", 1, 3, "")
P("HeaderSlot", 5, "grow", 3, 1, "")
P("HeaderSlot", 6, "max", 3, 1, "")
P("HeaderSlot", 7, "min", 3, 1, "")
P("HeaderSlot", 8, "name", 4, 3, "")
P("HeaderSlot", 9, "shrink", 3, 1, "")
P("HeaderSlot", 10, "visible", 1, 3, "")
P("HeaderSlot", 11, "slot-name", 4, 1, "")
r.at("PaneHeader").events={
    {1,"activateRequested",{{"pane",4}}},
    {2,"splitRequested",{{"pane",4},{"edge",4}}},
    {3,"maximizeRequested",{{"pane",4}}},
    {4,"closeRequested",{{"pane",4}}},
    {5,"broadcastRequested",{{"pane",4}}},
    {6,"liveRequested",{{"pane",4}}},
    {7,"menuRequested",{{"pane",4},{"x",3},{"y",3}}},
};
C("DragHandle", "optional-one", 0)
P("DragHandle", 1, "basis", 3, 1, "")
P("DragHandle", 2, "class", 4, 1, "")
P("DragHandle", 3, "description", 4, 3, "")
P("DragHandle", 4, "disabled", 1, 3, "")
P("DragHandle", 5, "grow", 3, 1, "")
P("DragHandle", 6, "max", 3, 1, "")
P("DragHandle", 7, "min", 3, 1, "")
P("DragHandle", 8, "name", 4, 3, "")
P("DragHandle", 9, "shrink", 3, 1, "")
P("DragHandle", 10, "visible", 1, 3, "")
P("DragHandle", 11, "source", 4, 3, "")
P("DragHandle", 12, "domain", 4, 1, "")
P("DragHandle", 13, "kind", 4, 1, "")
P("DragHandle", 14, "threshold", 3, 1, "")
r.at("DragHandle").events={{1,"dragStateChanged",{{"source",4},{"phase",2}}}};
C("DropSurface", "one", 0)
P("DropSurface", 1, "basis", 3, 1, "")
P("DropSurface", 2, "class", 4, 1, "")
P("DropSurface", 3, "description", 4, 3, "")
P("DropSurface", 4, "disabled", 1, 3, "")
P("DropSurface", 5, "grow", 3, 1, "")
P("DropSurface", 6, "max", 3, 1, "")
P("DropSurface", 7, "min", 3, 1, "")
P("DropSurface", 8, "name", 4, 3, "")
P("DropSurface", 9, "shrink", 3, 1, "")
P("DropSurface", 10, "visible", 1, 3, "")
P("DropSurface", 11, "domain", 4, 1, "")
P("DropSurface", 12, "role", 4, 1, "")
r.at("DropSurface").events={{1,"dropRequested",{{"source",4},{"target",4},{"edge",4},{"request",2},{"revision",2}}}};

// Localized native action captions; existing property IDs remain unchanged.
P("PaneHeader", 23, "split-label", 4, 3, "")
P("PaneHeader", 24, "maximize-label", 4, 3, "")
P("PaneHeader", 25, "close-label", 4, 3, "")
P("PaneHeader", 26, "live-label", 4, 3, "")
P("PaneHeader", 27, "menu-label", 4, 3, "")

P("PaneHeader", 28, "tooltip", 4, 3, "")
++r.at("PaneHeader").version; // Authored session tooltip.
