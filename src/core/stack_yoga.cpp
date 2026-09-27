#include "internal/frame_profile.h"
#include "oneui/layout/stack.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <array>

#ifdef ONEUI_HAS_YOGA
#include <yoga/Yoga.h>
#endif

namespace oneui {

bool Stack::setEngine(StackEngine engine) {
    if (engine != StackEngine::Legacy && engine != StackEngine::Yoga) return false;
#ifndef ONEUI_HAS_YOGA
    if (engine == StackEngine::Yoga) return false;
#endif
    engine_ = engine;
    if (engine == StackEngine::Legacy) { wrap_ = false; yoga_.reset(); }
    invalidate();
    return true;
}

bool Stack::setWrap(bool enabled) {
    if (enabled && engine_ != StackEngine::Yoga) return false;
    wrap_ = enabled;
    invalidate();
    return true;
}

Size Stack::measure(Size available) const {
    return engine_ == StackEngine::Yoga ? measureYoga(available) : naturalSize();
}

#ifdef ONEUI_HAS_YOGA
struct Stack::YogaState {
    struct Entry {
        YGNodeRef node;
        Widget* widget;
        std::shared_ptr<Widget> retained;
        std::vector<std::unique_ptr<Entry>> children;
        std::uint64_t revision = ~std::uint64_t{0};
        bool content = true;
        bool container = false;
        Entry(YGConfigRef config, Widget* w, std::shared_ptr<Widget> keep = {})
            : node(YGNodeNewWithConfig(config)), widget(w), retained(std::move(keep)) {
            YGNodeSetContext(node, this);
        }
        ~Entry() {
            YGNodeRemoveAllChildren(node);
            children.clear();
            YGNodeFree(node);
        }
    };
    YGConfigRef config = YGConfigNew();
    std::unique_ptr<Entry> root;
    std::vector<Stack*> arranged;
    std::uint64_t syncedRevision = ~std::uint64_t{0};
    struct Query { Size available{}, result{}; std::uint64_t revision = 0; bool valid = false, exact = false; };
    Query last;
    std::array<Query, 4> measures{};
    size_t nextMeasure = 0;
    static bool matches(const Query& q, Size size, std::uint64_t revision) {
        return q.valid && q.revision == revision && q.available.width == size.width && q.available.height == size.height;
    }
    Size measure(Size available) {
        auto* owner = static_cast<Stack*>(root->widget);
        const auto revision = owner->measureRevision();
        if (owner->layoutCacheEnabled_) for (const auto& query : measures) {
            if (matches(query, available, revision)) {
                ++owner->layoutStats_.measureCacheHits;
                return query.result;
            }
        }
        const auto result = calculate(available, false);
        measures[nextMeasure++ % measures.size()] = {available, result, revision, true, false};
        return result;
    }

    explicit YogaState(Stack* stack) {
        YGConfigSetUseWebDefaults(config, true);
        // OneUI paints logical pixels and performs its own DPI conversion.
        YGConfigSetPointScaleFactor(config, 0);
        root = std::make_unique<Entry>(config, stack);
    }
    ~YogaState() { root.reset(); YGConfigFree(config); }

    static float limit(float value) {
        return std::isfinite(value) ? std::max(0.0f, value) : YGUndefined;
    }
    static YGSize measureLeaf(YGNodeConstRef node, float width, YGMeasureMode wm,
                              float height, YGMeasureMode hm) {
        const auto& entry = *static_cast<Entry*>(YGNodeGetContext(node));
        const auto size = entry.content
            ? entry.widget->measure({wm == YGMeasureModeUndefined ? INFINITY : width,
                                     hm == YGMeasureModeUndefined ? INFINITY : height})
            : entry.widget->preferredSize();
        auto extent = [](float value, float bound, YGMeasureMode mode) {
            value = std::isfinite(value) ? std::ceil(std::max(0.0f, value)) : 0.0f;
            if (mode == YGMeasureModeExactly) return bound;
            return mode == YGMeasureModeAtMost ? std::min(value, bound) : value;
        };
        return {extent(size.width, width, wm), extent(size.height, height, hm)};
    }

    void sync(Entry& entry) {
        auto* stack = dynamic_cast<Stack*>(entry.widget);
        const bool container = stack && stack->engine_ == StackEngine::Yoga;
        if (container != entry.container) {
            YGNodeRemoveAllChildren(entry.node);
            entry.children.clear();
            YGNodeSetMeasureFunc(entry.node, container ? nullptr : measureLeaf);
            entry.container = container;
        }
        YGNodeStyleSetDisplay(entry.node, entry.widget->visible() ? YGDisplayFlex : YGDisplayNone);
        if (!container) {
            YGNodeSetMeasureFunc(entry.node, measureLeaf);
            if (entry.revision != entry.widget->measureRevision()) YGNodeMarkDirty(entry.node);
            entry.revision = entry.widget->measureRevision();
            return;
        }
        const bool column = stack->direction_ == StackDirection::Column;
        const YGAlign align[] = {YGAlignFlexStart, YGAlignCenter, YGAlignFlexEnd, YGAlignStretch};
        const YGJustify justify[] = {YGJustifyFlexStart, YGJustifyCenter, YGJustifyFlexEnd, YGJustifySpaceBetween};
        YGNodeStyleSetFlexDirection(entry.node, column ? YGFlexDirectionColumn : YGFlexDirectionRow);
        YGNodeStyleSetFlexWrap(entry.node, stack->wrap_ ? YGWrapWrap : YGWrapNoWrap);
        YGNodeStyleSetAlignItems(entry.node, align[static_cast<int>(stack->align_)]);
        YGNodeStyleSetAlignContent(entry.node, YGAlignFlexStart);
        YGNodeStyleSetJustifyContent(entry.node, justify[static_cast<int>(stack->justify_)]);
        YGNodeStyleSetGap(entry.node, YGGutterAll, std::max(0.0f, stack->gap_));
        YGNodeStyleSetPadding(entry.node, YGEdgeTop, std::max(0.0f, stack->padding_.top));
        YGNodeStyleSetPadding(entry.node, YGEdgeRight, std::max(0.0f, stack->padding_.right));
        YGNodeStyleSetPadding(entry.node, YGEdgeBottom, std::max(0.0f, stack->padding_.bottom));
        YGNodeStyleSetPadding(entry.node, YGEdgeLeft, std::max(0.0f, stack->padding_.left));
        const auto& children = stack->children();
        // Keep native and Yoga identities stable until the child list changes.
        bool same = entry.children.size() == children.size();
        for (size_t i = 0; same && i < children.size(); ++i)
            same = entry.children[i]->widget == children[i].get();
        if (!same) {
            YGNodeRemoveAllChildren(entry.node);
            entry.children.clear();
            for (const auto& child : children) {
                entry.children.push_back(std::make_unique<Entry>(config, child.get(), child));
                YGNodeInsertChild(entry.node, entry.children.back()->node, entry.children.size() - 1);
            }
        }
        // Removed Yoga entries have released their retained widgets above.
        // Match legacy Stack's cleanup of dead constraint registrations.
        for (auto it = stack->flex_.begin(); it != stack->flex_.end();) {
            if (it->first.expired()) it = stack->flex_.erase(it);
            else ++it;
        }
        for (size_t i = 0; i < children.size(); ++i) {
            auto& child = *entry.children[i];
            const auto preferred = children[i]->preferredSize();
            const auto flex = stack->itemFlex(children[i], column ? preferred.height : preferred.width);
            // An explicit main-axis basis must not replace cross-axis text
            // measurement with the widget's legacy preferred height.
            const bool measureContent = flex.contentBasis ||
                (flex.basis && stack->flex_.find(children[i]) != stack->flex_.end());
            if (child.content != measureContent) child.revision = ~std::uint64_t{0};
            child.content = measureContent;
            sync(child);
            YGNodeStyleSetFlexGrow(child.node, flex.grow);
            YGNodeStyleSetFlexShrink(child.node, flex.shrink);
            if (flex.basis) YGNodeStyleSetFlexBasis(child.node, *flex.basis);
            else if (flex.contentBasis) YGNodeStyleSetFlexBasisAuto(child.node);
            else YGNodeStyleSetFlexBasis(child.node, std::max(0.0f, column ? preferred.height : preferred.width));
            YGNodeStyleSetMinWidth(child.node, column ? YGUndefined : flex.min);
            YGNodeStyleSetMinHeight(child.node, column ? flex.min : YGUndefined);
            YGNodeStyleSetMaxWidth(child.node, column ? YGUndefined : limit(flex.max));
            YGNodeStyleSetMaxHeight(child.node, column ? limit(flex.max) : YGUndefined);
        }
    }

    Size calculate(Size available, bool exact) {
        auto* owner = static_cast<Stack*>(root->widget);
        const auto revision = owner->measureRevision();
        if (exact && owner->layoutCacheEnabled_ && last.exact && matches(last, available, revision)) {
            ++owner->layoutStats_.arrangeCacheHits;
            return last.result;
        }
        if (syncedRevision != root->widget->measureRevision()) {
            sync(*root);
            syncedRevision = root->widget->measureRevision();
        }
        YGNodeStyleSetWidth(root->node, limit(available.width));
        YGNodeStyleSetHeight(root->node, exact ? limit(available.height) : YGUndefined);
        YGNodeStyleSetMaxWidth(root->node, exact ? YGUndefined : limit(available.width));
        YGNodeStyleSetMaxHeight(root->node, exact ? YGUndefined : limit(available.height));
        ++owner->layoutStats_.calculations;
        YGNodeCalculateLayout(root->node, YGUndefined, YGUndefined, YGDirectionLTR);
        const Size result{YGNodeLayoutGetWidth(root->node), YGNodeLayoutGetHeight(root->node)};
        last = {available, result, revision, true, exact};
        return result;
    }

    void assign(Entry& entry, float x, float y) {
        if (entry.container) {
            auto* stack = static_cast<Stack*>(entry.widget);
            stack->yogaArranged_ = true;
            arranged.push_back(stack);
        }
        for (auto& child : entry.children) {
            const auto n = child->node;
            const Rect rect{x + YGNodeLayoutGetLeft(n), y + YGNodeLayoutGetTop(n),
                            YGNodeLayoutGetWidth(n), YGNodeLayoutGetHeight(n)};
            child->widget->setFrame(rect);
            assign(*child, rect.x, rect.y);
        }
    }
    void clearAssigned() {
        for (auto* stack : arranged) stack->yogaArranged_ = false;
        arranged.clear();
    }
};

Size Stack::measureYoga(Size available) const {
    if (!yoga_) yoga_ = std::make_shared<YogaState>(const_cast<Stack*>(this));
    return yoga_->measure(available);
}

void Stack::paintYoga(Canvas& canvas) {
    const bool outer = !yogaArranged_;
    std::shared_ptr<YogaState> state;
    if (outer) {
        if (!yoga_) yoga_ = std::make_shared<YogaState>(this);
        state = yoga_;
        internal::FrameSpan span(internal::FrameStage::Layout);
        state->calculate({frame().width, frame().height}, true);
        state->assign(*state->root, frame().x, frame().y);
    }
    struct Guard {
        std::shared_ptr<YogaState> state;
        ~Guard() { if (state) state->clearAssigned(); }
    } guard{state};
    if (styleBox_) paintStyleBox(canvas, frame(), *styleBox_);
    View::paint(canvas);
}
#else
Size Stack::measureYoga(Size) const { return {}; }
void Stack::paintYoga(Canvas&) {}
#endif
} // namespace oneui
