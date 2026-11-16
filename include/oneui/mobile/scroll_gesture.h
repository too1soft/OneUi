#pragma once
#include <algorithm>
#include <cmath>
#include <functional>
namespace oneui::mobile {
// Relative wheel movement; independent of a desktop protocol and UI toolkit.
class ScrollGesture {
  public:
    using Sink=std::function<void(float)>;
    explicit ScrollGesture(Sink sink={}):sink_(std::move(sink)){}
    void begin(float position){active_=std::isfinite(position);previous_=position;}
    void move(float position){if(!active_||!std::isfinite(position))return;float delta=std::clamp((previous_-position)*3.f,-120.f,120.f);previous_=position;if(delta&&sink_)sink_(delta);}
    void cancel(){active_=false;}
  private:
    Sink sink_;float previous_=0;bool active_=false;
};
}
