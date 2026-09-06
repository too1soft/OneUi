#pragma once

#include "oneui/clipboard.h"
#include "oneui/export.h"
#include "oneui/widget.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace oneui {
namespace text { class Layout; }

// LogView 是只读多行日志查看器：逐行文本 + 每行独立颜色（按日志等级着色），
// 支持鼠标框选（跨行）、Ctrl+A 全选、Ctrl+C 复制到剪贴板。自身不滚动——
// 高度随内容增长（appendLine 时自动更新 preferredSize），放进 ScrollView 使用。
class ONEUI_API LogView final : public Widget {
public:
    LogView();

    void appendLine(std::wstring text, Color color);
    void clearLines();
    std::size_t lineCount() const;

    void setFontSize(float size);
    void setLineHeight(float height);
    void setPadding(Insets padding);
    void setSelectionBackground(Color color);
    // contentHeight 返回当前内容总高（含内边距），供宿主 ScrollView 声明内容高度。
    float contentHeight() const;

    void setClipboard(std::shared_ptr<Clipboard> clipboard);
    void selectAll();
    void clearSelection();
    bool hasSelection() const;
    std::wstring selectedText() const;
    bool copySelectionToClipboard(Clipboard& clipboard) const;

    void paint(Canvas& canvas) override;
    bool onMouseDown(const MouseEvent& event) override;
    bool onMouseMove(const MouseEvent& event) override;
    bool onMouseUp(const MouseEvent& event) override;
    bool onKeyDown(const KeyEvent& event) override;
    CursorKind cursor(Point point) const override;
    bool isFocusable() const override;

private:
    struct LogLine {
        std::wstring text;
        Color color;
    };

    // 文本位置 = (行号, 行内字符下标)。选区由 anchor_/caret_ 两端点决定，无序存储。
    struct TextPos {
        std::size_t line = 0;
        std::size_t column = 0;

        bool operator==(const TextPos& other) const { return line == other.line && column == other.column; }
        bool operator<(const TextPos& other) const {
            return line != other.line ? line < other.line : column < other.column;
        }
    };

    TextPos selectionStart() const;
    TextPos selectionEnd() const;
    TextPos positionFromPoint(Point point) const;
    std::shared_ptr<text::Layout> lineLayout(std::size_t line) const;
    void invalidateMetrics();
    void syncPreferredHeight();
    bool hasInteractionState() const override;
    void resetInteractionState() override;

    std::vector<LogLine> lines_;
    float fontSize_ = 12.0f;
    float lineHeight_ = 20.0f;
    Insets padding_{6.0f, 10.0f, 6.0f, 10.0f};
    Color selectionBackground_{191, 219, 254, 255};

    TextPos anchor_;
    TextPos caret_;
    bool hasSelection_ = false;
    bool selecting_ = false;
    std::shared_ptr<Clipboard> clipboard_;

    // Shared shaping results are usable before paint; offsets remain native wide indices.
    mutable std::vector<std::shared_ptr<text::Layout>> layouts_;
    mutable std::wstring layoutFamily_;
    mutable float layoutScale_ = 0;
};

} // namespace oneui
