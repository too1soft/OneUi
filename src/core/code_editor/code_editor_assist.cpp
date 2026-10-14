#include "code_editor_impl.h"

#include "oneui/controls/popup.h"

#include <algorithm>
#include <cwctype>

namespace oneui {
namespace {

using code::Position;

constexpr std::size_t logLimit = 4096;

bool wordChar(wchar_t c) { return c == L'_' || std::iswalnum(c) || c >= 0x80; }

std::wstring lower(std::wstring text) {
    for (auto& c : text) c = static_cast<wchar_t>(std::towlower(c));
    return text;
}

std::size_t mapOffset(std::size_t offset, const code::ByteEdit& edit) {
    if (offset < edit.start) return offset;
    if (offset >= edit.start + edit.removed) return offset - edit.removed + edit.inserted;
    return edit.start + edit.inserted;
}

} // namespace

void CodeEditorImpl::logEdit(const code::Edit& edit) {
    // Tagged with the revision this change will produce (touched() bumps it).
    editLog.push_back({revision + 1, doc.byteOffset(edit.start), code::utf8Length(edit.removed), code::utf8Length(edit.inserted)});
    if (editLog.size() > logLimit) {
        const auto drop = editLog.size() - logLimit / 2;
        logFloor = editLog[drop - 1].revision;
        editLog.erase(editLog.begin(), editLog.begin() + static_cast<std::ptrdiff_t>(drop));
    }
}

void CodeEditorImpl::mapDecorations(const code::Edit& edit) {
    if (decorations.empty()) return;
    const Position removedEnd = code::Document::advance(edit.start, edit.removed);
    decorations.erase(std::remove_if(decorations.begin(), decorations.end(),
                                     [&](const code::Decoration& d) {
                                         // Deleted with the text it described.
                                         return !edit.removed.empty() && edit.start <= d.start && d.end <= removedEnd &&
                                                d.start < d.end;
                                     }),
                      decorations.end());
    for (auto& d : decorations) {
        const bool empty = d.start == d.end;
        d.start = code::shiftThrough(d.start, edit);
        d.end = empty ? d.start : code::shiftThrough(d.end, edit);
    }
}

Position CodeEditorImpl::wordStartBefore(Position caret) const {
    caret = doc.clamp(caret);
    const auto& text = doc.line(caret.line);
    auto column = caret.column;
    while (column > 0 && wordChar(text[column - 1])) --column;
    return {caret.line, column};
}

void CodeEditorImpl::filterCompletions() {
    std::wstring previousId;
    if (completionSelected >= 0 && static_cast<std::size_t>(completionSelected) < completionFiltered.size())
        previousId = completionItems[completionFiltered[static_cast<std::size_t>(completionSelected)]].id;
    completionFiltered.clear();
    const auto caret = selections[std::min(primary, selections.size() - 1)].caret;
    if (caret < completionStart || caret.line != completionStart.line) return;
    const auto prefix = lower(doc.text(completionStart, caret));
    std::vector<std::size_t> contains;
    for (std::size_t i = 0; i < completionItems.size(); ++i) {
        const auto label = lower(completionItems[i].label);
        if (prefix.empty() || label.compare(0, prefix.size(), prefix) == 0) completionFiltered.push_back(i);
        else if (label.find(prefix) != std::wstring::npos) contains.push_back(i);
    }
    completionFiltered.insert(completionFiltered.end(), contains.begin(), contains.end());
    completionSelected = completionFiltered.empty() ? -1 : 0;
    for (std::size_t i = 0; i < completionFiltered.size() && !previousId.empty(); ++i) {
        if (completionItems[completionFiltered[i]].id == previousId) {
            completionSelected = static_cast<int>(i);
            break;
        }
    }
    completionTop = std::clamp(completionTop, 0, std::max(0, static_cast<int>(completionFiltered.size()) - completionRows));
    if (completionSelected >= 0) moveCompletion(0);
}

void CodeEditorImpl::updateCompletionVisibility() {
    completionVisible = completionWanted && !completionFiltered.empty() && selections.size() == 1;
    owner.invalidate();
}

void CodeEditorImpl::requestCompletion(CodeCompletionTrigger trigger) {
    const auto caret = selections[std::min(primary, selections.size() - 1)].caret;
    if (!completionVisible) completionStart = wordStartBefore(caret);
    if (onCompletionRequested) onCompletionRequested(doc.byteOffset(caret), trigger);
}

void CodeEditorImpl::closeCompletion() {
    const bool was = completionVisible || completionWanted;
    completionVisible = false;
    completionWanted = false;
    completionSelected = -1;
    completionTop = 0;
    owner.invalidate();
    if (was && onCompletionClosed) onCompletionClosed();
}

bool CodeEditorImpl::acceptCompletion() {
    if (!completionVisible || completionSelected < 0 || readOnly) return false;
    const auto item = completionItems[completionFiltered[static_cast<std::size_t>(completionSelected)]];
    const auto caret = selections[std::min(primary, selections.size() - 1)].caret;
    Position end = caret;
    if (item.replaceEnd > item.replaceStart) end = std::max(caret, doc.fromByteOffset(item.replaceEnd));
    selections = {code::Selection{completionStart, end}};
    primary = 0;
    replaceSelections(item.insert.empty() ? item.label : item.insert, code::EditKind::Other);
    closeCompletion();
    if (onCompletionAccepted) onCompletionAccepted(item.id);
    return true;
}

void CodeEditorImpl::moveCompletion(int delta) {
    if (completionFiltered.empty()) return;
    const int last = static_cast<int>(completionFiltered.size()) - 1;
    completionSelected = std::clamp(std::max(0, completionSelected) + delta, 0, last);
    if (completionSelected < completionTop) completionTop = completionSelected;
    if (completionSelected >= completionTop + completionRows) completionTop = completionSelected - completionRows + 1;
    owner.invalidate();
}

void CodeEditorImpl::afterTyping(const std::wstring& text) {
    if (text.size() != 1 || selections.size() != 1) {
        closeCompletion();
        return;
    }
    const wchar_t c = text[0];
    if (c == L'.') {
        completionVisible = false;
        completionStart = selections[primary].caret;
        requestCompletion(CodeCompletionTrigger::Member);
        return;
    }
    if (!wordChar(c)) {
        closeCompletion();
        return;
    }
    if (completionVisible) {
        filterCompletions();
        updateCompletionVisibility();
        requestCompletion(CodeCompletionTrigger::Character);
        return;
    }
    // Start of a new word.
    const auto caret = selections[primary].caret;
    if (wordStartBefore(caret).column + 1 == caret.column) {
        completionStart = wordStartBefore(caret);
        requestCompletion(CodeCompletionTrigger::Character);
    }
}

Rect CodeEditorImpl::completionRect() const {
    const auto rows = std::min<std::size_t>(completionFiltered.size(), completionRows);
    const Size size{380.0f, static_cast<float>(rows) * completionRowHeight + 8.0f};
    const Rect anchor = caretRect(completionStart);
    const auto result = PopupPlacement::resolve(
        PopupPlacementRequest{Rect{anchor.x - 6.0f, anchor.y, 2.0f, anchor.height}, size, owner.frame(),
                              PopupPreferredPlacement::BottomStart, 2.0f});
    return result.rect;
}

int CodeEditorImpl::completionRowAt(Point point) const {
    if (!completionVisible) return -1;
    const Rect r = completionRect();
    if (!r.contains(point)) return -1;
    const int row = static_cast<int>((point.y - r.y - 4.0f) / completionRowHeight) + completionTop;
    return row >= 0 && row < static_cast<int>(completionFiltered.size()) ? row : -1;
}

} // namespace oneui
