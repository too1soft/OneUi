namespace oneui {
namespace {
bool sameTableRect(const Rect& a, const Rect& b) {
    return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
}
}

void VirtualTemplateList::useTableViewport() {
    tableViewport_ = true;
    tableLayoutPending_ = true;
    positions();
}

void VirtualTemplateList::setTableInsets(Insets insets) {
    const auto valid = [](float v) { return std::isfinite(v) && v >= 0.0f; };
    if (!valid(insets.top) || !valid(insets.right) || !valid(insets.bottom) || !valid(insets.left))
        throw std::invalid_argument("Invalid virtual table insets");
    tableInsets_ = insets;
    tableLayoutPending_ = true;
    invalidate();
}

void VirtualTemplateList::layoutInTable(Rect content, Rect viewport) {
    if (!sameTableRect(tableClip_, viewport) || !sameTableRect(frame(), content))
        tableLayoutPending_ = true;
    tableClip_ = viewport;
    setFrame(content);
    layoutChildren();
}

void VirtualTemplateList::layoutTableRows() {
    if (!tableLayoutPending_ && tableLayoutVisible_ == visible()) return;
    tableLayoutVisible_ = visible();
    auto content = frame().inset(tableInsets_);
    content.width = std::max(0.0f, content.width);
    const float top = tableClip_.y - content.y;
    const float bottom = top + tableClip_.height;
    std::set<std::string> keep;
    if (visible() && content.width > 0.0f && tableClip_.height > 0.0f &&
        bottom > 0.0f && top < tops_.back() && !keys_.empty() && tops_.back() > 0.0f) {
        const auto firstVisible = indexAt(std::max(0.0f, top));
        const auto lastVisible = indexAt(std::min(bottom, tops_.back()));
        const auto first = firstVisible > overscan_ ? firstVisible - overscan_ : 0;
        const auto last = std::min(keys_.size() - 1, lastVisible + overscan_);
        for (auto i = first; i <= last; ++i) {
            if (tops_[i + 1] == tops_[i]) continue;
            if (!mounted_.count(keys_[i])) mountRow(i);
            keep.insert(keys_[i]);
        }
    }
    publishRows(std::move(keep), content, 0.0f);
    tableLayoutPending_ = false;
}

bool VirtualTemplateList::focusTableRow(bool backwards, bool fromBoundary) {
    if (!interactive() || !factory_) return false;
    auto count = static_cast<std::ptrdiff_t>(keys_.size());
    const auto step = backwards ? -1 : 1;
    auto index = backwards ? count : -1;
    if (!fromBoundary && focusedChild()) {
        const auto current = std::find_if(keys_.begin(), keys_.end(), [&](const auto& key) {
            const auto row = mounted_.find(key);
            return row != mounted_.end() && row->second.widget.get() == focusedChild();
        });
        if (current != keys_.end()) index = current - keys_.begin();
    }
    for (index += step; index >= 0 && index < count; index += step) {
        const auto i = static_cast<size_t>(index);
        if (tops_[i + 1] <= tops_[i]) continue;
        auto found = mounted_.find(keys_[i]);
        if (found == mounted_.end()) {
            mountRow(i);
            found = mounted_.find(keys_[i]);
            tableLayoutPending_ = true;
        }
        auto child = found->second.widget;
        if (!child->visible() || child->disabled() || !child->isFocusable()) continue;
        std::set<std::string> keep;
        for (const auto& [key, row] : mounted_) keep.insert(key);
        auto content = frame().inset(tableInsets_);
        content.width = std::max(0.0f, content.width);
        publishRows(std::move(keep), content, 0.0f);
        internal::layoutWidgetSubtree(*child);
        focusChild(child.get(), true);
        if (backwards) child->focusLastLeaf();
        else focusFirstKeyboardStop(child.get());
        tableLayoutPending_ = true;
        return true;
    }
    return false;
}

bool VirtualTemplateList::onTableKeyDown(const KeyEvent& event) {
    if (event.key != Key::Tab) return View::onKeyDown(event);
    if (!interactive()) return false;
    const auto life = lifetimeToken();
    if (const auto child = activeFocusChild()) {
        child->setFocusVisible(true);
        if (child->onKeyDown(event)) return true;
        if (life.expired()) return true;
    }
    return focusTableRow(event.shift, false);
}

bool VirtualTemplateList::focusFirstLeaf() {
    return tableViewport_ ? focusTableRow(false, true) : View::focusFirstLeaf();
}

bool VirtualTemplateList::focusLastLeaf() {
    return tableViewport_ ? focusTableRow(true, true) : View::focusLastLeaf();
}
} // namespace oneui
