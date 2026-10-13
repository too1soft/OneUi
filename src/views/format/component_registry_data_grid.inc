// Frozen component contract: DataGrid. IDs remain unchanged.
C("DataGrid", "leaf", 0)
P("DataGrid", 1, "basis", 3, 1, "")
P("DataGrid", 2, "class", 4, 1, "")
P("DataGrid", 3, "description", 4, 3, "")
P("DataGrid", 4, "disabled", 1, 3, "")
P("DataGrid", 5, "grow", 3, 1, "")
P("DataGrid", 6, "max", 3, 1, "")
P("DataGrid", 7, "min", 3, 1, "")
P("DataGrid", 8, "name", 4, 3, "")
P("DataGrid", 9, "shrink", 3, 1, "")
P("DataGrid", 10, "visible", 1, 3, "")
P("DataGrid", 11, "grid-columns", 5, 2, "@grid.column")
P("DataGrid", 12, "grid-rows", 5, 2, "@grid.row")
P("DataGrid", 13, "row-count", 2, 3, "")
P("DataGrid", 14, "window-start", 2, 3, "")
P("DataGrid", 15, "frozen-columns", 2, 3, "")
P("DataGrid", 16, "row-height", 3, 3, "")
P("DataGrid", 17, "header-height", 3, 3, "")
P("DataGrid", 18, "row-numbers", 1, 3, "")
P("DataGrid", 19, "editable", 1, 3, "")
P("DataGrid", 20, "anchor-row", 2, 3, "")
P("DataGrid", 21, "anchor-column", 2, 3, "")
P("DataGrid", 22, "focus-row", 2, 3, "")
P("DataGrid", 23, "focus-column", 2, 3, "")
P("DataGrid", 24, "select-kind", 2, 3, "")
r.at("DataGrid").events={
    {1,"viewport",{{"first",2},{"count",2}}},
    {2,"sort",{{"column",2}}},
    {3,"resize",{{"column",2},{"width",3}}},
    {4,"selection",{{"anchorRow",2},{"anchorColumn",2},{"focusRow",2},{"focusColumn",2},{"kind",2}}},
    {5,"edit",{{"row",2},{"column",2},{"text",4}}},
    {6,"context",{{"row",2},{"column",2},{"x",3},{"y",3}}},
    {7,"copy",{}},
    {8,"activate",{{"row",2},{"column",2}}}
};
