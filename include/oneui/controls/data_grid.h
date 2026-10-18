#pragma once
#include "oneui/controls/text_field.h"
#include "oneui/view.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace oneui {

// Spreadsheet-style grid over a data source of any size. The application owns
// the rows: the grid knows only the row count and a window of materialized
// rows, and asks for the window it needs through onViewport. Columns resize,
// cells select as rectangles, and editable grids edit cells in place.

struct DataGridColumn {
    std::wstring id;
    std::wstring title;
    // Secondary header text (type name); shown after the title.
    std::wstring detail;
    float width = 120.0f;
    TextAlign alignment = TextAlign::Left;
    // 0 none, 1 ascending, 2 descending.
    int sort = 0;
    // Part of the row key; marked in the header.
    bool key = false;
};

struct DataGridCell {
    std::wstring text;
    // Space-separated presentation tokens: null, number, modified, inserted,
    // deleted, error, truncated, muted.
    std::string classes;
};

struct DataGridRow {
    std::wstring number;
    // Row presentation tokens: inserted, deleted, modified.
    std::string classes;
    std::vector<DataGridCell> cells;
};

enum class DataGridSelectKind { Cells = 0, Rows = 1, Columns = 2, All = 3 };

// Rows and columns are absolute indices. An empty grid has no focus (-1).
struct DataGridSelection {
    std::int64_t anchorRow = -1;
    int anchorColumn = -1;
    std::int64_t focusRow = -1;
    int focusColumn = -1;
    DataGridSelectKind kind = DataGridSelectKind::Cells;

    bool operator==(const DataGridSelection& other) const {
        return anchorRow == other.anchorRow && anchorColumn == other.anchorColumn && focusRow == other.focusRow &&
               focusColumn == other.focusColumn && kind == other.kind;
    }
    bool operator!=(const DataGridSelection& other) const { return !(*this == other); }
    bool empty() const { return focusRow < 0 || focusColumn < 0; }
    std::int64_t firstRow() const { return std::min(anchorRow, focusRow); }
    std::int64_t lastRow() const { return std::max(anchorRow, focusRow); }
    int firstColumn() const { return std::min(anchorColumn, focusColumn); }
    int lastColumn() const { return std::max(anchorColumn, focusColumn); }
    bool contains(std::int64_t row, int column) const;
};

class DataGridSurface;

class ONEUI_API DataGrid final : public View {
public:
    DataGrid();
    ~DataGrid() override;

    void setColumns(std::vector<DataGridColumn> columns);
    const std::vector<DataGridColumn>& columns() const;
    void setRowCount(std::int64_t count);
    std::int64_t rowCount() const;
    // Materialized rows start at `start`; rows outside the window paint as placeholders.
    void setWindow(std::int64_t start, std::vector<DataGridRow> rows);
    void setWindowStart(std::int64_t start);
    void insertWindowRows(std::size_t at, std::vector<DataGridRow> rows);
    void eraseWindowRows(std::size_t at, std::size_t count);
    void updateWindowRow(std::size_t at, DataGridRow row);
    std::int64_t windowStart() const;
    std::size_t windowSize() const;
    const DataGridRow* row(std::int64_t index) const;

    void setFrozenColumns(int count);
    void setRowHeight(float height);
    void setHeaderHeight(float height);
    void setRowNumbersVisible(bool visible);
    void setEditable(bool editable);
    bool editable() const;

    void setSelection(DataGridSelection selection);
    const DataGridSelection& selection() const;
    void scrollToCell(std::int64_t row, int column);
    float scrollOffset() const;
    float horizontalOffset() const;
    std::int64_t firstVisibleRow() const;
    std::int64_t visibleRowCount() const;

    // Starts editing the focused cell with `initial` text (empty: the cell's text).
    bool beginEdit(const std::wstring* initial = nullptr);
    void cancelEdit();
    bool editing() const;

    void setOnViewport(std::function<void(std::int64_t first, std::int64_t count)> callback);
    void setOnSort(std::function<void(int column)> callback);
    void setOnResize(std::function<void(int column, float width)> callback);
    void setOnSelectionChanged(std::function<void(const DataGridSelection&)> callback);
    void setOnEdit(std::function<void(std::int64_t row, int column, const std::wstring& text)> callback);
    void setOnContextMenu(std::function<void(std::int64_t row, int column, Point point)> callback);
    void setOnCopy(std::function<void()> callback);
    void setOnActivate(std::function<void(std::int64_t row, int column)> callback);

    bool onKeyDown(const KeyEvent& event) override;
    bool onMouseDown(const MouseEvent& event) override;
    bool onTextInput(wchar_t character) override;
    // IME composition and committed text start editing like a typed character.
    bool onTextCommitted(const std::wstring& text) override;
    TextInputState textInputState() const override;
    void setTextComposition(std::wstring text, std::size_t caret) override;
    Rect textInputCaretRect() const override;
    bool onFocusChanged(bool focused) override;
    AccessibilityInfo accessibilityInfo() const override;

protected:
    void layoutChildren() override;

private:
    friend class DataGridSurface;
    void finishEdit(bool commit, int columnStep, int rowStep);

    std::shared_ptr<DataGridSurface> surface_;
    std::shared_ptr<TextField> editor_;
    bool editing_ = false;
    std::int64_t editRow_ = -1;
    int editColumn_ = -1;
    std::function<void(std::int64_t, int, const std::wstring&)> onEdit_;
};

} // namespace oneui
