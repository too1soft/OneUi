#include "oneui/ui.h"
#include "support/recording_canvas.h"
#include <iostream>
#include <cmath>
using namespace oneui;
using oneui::test_support::RecordingCanvas;
int main() {
    int failures=0;
    auto check=[&](bool ok,const char* message){ if(!ok){std::cerr<<message<<'\n';++failures;} };
    ui::Builder ui;
    auto count=std::make_shared<Label>(L"42");
    auto root=ui.page({ui.pageHeader(ui.paragraph(L"Long title for a reusable page header", "heading"),ui.text(L"Description","muted"),{ui.button(L"Create",[]{},"primary")}),
        ui.metric(L"Items",ui.native(count),L"rows"),
        ui.emptyState(L"No records",L"Create a record to begin",{ui.button(L"Create record",[]{})})}).as<Stack>();
    auto scroll=ui.scroll(ui.native(root)).as<ScrollView>();
    scroll->setFrame({0,0,400,240}); RecordingCanvas canvas; scroll->paint(canvas);
    auto before=root->layoutStats();
    for(int i=0;i<40;++i) { RecordingCanvas frame; scroll->paint(frame); }
    auto after=root->layoutStats();
    check(after.calculations==before.calculations,"unchanged scroll paint must not alternate measure/arrange calculations");
    check(after.arrangeCacheHits>before.arrangeCacheHits,"arrange cache used");
    count->setText(L"424242424242"); scroll->paint(canvas);
    check(root->layoutStats().calculations>after.calculations,"live metric invalidates layout");
    auto oldHeight=root->frame().height;
    scroll->setFrame({0,0,760,240}); scroll->paint(canvas);
    check(root->frame().height<=oldHeight,"wider page reflows header");
    root->setLayoutCacheEnabled(false); auto disabled=root->layoutStats();
    scroll->paint(canvas); scroll->paint(canvas);
    check(root->layoutStats().calculations>disabled.calculations,"uncached diagnostic path works");
    bool rejected=false;
    try { ui.row({ui.text(L"invalid").basis(NAN)}); } catch(const std::invalid_argument&) {rejected=true;}
    check(rejected,"invalid component constraints rejected");
    int actions=0; auto action=ui.button(L"Run",[&]{++actions;}).as<Button>();
    action->setFrame({0,0,100,40}); action->onMouseDown({{10,10}}); action->onMouseUp({{10,10}});
    check(actions==1,"native button callback retained");
    return failures?1:0;
}
