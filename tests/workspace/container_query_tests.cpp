#include "oneui/ui_declarative.h"
#include "oneui/layout/container_query.h"
#include "oneui/controls/text_field.h"
#include "../support/recording_canvas.h"
#include <iostream>
#include <stdexcept>
using namespace oneui;
namespace {
void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
void comparisons() {
    auto q=parseContainerQuery("@container sftp (width >= 860px) and (height > 0px) and (aspect-ratio >= 8/5)");
    check(!q.matches({859.99f,500})&&q.matches({860,500})&&q.matches({860.01f,500}),"860 exact threshold");
    check(!q.matches({1000,625.01f})&&q.matches({1000,625})&&q.matches({1000,624.99f}),"ratio exact threshold");
    check(!q.matches({1100,0}),"zero height never matches aspect ratio");
    auto details=parseContainerQuery("@container sftp (width >= 1100px)");
    check(!details.matches({1099.99f,500})&&details.matches({1100,500})&&details.matches({1100.01f,500}),"1100 exact threshold");
    for(auto bad:{"@container (width >= 860px)","@container x (width >= 3em)","@container x (aspect-ratio >= 8/0)","@container x (width >= 1px) or (height > 2px)","@container x (width > 2px) and "}) {
        bool rejected=false;try{parseContainerQuery(bad);}catch(const std::invalid_argument&){rejected=true;}
        check(rejected,"invalid query syntax rejected");
    }
}
void stackChildren() {
    for(auto engine:{StackEngine::Legacy,StackEngine::Yoga}) {
        auto stack=std::make_shared<Stack>();stack->setEngine(engine);stack->setFrame({0,0,500,300});
        auto a=std::make_shared<TextField>(),b=std::make_shared<TextField>();
        stack->insert(0,a);stack->insert(1,b);stack->setFlex(a,{1,1,30,0});
        stack->requestFocus(a.get());a->setTextComposition(L"组合",2);
        stack->insert(1,a);check(stack->children()[1]==a&&a->hasTextComposition()&&a->focused(),"MOVE preserves focus and composition");
        check(stack->explicitFlex(a)->grow==1,"MOVE preserves flex metadata");
        auto other=std::make_shared<Stack>();bool rejected=false;
        try{other->insert(0,a);}catch(const std::invalid_argument&){rejected=true;}
        check(rejected&&stack->children()[1]==a,"cross parent insert rejected atomically");
        check(stack->remove(a)&&!stack->remove(a),"remove return and identity");
        check(!a->hasTextComposition()&&a->text()==L"组合","remove commits composition before focus loss");
        check(b->focused(),"remove moves focus to surviving input");
        check(!stack->explicitFlex(a),"removed flex metadata erased");
        other->insert(0,a);check(other->children()[0]==a,"explicit remove insert retains widget");
    }
}
const char* css=R"CSS(
.frame {container-name:sftp;container-type:size;}
@container sftp (width >= 860px) and (height > 0px) and (aspect-ratio >= 8/5) {
  .columns {flex-direction:row;gap:12px;}
  .tree {display:flex;flex-basis:224px;flex-shrink:0;}
  .browser {display:flex;flex-basis:0px;flex-grow:1;}
  .details {display:none;}
}
@container sftp (width >= 1100px) and (height > 0px) and (aspect-ratio >= 8/5) {
  .details {display:flex;flex-basis:292px;flex-shrink:0;}
}
)CSS";
void layout(StackEngine engine) {
    ui::Mount mount({},std::make_shared<ui::StyleRegistry>(),{});
    auto tree=mount.make("ContainerPage",{mount.make("Text")},"tree");
    auto browser=mount.make("ContainerPage",{mount.make("Text")},"browser");
    auto details=mount.make("ContainerPage",{mount.make("Text")},"details");
    details.as<ContainerPage>()->setSelected(true);
    auto columns=mount.make("Column",{tree,browser,details},"columns");columns.basis(0).grow();
    auto shell=mount.make("Column",{columns},"frame");
    shell.as<Stack>()->setEngine(engine);columns.as<Stack>()->setEngine(engine);
    mount.styles()->replace(css);test_support::RecordingCanvas canvas;
    auto paint=[&](float width,float height){shell.widget->setFrame({0,0,width,height});shell.widget->paint(canvas);};
    paint(859.99f,500);check(!tree.widget->visible()&&!browser.widget->visible()&&details.widget->visible(),"narrow selected detail survives");
    auto before=shell.as<Stack>()->layoutStats().calculations;
    paint(860,500);check(tree.widget->visible()&&browser.widget->visible()&&!details.widget->visible(),"wide two pane query applied");
    if(engine==StackEngine::Yoga)check(shell.as<Stack>()->layoutStats().calculations-before<=2,"at most one extra layout");
    check(columns.as<Stack>()->direction()==StackDirection::Row,"query direction consumed");
    check(std::abs(tree.widget->frame().width-224)<.01f,"query flex basis consumed");
    paint(1100,500);check(details.widget->visible()&&std::abs(details.widget->frame().width-292)<.01f,"details third pane");
    details.widget->setVisible(false);paint(1100.01f,500);check(!details.widget->visible(),"CSS cannot override authored false");
    details.widget->setVisible(true);paint(850,500);
    check(details.widget->visible()&&!tree.widget->visible()&&columns.as<Stack>()->direction()==StackDirection::Column,"query exit restores base layout");
    auto address=tree.widget.get();mount.styles()->replace(css);paint(1100,500);
    check(address==tree.widget.get()&&tree.widget->visible(),"stylesheet reload preserves identities and recomputes query");
    check(mount.styles()->queryDiagnostics().empty(),"independent shell has no diagnostics");
    mount.styles()->replace("");paint(1100,500);
    check(!tree.widget->visible()&&details.widget->visible()&&columns.as<Stack>()->direction()==StackDirection::Column,"removing query CSS restores authored defaults");
}
void invalidCssAndDependencies() {
    StyleSheet sheet;std::string error;
    check(sheet.addRulesFromCss(".a {color:#123456;}",&error),"initial CSS");auto count=sheet.rules().size();
    check(!sheet.addRulesFromCss("@container x (width > 3px) { .a {bogus:yes;} }",&error)&&sheet.rules().size()==count,"invalid CSS preserves previous sheet");
    check(error.find("CSS:")!=std::string::npos,"CSS source location");
    ui::Mount mount({},std::make_shared<ui::StyleRegistry>(),{});
    auto inner=mount.make("Column",{mount.make("Text")},"inner");
    auto outer=mount.make("Column",{inner},"outer");
    mount.styles()->replace(".outer{container-name:o;container-type:size;}.inner{container-name:i;container-type:size;}@container o (width > 1px){.inner{height:200px;}}");
    outer.widget->setFrame({0,0,500,500});test_support::RecordingCanvas canvas;outer.widget->paint(canvas);
    check(!mount.styles()->queryDiagnostics().empty(),"dependent nested container diagnosed instead of oscillating");
}
}
int main(){try{comparisons();stackChildren();layout(StackEngine::Yoga);layout(StackEngine::Legacy);invalidCssAndDependencies();
    std::cout<<"PASS container queries and Stack: 5 groups\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
