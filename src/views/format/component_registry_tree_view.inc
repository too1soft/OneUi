// Frozen component contract: TreeView. IDs remain unchanged.
C("TreeView", "leaf", 12)
P("TreeView", 1, "basis", 3, 1, "")
P("TreeView", 2, "class", 4, 1, "")
P("TreeView", 3, "description", 4, 3, "")
P("TreeView", 4, "disabled", 1, 3, "")
P("TreeView", 5, "grow", 3, 1, "")
P("TreeView", 6, "max", 3, 1, "")
P("TreeView", 7, "min", 3, 1, "")
P("TreeView", 8, "name", 4, 3, "")
P("TreeView", 9, "shrink", 3, 1, "")
P("TreeView", 10, "visible", 1, 3, "")
P("TreeView", 11, "tree-items", 5, 2, "@tree.node")
P("TreeView", 12, "selected-id", 4, 7, "")
P("TreeView", 13, "row-height", 3, 3, "")
P("TreeView", 14, "loading-text", 4, 3, "")
r.at("TreeView").events={
    {1,"expand",{{"id",4},{"expanded",1}}},
    {2,"activate",{{"id",4}}},
    {3,"context",{{"id",4},{"x",3},{"y",3}}}
};
