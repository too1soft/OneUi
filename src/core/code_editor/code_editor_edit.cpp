#include "code_editor_impl.h"

#include "oneui/clipboard.h"

#include <algorithm>
#include <cwctype>
#include <numeric>

namespace oneui {
namespace {

using code::Position;
using code::Selection;

bool wordChar(wchar_t c) { return c == L'_' || std::iswalnum(c) || c >= 0x80; }
bool lowSurrogate(wchar_t c) { return c >= 0xdc00 && c <= 0xdfff; }

std::size_t leadingSpace(const std::wstring& text) {
    std::size_t i = 0;
    while (i < text.size() && (text[i] == L' ' || text[i] == L'\t')) ++i;
    return i;
}

} // namespace

void CodeEditorImpl::changed(const code::LineChange& change) {
    const std::size_t oldCount = doc.lineCount() - change.inserted + change.removed;
    lexer.apply(doc, change);
    for (std::size_t i = 0; i < change.inserted; ++i)
        longestLine = std::max(longestLine, doc.line(change.first + i).size());
    if (wrap && rowsValid && rowStart.size() == oldCount + 1) {
        std::vector<std::size_t> counts(oldCount);
        for (std::size_t i = 0; i < oldCount; ++i) counts[i] = rowStart[i + 1] - rowStart[i];
        const auto first = static_cast<std::ptrdiff_t>(change.first);
        counts.erase(counts.begin() + first, counts.begin() + first + static_cast<std::ptrdiff_t>(change.removed));
        std::vector<std::size_t> added(change.inserted);
        for (std::size_t i = 0; i < change.inserted; ++i) added[i] = segments(change.first + i).size();
        counts.insert(counts.begin() + first, added.begin(), added.end());
        rowStart.assign(counts.size() + 1, 0);
        for (std::size_t i = 0; i < counts.size(); ++i) rowStart[i + 1] = rowStart[i] + counts[i];
    } else {
        rowsValid = false;
    }
    matchesDirty = true;
}

void CodeEditorImpl::applyEdit(const code::Edit& edit) {
    logEdit(edit);
    mapDecorations(edit);
    code::LineChange change;
    if (!edit.removed.empty()) {
        doc.erase(edit.start, code::Document::advance(edit.start, edit.removed), &change);
        changed(change);
    }
    if (!edit.inserted.empty()) {
        doc.insert(edit.start, edit.inserted, &change);
        changed(change);
    }
}

void CodeEditorImpl::normalizeSelections() {
    for (auto& s : selections) {
        s.anchor = doc.clamp(s.anchor);
        s.caret = doc.clamp(s.caret);
    }
    const Position primaryCaret = selections[std::min(primary, selections.size() - 1)].caret;
    std::sort(selections.begin(), selections.end(), [](const Selection& a, const Selection& b) { return a.start() < b.start(); });
    std::vector<Selection> merged;
    for (const auto& s : selections) {
        if (!merged.empty()) {
            auto& last = merged.back();
            const bool overlap = s.start() < last.end() || (s.start() == last.end() && (s.empty() || last.empty()));
            if (overlap || s.start() == last.start()) {
                const Position start = std::min(last.start(), s.start());
                const Position end = std::max(last.end(), s.end());
                const bool forward = !(last.caret < last.anchor);
                last.anchor = forward ? start : end;
                last.caret = forward ? end : start;
                continue;
            }
        }
        merged.push_back(s);
    }
    selections = std::move(merged);
    primary = selections.size() - 1;
    for (std::size_t i = 0; i < selections.size(); ++i) {
        if (selections[i].start() <= primaryCaret && primaryCaret <= selections[i].end()) {
            primary = i;
            break;
        }
    }
}

void CodeEditorImpl::apply(code::Transaction transaction, bool merge) {
    transaction.time = code::nowMs();
    editedSinceBound = true;
    if (merge && !undoStack.empty()) {
        auto& last = undoStack.back();
        const bool contiguous = last.after.size() == 1 && transaction.before.size() == 1 &&
                                last.after[0].caret == transaction.before[0].caret;
        const bool soon = transaction.time - last.time < 1000.0;
        bool newline = false;
        for (const auto& edit : transaction.edits) newline = newline || edit.inserted.find(L'\n') != std::wstring::npos;
        if (last.kind == transaction.kind && contiguous && soon && !newline) {
            last.edits.insert(last.edits.end(), transaction.edits.begin(), transaction.edits.end());
            last.after = transaction.after;
            last.time = transaction.time;
            redoStack.clear();
            touched();
            return;
        }
    }
    undoStack.push_back(std::move(transaction));
    if (undoStack.size() > 2000) undoStack.erase(undoStack.begin(), undoStack.begin() + 200);
    redoStack.clear();
    touched();
}

void CodeEditorImpl::replaceSelections(const std::vector<std::wstring>& texts, code::EditKind kind) {
    if (readOnly) return;
    normalizeSelections();
    code::Transaction transaction;
    transaction.kind = kind;
    transaction.before = selections;
    std::vector<std::size_t> order(selections.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return selections[b].start() < selections[a].start(); });
    std::vector<Position> carets(selections.size());
    std::vector<bool> done(selections.size(), false);
    for (const auto index : order) {
        const auto& s = selections[index];
        code::Edit edit{s.start(), doc.text(s.start(), s.end()), texts[std::min(index, texts.size() - 1)]};
        if (edit.removed.empty() && edit.inserted.empty()) {
            carets[index] = s.start();
            done[index] = true;
            continue;
        }
        applyEdit(edit);
        for (std::size_t j = 0; j < carets.size(); ++j)
            if (done[j]) carets[j] = code::shiftThrough(carets[j], edit);
        carets[index] = code::Document::advance(edit.start, edit.inserted);
        done[index] = true;
        transaction.edits.push_back(std::move(edit));
    }
    if (transaction.edits.empty()) return;
    for (std::size_t i = 0; i < selections.size(); ++i) selections[i] = Selection{carets[i], carets[i]};
    normalizeSelections();
    transaction.after = selections;
    apply(std::move(transaction), kind != code::EditKind::Other);
    reveal(selections[primary].caret);
}

void CodeEditorImpl::replaceSelections(const std::wstring& text, code::EditKind kind) {
    replaceSelections(std::vector<std::wstring>{text}, kind);
}

Position CodeEditorImpl::wordStart(Position p) const {
    p = doc.clamp(p);
    if (p.column == 0) return p.line == 0 ? p : Position{p.line - 1, doc.line(p.line - 1).size()};
    const auto& text = doc.line(p.line);
    auto i = p.column;
    while (i > 0 && (text[i - 1] == L' ' || text[i - 1] == L'\t')) --i;
    if (i > 0 && wordChar(text[i - 1])) {
        while (i > 0 && wordChar(text[i - 1])) --i;
    } else if (i > 0) {
        const wchar_t kind = text[i - 1];
        while (i > 0 && text[i - 1] == kind) --i;
    }
    return {p.line, i};
}

Position CodeEditorImpl::wordEnd(Position p) const {
    p = doc.clamp(p);
    const auto& text = doc.line(p.line);
    if (p.column >= text.size()) return p.line + 1 < doc.lineCount() ? Position{p.line + 1, 0} : p;
    auto i = p.column;
    while (i < text.size() && (text[i] == L' ' || text[i] == L'\t')) ++i;
    if (i < text.size() && wordChar(text[i])) {
        while (i < text.size() && wordChar(text[i])) ++i;
    } else if (i < text.size()) {
        const wchar_t kind = text[i];
        while (i < text.size() && text[i] == kind) ++i;
    }
    return {p.line, i};
}

void CodeEditorImpl::deleteBackward(bool word) {
    for (auto& s : selections) {
        if (!s.empty()) continue;
        Position p = s.caret;
        if (word) p = wordStart(p);
        else if (p.column > 0) {
            p.column -= 1;
            if (p.column > 0 && lowSurrogate(doc.line(p.line)[p.column])) p.column -= 1;
            // Remove a whole indentation step inside leading spaces.
            const auto& text = doc.line(p.line);
            if (insertSpaces && s.caret.column <= leadingSpace(text) && text[p.column] == L' ') {
                const auto unit = static_cast<std::size_t>(std::max(1, tabSize));
                const auto target = (s.caret.column - 1) / unit * unit;
                while (p.column > target && text[p.column - 1] == L' ') --p.column;
            }
        } else if (p.line > 0) p = {p.line - 1, doc.line(p.line - 1).size()};
        s.anchor = p;
    }
    replaceSelections(L"", code::EditKind::Deleting);
}

void CodeEditorImpl::deleteForward(bool word) {
    for (auto& s : selections) {
        if (!s.empty()) continue;
        Position p = s.caret;
        if (word) p = wordEnd(p);
        else if (p.column < doc.line(p.line).size()) {
            p.column += 1;
            if (p.column < doc.line(p.line).size() && lowSurrogate(doc.line(p.line)[p.column])) p.column += 1;
        } else if (p.line + 1 < doc.lineCount()) p = {p.line + 1, 0};
        s.anchor = p;
    }
    replaceSelections(L"", code::EditKind::Deleting);
}

void CodeEditorImpl::newline() {
    normalizeSelections();
    std::vector<std::wstring> texts;
    for (const auto& s : selections) {
        const auto& text = doc.line(s.start().line);
        texts.push_back(L"\n" + text.substr(0, std::min(leadingSpace(text), s.start().column)));
    }
    replaceSelections(texts, code::EditKind::Other);
}

void CodeEditorImpl::move(int dx, int dy, bool extend, bool word) {
    for (auto& s : selections) {
        Position p = s.caret;
        if (dx != 0) {
            if (!extend && !s.empty() && !word) p = dx < 0 ? s.start() : s.end();
            else if (dx < 0) {
                if (word) p = wordStart(p);
                else if (p.column > 0) {
                    p.column -= 1;
                    if (p.column > 0 && lowSurrogate(doc.line(p.line)[p.column])) p.column -= 1;
                } else if (p.line > 0) p = {p.line - 1, doc.line(p.line - 1).size()};
            } else {
                if (word) p = wordEnd(p);
                else if (p.column < doc.line(p.line).size()) {
                    p.column += 1;
                    if (p.column < doc.line(p.line).size() && lowSurrogate(doc.line(p.line)[p.column])) p.column += 1;
                } else if (p.line + 1 < doc.lineCount()) p = {p.line + 1, 0};
            }
            s.preferredX = -1.0f;
        }
        if (dy != 0) {
            const Rect caret = caretRect(p);
            const float x = s.preferredX >= 0.0f ? s.preferredX : caret.x + horizontal;
            const auto row = static_cast<long long>(rowOf(p)) + dy;
            if (row < 0) p = {0, 0};
            else if (row >= static_cast<long long>(rowCount())) p = doc.end();
            else {
                const Rect text = textRect();
                p = hit(Point{x - horizontal, text.y + 4.0f + static_cast<float>(row) * lineHeight - scroll + lineHeight * 0.5f});
            }
            s.preferredX = x;
        }
        s.caret = p;
        if (!extend) s.anchor = p;
    }
    normalizeSelections();
    reveal(selections[primary].caret);
    cursorMoved();
}

void CodeEditorImpl::home(bool extend, bool document) {
    for (auto& s : selections) {
        Position p = s.caret;
        if (document) p = {0, 0};
        else {
            std::size_t start = 0;
            if (wrap) {
                const auto segs = segments(p.line);
                for (auto b : segs) if (b <= p.column) start = b;
            }
            const auto indent = start == 0 ? leadingSpace(doc.line(p.line)) : start;
            p.column = p.column == indent ? start : indent;
        }
        s.caret = p;
        s.preferredX = -1.0f;
        if (!extend) s.anchor = p;
    }
    normalizeSelections();
    reveal(selections[primary].caret);
    cursorMoved();
}

void CodeEditorImpl::end(bool extend, bool document) {
    for (auto& s : selections) {
        Position p = s.caret;
        if (document) p = doc.end();
        else {
            std::size_t stop = doc.line(p.line).size();
            if (wrap) {
                const auto segs = segments(p.line);
                for (std::size_t i = 0; i + 1 < segs.size(); ++i)
                    if (segs[i] <= p.column && p.column < segs[i + 1]) stop = segs[i + 1];
            }
            p.column = stop;
        }
        s.caret = p;
        s.preferredX = -1.0f;
        if (!extend) s.anchor = p;
    }
    normalizeSelections();
    reveal(selections[primary].caret);
    cursorMoved();
}

void CodeEditorImpl::page(int direction, bool extend) {
    const int rows = std::max(1, static_cast<int>(textRect().height / lineHeight) - 1);
    setScroll(scroll + static_cast<float>(direction * rows) * lineHeight);
    move(0, direction * rows, extend, false);
}

std::pair<std::size_t, std::size_t> CodeEditorImpl::selectedLines(const Selection& s) const {
    auto first = s.start().line, last = s.end().line;
    if (last > first && s.end().column == 0) --last;
    return {first, last};
}

void CodeEditorImpl::editLines(const std::function<std::wstring(std::size_t, const std::wstring&)>& transform) {
    if (readOnly) return;
    normalizeSelections();
    std::vector<std::size_t> lines;
    for (const auto& s : selections) {
        const auto [first, last] = selectedLines(s);
        for (auto line = first; line <= last; ++line) lines.push_back(line);
    }
    std::sort(lines.begin(), lines.end());
    lines.erase(std::unique(lines.begin(), lines.end()), lines.end());
    code::Transaction transaction;
    transaction.before = selections;
    std::unordered_map<std::size_t, long long> delta;
    for (auto it = lines.rbegin(); it != lines.rend(); ++it) {
        const auto& old = doc.line(*it);
        auto next = transform(*it, old);
        if (next == old) continue;
        code::Edit edit{{*it, 0}, old, next};
        delta[*it] = static_cast<long long>(next.size()) - static_cast<long long>(old.size());
        applyEdit(edit);
        transaction.edits.push_back(std::move(edit));
    }
    if (transaction.edits.empty()) return;
    auto shift = [&](Position& p) {
        const auto found = delta.find(p.line);
        if (found == delta.end() || p.column == 0) return;
        p.column = static_cast<std::size_t>(std::max<long long>(0, static_cast<long long>(p.column) + found->second));
    };
    for (auto& s : selections) {
        shift(s.anchor);
        shift(s.caret);
    }
    normalizeSelections();
    transaction.after = selections;
    apply(std::move(transaction), false);
}

void CodeEditorImpl::resetHistory() {
    undoStack.clear();
    redoStack.clear();
}

bool CodeEditorImpl::undo() {
    if (undoStack.empty() || readOnly) return false;
    auto transaction = std::move(undoStack.back());
    undoStack.pop_back();
    for (auto it = transaction.edits.rbegin(); it != transaction.edits.rend(); ++it)
        applyEdit(code::Edit{it->start, it->inserted, it->removed});
    selections = transaction.before;
    normalizeSelections();
    redoStack.push_back(std::move(transaction));
    touched();
    reveal(selections[primary].caret);
    return true;
}

bool CodeEditorImpl::redo() {
    if (redoStack.empty() || readOnly) return false;
    auto transaction = std::move(redoStack.back());
    redoStack.pop_back();
    for (const auto& edit : transaction.edits) applyEdit(edit);
    selections = transaction.after;
    normalizeSelections();
    undoStack.push_back(std::move(transaction));
    touched();
    reveal(selections[primary].caret);
    return true;
}

void CodeEditorImpl::touched() {
    ++revision;
    matchesDirty = true;
    changedPending = true;
    cursorPending = true;
    notify(false);
    owner.invalidate();
}

void CodeEditorImpl::cursorMoved() {
    cursorPending = true;
    notify(false);
    owner.invalidate();
}

void CodeEditorImpl::notify(bool force) {
    const double now = code::nowMs();
    // Without a listener nothing is pending, so the quiet period is not reset.
    if (!onChanged) changedPending = false;
    if (!onCursor) cursorPending = false;
    if (changedPending && (force || now - lastChanged >= throttle)) {
        changedPending = false;
        lastChanged = now;
        onChanged(revision);
    }
    if (cursorPending && (force || now - lastCursor >= throttle)) {
        cursorPending = false;
        lastCursor = now;
        onCursor(state());
    }
    if (changedPending || cursorPending) owner.requestAnimationFrame();
}

CodeEditorState CodeEditorImpl::state() const {
    CodeEditorState result;
    const auto& s = selections[std::min(primary, selections.size() - 1)];
    result.revision = revision;
    result.caret = doc.byteOffset(s.caret);
    result.anchor = doc.byteOffset(s.anchor);
    result.line = s.caret.line + 1;
    std::size_t column = 1;
    const auto& text = doc.line(s.caret.line);
    for (std::size_t i = 0; i < s.caret.column && i < text.size(); ++i)
        if (!lowSurrogate(text[i])) ++column;
    result.column = column;
    result.length = doc.byteLength();
    result.lineCount = doc.lineCount();
    result.cursors = selections.size();
    return result;
}

std::optional<std::pair<Position, Position>> CodeEditorImpl::bracketMatch() const {
    const auto& s = selections[std::min(primary, selections.size() - 1)];
    if (!s.empty()) return std::nullopt;
    static const std::wstring open = L"([{", close = L")]}";
    const auto& text = doc.line(s.caret.line);
    auto at = [&](std::size_t column) -> std::optional<Position> {
        if (column >= text.size()) return std::nullopt;
        const wchar_t c = text[column];
        if (open.find(c) == std::wstring::npos && close.find(c) == std::wstring::npos) return std::nullopt;
        if (lexer.inert(doc, s.caret.line, column)) return std::nullopt;
        return Position{s.caret.line, column};
    };
    auto origin = at(s.caret.column);
    if (!origin && s.caret.column > 0) origin = at(s.caret.column - 1);
    if (!origin) return std::nullopt;
    const wchar_t c = doc.line(origin->line)[origin->column];
    const bool forward = open.find(c) != std::wstring::npos;
    const wchar_t mate = forward ? close[open.find(c)] : open[close.find(c)];
    int depth = 0;
    std::size_t scanned = 0;
    Position p = *origin;
    std::vector<CodeToken> tokens = lexer.tokens(doc, p.line);
    auto inert = [&](std::size_t column) {
        for (const auto& token : tokens)
            if (column >= token.start && column < token.start + token.length)
                return token.kind == CodeTokenKind::String || token.kind == CodeTokenKind::Comment ||
                       token.kind == CodeTokenKind::Identifier;
        return false;
    };
    while (scanned < 5000) {
        const auto& line = doc.line(p.line);
        if (p.column < line.size() && !inert(p.column)) {
            if (line[p.column] == c) ++depth;
            else if (line[p.column] == mate && --depth == 0) return std::make_pair(*origin, p);
        }
        if (forward) {
            if (p.column + 1 < line.size()) ++p.column;
            else {
                do {
                    if (p.line + 1 >= doc.lineCount()) return std::nullopt;
                    ++p.line;
                    ++scanned;
                } while (doc.line(p.line).empty());
                p.column = 0;
                tokens = lexer.tokens(doc, p.line);
            }
        } else {
            if (p.column > 0) --p.column;
            else {
                do {
                    if (p.line == 0) return std::nullopt;
                    --p.line;
                    ++scanned;
                } while (doc.line(p.line).empty());
                p.column = doc.line(p.line).size() - 1;
                tokens = lexer.tokens(doc, p.line);
            }
        }
    }
    return std::nullopt;
}

} // namespace oneui
