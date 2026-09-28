#pragma once
#include "oneui/style.h"
#include <algorithm>

namespace oneui {
// Bounded shadow lists are interpolated independently of layout and hit testing.
// Opposite inset modes cross-fade instead of becoming an unrelated hard border.
class ShadowTransition {
    std::vector<ControlShadowStyle> from_, target_;
    FloatTransition progress_{1};
    static bool equal(const std::vector<ControlShadowStyle>& a,const std::vector<ControlShadowStyle>& b) {
        if(a.size()!=b.size())return false;
        for(size_t i=0;i<a.size();++i) {
            const auto& x=a[i];const auto& y=b[i];
            if(x.color.r!=y.color.r || x.color.g!=y.color.g || x.color.b!=y.color.b || x.color.a!=y.color.a ||
               x.offset.x!=y.offset.x || x.offset.y!=y.offset.y || x.blurRadius!=y.blurRadius || x.spreadRadius!=y.spreadRadius || x.inset!=y.inset)return false;
        }
        return true;
    }
public:
    void reset(const std::vector<ControlShadowStyle>& shadows) {from_=target_=shadows;progress_.reset(1);}
    void animateTo(const std::vector<ControlShadowStyle>& shadows,double now,TransitionSpec spec) {
        if(equal(shadows,target_))return;
        from_=value();target_=shadows;progress_.reset(0);progress_.animateTo(1,now,spec);
    }
    bool tick(double now) {return progress_.tick(now);}
    bool running() const {return progress_.running();}
    std::vector<ControlShadowStyle> value() const {
        if(!running())return target_;
        const float t=progress_.value();std::vector<ControlShadowStyle> result;
        for(bool inset:{false,true}) {
            std::vector<ControlShadowStyle> aa,bb;
            for(const auto& s:from_)if(s.inset==inset)aa.push_back(s);
            for(const auto& s:target_)if(s.inset==inset)bb.push_back(s);
            const auto count=(std::max)(aa.size(),bb.size());
            for(size_t i=0;i<count;++i) {
                auto a=i<aa.size()?aa[i]:bb[i],b=i<bb.size()?bb[i]:aa[i];
                if(i>=aa.size())a.color.a=0;
                if(i>=bb.size())b.color.a=0;
                a.color=interpolateColor(a.color,b.color,t);
                a.offset={interpolateFloat(a.offset.x,b.offset.x,t),interpolateFloat(a.offset.y,b.offset.y,t)};
                a.blurRadius=interpolateFloat(a.blurRadius,b.blurRadius,t);
                a.spreadRadius=interpolateFloat(a.spreadRadius,b.spreadRadius,t);result.push_back(a);
            }
        }
        return result;
    }
};
}
