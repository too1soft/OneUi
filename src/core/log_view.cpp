#include "oneui/controls/log_view.h"

#include "oneui/canvas.h"

#include <algorithm>
#include <cmath>

namespace oneui {
namespace {

// 未经真实测量时的近似字宽（与 text_field.cpp 同一策略：CJK 全角按字号、
// 半角按 0.6 倍字号估算），保证首帧前的命中测试也有合理结果。
float approximateGlyphWidth(wchar_t character, float fontSize) {
    if (character >= 0x2E80) {
        return fontSize;
    }
    return fontSize * 0.6f;
}

} // namespace

LogView::LogView() {
    setPreferredSize(Size{480.0f, 40.0f});
}

void LogView::appendLine(std::wstring text, Color color) {
    lines_.push_back(LogLine{std::move(text), color});
    prefixWidths_.emplace_back();
    metricsExact_.push_back(false);
    syncPreferredHeight();
    invalidate();
}

void LogView::clearLines() {
    lines_.clear();
    invalidateMetrics();
    clearSelection();
    syncPreferredHeight();
    invalidate();
}

std::size_t LogView::lineCount() const {
    return lines_.size();
}

void LogView::setFontSize(float size) {
    if (size > 0.0f) {
        fontSize_ = size;
        invalidateMetrics();
        invalidate();
    }
}

void LogView::setLineHeight(float height) {
    if (height > 0.0f) {
        lineHeight_ = height;
        syncPreferredHeight();
        invalidate();
    }
}

void LogView::setPadding(Insets padding) {
    padding_ = padding;
    syncPreferredHeight();
    invalidate();
}

void LogView::setSelectionBackground(Color color) {
    selectionBackground_ = color;
    invalidate();
}

float LogView::contentHeight() const {
    return padding_.top + padding_.bottom + lineHeight_ * static_cast<float>(lines_.size());
}

void LogView::setClipboard(std::shared_ptr<Clipboard> clipboard) {
    clipboard_ = std::move(clipboard);
}

void LogView::selectAll() {
    if (lines_.empty()) {
        return;
    }
    anchor_ = TextPos{0, 0};
    caret_ = TextPos{lines_.size() - 1, lines_.back().text.size()};
    hasSelection_ = !(anchor_ == caret_);
    invalidate();
}

void LogView::clearSelection() {
    anchor_ = TextPos{};
    caret_ = TextPos{};
    hasSelection_ = false;
    invalidate();
}

bool LogView::hasSelection() const {
    return hasSelection_ && !(anchor_ == caret_);
}

std::wstring LogView::selectedText() const {
    if (!hasSelection()) {
        return {};
    }
    const TextPos start = selectionStart();
    const TextPos end = selectionEnd();
    std::wstring result;
    for (std::size_t line = start.line; line <= end.line && line < lines_.size(); ++line) {
        const std::wstring& text = lines_[line].text;
        const std::size_t from = line == start.line ? std::min(start.column, text.size()) : 0;
        const std::size_t to = line == end.line ? std::min(end.column, text.size()) : text.size();
        if (line != start.line) {
            result += L"\r\n"; // Windows 剪贴板换行惯例，粘贴到记事本等处保持分行
        }
        result += text.substr(from, to - from);
    }
    return result;
}

bool LogView::copySelectionToClipboard(Clipboard& clipboard) const {
    if (!hasSelection()) {
        return false;
    }
    clipboard.setText(selectedText());
    return true;
}

void LogView::paint(Canvas& canvas) {
    const Rect rect = frame();
    const bool selectionActive = hasSelection();
    const TextPos start = selectionActive ? selectionStart() : TextPos{};
    const TextPos end = selectionActive ? selectionEnd() : TextPos{};

    for (std::size_t line = 0; line < lines_.size(); ++line) {
        const float lineTop = rect.y + padding_.top + lineHeight_ * static_cast<float>(line);
        // 视口外的行跳过绘制（ScrollView 会裁剪，跳过纯属省绘制开销）。
        if (lineTop + lineHeight_ < rect.y || lineTop > rect.y + rect.height) {
            continue;
        }
        updateLineMetrics(line, &canvas);
        const Rect lineRect{rect.x + padding_.left, lineTop, rect.width - padding_.left - padding_.right, lineHeight_};

        if (selectionActive && line >= start.line && line <= end.line) {
            const std::wstring& text = lines_[line].text;
            const std::size_t from = line == start.line ? std::min(start.column, text.size()) : 0;
            std::size_t to = line == end.line ? std::min(end.column, text.size()) : text.size();
            float fromX = columnOffset(line, from);
            float toX = columnOffset(line, to);
            // 中间整行选中时，行尾额外亮出半个字宽，直观表示“换行也被选中”。
            if (line != end.line) {
                toX += fontSize_ * 0.4f;
            }
            if (toX > fromX) {
                canvas.fillRect(Rect{lineRect.x + fromX, lineTop + 1.0f, toX - fromX, lineHeight_ - 2.0f}, selectionBackground_, 3.0f);
            }
        }

        canvas.drawTextStyled(lines_[line].text, lineRect, lines_[line].color, fontSize_, TextAlign::Left);
    }
}

bool LogView::onMouseDown(const MouseEvent& event) {
    if (!interactive() || lines_.empty()) {
        return false;
    }
    anchor_ = positionFromPoint(event.position);
    caret_ = anchor_;
    hasSelection_ = true;
    selecting_ = true;
    invalidate();
    return true;
}

bool LogView::onMouseMove(const MouseEvent& event) {
    if (!selecting_) {
        return false;
    }
    const TextPos next = positionFromPoint(event.position);
    if (next == caret_) {
        return false;
    }
    caret_ = next;
    invalidate();
    return true;
}

bool LogView::onMouseUp(const MouseEvent& event) {
    (void)event;
    if (!selecting_) {
        return false;
    }
    selecting_ = false;
    if (anchor_ == caret_) {
        hasSelection_ = false;
        invalidate();
    }
    return true;
}

bool LogView::onKeyDown(const KeyEvent& event) {
    if (!interactive()) {
        return false;
    }
    if (event.control) {
        if (event.key == Key::A) {
            selectAll();
            return true;
        }
        if (event.key == Key::C && clipboard_) {
            return copySelectionToClipboard(*clipboard_);
        }
    }
    if (event.key == Key::Escape && hasSelection()) {
        clearSelection();
        return true;
    }
    return false;
}

CursorKind LogView::cursor(Point point) const {
    (void)point;
    return CursorKind::Text;
}

bool LogView::isFocusable() const {
    return interactive() && !lines_.empty();
}

LogView::TextPos LogView::selectionStart() const {
    return anchor_ < caret_ ? anchor_ : caret_;
}

LogView::TextPos LogView::selectionEnd() const {
    return anchor_ < caret_ ? caret_ : anchor_;
}

LogView::TextPos LogView::positionFromPoint(Point point) const {
    if (lines_.empty()) {
        return TextPos{};
    }
    const Rect rect = frame();
    const float localY = point.y - rect.y - padding_.top;
    const auto lineIndex = static_cast<std::ptrdiff_t>(std::floor(localY / lineHeight_));
    const std::size_t line = static_cast<std::size_t>(std::clamp<std::ptrdiff_t>(lineIndex, 0, static_cast<std::ptrdiff_t>(lines_.size()) - 1));

    updateLineMetrics(line, nullptr);
    const std::vector<float>& widths = prefixWidths_[line];
    const float localX = std::max(0.0f, point.x - rect.x - padding_.left);
    auto it = std::lower_bound(widths.begin(), widths.end(), localX);
    if (it == widths.begin()) {
        return TextPos{line, 0};
    }
    if (it == widths.end()) {
        return TextPos{line, lines_[line].text.size()};
    }
    const std::size_t upper = static_cast<std::size_t>(it - widths.begin());
    const std::size_t lower = upper - 1;
    const bool nearLower = std::fabs(localX - widths[lower]) <= std::fabs(widths[upper] - localX);
    return TextPos{line, nearLower ? lower : upper};
}

float LogView::columnOffset(std::size_t line, std::size_t column) const {
    if (line >= lines_.size()) {
        return 0.0f;
    }
    updateLineMetrics(line, nullptr);
    const std::vector<float>& widths = prefixWidths_[line];
    return widths[std::min(column, widths.size() - 1)];
}

void LogView::updateLineMetrics(std::size_t line, const Canvas* canvas) const {
    if (line >= lines_.size()) {
        return;
    }
    if (prefixWidths_.size() != lines_.size()) {
        prefixWidths_.resize(lines_.size());
        metricsExact_.resize(lines_.size(), false);
    }
    const std::wstring& text = lines_[line].text;
    std::vector<float>& widths = prefixWidths_[line];
    const bool sized = widths.size() == text.size() + 1;
    if (sized && (metricsExact_[line] || canvas == nullptr)) {
        return;
    }

    widths.assign(text.size() + 1, 0.0f);
    if (canvas) {
        widths = canvas->measureTextPrefixWidths(text, fontSize_);
        metricsExact_[line] = true;
        return;
    }
    for (std::size_t index = 0; index < text.size(); ++index) {
        widths[index + 1] = widths[index] + approximateGlyphWidth(text[index], fontSize_);
    }
    metricsExact_[line] = false;
}

void LogView::invalidateMetrics() {
    prefixWidths_.clear();
    metricsExact_.clear();
}

void LogView::syncPreferredHeight() {
    setPreferredSize(Size{preferredSize().width, std::max(40.0f, contentHeight())});
}

bool LogView::hasInteractionState() const {
    return selecting_;
}

void LogView::resetInteractionState() {
    selecting_ = false;
}

} // namespace oneui
