#pragma once
#include "oneui/controls/data_grid.h"
#include "oneui/style.h"

#include <deque>
#include <optional>

namespace oneui {

// The painted part of DataGrid: geometry, scrolling, selection and input.
// Row positions use double precision so ten million rows keep pixel accuracy.
class DataGridSurface final : public Widget {
public:
    explicit DataGridSurface(DataGrid& owner) : owner_(owner) { setPreferredSize({640.0f, 320.0f}); }

    // Data.
    std::vector<DataGridColumn> columns;
    std::int64_t rowCount = 0;
    std::int64_t windowStart = 0;
    std::deque<DataGridRow> window;
    int frozenColumns = 0;
    float rowHeight = 28.0f;
    float headerHeight = 30.0f;
    bool rowNumbers = true;
    bool editable = false;
    DataGridSelection selection;

    // Callbacks.
    std::function<void(std::int64_t, std::int64_t)> onViewport;
    std::function<void(int)> onSort;
    std::function<void(int, float)> onResize;
    std::function<void(const DataGridSelection&)> onSelectionChanged;
    std::function<void(std::int64_t, int, Point)> onContextMenu;
    std::function<void()> onCopy;
    std::function<void(std::int64_t, int)> onActivate;

    const DataGridRow* row(std::int64_t index) const;
    void setSelection(const DataGridSelection& next, bool notify);
    void dataChanged();
    void refresh() { invalidate(); }

    // Geometry in widget coordinates.
    float gutterWidth() const;
    float frozenWidth() const;
    float totalWidth() const;
    Rect bodyRect() const;
    double maxScroll() const;
    float maxHorizontal() const;
    std::optional<Rect> cellRect(std::int64_t row, int column) const;
    void scrollTo(std::int64_t row, int column);
    double scroll() const { return scroll_; }
    float horizontal() const { return horizontal_; }
    std::int64_t firstVisible() const;
    std::int64_t visibleCount() const;

    void paint(Canvas& canvas) override;
    bool onMouseDown(const MouseEvent& event) override;
    bool onMouseMove(const MouseEvent& event) override;
    bool onMouseUp(const MouseEvent& event) override;
    bool onMouseWheel(const MouseWheelEvent& event) override;
    bool onKeyDown(const KeyEvent& event) override;
    bool isFocusable() const override { return true; }
    CursorKind cursor(Point point) const override;
    void sizeChanged();

private:
    enum class Drag { None, Cells, Rows, Columns, Resize, VerticalThumb, HorizontalThumb };
    struct Hit {
        std::int64_t row = -1;
        int column = -1;
        bool header = false;
        bool gutter = false;
        bool sortButton = false;
        int resizeColumn = -1;
        bool verticalBar = false;
        bool horizontalBar = false;
    };

    Hit hit(Point point) const;
    float columnLeft(int column) const;
    float columnScreenX(int column) const;
    void setScroll(double value);
    void setHorizontal(float value);
    void requestWindow();
    void move(std::int64_t rows, int columns, bool extend);
    TableStyle style() const;
    void paintHeader(Canvas& canvas, const TableStyle& style);
    void paintRows(Canvas& canvas, const TableStyle& style);
    void paintCell(Canvas& canvas, const TableStyle& style, const DataGridCell* cell, Rect rect, int column);
    void paintScrollbars(Canvas& canvas, const TableStyle& style);
    Rect verticalThumb() const;
    Rect horizontalThumb() const;

    DataGrid& owner_;
    double scroll_ = 0.0;
    float horizontal_ = 0.0f;
    std::int64_t requestedFirst_ = -1, requestedCount_ = 0;
    Drag drag_ = Drag::None;
    int resizeColumn_ = -1;
    float resizeOrigin_ = 0.0f, resizeStartWidth_ = 0.0f;
    float thumbGrab_ = 0.0f;
    int autofitColumn_ = -1;
};

} // namespace oneui
