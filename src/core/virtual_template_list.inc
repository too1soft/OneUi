#include "oneui/layout/virtual_template_list.h"
#include <limits>
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace oneui {
void VirtualTemplateList::setFactory(Factory factory, Retire retire) {
    if (!mounted_.empty()) throw std::logic_error("Factory replacement requires an empty list");
    factory_ = std::move(factory);
    retire_ = std::move(retire);
}
void VirtualTemplateList::positions() {
    tops_.assign(keys_.size() + 1, 0);
    for (size_t i = 0; i < keys_.size(); ++i) {
        auto h = heights_.find(keys_[i]);
        tops_[i + 1] = tops_[i] + (h == heights_.end() ? estimate_ : h->second);
    }
}
size_t VirtualTemplateList::indexAt(float offset) const {
    if (keys_.empty()) return 0;
    return std::min(keys_.size() - 1, size_t(std::upper_bound(tops_.begin(), tops_.end(), offset) - tops_.begin() - 1));
}
float VirtualTemplateList::maxScrollOffset() const {
    return std::max(0.0f, tops_.back() - std::max(0.0f, frame().height));
}
VirtualTemplateList::Anchor VirtualTemplateList::anchor() const {
    if (keys_.empty()) return {{}, 0, following_};
    const auto i = indexAt(offset_);
    return {keys_[i], offset_ - tops_[i], following_ && offset_ >= std::max(0.0f, tops_.back() - height_) - 1};
}
void VirtualTemplateList::restore(const Anchor& a) {
    if (a.end) offset_ = maxScrollOffset();
    else {
        auto i = std::find(keys_.begin(), keys_.end(), a.key);
        if (i != keys_.end()) offset_ = tops_[size_t(i - keys_.begin())] + a.inset;
    }
    offset_ = std::clamp(offset_, 0.0f, maxScrollOffset());
}
void VirtualTemplateList::setKeys(std::vector<std::string> keys, bool refreshNow) {
    std::set<std::string> unique;
    for (const auto& key : keys)
        if (key.empty() || !unique.insert(key).second) throw std::invalid_argument("Empty or duplicate virtual key");
    const auto a = anchor();
    keys_ = std::move(keys);
    for (auto it = heights_.begin(); it != heights_.end();)
        if (!unique.count(it->first)) it = heights_.erase(it); else ++it;
    positions();
    restore(a);
    if (refreshNow) refresh();
}
void VirtualTemplateList::setEstimatedHeight(float height) {
    if (!std::isfinite(height) || height < 16) throw std::invalid_argument("Virtual row estimate must be >= 16");
    auto a = anchor();
    estimate_ = height;
    positions();
    restore(a);
    refresh();
}
void VirtualTemplateList::setOverscan(size_t rows) { overscan_ = std::min(size_t(16), rows); refresh(); }
void VirtualTemplateList::setFollowEnd(bool enabled) { following_ = enabled; }
void VirtualTemplateList::setScrollOffset(float offset) {
    if (!std::isfinite(offset)) throw std::invalid_argument("Invalid virtual scroll offset");
    offset_ = std::clamp(offset, 0.0f, maxScrollOffset());
    refresh();
}
void VirtualTemplateList::refresh() { layoutChildren(); invalidate(); }
void VirtualTemplateList::invalidateKey(const std::string& key) {
    auto row = mounted_.find(key);
    if (row == mounted_.end()) return;
    std::vector<std::shared_ptr<Widget>> next;
    for (const auto& child : children()) if (child != row->second.widget) next.push_back(child);
    reconcileChildren(std::move(next));
    mounted_.erase(row);
    if (retire_) retire_(key);
}
Rect VirtualTemplateList::rowFrame(const std::string& key) const {
    auto row = mounted_.find(key);
    return row == mounted_.end() ? Rect{} : row->second.widget->frame();
}
void VirtualTemplateList::layoutChildren() {
    if (layingOut_ || !factory_) return;
    layingOut_ = true;
    struct Reset { bool& value; ~Reset() { value = false; } } reset{layingOut_};
    auto a = anchor();
    const auto viewport = frame();
    height_ = viewport.height;
    if (width_ != viewport.width) {
        width_ = viewport.width;
        heights_.clear();
        positions();
        restore(a);
    }
    // Re-measure retained rows first. Streaming only changes the last row's
    // measure revision; stable rows keep the same widget and focus chain.
    auto measure = [&](const std::string& key, Row& row) {
        const auto size = row.widget->measure({std::max(0.0f, width_ - 10), std::numeric_limits<float>::infinity()});
        row.height = std::isfinite(size.height) ? std::max({16.0f, size.height, row.widget->preferredSize().height}) : estimate_;
        heights_[key] = row.height;
    };
    for (auto& [key, row] : mounted_) measure(key, row);
    positions();
    restore(a);
    // A bounded stabilization pass fills the viewport when estimates were too
    // large. The 16px minimum bounds mount count independently of data length.
    const size_t budget = size_t(std::max(0.0f, viewport.height) / 16) + 2 * overscan_ + 3;
    for (size_t pass = 0; pass < budget && !keys_.empty() && viewport.height > 0 && width_ > 0; ++pass) {
        size_t first = indexAt(offset_), last = indexAt(offset_ + viewport.height);
        first = first > overscan_ ? first - overscan_ : 0;
        last = std::min(keys_.size() - 1, last + overscan_);
        bool added = false;
        for (size_t i = first; i <= last; ++i) if (!mounted_.count(keys_[i])) {
            Row row{factory_(i, keys_[i]), 0};
            if (!row.widget) throw std::runtime_error("Virtual row factory returned null");
            measure(keys_[i], row);
            mounted_.emplace(keys_[i], std::move(row));
            added = true;
        }
        positions();
        restore(a);
        if (!added) break;
    }
    std::set<std::string> keep;
    if (!keys_.empty() && viewport.height > 0 && width_ > 0) {
        size_t first = indexAt(offset_), last = indexAt(offset_ + viewport.height);
        first = first > overscan_ ? first - overscan_ : 0;
        last = std::min(keys_.size() - 1, last + overscan_);
        for (size_t i = first; i <= last; ++i) keep.insert(keys_[i]);
    }
    for (const auto& [key, row] : mounted_)
        if (row.widget.get() == focusedChild() && std::find(keys_.begin(), keys_.end(), key) != keys_.end()) keep.insert(key);
    std::vector<std::shared_ptr<Widget>> children;
    std::vector<std::string> removed;
    for (size_t i = 0; i < keys_.size(); ++i) {
        auto row = mounted_.find(keys_[i]);
        if (row != mounted_.end() && keep.count(keys_[i])) {
            row->second.widget->setFrame({viewport.x, viewport.y + tops_[i] - offset_, std::max(0.0f, width_ - 10), tops_[i + 1] - tops_[i]});
            children.push_back(row->second.widget);
        }
    }
    for (const auto& [key, row] : mounted_) if (!keep.count(key)) removed.push_back(key);
    reconcileChildren(std::move(children));
    for (const auto& key : removed) { mounted_.erase(key); if (retire_) retire_(key); }
}
Rect VirtualTemplateList::thumb() const {
    auto f = frame();
    if (maxScrollOffset() <= 0 || f.height <= 0) return {};
    const float h = std::min(f.height, std::max(20.0f, f.height * f.height / tops_.back()));
    return {f.x + f.width - 6, f.y + (f.height - h) * offset_ / maxScrollOffset(), 4, h};
}
void VirtualTemplateList::paint(Canvas& canvas) {
    layoutChildren();
    canvas.save();
    canvas.clipRect(frame());
    View::paint(canvas);
    canvas.restore();
    auto t = thumb();
    if (t.height > 0) canvas.fillRect(t, theme().textMuted, 2);
}
bool VirtualTemplateList::onMouseWheel(const MouseWheelEvent& event) {
    if (!interactive() || !hitTest(event.position)) return false;
    if (View::onMouseWheel(event)) return true;
    const float old = offset_;
    setScrollOffset(offset_ - event.deltaY * 48);
    return old != offset_;
}
bool VirtualTemplateList::onMouseDown(const MouseEvent& event) {
    if (!interactive() || !hitTest(event.position)) return false;
    if (event.button == MouseButton::Left && thumb().contains(event.position)) {
        dragging_ = true; dragY_ = event.position.y; dragOffset_ = offset_; return true;
    }
    return View::onMouseDown(event);
}
bool VirtualTemplateList::onMouseMove(const MouseEvent& event) {
    if (!interactive()) { dragging_ = false; return false; }
    if (dragging_) {
        const float travel = frame().height - thumb().height;
        if (travel > 0) setScrollOffset(dragOffset_ + (event.position.y - dragY_) * maxScrollOffset() / travel);
        return true;
    }
    return View::onMouseMove(event);
}
bool VirtualTemplateList::onMouseUp(const MouseEvent& event) {
    if (dragging_) { dragging_ = false; return true; }
    return View::onMouseUp(event);
}
bool VirtualTemplateList::onKeyDown(const KeyEvent& event) {
    if (View::onKeyDown(event)) return true;
    if (!interactive()) return false;
    if (event.key == Key::PageDown) setScrollOffset(offset_ + frame().height);
    else if (event.key == Key::PageUp) setScrollOffset(offset_ - frame().height);
    else if (event.key == Key::Home) setScrollOffset(0);
    else if (event.key == Key::End) setScrollOffset(maxScrollOffset());
    else return false;
    return true;
}
} // namespace oneui
