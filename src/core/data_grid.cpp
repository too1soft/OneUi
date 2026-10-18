#include "oneui/controls/data_grid.h"

#include "data_grid_surface.h"

#include <algorithm>

namespace oneui {

DataGrid::DataGrid()
    : surface_(std::make_shared<DataGridSurface>(*this)), editor_(std::make_shared<TextField>()) {
    setAccessibleRole(AccessibilityRole::Table);
    editor_->setVisible(false);
    editor_->setSubmitOnEnter(false);
    add(surface_);
    add(editor_);
}

DataGrid::~DataGrid() = default;

void DataGrid::setColumns(std::vector<DataGridColumn> columns) {
    if (editing_ && editColumn_ >= static_cast<int>(columns.size())) cancelEdit();
    surface_->columns = std::move(columns);
    surface_->dataChanged();
    layoutChildren();
}

const std::vector<DataGridColumn>& DataGrid::columns() const { return surface_->columns; }

void DataGrid::setRowCount(std::int64_t count) {
    count = std::max<std::int64_t>(0, count);
    if (count == surface_->rowCount) return;
    if (editing_ && editRow_ >= count) cancelEdit();
    surface_->rowCount = count;
    surface_->dataChanged();
    layoutChildren();
}

std::int64_t DataGrid::rowCount() const { return surface_->rowCount; }

void DataGrid::setWindow(std::int64_t start, std::vector<DataGridRow> rows) {
    surface_->windowStart = std::max<std::int64_t>(0, start);
    surface_->window.assign(std::make_move_iterator(rows.begin()), std::make_move_iterator(rows.end()));
    surface_->refresh();
}

void DataGrid::setWindowStart(std::int64_t start) {
    surface_->windowStart = std::max<std::int64_t>(0, start);
    surface_->refresh();
}

void DataGrid::insertWindowRows(std::size_t at, std::vector<DataGridRow> rows) {
    auto& window = surface_->window;
    at = std::min(at, window.size());
    window.insert(window.begin() + static_cast<std::ptrdiff_t>(at), std::make_move_iterator(rows.begin()),
                  std::make_move_iterator(rows.end()));
    surface_->refresh();
}

void DataGrid::eraseWindowRows(std::size_t at, std::size_t count) {
    auto& window = surface_->window;
    if (at >= window.size()) return;
    count = std::min(count, window.size() - at);
    window.erase(window.begin() + static_cast<std::ptrdiff_t>(at), window.begin() + static_cast<std::ptrdiff_t>(at + count));
    surface_->refresh();
}

void DataGrid::updateWindowRow(std::size_t at, DataGridRow row) {
    if (at >= surface_->window.size()) return;
    surface_->window[at] = std::move(row);
    surface_->refresh();
}

std::int64_t DataGrid::windowStart() const { return surface_->windowStart; }
std::size_t DataGrid::windowSize() const { return surface_->window.size(); }
const DataGridRow* DataGrid::row(std::int64_t index) const { return surface_->row(index); }

void DataGrid::setFrozenColumns(int count) {
    surface_->frozenColumns = std::max(0, count);
    surface_->refresh();
}

void DataGrid::setRowHeight(float height) {
    surface_->rowHeight = std::clamp(height, 16.0f, 96.0f);
    surface_->dataChanged();
}

void DataGrid::setHeaderHeight(float height) {
    surface_->headerHeight = std::clamp(height, 16.0f, 96.0f);
    surface_->dataChanged();
}

void DataGrid::setRowNumbersVisible(bool visible) {
    surface_->rowNumbers = visible;
    surface_->dataChanged();
}

void DataGrid::setEditable(bool editable) {
    if (!editable && editing_) cancelEdit();
    surface_->editable = editable;
}

bool DataGrid::editable() const { return surface_->editable; }

void DataGrid::setSelection(DataGridSelection selection) {
    surface_->setSelection(selection, false);
    if (!selection.empty()) surface_->scrollTo(selection.focusRow, selection.focusColumn);
}

const DataGridSelection& DataGrid::selection() const { return surface_->selection; }
void DataGrid::scrollToCell(std::int64_t row, int column) { surface_->scrollTo(row, column); }
float DataGrid::scrollOffset() const { return static_cast<float>(surface_->scroll()); }
float DataGrid::horizontalOffset() const { return surface_->horizontal(); }
std::int64_t DataGrid::firstVisibleRow() const { return surface_->firstVisible(); }
std::int64_t DataGrid::visibleRowCount() const { return surface_->visibleCount(); }

bool DataGrid::beginEdit(const std::wstring* initial) {
    const auto& selection = surface_->selection;
    if (!surface_->editable || selection.empty() || editing_) return false;
    editRow_ = selection.focusRow;
    editColumn_ = selection.focusColumn;
    surface_->scrollTo(editRow_, editColumn_);
    std::wstring text;
    if (initial) {
        text = *initial;
    } else if (const auto* row = surface_->row(editRow_); row && editColumn_ < static_cast<int>(row->cells.size())) {
        const auto& cell = row->cells[editColumn_];
        // A NULL cell shows a marker, not editable text.
        if (cell.classes.find("null") == std::string::npos) text = cell.text;
    }
    editing_ = true;
    editor_->setMultiline(text.find(L'\n') != std::wstring::npos);
    editor_->setText(text);
    if (initial) editor_->setCaretIndex(text.size());
    else editor_->setSelectionRange(0, text.size());
    editor_->setVisible(true);
    layoutChildren();
    requestFocus(editor_.get(), true);
    invalidate();
    return true;
}

void DataGrid::cancelEdit() { finishEdit(false, 0, 0); }

bool DataGrid::editing() const { return editing_; }

void DataGrid::finishEdit(bool commit, int columnStep, int rowStep) {
    if (!editing_) return;
    editing_ = false;
    const auto row = editRow_;
    const auto column = editColumn_;
    const std::wstring text = editor_->text();
    editor_->setVisible(false);
    requestFocus(surface_.get(), true);
    if (commit && onEdit_) onEdit_(row, column, text);
    if (rowStep != 0 || columnStep != 0) {
        KeyEvent move;
        move.key = rowStep > 0 ? Key::Down : rowStep < 0 ? Key::Up : columnStep > 0 ? Key::Right : Key::Left;
        surface_->onKeyDown(move);
    }
    invalidate();
}

void DataGrid::layoutChildren() {
    surface_->setFrame(frame());
    surface_->sizeChanged();
    if (!editing_) return;
    if (auto rect = surface_->cellRect(editRow_, editColumn_)) {
        editor_->setFrame(*rect);
        editor_->setVisible(true);
    } else {
        editor_->setVisible(false);
    }
}

bool DataGrid::onKeyDown(const KeyEvent& event) {
    if (editing_) {
        switch (event.key) {
        case Key::Escape: cancelEdit(); return true;
        case Key::Enter:
            if (event.alt) {
                // Alt+Enter inserts a line break; the editor turns multiline on demand.
                if (!editor_->multiline()) editor_->setMultiline(true);
                KeyEvent newline = event;
                newline.alt = false;
                return editor_->onKeyDown(newline);
            }
            finishEdit(true, 0, event.shift ? -1 : 1);
            return true;
        case Key::Tab: finishEdit(true, event.shift ? -1 : 1, 0); return true;
        default: break;
        }
        return View::onKeyDown(event);
    }
    // Outside editing the surface is the grid's only keyboard target.
    return surface_->onKeyDown(event);
}

bool DataGrid::onMouseDown(const MouseEvent& event) {
    // Clicking anywhere but the editor keeps what was typed, like a spreadsheet.
    if (editing_ && !editor_->frame().contains(event.position)) finishEdit(true, 0, 0);
    return View::onMouseDown(event);
}

namespace {
// iswprint depends on the C locale and rejects CJK under "C"; any non-control
// character starts editing.
bool startsEdit(wchar_t character) { return character >= 0x20 && character != 0x7f; }
} // namespace

bool DataGrid::onTextInput(wchar_t character) {
    if (!editing_ && surface_->editable && !surface_->selection.empty() && startsEdit(character)) {
        const std::wstring initial(1, character);
        return beginEdit(&initial);
    }
    return View::onTextInput(character);
}

bool DataGrid::onTextCommitted(const std::wstring& text) {
    if (!editing_ && surface_->editable && !surface_->selection.empty() && !text.empty() && startsEdit(text.front()))
        return beginEdit(&text);
    return View::onTextCommitted(text);
}

TextInputState DataGrid::textInputState() const {
    if (editing_ || !surface_->editable || surface_->selection.empty()) return View::textInputState();
    // An editable cell accepts IME input before the editor exists.
    TextInputState state;
    state.editable = true;
    state.identity = surface_.get();
    return state;
}

void DataGrid::setTextComposition(std::wstring text, std::size_t caret) {
    if (!editing_ && !text.empty()) {
        const std::wstring empty;
        if (!beginEdit(&empty)) return;
    }
    View::setTextComposition(std::move(text), caret);
}

Rect DataGrid::textInputCaretRect() const {
    if (editing_ || surface_->selection.empty()) return View::textInputCaretRect();
    const auto& selection = surface_->selection;
    if (auto rect = surface_->cellRect(selection.focusRow, selection.focusColumn)) return *rect;
    return frame();
}

bool DataGrid::onFocusChanged(bool focused) {
    if (!focused && editing_) finishEdit(true, 0, 0);
    const bool handled = View::onFocusChanged(focused);
    if (focused && !editing_ && focusedChild() != surface_.get()) requestFocus(surface_.get(), focusVisible());
    return handled;
}

AccessibilityInfo DataGrid::accessibilityInfo() const {
    AccessibilityInfo info = View::accessibilityInfo();
    info.role = AccessibilityRole::Table;
    const auto& selection = surface_->selection;
    if (!selection.empty())
        info.value = L"row " + std::to_wstring(selection.focusRow + 1) + L", column " + std::to_wstring(selection.focusColumn + 1);
    return info;
}

void DataGrid::setOnViewport(std::function<void(std::int64_t, std::int64_t)> callback) {
    surface_->onViewport = std::move(callback);
    surface_->dataChanged();
}
void DataGrid::setOnSort(std::function<void(int)> callback) { surface_->onSort = std::move(callback); }
void DataGrid::setOnResize(std::function<void(int, float)> callback) { surface_->onResize = std::move(callback); }
void DataGrid::setOnSelectionChanged(std::function<void(const DataGridSelection&)> callback) {
    surface_->onSelectionChanged = std::move(callback);
}
void DataGrid::setOnEdit(std::function<void(std::int64_t, int, const std::wstring&)> callback) { onEdit_ = std::move(callback); }
void DataGrid::setOnContextMenu(std::function<void(std::int64_t, int, Point)> callback) {
    surface_->onContextMenu = std::move(callback);
}
void DataGrid::setOnCopy(std::function<void()> callback) { surface_->onCopy = std::move(callback); }
void DataGrid::setOnActivate(std::function<void(std::int64_t, int)> callback) { surface_->onActivate = std::move(callback); }

} // namespace oneui
