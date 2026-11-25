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
void projectedSelectorsAndTokens() {
    StyleSheet source;
    check(source.addRulesFromCss(
        ":root{--ink:#123456;} .cell{color:var(--ink);height:12px;}"
        ".cell:hover{height:18px;}"
        "@container frame (width >= 300px){.cell{height:24px;max-width:90px;order:3;}}"), "projection CSS");
    StyleRule unrelated;
    unrelated.selector=".unrelated";
    source.addRule(unrelated);
    auto narrow=source.forContainers({{"frame",{299,100}}});
    auto wide=source.forContainers({{"frame",{300,100}}});
    const StyleNode normal{"label",{"cell"},0},hover{"label",{"cell"},StyleStateHover};
    check(narrow.resolve(normal).height==12&&wide.resolve(normal).height==24,"projection retains rule order and exact tier");
    check(wide.resolve(hover).height==18,"compiled specificity survives projection");
    check(wide.resolveLayout(normal).maxWidth==90&&wide.resolveLayout(normal).flexOrder==3,"query layout metadata survives query removal");
    wide.setCustomProperty("--ink","#abcdef");
    check(wide.resolve(normal).foreground->r==0xab&&source.resolve(normal).foreground->r==0x12,"projected token/cache ownership remains independent");
    StyleRule extra;extra.selector=".cell";extra.box.height=31;wide.addRule(extra);
    check(wide.resolve(normal).height==31&&source.resolve(normal).height==12,"projected rule edits detach from source");
    auto repeated=source.forContainers({{"frame",{720,360}}});
    check(repeated.resolve(normal).height==24&&repeated.resolve(normal).foreground->r==0x12,
          "another size with the same matches uses the original rules and palette");
    StyleSheet counterpart=source;
    source.setCustomProperty("--ink","#224466");
    counterpart.setCustomProperty("--ink","#99bbee");
    auto sourceProjection=source.forContainers({{"frame",{720,360}}});
    auto otherProjection=counterpart.forContainers({{"frame",{720,360}}});
    auto cachedProjection=source.forContainers({{"frame",{721,361}}});
    check(sourceProjection.resolve(normal).foreground->r==0x22&&otherProjection.resolve(normal).foreground->r==0x99&&
          cachedProjection.resolve(normal).foreground->r==0x22,
          "matching rule projections do not share theme tokens");
    counterpart = StyleSheet{};
    StyleRule replacement;
    replacement.selector=".cell";
    replacement.box.height=42;
    source.addRule(replacement);
    auto changed=source.forContainers({{"frame",{720,360}}});
    check(changed.resolve(normal).height==42&&sourceProjection.resolve(normal).height==24,
          "in-place source rule changes invalidate previously selected rules");
}
const char* css=R"CSS(
.frame {container-name:sftp;container-type:size;}
.tree, .browser, .details {flex-grow:1;flex-basis:0px;}
@container sftp (width >= 860px) and (height > 0px) and (aspect-ratio >= 8/5) {
  .columns {flex-direction:row;gap:12px;}
  .tree {display:flex;flex-basis:224px;flex-grow:0;flex-shrink:0;}
  .browser {display:flex;flex-basis:0px;flex-grow:1;}
  .details {display:none;}
}
@container sftp (width >= 1100px) and (height > 0px) and (aspect-ratio >= 8/5) {
  .details {display:flex;flex-basis:292px;flex-grow:0;flex-shrink:0;}
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
    mount.styles()->applyBaseLayouts();
    paint(1100,500);
    check(tree.widget->visible()&&browser.widget->visible()&&details.widget->visible(),"VM base layout refresh reapplies queries at unchanged dimensions");
    check(std::abs(tree.widget->frame().width-224)<.01f&&std::abs(details.widget->frame().width-292)<.01f,"VM refresh preserves queried pane widths");
    details.widget->setVisible(false);paint(1100.01f,500);check(!details.widget->visible(),"CSS cannot override authored false");
    details.widget->setVisible(true);paint(850,500);
    check(details.widget->visible()&&!tree.widget->visible()&&columns.as<Stack>()->direction()==StackDirection::Column,"query exit restores base layout");
    auto address=tree.widget.get();mount.styles()->replace(css);paint(1100,500);
    check(address==tree.widget.get()&&tree.widget->visible(),"stylesheet reload preserves identities and recomputes query");
    check(mount.styles()->queryDiagnostics().empty(),"independent shell has no diagnostics");
    mount.styles()->replace("");paint(1100,500);
    check(!tree.widget->visible()&&details.widget->visible()&&columns.as<Stack>()->direction()==StackDirection::Column,"removing query CSS restores authored defaults");
}
void displayOnlyReplay() {
    ui::Mount mount({},std::make_shared<ui::StyleRegistry>(),{});
    auto child=mount.make("Text",{},"child");
    auto shell=mount.make("Column",{child},"shell");
    mount.styles()->replace(
        ".shell{container-name:shell;container-type:size;}"
        ".child{display:none;}"
        "@container shell (width >= 300px){.child{display:flex;}}");
    test_support::RecordingCanvas canvas;
    auto paint=[&]{shell.widget->setFrame({0,0,400,100});shell.widget->paint(canvas);};
    paint();check(child.widget->visible(),"display-only query enables child");
    child.as<Label>()->setText(L"Updated model text");
    const auto revision = shell.widget->measureRevision();
    mount.styles()->refreshBaseLayouts();
    check(child.widget->visible() && shell.widget->measureRevision() == revision,
          "data updates preserve the active container layout before paint");
    mount.styles()->applyBaseLayouts();
    check(!child.widget->visible(),"display-only base write actually replaced query");
    paint();check(child.widget->visible(),"same-size query reapplied after display-only base write");
    mount.styles()->applyAll();
    check(!child.widget->visible(),"base applyAll actually replaced display query");
    paint();check(child.widget->visible(),"same-size query reapplied after base applyAll");
    check(mount.styles()->queryDiagnostics().empty(),"display-only query remains valid");
}
void refreshAuthoredLayout() {
    ui::Mount mount({}, std::make_shared<ui::StyleRegistry>(), {});
    auto child = mount.make("Text", {}, "cell");
    auto first = mount.make("Row", {child});
    auto second = mount.make("Row");
    mount.styles()->replace(".cell{width:80px;}");
    mount.set(child, "grow", 2.0f);
    mount.styles()->refreshBaseLayouts();
    const auto initial = first.as<Stack>()->explicitFlex(child.widget);
    check(initial && initial->grow == 2 && initial->width == 80, "authored flex refresh keeps CSS width");
    first.as<Stack>()->remove(child.widget);
    second.as<Stack>()->add(child.widget);
    mount.styles()->refreshBaseLayouts();
    const auto moved = second.as<Stack>()->explicitFlex(child.widget);
    check(moved && moved->width == 80, "parent changes refresh base layout for retained widgets");
}
void sharedBaseLayoutSelectors() {
    ui::Mount mount({}, std::make_shared<ui::StyleRegistry>(), {});
    auto rowLabel = mount.make("Text", {}, "cell");
    auto columnLabel = mount.make("Text", {}, "cell");
    auto button = mount.make("Button", {}, "cell");
    auto row = mount.make("Row", {rowLabel, button}).as<Stack>();
    auto column = mount.make("Column", {columnLabel}).as<Stack>();
    auto apply = [&](const char* declarations) {
        mount.styles()->replace(ui::syntax::css(declarations));
        const auto horizontal = row->explicitFlex(rowLabel.widget);
        const auto vertical = column->explicitFlex(columnLabel.widget);
        const auto control = row->explicitFlex(button.widget);
        check(horizontal && vertical && control && horizontal->max == 70 && vertical->max == 24,
              "shared selectors still use each child's parent axis");
        return std::pair(horizontal->width, control->width);
    };
    const auto first = apply("Text.cell{width:40px;max-width:70px;max-height:24px;}"
                             "Button.cell{width:56px;}");
    const auto second = apply("Text.cell{width:42px;max-width:70px;max-height:24px;}"
                              "Button.cell{width:52px;}");
    check(first.first == 40 && first.second == 56 && second.first == 42 && second.second == 52,
          "style resolution separates component tags and picks up replacement rules");
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
    for(int frame=0;frame<40;++frame)outer.widget->paint(canvas);
    check(mount.styles()->queryDiagnostics().size()<=32,"repeated invalid layouts remain non-fatal with bounded diagnostics");
}
void maxWidthClamps() {
    for(auto engine:{StackEngine::Yoga,StackEngine::Legacy}) {
        ui::Mount mount({},std::make_shared<ui::StyleRegistry>(),{});
        auto cell=mount.make("Column",{mount.make("Text")},"cell");
        auto shell=mount.make("Row",{cell},"shell");
        shell.as<Stack>()->setEngine(engine);cell.as<Stack>()->setEngine(engine);
        mount.styles()->replace(
            ".shell{container-name:shell;container-type:size;}"
            ".cell{flex-basis:400px;flex-shrink:0;max-width:120px;}"
            "@container shell (width >= 600px){.cell{max-width:200px;}}");
        test_support::RecordingCanvas canvas;
        auto paint=[&](float width){shell.widget->setFrame({0,0,width,40});shell.widget->paint(canvas);};
        paint(599.99f);check(std::abs(cell.widget->frame().width-120)<.01f,"unconditional max-width clamps basis");
        paint(600);check(std::abs(cell.widget->frame().width-200)<.01f,"query max-width clamps basis");
        mount.styles()->applyBaseLayouts();paint(600);
        check(std::abs(cell.widget->frame().width-200)<.01f,"base layout refresh preserves same-size query override");
        mount.styles()->applyBaseLayouts();paint(601);
        check(std::abs(cell.widget->frame().width-200)<.01f,"base layout refresh preserves same-tier query override");
        paint(500);check(std::abs(cell.widget->frame().width-120)<.01f,"query exit restores unconditional max-width");
        check(mount.styles()->queryDiagnostics().empty(),"max-width on a row child has no diagnostics");
    }
    ui::Mount mount({},std::make_shared<ui::StyleRegistry>(),{});
    auto cell=mount.make("Text",{},"cell");
    auto shell=mount.make("Column",{cell},"shell");
    mount.styles()->replace(".shell{container-name:shell;container-type:size;}@container shell (width > 1px){.cell{max-width:50px;}}");
    shell.widget->setFrame({0,0,300,300});test_support::RecordingCanvas canvas;shell.widget->paint(canvas);
    check(!mount.styles()->queryDiagnostics().empty(),"cross-axis max-width diagnosed");
}
}
int main(){try{comparisons();stackChildren();layout(StackEngine::Yoga);layout(StackEngine::Legacy);invalidCssAndDependencies();maxWidthClamps();displayOnlyReplay();refreshAuthoredLayout();projectedSelectorsAndTokens();sharedBaseLayoutSelectors();
    std::cout<<"PASS container queries and Stack: 10 groups\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
