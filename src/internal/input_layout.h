#pragma once

#include "oneui/widget.h"
#include <memory>

namespace oneui::internal {

// Input must observe current geometry even while painting is frame-coalesced.
// The revision check keeps unchanged hover/down/up dispatch constant-time.
class InputLayoutSnapshot {
public:
    class PaintScope {
    public:
        explicit PaintScope(InputLayoutSnapshot& state) : state_(state), previous_(state.painting_) {
            state_.painting_ = true;
        }
        ~PaintScope() { state_.painting_ = previous_; }
        PaintScope(const PaintScope&) = delete;
        PaintScope& operator=(const PaintScope&) = delete;
    private:
        InputLayoutSnapshot& state_;
        bool previous_;
    };
    bool prepare(const std::shared_ptr<Widget>& root, Rect viewport) {
        if (!root || resolving_ || painting_) return false;
        if (valid_ && root_.lock() == root && revision_ == root->measureRevision() &&
            sameRect(viewport_, viewport) && sameRect(root->frame(), viewport)) return true;

        valid_ = false;
        struct ResolveGuard {
            bool& active;
            explicit ResolveGuard(bool& value) : active(value) { active = true; }
            ~ResolveGuard() { active = false; }
        } guard(resolving_);
        root->setFrame(viewport);
        for (int pass = 0; pass < 4; ++pass) {
            const auto before = root->measureRevision();
            layoutWidgetSubtree(*root);
            if (before != root->measureRevision()) continue;
            root_ = root;
            viewport_ = viewport;
            revision_ = before;
            valid_ = true;
            return true;
        }
        // A size observer can invalidate an ancestor during layout. Do not
        // dispatch into an unstable or recursively resolving hierarchy.
        return false;
    }

private:
    static bool sameRect(Rect a, Rect b) {
        return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
    }
    std::weak_ptr<Widget> root_;
    Rect viewport_{};
    std::uint64_t revision_ = 0;
    bool valid_ = false, resolving_ = false, painting_ = false;
};

} // namespace oneui::internal
