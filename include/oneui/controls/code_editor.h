#pragma once

#include "oneui/export.h"
#include "oneui/color.h"
#include "oneui/view.h"

#include <array>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace oneui {

class Clipboard;
class TextField;
struct CodeEditorImpl;

// Data-driven lexical rules. The editor colors text itself, so a product only
// describes its language once instead of sending colors per line.
struct CodeLanguage {
    bool caseSensitive = false;
    std::vector<std::wstring> keywords;
    std::vector<std::wstring> types;
    std::vector<std::wstring> functions;
    std::vector<std::wstring> constants;
    std::vector<std::wstring> lineComments;
    std::vector<std::pair<std::wstring, std::wstring>> blockComments;
    bool nestedBlockComments = false;
    // String quotes escape by doubling; backslashEscapes also allows \x.
    std::wstring stringQuotes = L"'";
    bool backslashEscapes = false;
    // Quoted identifiers as open/close pairs, e.g. "" `` [].
    std::vector<std::pair<wchar_t, wchar_t>> identifierQuotes;
    // Characters that start a parameter or variable (:name, @var, ?).
    std::wstring parameterPrefixes;
    // PostgreSQL $tag$ ... $tag$ strings.
    bool dollarQuotes = false;
};

// Parses the JSON language description documented in
// docs/61-sql-workbench-components.md. Throws std::invalid_argument.
ONEUI_API CodeLanguage parseCodeLanguage(std::string_view json);

enum class CodeTokenKind : std::uint8_t {
    Plain,
    Keyword,
    Type,
    Function,
    Constant,
    Number,
    String,
    Identifier,
    Comment,
    Operator,
    Parameter,
};

struct CodeToken {
    std::uint32_t start = 0; // UTF-16 column
    std::uint32_t length = 0;
    CodeTokenKind kind = CodeTokenKind::Plain;
};

struct CodeSearchOptions {
    bool regex = false;
    bool matchCase = false;
    bool wholeWord = false;
};

enum class CodeDecorationKind : std::uint8_t {
    Statement, // background of the statement the caret is in
    Error,     // red squiggle with a hover message
    Warning,   // amber squiggle with a hover message
    Running,   // statement being executed: gutter marker and tint
    Highlight, // neutral background
};

// Byte range [start, end) in UTF-8 offsets of the revision it was made for.
struct CodeDecoration {
    std::wstring id;
    std::size_t start = 0;
    std::size_t end = 0;
    CodeDecorationKind kind = CodeDecorationKind::Highlight;
    std::wstring message;
};

struct CodeCompletionItem {
    std::wstring id;
    std::wstring label;
    // Free-form category shown as a badge: table, view, column, function, ...
    std::wstring kind;
    std::wstring detail;
    // Inserted text; empty means the label.
    std::wstring insert;
    // Byte range replaced on acceptance, extended to the caret as the user
    // keeps typing. Both 0 means "the word before the caret".
    std::size_t replaceStart = 0;
    std::size_t replaceEnd = 0;
};

enum class CodeCompletionTrigger : int { Manual = 0, Character = 1, Member = 2 };

// Offsets are UTF-8 byte offsets into the document; line/column are 1-based,
// with the column counted in Unicode scalar values.
// Every color the editor paints. Declarative views fill it from the CodeEditor
// style rule and the --code-* custom properties (codeEditorStyleFromStyleSheet);
// codeEditorStyleFromTheme gives the built-in defaults.
struct CodeEditorStyle {
    Color background{0, 0, 0};
    Color foreground{0, 0, 0};
    Color gutterBackground{0, 0, 0};
    Color gutterForeground{0, 0, 0};
    Color gutterActiveForeground{0, 0, 0};
    Color gutterBorder{0, 0, 0};
    Color currentLine{0, 0, 0};
    Color selection{0, 0, 0};
    Color inactiveSelection{0, 0, 0};
    Color caret{0, 0, 0};
    Color indentGuide{0, 0, 0};
    // Bracket match, active find options and the selected completion.
    Color accent{0, 0, 0};
    Color accentSoft{0, 0, 0};
    Color match{0, 0, 0};
    Color currentMatch{0, 0, 0};
    Color error{0, 0, 0};
    Color warning{0, 0, 0};
    Color success{0, 0, 0};
    // Find bar and completion list.
    Color panelBackground{0, 0, 0};
    Color panelForeground{0, 0, 0};
    Color panelMuted{0, 0, 0};
    Color panelBorder{0, 0, 0};
    Color shadow{0, 0, 0};
    Color scrollbar{0, 0, 0};
    // Indexed by CodeTokenKind.
    std::array<Color, 11> tokens{Color{0, 0, 0}, Color{0, 0, 0}, Color{0, 0, 0}, Color{0, 0, 0}, Color{0, 0, 0}, Color{0, 0, 0}, Color{0, 0, 0}, Color{0, 0, 0}, Color{0, 0, 0}, Color{0, 0, 0}, Color{0, 0, 0}};
    Color token(CodeTokenKind kind) const { return tokens[static_cast<std::size_t>(kind)]; }
};

struct Theme;
ONEUI_API CodeEditorStyle codeEditorStyleFromTheme(const Theme& theme);

// Editing commands for toolbars and menus; keys reach the same actions.
enum class CodeEditorCommand : int {
    Undo = 1,
    Redo = 2,
    Find = 3,
    Replace = 4,
    CloseFind = 5,
    SelectAll = 6,
    ToggleComment = 7,
    Indent = 8,
    Outdent = 9,
    DuplicateLines = 10,
    Cut = 11,
    Copy = 12,
    Paste = 13,
};

struct CodeEditorState {
    std::uint64_t revision = 0;
    std::size_t caret = 0;
    std::size_t anchor = 0;
    std::size_t line = 1;
    std::size_t column = 1;
    std::size_t length = 0;
    std::size_t lineCount = 1;
    std::size_t cursors = 1;
};

class ONEUI_API CodeEditor final : public View {
public:
    CodeEditor();
    ~CodeEditor() override;

    // Document. setText replaces everything and clears undo history.
    void setText(const std::wstring& text);
    std::wstring text() const;
    std::string textUtf8() const;
    std::string textUtf8(std::size_t start, std::size_t end) const;
    std::size_t lineCount() const;
    std::size_t byteLength() const;
    // Incremented by every change, local or external.
    std::uint64_t revision() const;
    // Replaces the text when externalRevision is newer than the last accepted
    // one; stale values are ignored and return false.
    bool setExternalText(const std::wstring& text, std::uint64_t externalRevision);
    // One undoable edit over a UTF-8 byte range. With expectedRevision != 0
    // the edit is rejected when the document changed in between.
    bool replaceRange(std::size_t start, std::size_t end, const std::wstring& text, std::uint64_t expectedRevision = 0);
    // Declarative binding: `text` is applied when `text-revision` grows. With
    // revision 0 the text is initial content and applies until the user edits.
    void setBoundText(std::wstring text);
    void setBoundRevision(std::uint64_t revision);
    // Declarative decorations: the list and its revision arrive separately.
    void setBoundDecorations(std::vector<CodeDecoration> decorations);
    void setBoundDecorationsRevision(std::uint64_t revision);
    CodeEditorState state() const;
    void setSelection(std::size_t anchor, std::size_t caret);

    // Documents. One editor holds several documents and shows one; each keeps
    // its text, language, undo history, selections, scroll position and
    // decorations while hidden. The editor starts on the document "". Selecting
    // an unknown id creates an empty document and returns true; load it with
    // setText. Closing the shown document shows "" (empty when it was never
    // filled); closing "" itself empties it.
    bool selectDocument(const std::wstring& id);
    void closeDocument(const std::wstring& id);
    const std::wstring& documentId() const;
    std::vector<std::wstring> documentIds() const;
    bool canUndo() const;
    bool canRedo() const;
    // Returns false when the command does not apply (read-only, empty history).
    bool execute(CodeEditorCommand command);

    // Presentation.
    void setStyle(CodeEditorStyle style);
    const CodeEditorStyle& style() const;
    void setLanguage(CodeLanguage language);
    void setReadOnly(bool readOnly);
    bool readOnly() const;
    void setWordWrap(bool wrap);
    bool wordWrap() const;
    void setLineNumbers(bool visible);
    void setIndentGuides(bool visible);
    void setTabSize(int size);
    void setInsertSpaces(bool spaces);
    void setFontSize(float size);
    void setClipboard(std::shared_ptr<Clipboard> clipboard);
    // Line comment inserted by toggleComment; defaults to the language's first.
    void setCommentPrefix(std::wstring prefix);

    // Find and replace. The bar is part of the editor (Ctrl+F / Ctrl+H).
    void openFind(bool replace);
    void closeFind();
    bool findOpen() const;
    void setSearch(std::wstring query, CodeSearchOptions options);
    const std::wstring& searchQuery() const;
    std::size_t matchCount() const;
    // Index of the selected match, or -1.
    int currentMatch() const;
    bool findNext(bool backwards = false);
    bool replaceCurrent(const std::wstring& replacement);
    std::size_t replaceAll(const std::wstring& replacement);

    // Decorations made for `revision` (0 = current text). Ranges follow later
    // edits; a range deleted entirely disappears. Decorations older than the
    // retained edit history are dropped until the product republishes them.
    void setDecorations(std::vector<CodeDecoration> decorations, std::uint64_t revision = 0);
    // Current decorations, mapped to the current text.
    std::vector<CodeDecoration> decorations() const;

    // Completion. The editor asks, the product answers with items; the list is
    // drawn at the caret and filtered locally while the user keeps typing.
    void setCompletionItems(std::vector<CodeCompletionItem> items);
    void setCompletionOpen(bool open);
    bool completionOpen() const;
    // Index into the filtered list, or -1.
    int completionIndex() const;
    std::vector<std::wstring> visibleCompletions() const;
    bool acceptCompletion();
    void closeCompletion();
    void requestCompletion(CodeCompletionTrigger trigger);
    void setOnCompletionRequested(std::function<void(std::size_t offset, CodeCompletionTrigger trigger)> callback);
    void setOnCompletionClosed(std::function<void()> callback);
    void setOnCompletionAccepted(std::function<void(const std::wstring& id)> callback);

    // Editing commands, also reachable through keys.
    bool undo();
    bool redo();
    void toggleComment();
    void indentSelection();
    void outdentSelection();
    void duplicateLines();
    void moveLines(int direction);
    void addCursorVertical(int direction);
    void addCursorAt(std::size_t offset);
    void selectAll();
    bool copy();
    bool cut();
    bool paste();
    void insertText(const std::wstring& text);

    // Notifications are coalesced; the first change after a quiet period is
    // reported at once and further ones at most every `throttleMs`.
    void setOnChanged(std::function<void(std::uint64_t)> callback);
    void setOnCursorChanged(std::function<void(const CodeEditorState&)> callback);
    void setNotifyThrottle(double milliseconds);
    // Delivers pending throttled notifications now (tests and teardown).
    void flushNotifications();

    // Inspection, mainly for tests and accessibility.
    std::vector<CodeToken> lineTokens(std::size_t line) const;
    std::size_t relexedLines() const;
    float scrollOffset() const;
    float maxScrollOffset() const;
    void setScrollOffset(float offset);
    std::size_t firstVisibleLine() const;

    void paint(Canvas& canvas) override;
    Rect paintBounds() const override;
    bool hitTest(Point point) const override;
    bool paintsAboveSiblings() const override;
    const std::wstring* tooltipAt(Point point) const override;
    void layoutChildren() override;
    bool onMouseDown(const MouseEvent& event) override;
    bool onMouseMove(const MouseEvent& event) override;
    bool onMouseUp(const MouseEvent& event) override;
    bool onMouseWheel(const MouseWheelEvent& event) override;
    bool onKeyDown(const KeyEvent& event) override;
    bool onTextInput(wchar_t character) override;
    bool onTextCommitted(const std::wstring& text) override;
    TextInputState textInputState() const override;
    void setTextComposition(std::wstring text, std::size_t caret) override;
    bool hasTextComposition() const override;
    bool commitTextComposition() override;
    Rect textInputCaretRect() const override;
    bool onFocusChanged(bool focused) override;
    bool isFocusable() const override { return true; }
    CursorKind cursor(Point point) const override;
    bool tickAnimations(double nowMs) override;
    CommandResult dispatchBuiltinCommandKey(const KeyEvent& event, const std::string& logicalKey) override;
    AccessibilityInfo accessibilityInfo() const override;

private:
    friend struct CodeEditorImpl;
    std::unique_ptr<CodeEditorImpl> impl_;
    std::shared_ptr<TextField> findField_;
    std::shared_ptr<TextField> replaceField_;
};

} // namespace oneui
