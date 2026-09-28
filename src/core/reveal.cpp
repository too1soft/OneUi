#include "oneui/controls/reveal.h"
#include "internal/ui_clock.h"
#include "internal/developer_tools.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace oneui {
Reveal::Reveal(std::shared_ptr<Widget> content) {
    if(!content)throw std::invalid_argument("Reveal needs one child");
    add(std::move(content));
}
void Reveal::setOpen(bool value) {
    if(value==open_)return;
    ++motionRevision_;
    open_=value;
    if(!value){focusChild(nullptr);clearInteractionState();}
    else Widget::setVisible(true);
    auto spec=spec_;
    if(!hasAnimationScheduler() || reduced_ || !motionEnabled())spec.durationMs=0;
    if(!value)spec.durationMs*=0.75;
    const double now=internal::uiTimeMs();
    progress_.animateTo(value?1.0f:0.0f,now,spec);
    // Color feedback primes its first frame by 8ms. Geometry must start at its
    // current extent: priming jumps immediately and a fast first tick reverses.
    progress_.tick(now);
    if(progress_.running())requestAnimationFrame();
    else if(!value)Widget::setVisible(false);
    invalidate();
}
void Reveal::setPreset(RevealPreset preset) {preset_=preset;invalidate();}
void Reveal::setTransition(TransitionSpec spec) {
    if(!std::isfinite(spec.durationMs) || spec.durationMs<0)throw std::invalid_argument("Reveal duration must be finite and nonnegative");
    spec_=spec;
    if(spec.durationMs==0){progress_.reset(open_?1:0);if(!open_)Widget::setVisible(false);invalidate();}
}
void Reveal::setReducedMotion(bool reduced) {
    reduced_=reduced;
    if(reduced){progress_.reset(open_?1:0);if(!open_)Widget::setVisible(false);invalidate();}
}
void Reveal::setVisible(bool value) {
    // Hiding an ancestor/page does not leave a queued animation alive.
    if(!value)progress_.reset(open_?1:0);
    Widget::setVisible(value);
}
Size Reveal::measure(Size available) const {
    auto size=children().front()->measure({available.width,INFINITY});
    if(preset_==RevealPreset::Expand)size.height*=progress_.value();
    if(!open_ && !running())size.height=0;
    return size;
}
Size Reveal::naturalSize() const {return measure({INFINITY,INFINITY});}
void Reveal::layoutChildren() {
    const auto size=children().front()->measure({frame().width,INFINITY});
    children().front()->setFrame({frame().x,frame().y,frame().width,size.height});
}
Rect Reveal::paintBounds() const {
    if(progress_.value()<1)return frame();
    const auto a=frame(),b=children().front()->paintBounds();
    const float x=(std::min)(a.x,b.x),y=(std::min)(a.y,b.y);
    return {x,y,(std::max)(a.x+a.width,b.x+b.width)-x,(std::max)(a.y+a.height,b.y+b.height)-y};
}
void Reveal::paint(Canvas& canvas) {
    if(auto* diagnostics=internal::activeDeveloperRecorder())
        diagnostics->observeMotion(*this,motionRevision_,progress(),open_);
    if(!visible() || progress_.value()<=0)return;
    if(progress_.value()>=1){View::paint(canvas);return;}
    canvas.save();canvas.clipRect(frame());
    if(preset_==RevealPreset::Fade && progress_.value()<1)canvas.saveOpacity(frame(),progress_.value());
    View::paint(canvas);
    if(preset_==RevealPreset::Fade && progress_.value()<1)canvas.restore();
    canvas.restore();
}
bool Reveal::tickAnimations(double now) {
    if(!visible()){progress_.reset(open_?1:0);return false;}
    const bool changed=progress_.tick(now);
    if(changed){if(!progress_.running() && !open_)Widget::setVisible(false);invalidate();}
    const bool childrenRunning=open_ && View::tickAnimations(now);
    return progress_.running() || childrenRunning;
}
}
