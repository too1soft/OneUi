#include "oneui/selection_model.h"

#include <algorithm>
#include <numeric>

namespace oneui {
namespace {

void normalizeIndices(std::vector<int>& indices, int itemCount) {
    indices.erase(
        std::remove_if(indices.begin(), indices.end(), [itemCount](int index) {
            return index < 0 || index >= itemCount;
        }),
        indices.end());
    std::sort(indices.begin(), indices.end());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
}

} // namespace

void SelectionModel::setItemCount(int count) {
    itemCount_ = std::max(0, count);
    normalizeIndices(selectedIndices_, itemCount_);
    if (!validIndex(activeIndex_)) {
        activeIndex_ = selectedIndices_.empty() ? -1 : selectedIndices_.back();
    }
    if (!validIndex(anchorIndex_)) {
        anchorIndex_ = activeIndex_;
    }
}

int SelectionModel::itemCount() const {
    return itemCount_;
}

void SelectionModel::setMode(SelectionMode mode) {
    if (mode_ == mode) {
        return;
    }
    mode_ = mode;
    if (mode_ == SelectionMode::Single && selectedIndices_.size() > 1) {
        const int retained = contains(activeIndex_) ? activeIndex_ : selectedIndices_.back();
        selectedIndices_ = {retained};
        activeIndex_ = retained;
        anchorIndex_ = retained;
    }
}

SelectionMode SelectionModel::mode() const {
    return mode_;
}

bool SelectionModel::clear() {
    return setState({}, -1, -1);
}

bool SelectionModel::selectOnly(int index) {
    if (!validIndex(index)) {
        return clear();
    }
    return setState({index}, index, index);
}

bool SelectionModel::applyPointerSelection(int index, bool control, bool shift) {
    if (!validIndex(index)) {
        return false;
    }
    if (mode_ == SelectionMode::Single) {
        return selectOnly(index);
    }
    if (shift) {
        const int anchor = validIndex(anchorIndex_)
            ? anchorIndex_
            : (validIndex(activeIndex_) ? activeIndex_ : index);
        const bool changed = replaceRange(anchor, index, control);
        activeIndex_ = index;
        anchorIndex_ = anchor;
        return changed;
    }
    if (control) {
        auto next = selectedIndices_;
        const auto found = std::lower_bound(next.begin(), next.end(), index);
        if (found != next.end() && *found == index) {
            next.erase(found);
        } else {
            next.insert(found, index);
        }
        return setState(std::move(next), index, index);
    }
    return selectOnly(index);
}

bool SelectionModel::applyKeyboardSelection(int index, bool control, bool shift) {
    if (!validIndex(index)) {
        return false;
    }
    if (mode_ == SelectionMode::Single) {
        return selectOnly(index);
    }
    if (shift) {
        const int anchor = validIndex(anchorIndex_)
            ? anchorIndex_
            : (validIndex(activeIndex_) ? activeIndex_ : index);
        const bool changed = replaceRange(anchor, index, control);
        activeIndex_ = index;
        anchorIndex_ = anchor;
        return changed;
    }
    if (control) {
        const bool changed = activeIndex_ != index;
        activeIndex_ = index;
        if (!validIndex(anchorIndex_)) {
            anchorIndex_ = index;
        }
        return changed;
    }
    return selectOnly(index);
}

bool SelectionModel::selectAll() {
    if (mode_ != SelectionMode::Multiple || itemCount_ <= 0) {
        return false;
    }
    std::vector<int> all(static_cast<std::size_t>(itemCount_));
    std::iota(all.begin(), all.end(), 0);
    const int active = validIndex(activeIndex_) ? activeIndex_ : 0;
    const int anchor = validIndex(anchorIndex_) ? anchorIndex_ : active;
    return setState(std::move(all), active, anchor);
}

bool SelectionModel::setSelectedIndices(std::vector<int> indices) {
    normalizeIndices(indices, itemCount_);
    if (mode_ == SelectionMode::Single && indices.size() > 1) {
        indices.erase(indices.begin(), indices.end() - 1);
    }
    const int active = indices.empty() ? -1 : indices.back();
    return setState(std::move(indices), active, active);
}

const std::vector<int>& SelectionModel::selectedIndices() const {
    return selectedIndices_;
}

bool SelectionModel::contains(int index) const {
    return std::binary_search(selectedIndices_.begin(), selectedIndices_.end(), index);
}

int SelectionModel::activeIndex() const {
    return activeIndex_;
}

int SelectionModel::anchorIndex() const {
    return anchorIndex_;
}

bool SelectionModel::validIndex(int index) const {
    return index >= 0 && index < itemCount_;
}

bool SelectionModel::replaceRange(int first, int last, bool append) {
    if (first > last) {
        std::swap(first, last);
    }
    std::vector<int> next = append ? selectedIndices_ : std::vector<int>{};
    next.reserve(next.size() + static_cast<std::size_t>(last - first + 1));
    for (int index = first; index <= last; ++index) {
        next.push_back(index);
    }
    normalizeIndices(next, itemCount_);
    const bool changed = next != selectedIndices_;
    selectedIndices_ = std::move(next);
    return changed;
}

bool SelectionModel::setState(std::vector<int> indices, int activeIndex, int anchorIndex) {
    normalizeIndices(indices, itemCount_);
    if (mode_ == SelectionMode::Single && indices.size() > 1) {
        indices.erase(indices.begin(), indices.end() - 1);
    }
    if (!validIndex(activeIndex)) {
        activeIndex = indices.empty() ? -1 : indices.back();
    }
    if (!validIndex(anchorIndex)) {
        anchorIndex = activeIndex;
    }
    const bool changed = indices != selectedIndices_
        || activeIndex != activeIndex_
        || anchorIndex != anchorIndex_;
    selectedIndices_ = std::move(indices);
    activeIndex_ = activeIndex;
    anchorIndex_ = anchorIndex;
    return changed;
}

} // namespace oneui
