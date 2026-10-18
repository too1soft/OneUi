#include "oneui/clipboard.h"
#include "oneui/controls/code_editor.h"
#include "oneui/localization.h"
#include "oneui/style_adapter.h"

#include "core/code_editor/code_document.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

int failures = 0;

class RecordingCanvas final : public oneui::Canvas {
public:
    void save() override {}
    void restore() override {}
    void clipRect(oneui::Rect) override {}
    void clear(oneui::Color) override {}
    void fillRect(oneui::Rect, oneui::Color, float = 0.0f) override {}
    void strokeRect(oneui::Rect, oneui::Color, float, float = 1.0f) override {}
    void fillEllipse(oneui::Rect, oneui::Color) override {}
    void strokeEllipse(oneui::Rect, oneui::Color, float = 1.0f) override {}
    void drawLine(oneui::Point, oneui::Point, oneui::Color, float = 1.0f) override {}
    void drawText(const std::wstring& text, oneui::Rect, oneui::Color, float, oneui::TextAlign = oneui::TextAlign::Center) override {
        texts.push_back(text);
    }
    float measureTextWidth(const std::wstring& text, float, int = 400) const override {
        float width = 0.0f;
        for (const auto c : text) width += c >= 0x2e80 ? 14.0f : 7.0f;
        return width;
    }
    std::vector<std::wstring> texts;
};

void expectTrue(const char* name, bool value) {
    if (!value) {
        std::cerr << name << " failed\n";
        ++failures;
    }
}

template <typename T>
void expectEqual(const char* name, T actual, T expected) {
    if (!(actual == expected)) {
        std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
        ++failures;
    }
}

void expectText(const char* name, const std::wstring& actual, const std::wstring& expected) {
    if (actual != expected) {
        std::wcerr << name << L": got [" << actual << L"]\n";
        ++failures;
    }
}

const char* sqlLanguage = R"({
  "case_sensitive": false,
  "keywords": ["SELECT", "FROM", "WHERE", "AND", "AS"],
  "types": ["INT", "VARCHAR"],
  "functions": ["COUNT", "NOW"],
  "constants": ["NULL", "TRUE"],
  "line_comments": ["--", "#"],
  "block_comments": [["/*", "*/"]],
  "string_quotes": "'\"",
  "backslash_escapes": true,
  "identifier_quotes": ["``", "[]"],
  "parameter_prefixes": ":@?",
  "dollar_quotes": true,
  "future_key": {"ignored": [1, 2, {"x": null}]}
})";

oneui::KeyEvent key(oneui::Key k, bool shift = false, bool control = false, bool alt = false) {
    oneui::KeyEvent event;
    event.key = k;
    event.shift = shift;
    event.control = control;
    event.alt = alt;
    return event;
}

oneui::KeyEvent virtualKey(unsigned code, bool control = true, bool shift = false) {
    oneui::KeyEvent event;
    event.virtualKey = code;
    event.control = control;
    event.shift = shift;
    return event;
}

std::unique_ptr<oneui::CodeEditor> editor(const std::wstring& text) {
    auto e = std::make_unique<oneui::CodeEditor>();
    e->setFrame(oneui::Rect{0.0f, 0.0f, 800.0f, 400.0f});
    e->setLanguage(oneui::parseCodeLanguage(sqlLanguage));
    e->setText(text);
    RecordingCanvas canvas;
    e->paint(canvas);
    return e;
}

void testDocumentOffsets() {
    oneui::code::Document doc;
    doc.setText(L"ab\r\n中\U0001F4E6x\rlast");
    expectEqual("CRLF and CR are normalized", doc.lineCount(), std::size_t{3});
    expectEqual("byte length counts UTF-8", doc.byteLength(), std::size_t{2 + 1 + 3 + 4 + 1 + 1 + 4});
    const oneui::code::Position x{1, 3};
    expectEqual("byte offset after CJK and emoji", doc.byteOffset(x), std::size_t{10});
    expectTrue("byte offsets round-trip", doc.fromByteOffset(10) == x);
    expectTrue("offset inside a character snaps back", doc.fromByteOffset(8) == (oneui::code::Position{1, 1}));
    const auto end = doc.insert({0, 1}, L"1\n2\n3");
    expectTrue("insert returns the end", end == (oneui::code::Position{2, 1}));
    expectText("multi-line insert", doc.text(), L"a1\n2\n3b\n中\U0001F4E6x\nlast");
    doc.erase({0, 1}, {2, 1});
    expectText("multi-line erase", doc.text(), L"ab\n中\U0001F4E6x\nlast");
    expectEqual("prefix sums follow edits", doc.byteOffset({2, 0}), std::size_t{3 + 8 + 1});
    expectTrue("clamp never splits a surrogate pair", doc.clamp({1, 2}) == (oneui::code::Position{1, 1}));
}

void testLanguageAndTokens() {
    auto e = editor(L"SELECT count(*) AS n, 'it''s' FROM `t` -- note\n/* multi\nline */ WHERE x = :id AND y = $$a$$");
    const auto tokens = e->lineTokens(0);
    auto kindAt = [&](const std::vector<oneui::CodeToken>& line, std::uint32_t column) {
        for (const auto& t : line)
            if (column >= t.start && column < t.start + t.length) return t.kind;
        return oneui::CodeTokenKind::Plain;
    };
    expectTrue("keyword", kindAt(tokens, 0) == oneui::CodeTokenKind::Keyword);
    expectTrue("lower-case function call", kindAt(tokens, 7) == oneui::CodeTokenKind::Function);
    expectTrue("doubled quote stays in the string", kindAt(tokens, 24) == oneui::CodeTokenKind::String);
    expectTrue("quoted identifier", kindAt(tokens, 35) == oneui::CodeTokenKind::Identifier);
    expectTrue("line comment", kindAt(tokens, 41) == oneui::CodeTokenKind::Comment);
    expectTrue("block comment continues", kindAt(e->lineTokens(1), 2) == oneui::CodeTokenKind::Comment);
    const auto last = e->lineTokens(2);
    expectTrue("block comment closes", kindAt(last, 0) == oneui::CodeTokenKind::Comment && kindAt(last, 8) == oneui::CodeTokenKind::Keyword);
    expectTrue("named parameter", kindAt(last, 18) == oneui::CodeTokenKind::Parameter);
    expectTrue("dollar quote", kindAt(last, 31) == oneui::CodeTokenKind::String);
    bool threw = false;
    try {
        oneui::parseCodeLanguage("{\"keywords\": [1]}");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    expectTrue("bad descriptions are rejected", threw);
}

void testIncrementalLexing() {
    std::wstring text;
    for (int i = 0; i < 2000; ++i) text += L"SELECT " + std::to_wstring(i) + L" FROM t;\n";
    auto e = editor(text);
    e->setSelection(0, 0);
    e->insertText(L"x");
    expectTrue("a plain edit relexes one line", e->relexedLines() <= 2);
    e->setSelection(0, 0);
    e->insertText(L"/*");
    expectTrue("opening a comment relexes to the end", e->relexedLines() >= 1999);
    expectTrue("everything below became a comment", e->lineTokens(1500).front().kind == oneui::CodeTokenKind::Comment);
    e->undo();
    expectTrue("undo restores colors", e->lineTokens(1500).front().kind == oneui::CodeTokenKind::Keyword);
}

void testUndoMergesTyping() {
    auto e = editor(L"");
    for (const auto c : std::wstring(L"select")) e->onTextInput(c);
    e->onKeyDown(key(oneui::Key::Enter));
    for (const auto c : std::wstring(L"from")) e->onTextInput(c);
    expectText("typed text", e->text(), L"select\nfrom");
    e->undo();
    expectText("undo removes the last word", e->text(), L"select\n");
    e->undo();
    expectText("newline is its own step", e->text(), L"select");
    e->undo();
    expectText("typing merged into one step", e->text(), L"");
    e->redo();
    e->redo();
    expectText("redo", e->text(), L"select\n");
    e->onKeyDown(key(oneui::Key::Backspace));
    e->onKeyDown(key(oneui::Key::Backspace));
    e->onKeyDown(key(oneui::Key::Backspace));
    expectText("backspace", e->text(), L"sele");
    e->undo();
    expectText("deletes merge too", e->text(), L"select\n");
}

void testMultiCursorEditing() {
    auto e = editor(L"a\nb\nc");
    e->setSelection(0, 0);
    e->addCursorVertical(1);
    e->addCursorVertical(1);
    expectEqual("three cursors", e->state().cursors, std::size_t{3});
    e->onTextInput(L'-');
    expectText("each line edited", e->text(), L"-a\n-b\n-c");
    e->onKeyDown(key(oneui::Key::End));
    e->insertText(L";");
    expectText("end of each line", e->text(), L"-a;\n-b;\n-c;");
    e->undo();
    e->undo();
    expectText("undo all cursors", e->text(), L"a\nb\nc");
    e->onKeyDown(key(oneui::Key::Escape));
    expectEqual("Esc keeps one cursor", e->state().cursors, std::size_t{1});
    auto clipboard = std::make_shared<oneui::MemoryClipboard>();
    e->setClipboard(clipboard);
    clipboard->setText(L"1\n2\n3");
    e->setSelection(0, 0);
    e->addCursorAt(2);
    e->addCursorAt(4);
    e->paste();
    expectText("one pasted line per cursor", e->text(), L"1a\n2b\n3c");
}

void testLineCommands() {
    auto e = editor(L"select 1\n  from t\n\nwhere x");
    e->setSelection(0, 17);
    e->toggleComment();
    expectText("comment aligned at the smallest indent", e->text(), L"-- select 1\n--   from t\n\nwhere x");
    e->toggleComment();
    expectText("uncomment", e->text(), L"select 1\n  from t\n\nwhere x");
    e->indentSelection();
    expectText("indent", e->text(), L"    select 1\n      from t\n\nwhere x");
    e->outdentSelection();
    e->outdentSelection();
    expectText("outdent", e->text(), L"select 1\nfrom t\n\nwhere x");
    e->setSelection(0, 0);
    e->duplicateLines();
    expectText("duplicate", e->text(), L"select 1\nselect 1\nfrom t\n\nwhere x");
    expectEqual("caret follows the copy", e->state().line, std::size_t{2});
    e->moveLines(1);
    expectText("move down", e->text(), L"select 1\nfrom t\nselect 1\n\nwhere x");
    e->moveLines(-1);
    e->moveLines(-1);
    expectText("move up twice", e->text(), L"select 1\nselect 1\nfrom t\n\nwhere x");
    expectTrue("Ctrl+/ toggles comments", e->dispatchBuiltinCommandKey(virtualKey(0xbf), {}) == oneui::CommandResult::Executed);
    expectText("shortcut comment", e->text(), L"-- select 1\nselect 1\nfrom t\n\nwhere x");
}

void testFindAndReplace() {
    auto e = editor(L"select id from users;\nSELECT ID FROM users_old;\nselect idx");
    e->setSearch(L"id", {});
    expectEqual("case-insensitive by default", e->matchCount(), std::size_t{3});
    e->setSearch(L"id", {false, true, false});
    expectEqual("match case", e->matchCount(), std::size_t{2});
    e->setSearch(L"id", {false, false, true});
    expectEqual("whole word", e->matchCount(), std::size_t{2});
    e->setSearch(L"users(_\\w+)?", {true, false, false});
    expectEqual("regex", e->matchCount(), std::size_t{2});
    e->setSelection(0, 0);
    expectTrue("find next", e->findNext());
    expectEqual("first match selected", e->state().anchor, std::size_t{15});
    e->setSearch(L"(\\w+)_old", {true, false, false});
    e->setSelection(0, 0);
    e->findNext();
    e->replaceCurrent(L"$1_new");
    expectText("regex replacement with groups", e->text(), L"select id from users;\nSELECT ID FROM users_new;\nselect idx");
    e->setSearch(L"select", {});
    expectEqual("replace all", e->replaceAll(L"SELECT"), std::size_t{3});
    expectText("replaced", e->text(), L"SELECT id from users;\nSELECT ID FROM users_new;\nSELECT idx");
    e->undo();
    expectText("replace all is one undo step", e->text(), L"select id from users;\nSELECT ID FROM users_new;\nselect idx");
    e->setSearch(L"(", {true, false, false});
    expectEqual("invalid regex finds nothing", e->matchCount(), std::size_t{0});
    expectTrue("Ctrl+F opens the bar", e->dispatchBuiltinCommandKey(key(oneui::Key::Other, false, true), "f") == oneui::CommandResult::Executed && e->findOpen());
    RecordingCanvas canvas;
    e->paint(canvas);
    e->onKeyDown(key(oneui::Key::Escape));
    expectTrue("Esc closes the bar", !e->findOpen());
}

void testSynchronization() {
    auto e = editor(L"select 1");
    std::vector<std::uint64_t> changes;
    std::vector<oneui::CodeEditorState> cursors;
    e->setNotifyThrottle(10000.0);
    e->setOnChanged([&](std::uint64_t revision) { changes.push_back(revision); });
    e->setOnCursorChanged([&](const oneui::CodeEditorState& state) { cursors.push_back(state); });
    e->setSelection(8, 8);
    e->insertText(L";");
    expectEqual("first change is reported at once", changes.size(), std::size_t{1});
    e->insertText(L" ");
    e->insertText(L"x");
    expectEqual("further changes are coalesced", changes.size(), std::size_t{1});
    e->flushNotifications();
    expectEqual("flush delivers the pending change", changes.size(), std::size_t{2});
    expectEqual("with the latest revision", changes.back(), e->revision());
    expectTrue("cursor reports UTF-8 offsets", !cursors.empty() && cursors.back().caret == 11);
    const auto revision = e->revision();
    expectTrue("stale range edits are rejected", !e->replaceRange(0, 6, L"SELECT", revision - 1));
    expectTrue("current range edits apply", e->replaceRange(0, 6, L"SELECT", revision));
    expectText("range edit", e->text(), L"SELECT 1; x");
    expectTrue("external text with a new revision", e->setExternalText(L"one", 5));
    expectTrue("older external revisions are ignored", !e->setExternalText(L"two", 4) && !e->setExternalText(L"two", 5));
    expectText("external text", e->text(), L"one");
    e->setText(L"中文 abc");
    expectEqual("UTF-8 range read", e->textUtf8(7, 10), std::string("abc"));
}

void testImeAndKeyRouting() {
    auto e = editor(L"");
    e->setTextComposition(L"zhong", 5);
    expectTrue("composition is held, not inserted", e->hasTextComposition() && e->text().empty());
    e->onTextCommitted(L"中文");
    expectText("commit inserts once", e->text(), L"中文");
    expectTrue("composition cleared", !e->hasTextComposition());
    expectTrue("Ctrl+Enter is left to the page", !e->onKeyDown(key(oneui::Key::Enter, false, true)));
    expectTrue("Ctrl+Enter is not an editor command",
               e->dispatchBuiltinCommandKey(key(oneui::Key::Enter, false, true), {}) == oneui::CommandResult::NotFound);
    oneui::KeyEvent f5;
    f5.virtualKey = 0x74;
    expectTrue("F5 is left to the page", !e->onKeyDown(f5) &&
               e->dispatchBuiltinCommandKey(f5, {}) == oneui::CommandResult::NotFound);
    expectTrue("Ctrl+Z is an editor command", e->dispatchBuiltinCommandKey(key(oneui::Key::Other, false, true), "z") == oneui::CommandResult::Executed);
    e->setReadOnly(true);
    e->onTextInput(L'x');
    expectText("read-only ignores typing", e->text(), L"");
}

void testLargeDocumentPerformance() {
    std::wstring text;
    text.reserve(100000 * 48);
    for (int i = 0; i < 100000; ++i)
        text += L"SELECT id, name, created_at FROM t" + std::to_wstring(i % 97) + L" WHERE id = " + std::to_wstring(i) + L";\n";
    oneui::CodeEditor e;
    e.setFrame(oneui::Rect{0.0f, 0.0f, 1000.0f, 700.0f});
    e.setLanguage(oneui::parseCodeLanguage(sqlLanguage));
    auto start = std::chrono::steady_clock::now();
    e.setText(text);
    RecordingCanvas first;
    e.paint(first);
    const auto openMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    std::cout << "code editor open (100k lines, incl. lexing and first paint): " << openMs << " ms\n";
    expectTrue("opening stays under 300 ms", openMs < 300.0);
    expectEqual("lines", e.lineCount(), std::size_t{100001});
    e.setSelection(e.byteLength(), e.byteLength());
    double worst = 0.0;
    for (int i = 0; i < 50; ++i) {
        const auto t0 = std::chrono::steady_clock::now();
        e.onTextInput(L'a');
        RecordingCanvas canvas;
        e.paint(canvas);
        worst = std::max(worst, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
    }
    std::cout << "code editor keystroke at the end (edit + paint, worst of 50): " << worst << " ms\n";
    expectTrue("keystrokes stay under 16 ms", worst < 16.0);
    expectTrue("the end is scrolled into view", e.firstVisibleLine() > 99000);
    e.setWordWrap(true);
    start = std::chrono::steady_clock::now();
    RecordingCanvas wrapped;
    e.paint(wrapped);
    std::cout << "code editor wrap layout (100k lines): "
              << std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() << " ms\n";
    worst = 0.0;
    for (int i = 0; i < 20; ++i) {
        const auto t0 = std::chrono::steady_clock::now();
        e.onTextInput(L'b');
        RecordingCanvas canvas;
        e.paint(canvas);
        worst = std::max(worst, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
    }
    std::cout << "code editor keystroke with wrapping (worst of 20): " << worst << " ms\n";
    expectTrue("wrapped keystrokes stay under 16 ms", worst < 16.0);
}

void testDecorationsFollowEdits() {
    auto e = editor(L"select 1;\nselect 2;");
    e->setDecorations({{L"err", 17, 18, oneui::CodeDecorationKind::Error, L"bad literal"},
                       {L"stmt", 10, 19, oneui::CodeDecorationKind::Statement, L""}});
    e->setSelection(0, 0);
    e->insertText(L"xx");
    auto decorations = e->decorations();
    expectTrue("decorations shift with edits", decorations.size() == 2 && decorations[0].start == 19 && decorations[0].end == 20 &&
                                                  decorations[1].start == 12 && decorations[1].end == 21);
    // Hover over the squiggle shows its message (gutter 43, text at 49, 7 px per character, 20 px lines).
    RecordingCanvas canvas;
    e->paint(canvas);
    const auto* tip = e->tooltipAt(oneui::Point{49.0f + 7.0f * 7.0f + 3.0f, 4.0f + 20.0f + 10.0f});
    expectTrue("error decorations explain themselves on hover", tip && *tip == L"bad literal");
    expectTrue("no tooltip elsewhere", e->tooltipAt(oneui::Point{49.0f + 2.0f, 4.0f + 10.0f}) == nullptr);
    e->setSelection(11, 21);
    e->insertText(L"");
    e->onKeyDown(key(oneui::Key::Backspace));
    expectEqual("deleting the text removes its decorations", e->decorations().size(), std::size_t{0});
    // Decorations computed for an older revision are mapped forward.
    e->setText(L"select 1;\nselect 2;");
    const auto revision = e->revision();
    e->setSelection(0, 0);
    e->insertText(L"-- ");
    e->setDecorations({{L"late", 17, 18, oneui::CodeDecorationKind::Warning, L"late"}}, revision);
    decorations = e->decorations();
    expectTrue("late decorations are mapped through newer edits", decorations.size() == 1 && decorations[0].start == 20);
    e->setDecorations({{L"future", 0, 1, oneui::CodeDecorationKind::Error, L""}}, e->revision() + 5);
    expectEqual("decorations from an unknown revision are dropped", e->decorations().size(), std::size_t{0});
}

void testCompletionFlow() {
    auto e = editor(L"");
    std::vector<std::pair<std::size_t, oneui::CodeCompletionTrigger>> requests;
    int closed = 0;
    std::vector<std::wstring> accepted;
    e->setOnCompletionRequested([&](std::size_t offset, oneui::CodeCompletionTrigger trigger) { requests.push_back({offset, trigger}); });
    e->setOnCompletionClosed([&] { ++closed; });
    e->setOnCompletionAccepted([&](const std::wstring& id) { accepted.push_back(id); });
    auto answer = [&] {
        e->setCompletionItems({{L"k:select", L"SELECT", L"keyword", L"", L"SELECT ", 0, 0},
                               {L"k:set", L"SET", L"keyword", L"", L"", 0, 0},
                               {L"t:sessions", L"sessions", L"table", L"public", L"", 0, 0},
                               {L"f:substr", L"SUBSTR", L"function", L"text", L"SUBSTR(", 0, 0}});
        e->setCompletionOpen(true);
    };
    e->onTextInput(L's');
    expectTrue("a new word asks for completions", requests.size() == 1 && requests[0].second == oneui::CodeCompletionTrigger::Character);
    answer();
    expectTrue("the list opens", e->completionOpen() && e->visibleCompletions().size() == 4);
    e->onTextInput(L'e');
    expectTrue("typing filters locally", e->visibleCompletions().size() == 3);
    expectTrue("and asks again", requests.size() == 2);
    e->onKeyDown(key(oneui::Key::Down));
    expectEqual("arrow keys move the selection", e->completionIndex(), 1);
    e->onKeyDown(key(oneui::Key::Up));
    e->onKeyDown(key(oneui::Key::Enter));
    expectText("Enter accepts the insert text", e->text(), L"SELECT ");
    expectTrue("acceptance closes and reports", !e->completionOpen() && closed == 1 && accepted.size() == 1 && accepted[0] == L"k:select");
    e->onTextInput(L'u');
    e->onTextInput(L'.');
    expectTrue("a dot asks for members", requests.back().second == oneui::CodeCompletionTrigger::Member && requests.back().first == 9);
    e->setCompletionItems({{L"c:id", L"id", L"column", L"int", L"", 0, 0}, {L"c:name", L"name", L"column", L"text", L"", 0, 0}});
    e->setCompletionOpen(true);
    expectTrue("member list opens", e->completionOpen() && e->visibleCompletions().size() == 2);
    RecordingCanvas canvas;
    e->paint(canvas);
    expectTrue("the open list paints above siblings", e->paintsAboveSiblings());
    e->onKeyDown(key(oneui::Key::Tab));
    expectText("Tab accepts", e->text(), L"SELECT u.id");
    e->onTextInput(L' ');
    e->onTextInput(L'n');
    answer();
    e->onKeyDown(key(oneui::Key::Escape));
    expectTrue("Esc closes without inserting", !e->completionOpen() && e->text() == L"SELECT u.id n");
    e->onKeyDown(key(oneui::Key::Backspace));
    expectTrue("Ctrl+Space asks explicitly", e->dispatchBuiltinCommandKey(key(oneui::Key::Space, false, true), {}) ==
                                                 oneui::CommandResult::Executed &&
                                             requests.back().second == oneui::CodeCompletionTrigger::Manual);
    answer();
    e->onKeyDown(key(oneui::Key::Backspace));
    expectTrue("deleting past the word start closes the list", !e->completionOpen());
}

class ColorCanvas final : public oneui::Canvas {
public:
    void save() override {}
    void restore() override {}
    void clipRect(oneui::Rect) override {}
    void clear(oneui::Color) override {}
    void fillRect(oneui::Rect, oneui::Color color, float = 0.0f) override { fills.push_back(color); }
    void strokeRect(oneui::Rect, oneui::Color, float, float = 1.0f) override {}
    void fillEllipse(oneui::Rect, oneui::Color) override {}
    void strokeEllipse(oneui::Rect, oneui::Color, float = 1.0f) override {}
    void drawLine(oneui::Point, oneui::Point, oneui::Color, float = 1.0f) override {}
    void drawText(const std::wstring& text, oneui::Rect, oneui::Color, float, oneui::TextAlign = oneui::TextAlign::Center) override {
        texts.push_back(text);
    }
    float measureTextWidth(const std::wstring& text, float, int = 400) const override {
        return 7.0f * static_cast<float>(text.size());
    }
    std::vector<oneui::Color> fills;
    std::vector<std::wstring> texts;
};

bool same(oneui::Color a, oneui::Color b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; }

void testDocumentsKeepTheirState() {
    oneui::CodeEditor editor;
    editor.setFrame({0.0f, 0.0f, 600.0f, 300.0f});
    editor.setLanguage(oneui::parseCodeLanguage(R"({"keywords":["SELECT"]})"));
    editor.setText(L"select 1");
    editor.setSelection(3, 3);
    editor.insertText(L"X");
    expectTrue("documents: a new id is created", editor.selectDocument(L"b"));
    expectText("documents: a new document starts empty", editor.text(), L"");
    expectTrue("documents: a new document has no history", !editor.canUndo());
    editor.setLanguage(oneui::parseCodeLanguage(R"({"case_sensitive":true,"keywords":["echo"]})"));
    editor.setText(L"echo hi");
    editor.setSelection(5, 7);
    expectTrue("documents: returning is not a creation", !editor.selectDocument(L""));
    expectText("documents: text is restored", editor.text(), L"selXect 1");
    expectEqual("documents: caret is restored", editor.state().caret, std::size_t{4});
    expectTrue("documents: undo history is kept", editor.canUndo());
    expectTrue("documents: undo command applies", editor.execute(oneui::CodeEditorCommand::Undo));
    expectText("documents: undo restores the text", editor.text(), L"select 1");
    expectTrue("documents: language is kept",
               !editor.lineTokens(0).empty() && editor.lineTokens(0)[0].kind == oneui::CodeTokenKind::Keyword);
    expectTrue("documents: redo becomes available", editor.canRedo());
    editor.selectDocument(L"b");
    expectEqual("documents: other selection kept", editor.state().anchor, std::size_t{5});
    expectTrue("documents: other language kept",
               !editor.lineTokens(0).empty() && editor.lineTokens(0)[0].kind == oneui::CodeTokenKind::Keyword);
    expectEqual("documents: both are listed", editor.documentIds().size(), std::size_t{2});
    editor.closeDocument(L"b");
    expectText("documents: closing the shown one shows \"\"", editor.text(), L"select 1");
    expectEqual("documents: closed id is gone", editor.documentIds().size(), std::size_t{1});
    expectTrue("documents: reopening creates again", editor.selectDocument(L"b"));
    expectText("documents: a closed document is not restored", editor.text(), L"");
}

void testCommands() {
    oneui::CodeEditor editor;
    editor.setFrame({0.0f, 0.0f, 600.0f, 300.0f});
    editor.setText(L"a\nb");
    expectTrue("commands: nothing to undo yet", !editor.execute(oneui::CodeEditorCommand::Undo));
    expectTrue("commands: find opens", editor.execute(oneui::CodeEditorCommand::Find) && editor.findOpen());
    expectTrue("commands: close find", editor.execute(oneui::CodeEditorCommand::CloseFind) && !editor.findOpen());
    expectTrue("commands: nothing to close", !editor.execute(oneui::CodeEditorCommand::CloseFind));
    editor.execute(oneui::CodeEditorCommand::SelectAll);
    editor.execute(oneui::CodeEditorCommand::Indent);
    expectText("commands: indent", editor.text(), L"    a\n    b");
    editor.setReadOnly(true);
    expectTrue("commands: replace needs an editable document", !editor.execute(oneui::CodeEditorCommand::Replace));
    expectTrue("commands: read-only refuses edits", !editor.execute(oneui::CodeEditorCommand::Outdent));
    expectTrue("commands: find still works read-only", editor.execute(oneui::CodeEditorCommand::Find));
}

void testStyleFromStyleSheet() {
    oneui::StyleSheet sheet;
    std::string error;
    expectTrue("style: css parses", sheet.addRulesFromCss(
        ":root { --accent: #00ff00; --code-accent: var(--accent); --code-keyword: #ff0000; }\n"
        "code-editor.dark { background-color: #101010; color: #eeeeee; caret-color: #ffcc00; }", &error));
    const auto s = oneui::codeEditorStyleFromStyleSheet(sheet, oneui::StyleNode{"code-editor", {"dark"}});
    expectTrue("style: background", same(s.background, oneui::Color{16, 16, 16}));
    expectTrue("style: foreground", same(s.foreground, oneui::Color{238, 238, 238}));
    expectTrue("style: caret", same(s.caret, oneui::Color{255, 204, 0}));
    expectTrue("style: gutter follows the surface", same(s.gutterBackground, s.background));
    expectTrue("style: panel derives from the surface", s.panelBackground.r < 64);
    expectTrue("style: keyword variable", same(s.token(oneui::CodeTokenKind::Keyword), oneui::Color{255, 0, 0}));
    expectTrue("style: var() chains resolve", same(s.accent, oneui::Color{0, 255, 0}));
    expectTrue("style: plain text follows color", same(s.token(oneui::CodeTokenKind::Plain), s.foreground));

    oneui::CodeEditor editor;
    editor.setFrame({0.0f, 0.0f, 400.0f, 200.0f});
    editor.setStyle(s);
    ColorCanvas canvas;
    editor.paint(canvas);
    expectTrue("style: the editor paints its background", !canvas.fills.empty() && same(canvas.fills.front(), s.background));
}

void testFindBarTextIsLocalized() {
    oneui::setBuiltinText(oneui::BuiltinText::FindNoResults, L"无结果");
    oneui::setBuiltinText(oneui::BuiltinText::FindMatchCount, L"{0}/{1}");
    oneui::CodeEditor editor;
    editor.setFrame({0.0f, 0.0f, 600.0f, 300.0f});
    editor.setText(L"alpha beta alpha");
    editor.openFind(false);
    editor.setSearch(L"zeta", {});
    ColorCanvas canvas;
    editor.paint(canvas);
    expectTrue("find text: no results", std::find(canvas.texts.begin(), canvas.texts.end(), L"无结果") != canvas.texts.end());
    editor.setSearch(L"alpha", {});
    editor.findNext();
    canvas.texts.clear();
    editor.paint(canvas);
    expectTrue("find text: count template", std::find(canvas.texts.begin(), canvas.texts.end(), L"1/2") != canvas.texts.end() ||
                                                std::find(canvas.texts.begin(), canvas.texts.end(), L"2/2") != canvas.texts.end());
    oneui::setBuiltinText(oneui::BuiltinText::FindNoResults, std::nullopt);
    oneui::setBuiltinText(oneui::BuiltinText::FindMatchCount, std::nullopt);
}

} // namespace

int main() {
    testDocumentsKeepTheirState();
    testCommands();
    testStyleFromStyleSheet();
    testFindBarTextIsLocalized();
    testDocumentOffsets();
    testLanguageAndTokens();
    testIncrementalLexing();
    testUndoMergesTyping();
    testMultiCursorEditing();
    testLineCommands();
    testFindAndReplace();
    testSynchronization();
    testImeAndKeyRouting();
    testDecorationsFollowEdits();
    testCompletionFlow();
    testLargeDocumentPerformance();
    if (failures) std::cerr << failures << " code editor failure(s)\n";
    return failures ? 1 : 0;
}
