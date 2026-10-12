#pragma once

#include "oneui/view.h"
#include <map>
#include <string>

namespace oneui {

// A keyed, variable-height viewport. Only visible rows, overscan and at most
// one focused row have native children. Data/model ownership stays with the
// caller; the factory is invoked only when a row enters the retained range.
class ONEUI_API VirtualTemplateList final : public View {
public:
    using Factory = std::function<std::shared_ptr<Widget>(size_t, const std::string&)>;
    using Retire = std::function<void(const std::string&)>;
    void setFactory(Factory factory, Retire retire);
    void setKeys(std::vector<std::string> keys, bool refreshNow = true);
    void setEstimatedHeight(float height);
    void setOverscan(size_t rows);
    void setFollowEnd(bool enabled);
    void setScrollOffset(float offset);
    float scrollOffset() const { return offset_; }
    float maxScrollOffset() const;
    void refresh();
    void invalidateKey(const std::string& key);
    size_t mountedCount() const { return mounted_.size(); }
    Rect rowFrame(const std::string& key) const;
    void paint(Canvas&) override;
    bool onMouseWheel(const MouseWheelEvent&) override;
    bool onMouseDown(const MouseEvent&) override;
    bool onMouseMove(const MouseEvent&) override;
    bool onMouseUp(const MouseEvent&) override;
    bool onKeyDown(const KeyEvent&) override;

protected:
    void layoutChildren() override;

private:
    struct Row { std::shared_ptr<Widget> widget; float height = 0; };
    struct Anchor { std::string key; float inset = 0; bool end = false; };
    Anchor anchor() const;
    void restore(const Anchor&);
    void positions();
    size_t indexAt(float offset) const;
    Rect thumb() const;
    std::vector<std::string> keys_;
    std::vector<float> tops_{0};
    std::map<std::string, float> heights_;
    std::map<std::string, Row> mounted_;
    Factory factory_;
    Retire retire_;
    float estimate_ = 80, offset_ = 0, width_ = -1, height_ = 0;
    size_t overscan_ = 2;
    bool following_ = false, layingOut_ = false, dragging_ = false;
    float dragY_ = 0, dragOffset_ = 0;
};

} // namespace oneui
