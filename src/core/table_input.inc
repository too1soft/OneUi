// Table pointer, keyboard, header and scrollbar interactions.
// Included within namespace oneui by table.cpp.
void Table::dragScrollbar(float y) {
    const auto style = resolvedStyle();
    const float height = std::max(0.0f, frame().height - style.headerHeight);
    const auto thumb = verticalThumbRect(style.scrollbarWidth, height);
    const float travel = std::max(0.0f, height - 8.0f - thumb.height);
    if (travel <= 0.0f) return;
    setScrollOffset(std::clamp((y - frame().y - style.headerHeight - 4.0f - scrollbarGrab_) / travel, 0.0f, 1.0f) * maxScrollOffset());
}

bool Table::onMouseMove(const MouseEvent& event) {
    if (draggingScrollbar_) { if (interactive()) dragScrollbar(event.position.y); return true; }
    if (!interactive()) return false;
    if ((reorderEnabled_ || itemDragEnabled_) && reorderSourceIndex_ >= 0) {
        if (!reordering_ && detail::exceedsReorderDragThreshold(
                event.position.x - reorderStartPoint_.x,
                event.position.y - reorderStartPoint_.y)) {
            reordering_ = true;
            reorderCurrentPoint_ = event.position;
            if (itemDragEnabled_ && !contains(event.position)) {
                externalDragging_ = true;
                reorderTargetIndex_ = -1;
                reorderInsertionIndex_ = -1;
                invalidate();
                emitItemDrag(ItemDragPhase::Started, event.position);
                return true;
            }
        }
        if (reordering_) {
            reorderCurrentPoint_ = event.position;
            if (externalDragging_) {
                invalidate();
                emitItemDrag(ItemDragPhase::Updated, event.position);
                return true;
            }
            if (itemDragEnabled_ && !contains(event.position)) {
                externalDragging_ = true;
                reorderTargetIndex_ = -1;
                reorderInsertionIndex_ = -1;
                invalidate();
                emitItemDrag(ItemDragPhase::Started, event.position);
                return true;
            }
            if (reorderEnabled_) updateReorderTarget(event.position);
            else {
                reorderTargetIndex_ = -1;
                reorderInsertionIndex_ = -1;
            }
            hoveredIndex_ = hitRowIndex(event.position);
            invalidate();
            return true;
        }
    }
    const int next = hitRowIndex(event.position);
    if (next == hoveredIndex_) return false;
    hoveredIndex_ = next;
    invalidate();
    return true;
}

void Table::setHeaderSort(int column, bool descending) {
    sortColumn_ = column < 0 ? -1 : column; sortDescending_ = descending;
    if (sortColumn_ < 0) focusedHeader_ = -1; invalidate();
}

bool Table::onMouseDown(const MouseEvent& event) {
    if (!interactive() || (event.button != MouseButton::Left && event.button != MouseButton::Right)) return false;
    resetReorderState();
    const auto style = resolvedStyle();
    const float body = std::max(0.0f, frame().height - style.headerHeight);
    if (event.button == MouseButton::Left && contains(event.position) && maxScrollOffset() > .001f &&
        event.position.y >= frame().y + style.headerHeight &&
        event.position.x >= frame().x + frame().width - std::max(10.0f, style.scrollbarWidth + 6.0f)) {
        const auto thumb = verticalThumbRect(style.scrollbarWidth, body);
        draggingScrollbar_ = true;
        pressedIndex_ = -1; pressedSelection_ = false;
        scrollbarGrab_ = event.position.y >= thumb.y && event.position.y <= thumb.y + thumb.height
            ? event.position.y - thumb.y : thumb.height * .5f;
        dragScrollbar(event.position.y);
        return true;
    }
    pressedColumn_ = hitColumnIndex(event.position);
    pressedIndex_ = hitRowIndex(event.position);
    pressedClickCount_ = event.clickCount;
    pressedSelection_ = selectionColumnVisible_ && event.button == MouseButton::Left
        && contains(event.position) && event.position.x < frame().x + selectionGutter();
    if (pressedSelection_) {
        if (event.position.y < frame().y + resolvedStyle().headerHeight) pressedIndex_ = -2;
        return pressedIndex_ >= 0 || pressedIndex_ == -2;
    }
    if (pressedIndex_ < 0 && event.button == MouseButton::Left && sortColumn_ >= 0 && onCellAction_ && contains(event.position) && pressedColumn_ >= 0 && event.position.y < frame().y + style.headerHeight) {
        pressedIndex_ = -3; return true;
    }
    focusedHeader_ = -1;
    if (pressedIndex_ < 0) return false;
    if (event.button == MouseButton::Left
        && (reorderEnabled_ || (itemDragEnabled_ && itemDragIds_.size() == rows_.size()))) {
        reorderStartPoint_ = event.position;
        reorderCurrentPoint_ = event.position;
        reorderSourceIndex_ = pressedIndex_;
        reorderTargetIndex_ = pressedIndex_;
    }
    invalidate();
    return true;
}

bool Table::onMouseUp(const MouseEvent& event) {
    if (!interactive()) return false;
    if (draggingScrollbar_) { dragScrollbar(event.position.y); draggingScrollbar_ = false; return true; }
    if (pressedSelection_) {
        pressedSelection_ = false;
        const int pressed = pressedIndex_;pressedIndex_ = -1;
        const bool inGutter = contains(event.position) && event.position.x < frame().x + selectionGutter();
        const bool header = event.position.y < frame().y + resolvedStyle().headerHeight;
        if (event.button == MouseButton::Left && inGutter && ((pressed == -2 && header) || (pressed >= 0 && hitRowIndex(event.position)==pressed))) {
            const auto previous=selection_.selectedIndices();const int active=selectedIndex();
            if (pressed == -2) {
                if (selection_.selectedIndices().size()==rows_.size()) selection_.clear(); else selection_.selectAll();
            } else selection_.applyPointerSelection(pressed,true,event.shift);
            notifySelectionChanged(previous,active);invalidate();
        }
        return true;
    }
    if (event.button == MouseButton::Left && reordering_) {
        if (!externalDragging_ && reorderEnabled_ && contains(event.position)) {
            updateReorderTarget(event.position);
        } else {
            reorderTargetIndex_ = -1;
            reorderInsertionIndex_ = -1;
        }
        const int source = reorderSourceIndex_;
        const int target = reorderTargetIndex_;
        const std::wstring sourceId = source >= 0 && source < static_cast<int>(itemDragIds_.size())
            ? itemDragIds_[static_cast<std::size_t>(source)]
            : std::wstring{};
        const bool externalDrop = externalDragging_;
        const bool internalDrop = !externalDrop && reorderEnabled_ && contains(event.position);
        const auto reorderCallback = onReorderRequested_;
        const auto dragCallback = onItemDrag_;
        const ItemDragEvent dragEvent{sourceId, ItemDragPhase::Dropped, event.position};
        pressedIndex_ = -1;
        pressedClickCount_ = 1;
        resetReorderState();
        invalidate();
        if (externalDrop) {
            if (itemDragEnabled_ && !sourceId.empty() && dragCallback) dragCallback(dragEvent);
        } else if (internalDrop && source >= 0 && target >= 0 && source != target && reorderCallback) {
            reorderCallback(source, target);
        }
        return true;
    }
    const int pressed = pressedIndex_;
    const int pressedColumn = pressedColumn_;
    pressedColumn_ = -1;
    const int clickCount = pressedClickCount_;
    pressedIndex_ = -1;
    pressedClickCount_ = 1;
    resetReorderState();
    if (pressed == -3) {
        if (event.button == MouseButton::Left && contains(event.position) && event.position.y < frame().y + resolvedStyle().headerHeight && hitColumnIndex(event.position) == pressedColumn && onCellAction_) {
            focusedHeader_ = pressedColumn; onCellAction_(-1, pressedColumn); invalidate();
        }
        return true;
    }
    if (pressed < 0) return false;
    if (hitRowIndex(event.position) == pressed) {
        const auto previousIndices = selection_.selectedIndices();
        const int previousSelectedIndex = selectedIndex();
        if (event.button == MouseButton::Right) {
            if (!selection_.contains(pressed)) selection_.selectOnly(pressed);
        } else {
            selection_.applyPointerSelection(pressed, event.control, event.shift);
        }
        ensureSelectionVisible();
        notifySelectionChanged(previousIndices, previousSelectedIndex);
        if (event.button == MouseButton::Right && onContextMenuRequested_) {
            onContextMenuRequested_(pressed, event.position);
        } else if (event.button == MouseButton::Left && pressedColumn >= 0 && pressedColumn == hitColumnIndex(event.position)
                   && pressedColumn < static_cast<int>(columns_.size()) && columns_[pressedColumn].action && onCellAction_) {
            onCellAction_(pressed, pressedColumn);
        } else if (event.button == MouseButton::Left && clickCount == 2 && onActivated_) {
            onActivated_(pressed);
        }
    }
    invalidate();
    return true;
}

bool Table::onMouseWheel(const MouseWheelEvent& event) {
    if (!interactive() || !contains(event.position) || maxScrollOffset() <= 0.001f) return false;
    const double nowMs = event.timestampMs > 0.0 ? event.timestampMs : currentTimeMs();
    const bool caughtUp = advanceScrollMotion(nowMs);
    const bool accepted = scrollMotion_.addDelta(
        -event.deltaY * wheelStep_, 0.0f, maxScrollOffset(), nowMs, kDefaultWheelScrollMotionSpec);
    if (accepted || scrollMotion_.running()) requestAnimationFrame();
    return accepted || caughtUp;
}

bool Table::onKeyDown(const KeyEvent& event) {
    if (!interactive()) return false;
    if (sortColumn_ >= 0 && onCellAction_ && !columns_.empty()) {
        if (focusedHeader_ < 0 && event.key == Key::Up && selectedIndex() <= 0 && !event.control && !event.shift) {
            focusedHeader_ = std::min(sortColumn_, static_cast<int>(columns_.size()) - 1); invalidate(); return true;
        }
        if (focusedHeader_ >= 0) {
            if (event.key == Key::Left) focusedHeader_ = std::max(0, focusedHeader_ - 1);
            else if (event.key == Key::Right) focusedHeader_ = std::min(static_cast<int>(columns_.size()) - 1, focusedHeader_ + 1);
            else if (event.key == Key::Enter || event.key == Key::Space) { onCellAction_(-1, focusedHeader_); return true; }
            else if (event.key == Key::Down) { focusedHeader_ = -1; if (!rows_.empty()) setSelectedIndex(0); }
            else return false;
            invalidate(); return true;
        }
    }
    if (rows_.empty()) return false;
    if (event.key == Key::A && event.control && selection_.mode() == SelectionMode::Multiple) {
        const auto previous = selection_.selectedIndices();
        const int previousIndex = selectedIndex();
        selection_.selectAll();
        notifySelectionChanged(previous, previousIndex);
        invalidate();
        return true;
    }
    const int active = selection_.activeIndex();
    if (selectionColumnVisible_ && active >= 0 && event.key == Key::Space) {
        const auto previous = selection_.selectedIndices();const int before=selectedIndex();
        selection_.applyPointerSelection(active,true,event.shift);
        notifySelectionChanged(previous,before);invalidate();return true;
    }
    if (reorderEnabled_ && event.alt && !event.control && active >= 0
        && (event.key == Key::Up || event.key == Key::Down)) {
        const int target = event.key == Key::Up
            ? std::max(0, active - 1)
            : std::min(static_cast<int>(rows_.size()) - 1, active + 1);
        const auto callback = onReorderRequested_;
        if (target != active && callback) callback(active, target);
        return true;
    }
    if (active >= 0 && event.key == Key::Space && onCellAction_) {
        for (int column = 0; column < static_cast<int>(columns_.size()); ++column) {
            if (columns_[column].action) { onCellAction_(active, column); return true; }
        }
    }
    if (active >= 0 && event.key == Key::Enter && onActivated_) { onActivated_(active); return true; }
    if (active >= 0 && event.key == Key::F2 && onEditRequested_) { onEditRequested_(active); return true; }
    if (active >= 0 && event.key == Key::Delete && onDeleteRequested_) { onDeleteRequested_(selection_.selectedIndices()); return true; }

    int target = -1;
    if (event.key == Key::Down) target = std::min(static_cast<int>(rows_.size()) - 1, std::max(-1, active) + 1);
    else if (event.key == Key::Up) target = active < 0 ? 0 : std::max(0, active - 1);
    else if (event.key == Key::Home) target = 0;
    else if (event.key == Key::End) target = static_cast<int>(rows_.size()) - 1;
    else if (event.key == Key::PageDown || event.key == Key::PageUp) {
        const TableStyle style = resolvedStyle();
        const float contentHeight = std::max(0.0f, frame().height - style.headerHeight);
        const int visibleRows = std::max(1, static_cast<int>(std::floor(contentHeight / effectiveRowHeight(contentHeight))));
        const int origin = active < 0 ? 0 : active;
        target = event.key == Key::PageDown
            ? std::min(static_cast<int>(rows_.size()) - 1, origin + visibleRows)
            : std::max(0, origin - visibleRows);
    }
    if (target < 0) return false;
    const auto previous = selection_.selectedIndices();
    const int previousIndex = selectedIndex();
    selection_.applyKeyboardSelection(target, event.control, event.shift);
    ensureSelectionVisible();
    notifySelectionChanged(previous, previousIndex);
    invalidate();
    return true;
}

