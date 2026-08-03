#pragma once

#include "oneui/export.h"

#include <vector>

namespace oneui {

enum class SelectionMode {
    Single,
    Multiple
};

/// Platform-neutral selection state shared by list-like controls.
///
/// The model keeps selection, keyboard focus, and range anchor separate so
/// native Ctrl/Shift behavior stays consistent across lists, tables, and trees.
class ONEUI_API SelectionModel {
public:
    void setItemCount(int count);
    int itemCount() const;

    void setMode(SelectionMode mode);
    SelectionMode mode() const;

    bool clear();
    bool selectOnly(int index);
    bool applyPointerSelection(int index, bool control, bool shift);
    bool applyKeyboardSelection(int index, bool control, bool shift);
    bool selectAll();
    bool setSelectedIndices(std::vector<int> indices);

    const std::vector<int>& selectedIndices() const;
    bool contains(int index) const;
    int activeIndex() const;
    int anchorIndex() const;

private:
    bool validIndex(int index) const;
    bool replaceRange(int first, int last, bool append);
    bool setState(std::vector<int> indices, int activeIndex, int anchorIndex);

    int itemCount_ = 0;
    SelectionMode mode_ = SelectionMode::Single;
    std::vector<int> selectedIndices_;
    int activeIndex_ = -1;
    int anchorIndex_ = -1;
};

} // namespace oneui
