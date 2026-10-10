#pragma once

#include "oneui/export.h"
#include "oneui/geometry.h"
#include "oneui/style_sheet.h"
#include "oneui/view.h"

#include <optional>
#include <map>
#include <limits>

namespace oneui {

enum class StackDirection {
    Row,
    Column
};

enum class StackAlign {
    Start,
    Center,
    End,
    Stretch
};

enum class StackEngine { Legacy, Yoga };

enum class StackJustify { Start, Center, End, SpaceBetween };

// Main-axis constraints. Explicit flex items shrink proportionally to
// shrink * basis. Without an entry, the legacy preferred-size rules apply.
struct StackFlex {
    float grow = 0.0f;
    float shrink = 1.0f;
    std::optional<float> basis;
    float min = 0.0f;
    float max = std::numeric_limits<float>::infinity();
    bool contentBasis = false;
    std::optional<float> width,height;
    float minWidth=0,minHeight=0;
};

struct StackLayoutStats { std::uint64_t calculations = 0, measureCacheHits = 0, arrangeCacheHits = 0; };

class ONEUI_API Stack final : public View {
public:
    explicit Stack(StackDirection direction = StackDirection::Column);
    void insert(size_t index,std::shared_ptr<Widget> child);
    bool remove(const std::shared_ptr<Widget>& child);

    // Opt-in. Returns false without changing state when Yoga was not built.
    bool setEngine(StackEngine engine);
    StackEngine engine() const { return engine_; }
    StackDirection direction() const { return direction_; }
    StackAlign align() const {return align_;}
    float gap() const {return gap_;}
    bool wrap() const {return wrap_;}
    void setSizeContainment(bool value){if(sizeContained_!=value){sizeContained_=value;invalidate();}}
    bool sizeContained() const {return sizeContained_;}
    void setContainerQuery(std::function<bool()> evaluate){query_=std::move(evaluate);}
    std::optional<StackFlex> explicitFlex(const std::shared_ptr<Widget>& child) const {
        auto found=flex_.find(child);return found==flex_.end()?std::optional<StackFlex>{}:found->second;
    }
    bool setWrap(bool enabled);
    Size measure(Size available) const override;
    StackLayoutStats layoutStats() const { return layoutStats_; }
    void setLayoutCacheEnabled(bool enabled) { layoutCacheEnabled_ = enabled; invalidate(); }
    void setDirection(StackDirection direction);
    void setGap(float gap);
    void setPadding(Insets padding);
    void setAlign(StackAlign align);
    void setJustify(StackJustify justify);
    void setFlex(const std::shared_ptr<Widget>& child, StackFlex flex);
    void clearFlex(const std::shared_ptr<Widget>& child);
    void setStyleBox(StyleBox style);
    void clearStyleBox();
    float contentWidth() const;
    float contentHeight() const;
    Size naturalSize() const override;

    Rect paintBounds() const override;
    void paint(Canvas& canvas) override;

private:
    void layoutChildren() override;
    struct YogaState;
    mutable std::shared_ptr<YogaState> yoga_;
    mutable StackLayoutStats layoutStats_;
    bool layoutCacheEnabled_ = true;
    StackEngine engine_ = StackEngine::Legacy;
    bool wrap_ = false;
    bool sizeContained_ = false;
    std::function<bool()> query_;
    bool yogaArranged_ = false;
    Size measureYoga(Size available) const;
    void paintYoga(Canvas& canvas);

    StackDirection direction_ = StackDirection::Column;
    float gap_ = 0.0f;
    Insets padding_;
    StackAlign align_ = StackAlign::Stretch;
    StackJustify justify_ = StackJustify::Start;
    std::map<std::weak_ptr<Widget>, StackFlex, std::owner_less<std::weak_ptr<Widget>>> flex_;
    struct FlexItem {
        Widget* child;
        float size, grow, shrinkWeight, min, max, cross, gapBefore;
        bool fixedCross=false;
        float minCross=0;
    };
    // Reuse storage during animation instead of allocating on each paint.
    std::vector<FlexItem> layoutItems_;
    StackFlex itemFlex(const std::shared_ptr<Widget>& child, float preferred) const;
    Size itemSize(const std::shared_ptr<Widget>& child) const;
    std::optional<StyleBox> styleBox_;
};

} // namespace oneui
