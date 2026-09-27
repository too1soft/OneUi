#include "oneui/layout/stack.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace oneui {

Stack::Stack(StackDirection direction) : direction_(direction) {}

void Stack::setDirection(StackDirection direction) {
    direction_ = direction;
    invalidate();
}

void Stack::setGap(float gap) {
    gap_ = gap;
    invalidate();
}

void Stack::setPadding(Insets padding) {
    padding_ = padding;
    invalidate();
}

void Stack::setAlign(StackAlign align) {
    align_ = align;
    invalidate();
}

void Stack::setJustify(StackJustify justify) {
    justify_ = justify;
    invalidate();
}

void Stack::setFlex(const std::shared_ptr<Widget>& child, StackFlex flex) {
    if (!child) return;
    auto nonnegative = [](float value) { return std::isfinite(value) ? std::max(0.0f, value) : 0.0f; };
    flex.grow = nonnegative(flex.grow);
    flex.shrink = nonnegative(flex.shrink);
    flex.min = nonnegative(flex.min);
    flex.max = std::isnan(flex.max) ? flex.min : std::max(flex.min, flex.max);
    if (flex.basis) flex.basis = nonnegative(*flex.basis);
    flex_[child] = flex;
    invalidate();
}

void Stack::clearFlex(const std::shared_ptr<Widget>& child) {
    flex_.erase(child);
    invalidate();
}

StackFlex Stack::itemFlex(const std::shared_ptr<Widget>& child, float preferred) const {
    const auto it = flex_.find(child);
    if (it != flex_.end()) return it->second;
    return StackFlex{preferred <= 0.0f ? 1.0f : 0.0f, 0.0f, std::max(0.0f, preferred)};
}

Size Stack::itemSize(const std::shared_ptr<Widget>& child) const {
    const auto it = flex_.find(child);
    if (it == flex_.end() || !it->second.contentBasis) return child->preferredSize();
    const auto size = child->naturalSize();
    // Reserve complete logical pixels so fractional glyph advances do not
    // become an accidental ellipsis when the text is shaped under a width.
    return {std::ceil(std::max(0.0f, size.width)), std::ceil(std::max(0.0f, size.height))};
}

void Stack::setStyleBox(StyleBox style) {
    styleBox_ = std::move(style);
    invalidate();
}

void Stack::clearStyleBox() {
    styleBox_.reset();
    invalidate();
}

Size Stack::naturalSize() const {
    if (engine_ == StackEngine::Yoga) return measureYoga({INFINITY, INFINITY});
    Size result{};
    int count = 0;
    const bool column = direction_ == StackDirection::Column;
    for (const auto& child : children()) {
        if (!child->visible()) continue;
        ++count;
        const Size preferred = itemSize(child);
        const float main = column ? preferred.height : preferred.width;
        const auto flex = itemFlex(child, main);
        const float extent = std::clamp(flex.basis.value_or(std::max(0.0f, main)), flex.min, flex.max);
        if (column) {
            result.height += extent;
            result.width = std::max(result.width, preferred.width);
        } else {
            result.width += extent;
            result.height = std::max(result.height, preferred.height);
        }
    }
    const float gaps = std::max(0, count - 1) * std::max(0.0f, gap_);
    if (column) result.height += gaps;
    else result.width += gaps;
    result.width += padding_.horizontal();
    result.height += padding_.vertical();
    return result;
}

float Stack::contentWidth() const { return naturalSize().width; }
float Stack::contentHeight() const { return naturalSize().height; }

void Stack::paint(Canvas& canvas) {
    if (engine_ == StackEngine::Yoga) { paintYoga(canvas); return; }
    if (styleBox_) {
        paintStyleBox(canvas, frame(), *styleBox_);
    }
    View::paint(canvas);
}

void Stack::layoutChildren() {
    if (engine_ == StackEngine::Yoga) return; // Assigned by the outer Yoga tree before paint.
    for (auto it = flex_.begin(); it != flex_.end();) {
        if (it->first.expired()) it = flex_.erase(it);
        else ++it;
    }
    Rect content = frame().inset(padding_);
    content.width = std::max(0.0f, content.width);
    content.height = std::max(0.0f, content.height);
    const bool column = direction_ == StackDirection::Column;
    const float gap = std::max(0.0f, gap_);
    layoutItems_.clear();
    float occupied = 0.0f;
    for (const auto& child : children()) {
        if (!child->visible()) continue;
        const Size preferred = itemSize(child);
        const float main = column ? preferred.height : preferred.width;
        const StackFlex flex = itemFlex(child, main);
        const float basis = flex.basis.value_or(std::max(0.0f, main));
        const float size = std::clamp(basis, flex.min, flex.max);
        layoutItems_.push_back({child.get(), size, flex.grow, flex.shrink * basis, flex.min, flex.max,
                               column ? preferred.width : preferred.height});
        occupied += size;
    }
    if (layoutItems_.empty()) return;
    const float available = (column ? content.height : content.width) - gap * (layoutItems_.size() - 1);
    float free = available - occupied;
    const bool growing = free > 0.0f;
    // Freeze items at their limit and redistribute the remainder. Minimums
    // deliberately overflow when the container cannot satisfy them.
    for (size_t pass = 0; pass < layoutItems_.size() + 1 && std::abs(free) > 0.001f; ++pass) {
        double weight = 0.0;
        for (const auto& item : layoutItems_) {
            if (growing ? item.size < item.max : item.size > item.min)
                weight += growing ? item.grow : item.shrinkWeight;
        }
        if (weight <= 0.0) break;
        float distributed = 0.0f;
        for (auto& item : layoutItems_) {
            if (!(growing ? item.size < item.max : item.size > item.min)) continue;
            const float share = static_cast<float>(free * ((growing ? item.grow : item.shrinkWeight) / weight));
            const float next = std::clamp(item.size + share, item.min, item.max);
            distributed += next - item.size;
            item.size = next;
        }
        free -= distributed;
        if (std::abs(distributed) < 0.001f) break;
    }
    float cursor = column ? content.y : content.x;
    float spacing = gap;
    const float leftover = std::max(0.0f, free);
    if (justify_ == StackJustify::Center) cursor += leftover / 2.0f;
    if (justify_ == StackJustify::End) cursor += leftover;
    if (justify_ == StackJustify::SpaceBetween && layoutItems_.size() > 1)
        spacing += leftover / (layoutItems_.size() - 1);
    const float availableCross = column ? content.width : content.height;
    for (const auto& item : layoutItems_) {
        const float preferredCross = item.cross;
        const float crossSize = align_ == StackAlign::Stretch || preferredCross <= 0.0f
            ? availableCross : std::min(preferredCross, availableCross);
        float cross = column ? content.x : content.y;
        if (align_ == StackAlign::Center) cross += (availableCross - crossSize) / 2.0f;
        if (align_ == StackAlign::End) cross += availableCross - crossSize;
        item.child->setFrame(column ? Rect{cross, cursor, crossSize, item.size}
                                   : Rect{cursor, cross, item.size, crossSize});
        cursor += item.size + spacing;
    }
}

} // namespace oneui
