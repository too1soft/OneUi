#include "oneui/controls/log_view.h"

#include "oneui/canvas.h"
#include "text/text_layout.h"

#include <algorithm>
#include <cmath>

namespace oneui {
LogView::LogView() {
    setPreferredSize(Size{480.0f, 40.0f});
}

void LogView::appendLine(std::wstring text, Color color) {
    lines_.push_back(LogLine{std::move(text), color});
    layouts_.emplace_back();
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
        invalidateMetrics();
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
    try { clipboard.setText(selectedText()); return true; }
    catch (...) { return false; }
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
        const auto layout = lineLayout(line);
        const Rect lineRect{rect.x + padding_.left, lineTop, rect.width - padding_.left - padding_.right, lineHeight_};
        const float textTop = lineTop + (lineHeight_ - layout->height()) / 2.0f;
        canvas.save();
        canvas.clipRect(lineRect);

        if (selectionActive && line >= start.line && line <= end.line) {
            const std::wstring& text = lines_[line].text;
            const std::size_t from = line == start.line ? std::min(start.column, text.size()) : 0;
            const std::size_t to = line == end.line ? std::min(end.column, text.size()) : text.size();
            for (auto box : layout->selection(from, to)) {
                box.x += lineRect.x; box.y += textTop;
                canvas.fillRect(box, selectionBackground_, 3.0f);
            }
            // Mark the selected line separator separately, also in RTL text.
            if (line != end.line) {
                auto box = layout->caret({text.size(), TextAffinity::Upstream});
                box.x += lineRect.x; box.y += textTop; box.width = fontSize_ * 0.4f;
                canvas.fillRect(box, selectionBackground_, 3.0f);
            }
        }

        TextBlockStyle style;
        style.fontSize = fontSize_; style.lineHeight = lineHeight_;
        style.fontFamily = textFontFamily(); style.dpiScale = textDpiScale();
        canvas.drawTextBlock(lines_[line].text, {lineRect.x, textTop, lineRect.width, layout->height()}, lines_[line].color, style);
        canvas.restore();
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
    if (event.editShortcut()) {
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

    const auto layout = lineLayout(line);
    return {line, layout->hitTest({point.x - rect.x - padding_.left, layout->height() / 2.0f}).offset};
}

std::shared_ptr<text::Layout> LogView::lineLayout(std::size_t line) const {
    if (layoutFamily_ != textFontFamily() || layoutScale_ != textDpiScale()) {
        layouts_.clear(); layoutFamily_ = textFontFamily(); layoutScale_ = textDpiScale();
    }
    layouts_.resize(lines_.size());
    if (!layouts_.at(line)) {
        text::LayoutOptions options;
        options.family = layoutFamily_; options.scale = layoutScale_;
        options.size = fontSize_; options.lineHeight = lineHeight_;
        layouts_[line] = text::Layout::make(lines_[line].text, options);
    }
    return layouts_[line];
}

void LogView::invalidateMetrics() {
    layouts_.clear();
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
