// Default colors, document switching and toolbar commands.
#include "code_editor_impl.h"

#include "oneui/controls/text_field.h"

#include <algorithm>
#include <cmath>

namespace oneui {
namespace {

Color blend(Color a, Color b, float t) {
    auto mix = [t](std::uint8_t x, std::uint8_t y) {
        return static_cast<std::uint8_t>(std::lround(static_cast<float>(x) + (static_cast<float>(y) - static_cast<float>(x)) * t));
    };
    return Color{mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b), mix(a.a, b.a)};
}

Color withAlpha(Color color, std::uint8_t a) {
    color.a = a;
    return color;
}

} // namespace

CodeEditorStyle codeEditorStyleFromTheme(const Theme& t) {
    CodeEditorStyle s;
    s.background = t.surface;
    s.foreground = t.text;
    s.gutterBackground = t.surfaceMuted;
    s.gutterForeground = t.textSubtle;
    s.gutterActiveForeground = t.textMuted;
    s.gutterBorder = withAlpha(t.border, 200);
    s.currentLine = withAlpha(t.hoverBackground, 200);
    s.selection = withAlpha(t.primary, 70);
    s.inactiveSelection = withAlpha(t.textSubtle, 60);
    s.caret = t.text;
    s.indentGuide = withAlpha(t.border, 160);
    s.accent = t.primary;
    s.accentSoft = t.primarySoft;
    s.match = withAlpha(t.warning, 50);
    s.currentMatch = withAlpha(t.warning, 110);
    s.error = t.error;
    s.warning = t.warning;
    s.success = t.success;
    s.panelBackground = t.surface;
    s.panelForeground = t.text;
    s.panelMuted = t.textMuted;
    s.panelBorder = t.border;
    s.shadow = t.shadowColor;
    s.scrollbar = withAlpha(t.borderStrong, 150);
    auto set = [&](CodeTokenKind kind, Color color) { s.tokens[static_cast<std::size_t>(kind)] = color; };
    set(CodeTokenKind::Plain, t.text);
    set(CodeTokenKind::Keyword, t.primary);
    set(CodeTokenKind::Type, blend(t.primary, t.error, 0.45f));
    set(CodeTokenKind::Function, blend(t.warning, t.text, 0.35f));
    set(CodeTokenKind::Constant, blend(t.error, t.primary, 0.3f));
    set(CodeTokenKind::Number, t.warning);
    set(CodeTokenKind::String, t.success);
    set(CodeTokenKind::Identifier, blend(t.text, t.primary, 0.25f));
    set(CodeTokenKind::Comment, t.textSubtle);
    set(CodeTokenKind::Operator, t.textMuted);
    set(CodeTokenKind::Parameter, blend(t.error, t.warning, 0.5f));
    return s;
}

void CodeEditorImpl::park(ParkedDocument& out) {
    out.doc = std::move(doc);
    out.lexer = std::move(lexer);
    out.selections = std::move(selections);
    out.primary = primary;
    out.undoStack = std::move(undoStack);
    out.redoStack = std::move(redoStack);
    out.scroll = scroll;
    out.horizontal = horizontal;
    out.longestLine = longestLine;
    out.decorations = std::move(decorations);
    out.editLog = std::move(editLog);
    out.logFloor = logFloor;
}

void CodeEditorImpl::unpark(ParkedDocument&& in) {
    doc = std::move(in.doc);
    lexer = std::move(in.lexer);
    selections = std::move(in.selections);
    if (selections.empty()) selections = {code::Selection{}};
    primary = std::min(in.primary, selections.size() - 1);
    undoStack = std::move(in.undoStack);
    redoStack = std::move(in.redoStack);
    scroll = in.scroll;
    horizontal = in.horizontal;
    longestLine = in.longestLine;
    decorations = std::move(in.decorations);
    editLog = std::move(in.editLog);
    logFloor = in.logFloor;
}

void CodeEditorImpl::styleFindFields(TextField& field) const {
    TextFieldStateStyleOverride normal;
    normal.background = blend(style.panelBackground, style.panelForeground, 0.06f);
    normal.foreground = style.panelForeground;
    normal.placeholderForeground = style.panelMuted;
    normal.border = style.panelBorder;
    normal.selectionBackground = style.selection;
    normal.caretColor = style.caret;
    normal.borderWidth = 1.0f;
    normal.radius = 4.0f;
    auto focused = normal;
    focused.border = style.accent;
    TextFieldStyleOverride override;
    override.normal = normal;
    override.hovered = normal;
    override.focusVisible = focused;
    field.setStyleOverride(override);
}

bool CodeEditor::selectDocument(const std::wstring& id) {
    auto& d = *impl_;
    if (id == d.documentId) return false;
    d.closeCompletion();
    d.preedit.clear();
    d.drag = CodeEditorImpl::Drag::None;
    // Language and line states belong to the document; the shown one moves aside.
    d.park(d.parked[d.documentId]);
    const auto found = d.parked.find(id);
    const bool created = found == d.parked.end();
    if (created) {
        // A fresh document keeps the current language until the product sets one.
        CodeEditorImpl::ParkedDocument fresh;
        fresh.lexer.setLanguage(d.parked[d.documentId].lexer.language());
        fresh.lexer.reset(fresh.doc);
        d.unpark(std::move(fresh));
    } else {
        d.unpark(std::move(found->second));
        d.parked.erase(found);
    }
    d.documentId = id;
    d.rowsValid = false;
    d.matchesDirty = true;
    d.current = -1;
    d.cursorMoved();
    invalidate();
    return created;
}

void CodeEditor::closeDocument(const std::wstring& id) {
    auto& d = *impl_;
    if (id != d.documentId) {
        d.parked.erase(id);
        return;
    }
    if (!id.empty()) {
        // Show "" (restored, or new and empty) and forget the closed document.
        selectDocument(L"");
        d.parked.erase(id);
        return;
    }
    // Closing "" itself leaves an empty "" in its place.
    const auto language = d.lexer.language();
    d.closeCompletion();
    d.preedit.clear();
    CodeEditorImpl::ParkedDocument fresh;
    fresh.lexer.setLanguage(language);
    fresh.lexer.reset(fresh.doc);
    d.unpark(std::move(fresh));
    d.rowsValid = false;
    d.matchesDirty = true;
    d.current = -1;
    d.cursorMoved();
    invalidate();
}

const std::wstring& CodeEditor::documentId() const { return impl_->documentId; }

std::vector<std::wstring> CodeEditor::documentIds() const {
    std::vector<std::wstring> ids{impl_->documentId};
    for (const auto& [id, _] : impl_->parked) ids.push_back(id);
    std::sort(ids.begin(), ids.end());
    return ids;
}

bool CodeEditor::canUndo() const { return !impl_->undoStack.empty() && !impl_->readOnly; }
bool CodeEditor::canRedo() const { return !impl_->redoStack.empty() && !impl_->readOnly; }

bool CodeEditor::execute(CodeEditorCommand command) {
    auto& d = *impl_;
    const auto editable = [&] { return !d.readOnly; };
    switch (command) {
    case CodeEditorCommand::Undo: return undo();
    case CodeEditorCommand::Redo: return redo();
    case CodeEditorCommand::Find: openFind(false); return true;
    case CodeEditorCommand::Replace:
        if (!editable()) return false;
        openFind(true);
        return true;
    case CodeEditorCommand::CloseFind:
        if (!findOpen()) return false;
        closeFind();
        return true;
    case CodeEditorCommand::SelectAll: selectAll(); return true;
    case CodeEditorCommand::ToggleComment:
        if (!editable()) return false;
        toggleComment();
        return true;
    case CodeEditorCommand::Indent:
        if (!editable()) return false;
        indentSelection();
        return true;
    case CodeEditorCommand::Outdent:
        if (!editable()) return false;
        outdentSelection();
        return true;
    case CodeEditorCommand::DuplicateLines:
        if (!editable()) return false;
        duplicateLines();
        return true;
    case CodeEditorCommand::Cut: return editable() && cut();
    case CodeEditorCommand::Copy: return copy();
    case CodeEditorCommand::Paste: return editable() && paste();
    }
    return false;
}

void CodeEditor::setStyle(CodeEditorStyle style) {
    impl_->style = style;
    impl_->styleFindFields(*findField_);
    impl_->styleFindFields(*replaceField_);
    invalidate();
}

const CodeEditorStyle& CodeEditor::style() const { return impl_->style; }

} // namespace oneui
