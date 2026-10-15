#include "code_editor_impl.h"
#include "oneui/localization.h"

#include "oneui/clipboard.h"
#include "oneui/controls/text_field.h"
#include "internal/unicode.h"

#include <algorithm>
#include <cmath>
#include <cwctype>

namespace oneui {
namespace {

using code::Position;
using code::Selection;

constexpr unsigned slashKey = 0xbf, openBracketKey = 0xdb, closeBracketKey = 0xdd, f3Key = 0x72;

std::wstring indentUnit(const CodeEditorImpl& impl, std::size_t column) {
    if (!impl.insertSpaces) return L"\t";
    const auto unit = static_cast<std::size_t>(std::max(1, impl.tabSize));
    return std::wstring(unit - column % unit, L' ');
}

bool leadingPrefix(const std::wstring& text, const std::wstring& prefix, std::size_t& at) {
    at = 0;
    while (at < text.size() && (text[at] == L' ' || text[at] == L'\t')) ++at;
    return text.compare(at, prefix.size(), prefix) == 0;
}

} // namespace

CodeEditor::CodeEditor()
    : impl_(std::make_unique<CodeEditorImpl>(*this)),
      findField_(std::make_shared<TextField>()),
      replaceField_(std::make_shared<TextField>()) {
    setAccessibleRole(AccessibilityRole::TextBox);
    setPreferredSize({480.0f, 240.0f});
    findField_->setPlaceholder(builtinText(BuiltinText::FindPlaceholder));
    replaceField_->setPlaceholder(builtinText(BuiltinText::ReplacePlaceholder));
    for (auto* field : {findField_.get(), replaceField_.get()}) {
        impl_->styleFindFields(*field);
        field->setVisible(false);
        field->setSubmitOnEnter(false);
    }
    findField_->setOnChanged([this](const std::wstring& text) {
        impl_->query = text;
        impl_->matchesDirty = true;
        impl_->refreshMatches();
        // Incremental search: move onto the nearest match without taking focus.
        if (impl_->current >= 0) impl_->selectMatch(impl_->current);
        invalidate();
    });
    add(findField_);
    add(replaceField_);
}

CodeEditor::~CodeEditor() = default;

void CodeEditor::setText(const std::wstring& text) {
    auto& d = *impl_;
    d.doc.setText(text);
    d.lexer.reset(d.doc);
    d.longestLine = 0;
    for (std::size_t i = 0; i < d.doc.lineCount(); ++i) d.longestLine = std::max(d.longestLine, d.doc.line(i).size());
    d.selections = {Selection{}};
    d.primary = 0;
    d.preedit.clear();
    d.scroll = 0.0f;
    d.horizontal = 0.0f;
    d.rowsValid = false;
    d.resetHistory();
    d.decorations.clear();
    d.editLog.clear();
    d.closeCompletion();
    d.touched();
    d.logFloor = d.revision;
}

std::wstring CodeEditor::text() const { return impl_->doc.text(); }
std::string CodeEditor::textUtf8() const { return unicode::toUtf8(impl_->doc.text()); }

std::string CodeEditor::textUtf8(std::size_t start, std::size_t end) const {
    const auto& doc = impl_->doc;
    if (end < start) std::swap(start, end);
    return doc.utf8(doc.fromByteOffset(start), doc.fromByteOffset(std::min(end, doc.byteLength())));
}

std::size_t CodeEditor::lineCount() const { return impl_->doc.lineCount(); }
std::size_t CodeEditor::byteLength() const { return impl_->doc.byteLength(); }
std::uint64_t CodeEditor::revision() const { return impl_->revision; }

bool CodeEditor::setExternalText(const std::wstring& text, std::uint64_t externalRevision) {
    if (externalRevision <= impl_->externalRevision) return false;
    impl_->externalRevision = externalRevision;
    setText(text);
    return true;
}

bool CodeEditor::replaceRange(std::size_t start, std::size_t end, const std::wstring& text, std::uint64_t expectedRevision) {
    auto& d = *impl_;
    if (d.readOnly || (expectedRevision != 0 && expectedRevision != d.revision)) return false;
    if (end < start) std::swap(start, end);
    const auto before = d.selections;
    d.selections = {Selection{d.doc.fromByteOffset(start), d.doc.fromByteOffset(end)}};
    d.primary = 0;
    d.replaceSelections(text, code::EditKind::Other);
    (void)before;
    return true;
}

void CodeEditor::setBoundText(std::wstring text) {
    auto& d = *impl_;
    d.boundText = std::move(text);
    d.boundTextSet = true;
    if (d.boundRevision == 0) {
        if (!d.editedSinceBound && d.boundText != d.doc.text()) {
            setText(d.boundText);
            d.editedSinceBound = false;
        }
    } else if (d.boundRevision > d.externalRevision) {
        setExternalText(d.boundText, d.boundRevision);
        d.editedSinceBound = false;
    }
}

void CodeEditor::setBoundRevision(std::uint64_t revision) {
    auto& d = *impl_;
    d.boundRevision = revision;
    if (d.boundTextSet && revision > d.externalRevision) {
        setExternalText(d.boundText, revision);
        d.editedSinceBound = false;
    }
}

void CodeEditor::setBoundDecorations(std::vector<CodeDecoration> decorations) {
    impl_->boundDecorations = std::move(decorations);
    setDecorations(impl_->boundDecorations, impl_->boundDecorationsRevision);
}

void CodeEditor::setBoundDecorationsRevision(std::uint64_t revision) {
    impl_->boundDecorationsRevision = revision;
    setDecorations(impl_->boundDecorations, revision);
}

CodeEditorState CodeEditor::state() const { return impl_->state(); }

void CodeEditor::setSelection(std::size_t anchor, std::size_t caret) {
    auto& d = *impl_;
    d.selections = {Selection{d.doc.fromByteOffset(anchor), d.doc.fromByteOffset(caret)}};
    d.primary = 0;
    d.reveal(d.selections[0].caret);
    d.cursorMoved();
}

void CodeEditor::setLanguage(CodeLanguage language) {
    impl_->lexer.setLanguage(std::move(language));
    impl_->lexer.reset(impl_->doc);
    invalidate();
}

void CodeEditor::setReadOnly(bool readOnly) {
    impl_->readOnly = readOnly;
    invalidate();
}
bool CodeEditor::readOnly() const { return impl_->readOnly; }

void CodeEditor::setWordWrap(bool wrap) {
    if (impl_->wrap == wrap) return;
    impl_->wrap = wrap;
    impl_->horizontal = 0.0f;
    impl_->rowsValid = false;
    invalidate();
}
bool CodeEditor::wordWrap() const { return impl_->wrap; }

void CodeEditor::setLineNumbers(bool visible) {
    impl_->lineNumbers = visible;
    impl_->rowsValid = false;
    invalidate();
}

void CodeEditor::setIndentGuides(bool visible) {
    impl_->indentGuides = visible;
    invalidate();
}

void CodeEditor::setTabSize(int size) {
    impl_->tabSize = std::clamp(size, 1, 16);
    impl_->rowsValid = false;
    invalidate();
}

void CodeEditor::setInsertSpaces(bool spaces) { impl_->insertSpaces = spaces; }

void CodeEditor::setFontSize(float size) {
    impl_->fontSize = std::clamp(size, 8.0f, 40.0f);
    impl_->lineHeight = std::round(impl_->fontSize * 1.55f);
    impl_->measured = false;
    impl_->rowsValid = false;
    invalidate();
}

void CodeEditor::setClipboard(std::shared_ptr<Clipboard> clipboard) {
    impl_->clipboard = clipboard;
    findField_->setClipboard(clipboard);
    replaceField_->setClipboard(clipboard);
}

void CodeEditor::setCommentPrefix(std::wstring prefix) { impl_->commentPrefix = std::move(prefix); }

void CodeEditor::openFind(bool replace) {
    auto& d = *impl_;
    d.findVisible = true;
    d.replaceVisible = replace;
    // Text set after construction (the application's language) applies here.
    findField_->setPlaceholder(builtinText(BuiltinText::FindPlaceholder));
    replaceField_->setPlaceholder(builtinText(BuiltinText::ReplacePlaceholder));
    const auto& s = d.selections[d.primary];
    if (!s.empty() && s.start().line == s.end().line) {
        const auto selected = d.doc.text(s.start(), s.end());
        findField_->setText(selected);
        d.query = selected;
        d.matchesDirty = true;
    }
    findField_->setVisible(true);
    replaceField_->setVisible(replace);
    layoutChildren();
    findField_->selectAll();
    requestFocus(findField_.get(), true);
    invalidate();
}

void CodeEditor::closeFind() {
    auto& d = *impl_;
    if (!d.findVisible) return;
    d.findVisible = false;
    d.replaceVisible = false;
    findField_->setVisible(false);
    replaceField_->setVisible(false);
    focusChild(nullptr);
    invalidate();
}

bool CodeEditor::findOpen() const { return impl_->findVisible; }

void CodeEditor::setSearch(std::wstring query, CodeSearchOptions options) {
    auto& d = *impl_;
    if (findField_->text() != query) findField_->setText(query);
    d.query = std::move(query);
    d.options = options;
    d.matchesDirty = true;
    d.refreshMatches();
    invalidate();
}

const std::wstring& CodeEditor::searchQuery() const { return impl_->query; }

std::size_t CodeEditor::matchCount() const {
    impl_->refreshMatches();
    return impl_->matches.size();
}

int CodeEditor::currentMatch() const {
    impl_->refreshMatches();
    return impl_->current;
}

void CodeEditor::setDecorations(std::vector<CodeDecoration> decorations, std::uint64_t revision) {
    auto& d = *impl_;
    d.decorations.clear();
    if (revision != 0 && revision != d.revision) {
        // Too old to map, or from the future: wait for a fresh publication.
        if (revision > d.revision || revision < d.logFloor) {
            invalidate();
            return;
        }
    }
    const auto length = d.doc.byteLength();
    for (auto& item : decorations) {
        std::size_t start = std::min(item.start, item.end), end = std::max(item.start, item.end);
        if (revision != 0)
            for (const auto& e : d.editLog) {
                if (e.revision <= revision) continue;
                auto map = [&](std::size_t o) {
                    if (o < e.start) return o;
                    if (o >= e.start + e.removed) return o - e.removed + e.inserted;
                    return e.start + e.inserted;
                };
                start = map(start);
                end = map(end);
            }
        code::Decoration decoration;
        decoration.id = std::move(item.id);
        decoration.start = d.doc.fromByteOffset(std::min(start, length));
        decoration.end = d.doc.fromByteOffset(std::min(end, length));
        decoration.kind = item.kind;
        decoration.message = std::move(item.message);
        d.decorations.push_back(std::move(decoration));
    }
    invalidate();
}

std::vector<CodeDecoration> CodeEditor::decorations() const {
    std::vector<CodeDecoration> result;
    for (const auto& d : impl_->decorations)
        result.push_back({d.id, impl_->doc.byteOffset(d.start), impl_->doc.byteOffset(d.end), d.kind, d.message});
    return result;
}

void CodeEditor::setCompletionItems(std::vector<CodeCompletionItem> items) {
    auto& d = *impl_;
    d.completionItems = std::move(items);
    const auto caret = d.selections[std::min(d.primary, d.selections.size() - 1)].caret;
    Position start = d.completionVisible ? d.completionStart : d.wordStartBefore(caret);
    if (!d.completionItems.empty() && d.completionItems.front().replaceEnd > d.completionItems.front().replaceStart) {
        const auto given = d.doc.fromByteOffset(d.completionItems.front().replaceStart);
        if (given.line == caret.line && given <= caret) start = given;
    }
    d.completionStart = start;
    d.filterCompletions();
    d.updateCompletionVisibility();
}

void CodeEditor::setCompletionOpen(bool open) {
    auto& d = *impl_;
    d.completionWanted = open;
    if (open) d.filterCompletions();
    d.updateCompletionVisibility();
}

bool CodeEditor::completionOpen() const { return impl_->completionVisible; }
int CodeEditor::completionIndex() const { return impl_->completionVisible ? impl_->completionSelected : -1; }

std::vector<std::wstring> CodeEditor::visibleCompletions() const {
    std::vector<std::wstring> result;
    if (!impl_->completionVisible) return result;
    for (const auto index : impl_->completionFiltered) result.push_back(impl_->completionItems[index].label);
    return result;
}

bool CodeEditor::acceptCompletion() { return impl_->acceptCompletion(); }
void CodeEditor::closeCompletion() { impl_->closeCompletion(); }
void CodeEditor::requestCompletion(CodeCompletionTrigger trigger) { impl_->requestCompletion(trigger); }
void CodeEditor::setOnCompletionRequested(std::function<void(std::size_t, CodeCompletionTrigger)> callback) {
    impl_->onCompletionRequested = std::move(callback);
}
void CodeEditor::setOnCompletionClosed(std::function<void()> callback) { impl_->onCompletionClosed = std::move(callback); }
void CodeEditor::setOnCompletionAccepted(std::function<void(const std::wstring&)> callback) {
    impl_->onCompletionAccepted = std::move(callback);
}

bool CodeEditor::findNext(bool backwards) { return impl_->findNext(backwards); }
bool CodeEditor::replaceCurrent(const std::wstring& replacement) { return impl_->replaceCurrent(replacement); }
std::size_t CodeEditor::replaceAll(const std::wstring& replacement) { return impl_->replaceAll(replacement); }
bool CodeEditor::undo() { return impl_->undo(); }
bool CodeEditor::redo() { return impl_->redo(); }

void CodeEditor::toggleComment() {
    auto& d = *impl_;
    std::wstring prefix = d.commentPrefix;
    if (prefix.empty()) prefix = d.lexer.language().lineComments.empty() ? L"--" : d.lexer.language().lineComments.front();
    // Comment when any selected non-blank line is uncommented; align prefixes
    // at the smallest indentation like most editors.
    bool allCommented = true;
    std::size_t indent = std::wstring::npos;
    for (const auto& s : d.selections) {
        const auto [first, last] = d.selectedLines(s);
        for (auto line = first; line <= last; ++line) {
            const auto& text = d.doc.line(line);
            std::size_t at = 0;
            const bool commented = leadingPrefix(text, prefix, at);
            if (at == text.size()) continue;
            indent = std::min(indent, at);
            allCommented = allCommented && commented;
        }
    }
    if (indent == std::wstring::npos) return;
    d.editLines([&](std::size_t, const std::wstring& text) {
        std::size_t at = 0;
        const bool commented = leadingPrefix(text, prefix, at);
        if (at == text.size()) return text;
        if (allCommented) {
            if (!commented) return text;
            auto end = at + prefix.size();
            if (end < text.size() && text[end] == L' ') ++end;
            return text.substr(0, at) + text.substr(end);
        }
        return text.substr(0, indent) + prefix + L" " + text.substr(indent);
    });
}

void CodeEditor::indentSelection() {
    auto& d = *impl_;
    d.editLines([&](std::size_t, const std::wstring& text) {
        if (text.empty()) return text;
        return (d.insertSpaces ? std::wstring(static_cast<std::size_t>(d.tabSize), L' ') : std::wstring(L"\t")) + text;
    });
}

void CodeEditor::outdentSelection() {
    auto& d = *impl_;
    d.editLines([&](std::size_t, const std::wstring& text) {
        if (!text.empty() && text[0] == L'\t') return text.substr(1);
        std::size_t n = 0;
        while (n < text.size() && n < static_cast<std::size_t>(d.tabSize) && text[n] == L' ') ++n;
        return text.substr(n);
    });
}

void CodeEditor::duplicateLines() {
    auto& d = *impl_;
    if (d.readOnly) return;
    d.normalizeSelections();
    code::Transaction transaction;
    transaction.before = d.selections;
    std::vector<Selection> after = d.selections;
    for (std::size_t k = d.selections.size(); k-- > 0;) {
        const auto [first, last] = d.selectedLines(d.selections[k]);
        const auto block = d.doc.text(Position{first, 0}, Position{last, d.doc.line(last).size()});
        code::Edit edit{Position{last, d.doc.line(last).size()}, L"", L"\n" + block};
        d.applyEdit(edit);
        const auto lines = last - first + 1;
        for (std::size_t j = k; j < after.size(); ++j) {
            if (j != k) {
                after[j].anchor.line += lines;
                after[j].caret.line += lines;
            }
        }
        after[k].anchor.line += lines;
        after[k].caret.line += lines;
        transaction.edits.push_back(std::move(edit));
    }
    d.selections = after;
    d.normalizeSelections();
    transaction.after = d.selections;
    d.apply(std::move(transaction), false);
    d.reveal(d.selections[d.primary].caret);
}

void CodeEditor::moveLines(int direction) {
    auto& d = *impl_;
    if (d.readOnly || direction == 0) return;
    d.normalizeSelections();
    // Union of the selected line ranges.
    std::vector<std::pair<std::size_t, std::size_t>> blocks;
    for (const auto& s : d.selections) {
        const auto range = d.selectedLines(s);
        if (!blocks.empty() && range.first <= blocks.back().second + 1) blocks.back().second = std::max(blocks.back().second, range.second);
        else blocks.push_back(range);
    }
    if (direction < 0 && blocks.front().first == 0) return;
    if (direction > 0 && blocks.back().second + 1 >= d.doc.lineCount()) return;
    code::Transaction transaction;
    transaction.before = d.selections;
    auto moveBlock = [&](std::size_t first, std::size_t last) {
        // Swap the block with the neighbouring line as one replacement.
        const std::size_t from = direction < 0 ? first - 1 : first;
        const std::size_t to = direction < 0 ? last : last + 1;
        const Position start{from, 0}, stop{to, d.doc.line(to).size()};
        const auto block = d.doc.text(Position{first, 0}, Position{last, d.doc.line(last).size()});
        const auto neighbour = d.doc.line(direction < 0 ? first - 1 : last + 1);
        code::Edit edit{start, d.doc.text(start, stop), direction < 0 ? block + L"\n" + neighbour : neighbour + L"\n" + block};
        d.applyEdit(edit);
        transaction.edits.push_back(std::move(edit));
    };
    if (direction < 0) for (const auto& b : blocks) moveBlock(b.first, b.second);
    else for (auto it = blocks.rbegin(); it != blocks.rend(); ++it) moveBlock(it->first, it->second);
    for (auto& s : d.selections) {
        s.anchor.line = static_cast<std::size_t>(static_cast<long long>(s.anchor.line) + (direction < 0 ? -1 : 1));
        s.caret.line = static_cast<std::size_t>(static_cast<long long>(s.caret.line) + (direction < 0 ? -1 : 1));
    }
    d.normalizeSelections();
    transaction.after = d.selections;
    d.apply(std::move(transaction), false);
    d.reveal(d.selections[d.primary].caret);
}

void CodeEditor::addCursorVertical(int direction) {
    auto& d = *impl_;
    d.normalizeSelections();
    const auto& edge = direction < 0 ? d.selections.front() : d.selections.back();
    const auto row = static_cast<long long>(d.rowOf(edge.caret)) + direction;
    if (row < 0 || row >= static_cast<long long>(d.rowCount())) return;
    const Rect caret = d.caretRect(edge.caret);
    const float x = edge.preferredX >= 0.0f ? edge.preferredX : caret.x + d.horizontal;
    const Rect text = d.textRect();
    const Position p = d.hit(Point{x - d.horizontal, text.y + 4.0f + static_cast<float>(row) * d.lineHeight - d.scroll + d.lineHeight * 0.5f});
    Selection added{p, p, x};
    d.selections.push_back(added);
    d.primary = d.selections.size() - 1;
    d.normalizeSelections();
    d.reveal(p);
    d.cursorMoved();
}

void CodeEditor::addCursorAt(std::size_t offset) {
    auto& d = *impl_;
    const auto p = d.doc.fromByteOffset(offset);
    d.selections.push_back(Selection{p, p});
    d.primary = d.selections.size() - 1;
    d.normalizeSelections();
    d.cursorMoved();
}

void CodeEditor::selectAll() {
    auto& d = *impl_;
    d.selections = {Selection{Position{0, 0}, d.doc.end()}};
    d.primary = 0;
    d.cursorMoved();
}

bool CodeEditor::copy() {
    auto& d = *impl_;
    if (!d.clipboard) return false;
    d.normalizeSelections();
    std::wstring text;
    bool any = false;
    for (const auto& s : d.selections) any = any || !s.empty();
    for (std::size_t i = 0; i < d.selections.size(); ++i) {
        const auto& s = d.selections[i];
        if (i) text += L"\n";
        // Without a selection, copy the whole line (with its newline).
        if (!any) text += d.doc.line(s.caret.line) + L"\n";
        else text += d.doc.text(s.start(), s.end());
    }
    d.clipboard->setText(text);
    return true;
}

bool CodeEditor::cut() {
    auto& d = *impl_;
    if (d.readOnly || !copy()) return false;
    bool any = false;
    for (const auto& s : d.selections) any = any || !s.empty();
    if (!any) {
        for (auto& s : d.selections) {
            const auto line = s.caret.line;
            s.anchor = Position{line, 0};
            s.caret = line + 1 < d.doc.lineCount() ? Position{line + 1, 0} : Position{line, d.doc.line(line).size()};
        }
    }
    d.replaceSelections(L"", code::EditKind::Other);
    return true;
}

bool CodeEditor::paste() {
    auto& d = *impl_;
    if (d.readOnly || !d.clipboard) return false;
    const auto text = code::normalizeNewlines(d.clipboard->text());
    if (text.empty()) return false;
    // One line per cursor when the counts match.
    std::vector<std::wstring> parts;
    if (d.selections.size() > 1) {
        std::size_t start = 0;
        while (true) {
            const auto end = text.find(L'\n', start);
            parts.push_back(text.substr(start, end == std::wstring::npos ? std::wstring::npos : end - start));
            if (end == std::wstring::npos) break;
            start = end + 1;
        }
        if (!parts.empty() && parts.back().empty()) parts.pop_back();
    }
    if (parts.size() == d.selections.size() && parts.size() > 1) d.replaceSelections(parts, code::EditKind::Other);
    else d.replaceSelections(text, code::EditKind::Other);
    return true;
}

void CodeEditor::insertText(const std::wstring& text) {
    impl_->replaceSelections(text, code::EditKind::Typing);
}

void CodeEditor::setOnChanged(std::function<void(std::uint64_t)> callback) { impl_->onChanged = std::move(callback); }
void CodeEditor::setOnCursorChanged(std::function<void(const CodeEditorState&)> callback) { impl_->onCursor = std::move(callback); }
void CodeEditor::setNotifyThrottle(double milliseconds) { impl_->throttle = std::max(0.0, milliseconds); }
void CodeEditor::flushNotifications() { impl_->notify(true); }

std::vector<CodeToken> CodeEditor::lineTokens(std::size_t line) const { return impl_->lexer.tokens(impl_->doc, line); }
std::size_t CodeEditor::relexedLines() const { return impl_->lexer.relexed(); }
float CodeEditor::scrollOffset() const { return impl_->scroll; }
float CodeEditor::maxScrollOffset() const { return impl_->maxScroll(); }
void CodeEditor::setScrollOffset(float offset) { impl_->setScroll(offset); }

std::size_t CodeEditor::firstVisibleLine() const {
    const auto row = static_cast<std::size_t>(std::max(0.0f, impl_->scroll / impl_->lineHeight));
    return impl_->lineOfRow(row).first;
}

void CodeEditor::paint(Canvas& canvas) {
    impl_->paint(canvas, focused(), focused() && focusedChild() == nullptr);
    View::paint(canvas);
}

Rect CodeEditor::paintBounds() const {
    Rect bounds = View::paintBounds();
    if (impl_->completionVisible) {
        const Rect popup = impl_->completionRect();
        const float left = std::min(bounds.x, popup.x - 16.0f), top = std::min(bounds.y, popup.y - 16.0f);
        const float right = std::max(bounds.x + bounds.width, popup.x + popup.width + 16.0f);
        const float bottom = std::max(bounds.y + bounds.height, popup.y + popup.height + 16.0f);
        bounds = Rect{left, top, right - left, bottom - top};
    }
    return bounds;
}

bool CodeEditor::hitTest(Point point) const {
    return View::hitTest(point) || (impl_->completionVisible && impl_->completionRect().contains(point));
}

bool CodeEditor::paintsAboveSiblings() const { return impl_->completionVisible; }

const std::wstring* CodeEditor::tooltipAt(Point point) const {
    const auto& d = *impl_;
    if (d.completionVisible && d.completionRect().contains(point)) return nullptr;
    if (d.findVisible && d.findLayout().bar.contains(point)) {
        const auto l = d.findLayout();
        const std::pair<Rect, BuiltinText> hints[] = {
            {l.matchCase, BuiltinText::FindMatchCase}, {l.regex, BuiltinText::FindRegex},
            {l.word, BuiltinText::FindWholeWord},      {l.previous, BuiltinText::FindPrevious},
            {l.next, BuiltinText::FindNext},           {l.close, BuiltinText::FindClose},
        };
        for (const auto& [rect, key] : hints) {
            if (!rect.contains(point)) continue;
            d.findTooltip = builtinText(key);
            return &d.findTooltip;
        }
        return View::tooltipAt(point);
    }
    if (!d.decorations.empty() && d.textRect().contains(point)) {
        const auto p = d.hit(point);
        for (const auto& decoration : d.decorations) {
            if (decoration.message.empty()) continue;
            const bool squiggle = decoration.kind == CodeDecorationKind::Error || decoration.kind == CodeDecorationKind::Warning;
            const auto end = decoration.start == decoration.end ? Position{decoration.end.line, decoration.end.column + 1} : decoration.end;
            if ((squiggle || decoration.kind == CodeDecorationKind::Running) && decoration.start <= p && p < end)
                return &decoration.message;
        }
    }
    return View::tooltipAt(point);
}

void CodeEditor::layoutChildren() {
    if (!impl_->findVisible) return;
    const auto l = impl_->findLayout();
    findField_->setFrame(l.find);
    if (impl_->replaceVisible) replaceField_->setFrame(l.replace);
}

bool CodeEditor::onMouseDown(const MouseEvent& event) {
    auto& d = *impl_;
    if (!interactive()) return false;
    if (d.completionVisible) {
        const int row = d.completionRowAt(event.position);
        if (row >= 0) {
            d.completionSelected = row;
            d.acceptCompletion();
            return true;
        }
        if (d.completionRect().contains(event.position)) return true;
        d.closeCompletion();
    }
    if (d.findVisible) {
        const auto l = d.findLayout();
        if (l.bar.contains(event.position)) {
            auto toggle = [&](bool& flag) {
                flag = !flag;
                d.matchesDirty = true;
                d.refreshMatches();
                invalidate();
            };
            if (l.matchCase.contains(event.position)) toggle(d.options.matchCase);
            else if (l.regex.contains(event.position)) toggle(d.options.regex);
            else if (l.word.contains(event.position)) toggle(d.options.wholeWord);
            else if (l.previous.contains(event.position)) d.findNext(true);
            else if (l.next.contains(event.position)) d.findNext(false);
            else if (l.close.contains(event.position)) closeFind();
            else if (d.replaceVisible && l.replaceOne.contains(event.position)) d.replaceCurrent(replaceField_->text());
            else if (d.replaceVisible && l.replaceAll.contains(event.position)) d.replaceAll(replaceField_->text());
            else return View::onMouseDown(event);
            return true;
        }
    }
    focusChild(nullptr);
    const Rect v = d.verticalThumb();
    const Rect f = frame();
    if (event.position.x >= f.x + f.width - 10.0f && d.maxScroll() > 0.0f) {
        if (v.contains(event.position)) {
            d.drag = CodeEditorImpl::Drag::VerticalBar;
            d.dragGrab = event.position.y - v.y;
        } else {
            d.setScroll(d.scroll + (event.position.y < v.y ? -1.0f : 1.0f) * d.textRect().height);
        }
        return true;
    }
    const Rect h = d.horizontalThumb();
    if (h.width > 0.0f && event.position.y >= f.y + f.height - 10.0f) {
        if (h.contains(event.position)) {
            d.drag = CodeEditorImpl::Drag::HorizontalBar;
            d.dragGrab = event.position.x - h.x;
        } else {
            d.setHorizontal(d.horizontal + (event.position.x < h.x ? -1.0f : 1.0f) * d.textRect().width);
        }
        return true;
    }
    const Position p = d.hit(event.position);
    if (event.button == MouseButton::Right) {
        bool inside = false;
        for (const auto& s : d.selections) inside = inside || (!s.empty() && s.start() <= p && p <= s.end());
        if (!inside) {
            d.selections = {Selection{p, p}};
            d.primary = 0;
            d.cursorMoved();
        }
        return true;
    }
    if (event.button != MouseButton::Left) return true;
    d.clickCount = static_cast<std::size_t>(std::max(1, event.clickCount));
    if (event.alt) {
        d.selections.push_back(Selection{p, p});
        d.primary = d.selections.size() - 1;
    } else if (event.shift) {
        auto s = d.selections[d.primary];
        s.caret = p;
        d.selections = {s};
        d.primary = 0;
    } else if (d.clickCount == 2) {
        const auto& line = d.doc.line(p.line);
        Position a = p, b = p;
        auto word = [](wchar_t c) { return c == L'_' || std::iswalnum(c) || c >= 0x80; };
        while (a.column > 0 && word(line[a.column - 1])) --a.column;
        while (b.column < line.size() && word(line[b.column])) ++b.column;
        d.selections = {Selection{a, b}};
        d.wordAnchorStart = a;
        d.wordAnchorEnd = b;
        d.primary = 0;
    } else if (d.clickCount >= 3) {
        const Position a{p.line, 0};
        const Position b = p.line + 1 < d.doc.lineCount() ? Position{p.line + 1, 0} : Position{p.line, d.doc.line(p.line).size()};
        d.selections = {Selection{a, b}};
        d.primary = 0;
    } else {
        d.selections = {Selection{p, p}};
        d.primary = 0;
    }
    d.normalizeSelections();
    d.drag = CodeEditorImpl::Drag::Text;
    d.cursorMoved();
    return true;
}

bool CodeEditor::onMouseMove(const MouseEvent& event) {
    auto& d = *impl_;
    switch (d.drag) {
    case CodeEditorImpl::Drag::VerticalBar: {
        const Rect text = d.textRect();
        const Rect v = d.verticalThumb();
        const float track = std::max(1.0f, text.height - v.height);
        d.setScroll((event.position.y - d.dragGrab - text.y) / track * d.maxScroll());
        return true;
    }
    case CodeEditorImpl::Drag::HorizontalBar: {
        const Rect text = d.textRect();
        const Rect h = d.horizontalThumb();
        const float track = std::max(1.0f, text.width - h.width);
        d.setHorizontal((event.position.x - d.dragGrab - text.x) / track * d.maxHorizontal());
        return true;
    }
    case CodeEditorImpl::Drag::Text: {
        const Position p = d.hit(event.position);
        auto& s = d.selections[d.primary];
        if (d.clickCount == 2) {
            const auto& line = d.doc.line(p.line);
            auto word = [](wchar_t c) { return c == L'_' || std::iswalnum(c) || c >= 0x80; };
            Position a = p, b = p;
            while (a.column > 0 && word(line[a.column - 1])) --a.column;
            while (b.column < line.size() && word(line[b.column])) ++b.column;
            if (p < d.wordAnchorStart) {
                s.anchor = d.wordAnchorEnd;
                s.caret = a;
            } else {
                s.anchor = d.wordAnchorStart;
                s.caret = std::max(b, d.wordAnchorEnd);
            }
        } else {
            s.caret = p;
        }
        d.reveal(p);
        d.cursorMoved();
        return true;
    }
    default: return View::onMouseMove(event);
    }
}

bool CodeEditor::onMouseUp(const MouseEvent& event) {
    auto& d = *impl_;
    if (d.drag != CodeEditorImpl::Drag::None) {
        d.drag = CodeEditorImpl::Drag::None;
        d.normalizeSelections();
        return true;
    }
    return View::onMouseUp(event);
}

bool CodeEditor::onMouseWheel(const MouseWheelEvent& event) {
    auto& d = *impl_;
    if (d.completionVisible && d.completionRect().contains(event.position)) {
        const int last = std::max(0, static_cast<int>(d.completionFiltered.size()) - CodeEditorImpl::completionRows);
        d.completionTop = std::clamp(d.completionTop - static_cast<int>(event.deltaY * 3.0f), 0, last);
        invalidate();
        return true;
    }
    if (!frame().contains(event.position)) return false;
    if (event.shift) d.setHorizontal(d.horizontal - event.deltaY * d.advance(L'M') * 8.0f);
    else d.setScroll(d.scroll - event.deltaY * d.lineHeight * 3.0f);
    return true;
}

bool CodeEditor::onKeyDown(const KeyEvent& event) {
    auto& d = *impl_;
    if (focusedChild()) {
        // Keys inside the find bar.
        if (event.key == Key::Escape) {
            closeFind();
            return true;
        }
        if (event.key == Key::Enter && focusedChild() == findField_.get()) {
            d.findNext(event.shift);
            return true;
        }
        if (event.key == Key::Enter && focusedChild() == replaceField_.get()) {
            if (event.control) d.replaceAll(replaceField_->text());
            else d.replaceCurrent(replaceField_->text());
            return true;
        }
        return View::onKeyDown(event);
    }
    const bool shift = event.shift, ctrl = event.control, alt = event.alt;
    if (d.completionVisible && !ctrl && !alt) {
        switch (event.key) {
        case Key::Up: d.moveCompletion(-1); return true;
        case Key::Down: d.moveCompletion(1); return true;
        case Key::PageUp: d.moveCompletion(-CodeEditorImpl::completionRows); return true;
        case Key::PageDown: d.moveCompletion(CodeEditorImpl::completionRows); return true;
        case Key::Enter:
        case Key::Tab:
            if (!shift && d.acceptCompletion()) return true;
            break;
        case Key::Escape: d.closeCompletion(); return true;
        case Key::Backspace:
            d.deleteBackward(false);
            if (d.selections[d.primary].caret < d.completionStart) d.closeCompletion();
            else {
                d.filterCompletions();
                d.updateCompletionVisibility();
            }
            return true;
        default: d.closeCompletion(); break;
        }
    }
    switch (event.key) {
    case Key::Left: d.move(-1, 0, shift, ctrl); return true;
    case Key::Right: d.move(1, 0, shift, ctrl); return true;
    case Key::Up:
    case Key::Down: {
        const int dir = event.key == Key::Up ? -1 : 1;
        if (ctrl && alt) addCursorVertical(dir);
        else if (alt && shift) duplicateLines();
        else if (alt) moveLines(dir);
        else if (ctrl) d.setScroll(d.scroll + static_cast<float>(dir) * d.lineHeight);
        else d.move(0, dir, shift, false);
        return true;
    }
    case Key::Home: d.home(shift, ctrl); return true;
    case Key::End: d.end(shift, ctrl); return true;
    case Key::PageUp: d.page(-1, shift); return true;
    case Key::PageDown: d.page(1, shift); return true;
    case Key::Backspace:
        if (alt) return false;
        d.deleteBackward(ctrl);
        return true;
    case Key::Delete:
        if (alt) return false;
        d.deleteForward(ctrl);
        return true;
    case Key::Enter:
        // Modified Enter belongs to the page (for example Ctrl+Enter runs).
        if (ctrl || alt || d.readOnly) return false;
        d.newline();
        return true;
    case Key::Tab: {
        if (ctrl || alt || d.readOnly) return false;
        bool block = false;
        for (const auto& s : d.selections) block = block || s.start().line != s.end().line;
        if (shift) outdentSelection();
        else if (block) indentSelection();
        else {
            std::vector<std::wstring> texts;
            for (const auto& s : d.selections) texts.push_back(indentUnit(d, s.start().column));
            d.replaceSelections(texts, code::EditKind::Typing);
        }
        return true;
    }
    case Key::Escape:
        if (d.findVisible) {
            closeFind();
            return true;
        }
        if (d.selections.size() > 1) {
            d.selections = {d.selections[d.primary]};
            d.primary = 0;
            d.cursorMoved();
            return true;
        }
        return false;
    default: break;
    }
    if (event.virtualKey == f3Key && !ctrl && !alt) {
        if (d.query.empty()) return false;
        d.findNext(shift);
        return true;
    }
    return false;
}

CommandResult CodeEditor::dispatchBuiltinCommandKey(const KeyEvent& event, const std::string& logicalKey) {
    auto& d = *impl_;
    if (!event.pressed || !event.editShortcut() || event.alt) return CommandResult::NotFound;
    const auto key = logicalKey.empty() ? logicalKeyName(event) : logicalKey;
    const bool shift = event.shift;
    if (key == "f" && !shift) {
        openFind(false);
        return CommandResult::Executed;
    }
    if (key == "h" && !shift) {
        openFind(true);
        return CommandResult::Executed;
    }
    // Inside the find bar the text field handles editing shortcuts.
    if (focusedChild()) return CommandResult::NotFound;
    if (key == "space" && !shift) {
        d.requestCompletion(CodeCompletionTrigger::Manual);
        return CommandResult::Executed;
    }
    const auto done = [this] {
        invalidate();
        return CommandResult::Executed;
    };
    if (key == "a" && !shift) { selectAll(); return done(); }
    if (key == "c" && !shift) { copy(); return done(); }
    if (key == "x" && !shift) { cut(); return done(); }
    if (key == "v" && !shift) { paste(); return done(); }
    if (key == "z") { shift ? redo() : undo(); return done(); }
    if (key == "y" && !shift) { redo(); return done(); }
    if (key == "d" && !shift) { duplicateLines(); return done(); }
    if (key == "k" && shift) {
        // Delete the selected lines.
        for (auto& s : d.selections) {
            const auto [first, last] = d.selectedLines(s);
            s.anchor = Position{first, 0};
            s.caret = last + 1 < d.doc.lineCount() ? Position{last + 1, 0} : Position{last, d.doc.line(last).size()};
            if (s.caret.line == last && first > 0) s.anchor = Position{first - 1, d.doc.line(first - 1).size()};
        }
        d.replaceSelections(L"", code::EditKind::Other);
        return done();
    }
    if (event.virtualKey == slashKey && !shift) { toggleComment(); return done(); }
    if (event.virtualKey == closeBracketKey && !shift) { indentSelection(); return done(); }
    if (event.virtualKey == openBracketKey && !shift) { outdentSelection(); return done(); }
    return CommandResult::NotFound;
}

bool CodeEditor::onTextInput(wchar_t character) {
    if (focusedChild()) return View::onTextInput(character);
    if (impl_->readOnly || character < 0x20 || character == 0x7f) return false;
    insertText(std::wstring(1, character));
    impl_->afterTyping(std::wstring(1, character));
    return true;
}

bool CodeEditor::onTextCommitted(const std::wstring& text) {
    if (focusedChild()) return View::onTextCommitted(text);
    impl_->preedit.clear();
    if (impl_->readOnly || text.empty()) {
        invalidate();
        return false;
    }
    insertText(code::normalizeNewlines(text));
    impl_->afterTyping(text);
    return true;
}

TextInputState CodeEditor::textInputState() const {
    if (focusedChild()) return View::textInputState();
    const auto& d = *impl_;
    const auto& s = d.selections[std::min(d.primary, d.selections.size() - 1)];
    TextInputState state;
    state.text = d.doc.line(s.caret.line);
    state.caret = s.caret.column;
    state.anchor = s.anchor.line == s.caret.line ? s.anchor.column : s.caret.column;
    state.editable = !d.readOnly;
    state.identity = this;
    state.drawsPreedit = true;
    state.session = textInputSession();
    return state;
}

void CodeEditor::setTextComposition(std::wstring text, std::size_t caret) {
    if (focusedChild()) {
        View::setTextComposition(std::move(text), caret);
        return;
    }
    impl_->preedit = std::move(text);
    impl_->preeditCaret = caret;
    invalidate();
}

bool CodeEditor::hasTextComposition() const {
    return focusedChild() ? View::hasTextComposition() : !impl_->preedit.empty();
}

bool CodeEditor::commitTextComposition() {
    if (focusedChild()) return View::commitTextComposition();
    if (impl_->preedit.empty()) return false;
    const auto text = std::move(impl_->preedit);
    impl_->preedit.clear();
    insertText(text);
    return true;
}

Rect CodeEditor::textInputCaretRect() const {
    if (focusedChild()) return View::textInputCaretRect();
    const auto& d = *impl_;
    return d.caretRect(d.selections[std::min(d.primary, d.selections.size() - 1)].caret);
}

bool CodeEditor::onFocusChanged(bool focused) {
    const bool handled = View::onFocusChanged(focused);
    if (!focused) commitTextComposition();
    invalidate();
    return handled;
}

CursorKind CodeEditor::cursor(Point point) const {
    const auto& d = *impl_;
    if (d.findVisible && d.findLayout().bar.contains(point)) {
        const auto l = d.findLayout();
        if (l.find.contains(point) || (d.replaceVisible && l.replace.contains(point))) return CursorKind::Text;
        return CursorKind::Default;
    }
    const Rect text = d.textRect();
    return text.contains(point) ? CursorKind::Text : CursorKind::Default;
}

bool CodeEditor::tickAnimations(double nowMs) {
    impl_->notify(false);
    const bool views = View::tickAnimations(nowMs);
    return impl_->changedPending || impl_->cursorPending || views;
}

AccessibilityInfo CodeEditor::accessibilityInfo() const {
    auto info = View::accessibilityInfo();
    info.role = AccessibilityRole::TextBox;
    const auto state = impl_->state();
    info.value = L"Line " + std::to_wstring(state.line) + L", column " + std::to_wstring(state.column);
    return info;
}

} // namespace oneui
