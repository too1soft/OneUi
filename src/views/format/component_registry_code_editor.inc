// Frozen component contract: CodeEditor. IDs remain unchanged.
C("CodeEditor", "leaf", 0)
P("CodeEditor", 1, "basis", 3, 1, "")
P("CodeEditor", 2, "class", 4, 1, "")
P("CodeEditor", 3, "description", 4, 3, "")
P("CodeEditor", 4, "disabled", 1, 3, "")
P("CodeEditor", 5, "grow", 3, 1, "")
P("CodeEditor", 6, "max", 3, 1, "")
P("CodeEditor", 7, "min", 3, 1, "")
P("CodeEditor", 8, "name", 4, 3, "")
P("CodeEditor", 9, "shrink", 3, 1, "")
P("CodeEditor", 10, "visible", 1, 3, "")
P("CodeEditor", 11, "language", 4, 3, "")
P("CodeEditor", 12, "text", 4, 3, "")
P("CodeEditor", 13, "text-revision", 2, 3, "")
P("CodeEditor", 14, "read-only", 1, 3, "")
P("CodeEditor", 15, "word-wrap", 1, 3, "")
P("CodeEditor", 16, "line-numbers", 1, 3, "")
P("CodeEditor", 17, "indent-guides", 1, 3, "")
P("CodeEditor", 18, "tab-size", 2, 3, "")
P("CodeEditor", 19, "insert-spaces", 1, 3, "")
P("CodeEditor", 20, "font-size", 3, 3, "")
P("CodeEditor", 21, "comment-prefix", 4, 3, "")
r.at("CodeEditor").events={
    {1,"changed",{{"revision",2}}},
    {2,"cursor",{{"caret",2},{"anchor",2},{"line",2},{"column",2},{"cursors",2}}}
};
