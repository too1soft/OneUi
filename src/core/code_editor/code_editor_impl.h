#pragma once

#include "oneui/controls/code_editor.h"
#include "oneui/style.h"

#include "code_document.h"
#include "code_lexer.h"

#include <array>
#include <chrono>
#include <optional>
#include <unordered_map>

namespace oneui {
namespace code {

struct Selection {
    Position anchor;
    Position caret;
    // Remembered x for vertical movement; negative when unset.
    float preferredX = -1.0f;
    Position start() const { return anchor < caret ? anchor : caret; }
    Position end() const { return anchor < caret ? caret : anchor; }
    bool empty() const { return anchor == caret; }
};

struct Edit {
    Position start;
    std::wstring removed;
    std::wstring inserted;
};

enum class EditKind { Other, Typing, Deleting };

struct Transaction {
    std::vector<Edit> edits;
    std::vector<Selection> before;
    std::vector<Selection> after;
    EditKind kind = EditKind::Other;
    double time = 0.0;
};

struct Match {
    Position start;
    Position end;
};

struct Decoration {
    std::wstring id;
    Position start;
    Position end;
    CodeDecorationKind kind = CodeDecorationKind::Highlight;
    std::wstring message;
};

// An applied edit in UTF-8 byte space, tagged with the revision it produced.
struct ByteEdit {
    std::uint64_t revision = 0;
    std::size_t start = 0;
    std::size_t removed = 0;
    std::size_t inserted = 0;
};

// Maps a position through an edit applied at or before it.
Position shiftThrough(Position position, const Edit& edit);

double nowMs();

} // namespace code

struct CodeEditorImpl {
    explicit CodeEditorImpl(CodeEditor& editor);

    CodeEditor& owner;
    CodeEditorStyle style;

    // Documents not on screen, with everything that belongs to their text.
    struct ParkedDocument {
        code::Document doc;
        code::Lexer lexer;
        std::vector<code::Selection> selections{code::Selection{}};
        std::size_t primary = 0;
        std::vector<code::Transaction> undoStack;
        std::vector<code::Transaction> redoStack;
        float scroll = 0.0f;
        float horizontal = 0.0f;
        std::size_t longestLine = 0;
        std::vector<code::Decoration> decorations;
        std::vector<code::ByteEdit> editLog;
        std::uint64_t logFloor = 0;
    };
    std::wstring documentId;
    std::unordered_map<std::wstring, ParkedDocument> parked;
    void park(ParkedDocument& out);
    void unpark(ParkedDocument&& in);
    void styleFindFields(TextField& field) const;
    code::Document doc;
    code::Lexer lexer;
    std::vector<code::Selection> selections{code::Selection{}};
    std::size_t primary = 0;
    std::vector<code::Transaction> undoStack;
    std::vector<code::Transaction> redoStack;
    std::uint64_t revision = 0;
    std::uint64_t externalRevision = 0;
    std::wstring boundText;
    std::uint64_t boundRevision = 0;
    bool boundTextSet = false;
    bool editedSinceBound = false;
    bool readOnly = false;
    bool wrap = false;
    bool lineNumbers = true;
    bool indentGuides = true;
    bool insertSpaces = true;
    int tabSize = 4;
    float fontSize = 13.0f;
    std::wstring commentPrefix;
    std::shared_ptr<Clipboard> clipboard;

    // Geometry and measurement. Advances are measured while painting and
    // cached per character; before the first paint an estimate is used.
    float scroll = 0.0f;
    float horizontal = 0.0f;
    float lineHeight = 20.0f;
    mutable std::array<float, 128> ascii{};
    mutable std::unordered_map<wchar_t, float> wide;
    mutable bool measured = false;
    std::size_t longestLine = 0;
    // Visual rows when wrapping: rowStart[line] is the first row of a line.
    mutable std::vector<std::size_t> rowStart;
    mutable bool rowsValid = false;
    mutable float rowsWidth = -1.0f;

    // IME preedit shown at the primary caret.
    std::wstring preedit;
    std::size_t preeditCaret = 0;

    // Find.
    bool findVisible = false;
    bool replaceVisible = false;
    std::wstring query;
    CodeSearchOptions options;
    std::vector<code::Match> matches;
    int current = -1;
    bool matchesDirty = true;
    std::wstring searchError;
    mutable std::wstring findTooltip;

    // Throttled notifications.
    std::function<void(std::uint64_t)> onChanged;
    std::function<void(const CodeEditorState&)> onCursor;
    double throttle = 150.0;
    double lastChanged = -1e18;
    double lastCursor = -1e18;
    bool changedPending = false;
    bool cursorPending = false;

    // Pointer state.
    enum class Drag { None, Text, VerticalBar, HorizontalBar };
    Drag drag = Drag::None;
    float dragGrab = 0.0f;
    std::size_t clickCount = 0;
    code::Position wordAnchorStart, wordAnchorEnd;

    // --- geometry (code_editor_view.cpp)
    float advance(wchar_t c) const;
    void measure(Canvas& canvas) const;
    float spaceWidth() const { return advance(L' '); }
    float gutterWidth() const;
    Rect textRect() const;
    Rect findBarRect() const;
    float columnX(std::size_t line, std::size_t segmentStart, std::size_t column) const;
    std::vector<std::size_t> segments(std::size_t line) const;
    void ensureRows() const;
    std::size_t rowCount() const;
    std::size_t rowOfLine(std::size_t line) const;
    std::pair<std::size_t, std::size_t> lineOfRow(std::size_t row) const; // line, segment index
    std::size_t rowOf(code::Position position) const;
    float contentHeight() const;
    float contentWidth() const;
    float maxScroll() const;
    float maxHorizontal() const;
    Rect caretRect(code::Position position) const;
    code::Position hit(Point point) const;
    void setScroll(float value);
    void setHorizontal(float value);
    void reveal(code::Position position);
    Rect verticalThumb() const;
    Rect horizontalThumb() const;
    void invalidateRows() { rowsValid = false; }

    // --- editing (code_editor_edit.cpp)
    void apply(code::Transaction transaction, bool merge);
    void applyEdit(const code::Edit& edit);
    void changed(const code::LineChange& change);
    void replaceSelections(const std::vector<std::wstring>& texts, code::EditKind kind);
    void replaceSelections(const std::wstring& text, code::EditKind kind);
    void deleteBackward(bool word);
    void deleteForward(bool word);
    void newline();
    void normalizeSelections();
    void move(int dx, int dy, bool extend, bool word);
    void home(bool extend, bool document);
    void end(bool extend, bool document);
    void page(int direction, bool extend);
    code::Position wordStart(code::Position position) const;
    code::Position wordEnd(code::Position position) const;
    std::pair<std::size_t, std::size_t> selectedLines(const code::Selection& selection) const;
    void editLines(const std::function<std::wstring(std::size_t line, const std::wstring& text)>& transform);
    bool undo();
    bool redo();
    void resetHistory();
    void touched();
    void cursorMoved();
    void notify(bool force);
    CodeEditorState state() const;
    std::optional<std::pair<code::Position, code::Position>> bracketMatch() const;

    // --- find (code_editor_find.cpp)
    void refreshMatches();
    bool findNext(bool backwards);
    bool replaceCurrent(const std::wstring& replacement);
    std::size_t replaceAll(const std::wstring& replacement);
    void selectMatch(int index);

    // Decorations and the byte-level edit log used to map late decorations.
    std::vector<code::Decoration> decorations;
    std::vector<code::ByteEdit> editLog;
    std::vector<CodeDecoration> boundDecorations;
    std::uint64_t boundDecorationsRevision = 0;
    std::uint64_t logFloor = 0;

    // Completion.
    std::vector<CodeCompletionItem> completionItems;
    std::vector<std::size_t> completionFiltered;
    int completionSelected = -1;
    int completionTop = 0;
    bool completionWanted = false;
    bool completionVisible = false;
    code::Position completionStart;
    std::function<void(std::size_t, CodeCompletionTrigger)> onCompletionRequested;
    std::function<void()> onCompletionClosed;
    std::function<void(const std::wstring&)> onCompletionAccepted;

    // --- decorations and completion (code_editor_assist.cpp)
    void logEdit(const code::Edit& edit);
    void mapDecorations(const code::Edit& edit);
    code::Position wordStartBefore(code::Position caret) const;
    void filterCompletions();
    void updateCompletionVisibility();
    void requestCompletion(CodeCompletionTrigger trigger);
    void closeCompletion();
    bool acceptCompletion();
    void moveCompletion(int delta);
    void afterTyping(const std::wstring& text);
    Rect completionRect() const;
    int completionRowAt(Point point) const;
    static constexpr float completionRowHeight = 22.0f;
    static constexpr int completionRows = 10;

    // --- painting (code_editor_paint.cpp)
    struct FindLayout {
        Rect bar, find, replace, matchCase, regex, word, count, previous, next, close, replaceOne, replaceAll;
    };
    FindLayout findLayout() const;
    void paint(Canvas& canvas, bool focused, bool caretVisible);
};

} // namespace oneui
