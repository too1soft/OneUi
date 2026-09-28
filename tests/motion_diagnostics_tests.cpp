#include "oneui/ui_motion_diagnostics.h"
#include <iostream>
#include <limits>
using namespace oneui::ui;
#define CHECK(x) do{if(!(x)){std::cerr<<"Failed: " #x;return 1;}}while(0)
int main(){
    MotionAudit good;good.begin(0,0,true);
    for(int i=1;i<=10;++i)good.observe(i*16.,i*.1,100+i*10,100+i*10,2);
    CHECK(good.report().continuous() && !good.report().lateFrames);
    MotionAudit bad;bad.begin(0,0,true);bad.observe(60,.5,170,150,40);bad.observe(76,.4,140,140,1);
    CHECK(bad.report().geometryErrors==1 && bad.report().reverseSteps==1 && bad.report().stalls==1);
    // Idle time and deliberate interruption must not count as dropped frames
    // or as the previous transition reversing direction.
    good.begin(1000,1,false);good.observe(1016,.9,190,190,2);CHECK(good.report().continuous() && !good.report().lateFrames);
    good.begin(1020,.9,true);good.observe(1036,.95,195,195,2);CHECK(good.report().continuous());
    bool invalid=false;try{MotionAudit zero(0);}catch(const std::invalid_argument&){invalid=true;}CHECK(invalid);
    invalid=false;try{MotionAudit unset;unset.observe(0,0,0,0,0);}catch(const std::logic_error&){invalid=true;}CHECK(invalid);
    std::cout<<"Motion temporal diagnostics: geometry, reversal, cadence, interruption and idle boundaries passed.\n";
}
