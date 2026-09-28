#include "component_gallery.hpp"
#include "support/recording_canvas.h"
#include <iostream>
#define CHECK(x) do{if(!(x))throw std::runtime_error("Failed: " #x);}while(0)
using namespace connection_demo;
static void recipeTests(){
    int cases=0;
    for(bool code:{false,true}) {
        UiMailbox mailbox;VM vm(mailbox.sender());Samples samples;Mount ui;
        auto root=buildGallery(ui,vm,samples,code);samples.scene.set(4);ui.flush();
        auto panes=std::dynamic_pointer_cast<AdaptivePanes>(ui.find("recipePanes"));
        auto shell=std::dynamic_pointer_cast<AdaptivePanes>(ui.find("recipeShell"));
        auto table=std::dynamic_pointer_cast<Table>(ui.find("recipeTable"));
        auto input=std::dynamic_pointer_cast<TextField>(ui.find("recipeName"));
        CHECK(panes && shell && table && input);
        const auto subscriptions=ui.diagnostics().subscriptions,styles=ui.styles()->size();
        oneui::test_support::RecordingCanvas canvas;
        for(bool dark:{false,true})for(auto density:{Density::Comfortable,Density::Compact})for(auto preset:{VisualPreset::Standard,VisualPreset::Soft})for(float width:{1320.f,960.f,800.f,640.f,426.f})for(int mode:{0,1,2}) {
            samples.workspaceMode.set(mode==2?1:0);samples.selected.set(L"40");samples.detailOpen.set(mode==1);ui.flush();
            ui.styles()->replace(declarativeTheme(dark,density,preset)+styles_Gallery(),density);
            root.widget->setFrame({0,0,width,1000});root.widget->paint(canvas);
            const auto issues=inspectLayout(root.widget,*ui.styles());
            if(!issues.empty())throw std::runtime_error("recipe code="+std::to_string(code)+" width="+std::to_string(width)+" mode="+std::to_string(mode)+" "+formatLayoutIssues(issues));
            CHECK(shell->narrow()==(shell->frame().width<960));
            if(mode!=2){CHECK(panes->narrow()==(panes->frame().width<800));CHECK(table->selectedIndex()==39);}
            ++cases;
        }
        samples.workspaceMode.set(1);ui.flush();root.widget->setFrame({0,0,1320,1000});root.widget->paint(canvas);
        CHECK(root.as<View>()->requestFocus(input.get()));input->setCaretIndex(2);input->setSelectionRange(1,2);input->setTextComposition(L"中文",1);
        for(int i=0;i<60;++i){ui.styles()->replace(declarativeTheme(i%2,Density::Comfortable,i%3?VisualPreset::Soft:VisualPreset::Standard));root.widget->setFrame({0,0,i%2?640.f:1320.f,1000});root.widget->paint(canvas);CHECK(input->focused());CHECK(input->caretIndex()==2);CHECK(input->hasTextComposition());}
        input->setTextComposition(L"",0);
        samples.workspaceMode.set(0);samples.detailOpen.set(false);ui.flush();root.widget->setFrame({0,0,640,1000});root.widget->paint(canvas);
        auto search=ui.find("recipeSearch");CHECK(root.as<View>()->requestFocus(search.get()));
        samples.openDetail.execute();ui.flush();root.widget->paint(canvas);CHECK(!search->focused());CHECK(!root.as<View>()->requestFocus(search.get()));
        samples.closeDetail.execute();ui.flush();root.widget->paint(canvas);CHECK(root.as<View>()->requestFocus(search.get()));
        root.widget->setFrame({0,0,1320,1000});root.widget->paint(canvas);
        CHECK(root.as<View>()->requestFocus(search.get()));samples.openDetail.execute();ui.flush();root.widget->paint(canvas);CHECK(ui.find("recipeAppearance")->focused());
        samples.closeDetail.execute();ui.flush();root.widget->paint(canvas);CHECK(search->focused());
        table->setScrollOffset(44*20);const auto offset=table->scrollOffset();
        for(int i=0;i<300;++i){samples.detailOpen.set(i%2);samples.workspaceMode.set(i%3==0?1:0);samples.scene.set(i%5?4:0);ui.flush();root.widget->paint(canvas);}
        samples.scene.set(4);samples.workspaceMode.set(0);samples.detailOpen.set(false);ui.flush();root.widget->paint(canvas);
        CHECK(ui.find("recipeTable")==table);CHECK(samples.selected.get()==L"40");CHECK(table->scrollOffset()==offset);CHECK(ui.diagnostics().subscriptions==subscriptions);CHECK(ui.styles()->size()==styles);
        samples.name.set(L"新工作区");ui.flush();CHECK(samples.saveStatus.get()!=L"设置已保存");samples.applySettings.execute();ui.flush();CHECK(samples.savedName.get()==L"新工作区");
    }
    std::cout<<"Recipes: "<<cases<<" code/template, theme, density, preset, width and navigation cases; retained state/focus/composition and 300 navigation cycles passed.\n";
}
int main(){try {recipeTests();
    UiMailbox mailbox;VM vm(mailbox.sender());Samples samples;Mount ui;
    auto root=buildGallery(ui,vm,samples);
    auto input=std::dynamic_pointer_cast<TextField>(ui.find("galleryName"));
    auto table=std::dynamic_pointer_cast<Table>(ui.find("galleryTable"));
    const auto subscriptions=ui.diagnostics().subscriptions,styles=ui.styles()->size();
    int cases=0;
    for(bool dark:{false,true})for(auto density:{Density::Comfortable,Density::Compact})for(float width:{1320.f,640.f,426.f})for(int scene=0;scene<4;++scene) {
        samples.scene.set(scene);ui.flush();ui.styles()->replace(declarativeTheme(dark,density)+styles_Gallery()+samples.effectCss(),density);
        root.widget->setFrame({0,0,width,800});oneui::test_support::RecordingCanvas canvas;root.widget->paint(canvas);
        const auto issues=inspectLayout(root.widget,*ui.styles());if(!issues.empty())throw std::runtime_error("width="+std::to_string(width)+" scene="+std::to_string(scene)+" "+formatLayoutIssues(issues));
        CHECK(table->rowHeight()==tableRowHeight(density));CHECK(input->preferredSize().height==controlHeight(density));++cases;
    }
    samples.scene.set(3);samples.strength.set(2);samples.gradient.set(1);ui.flush();ui.styles()->replace(declarativeTheme()+styles_Gallery()+samples.effectCss());
    root.widget->tickAnimations(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count()+1000);root.widget->setFrame({0,0,1320,1400});oneui::test_support::RecordingCanvas effectCanvas;root.widget->paint(effectCanvas);
    CHECK(!effectCanvas.gradients.empty());CHECK(effectCanvas.gradients.front().gradient.radial);CHECK(!effectCanvas.insetShadows.empty());CHECK(effectCanvas.insetShadows.front().shadow.blurRadius==10);
    // Geometry is computed by Yoga, without callbacks or reconstructing fields.
    Mount gridUi;auto first=gridUi.make("Input"),second=gridUi.make("Input");
    auto a=gridUi.make("FormRow",{first}),b=gridUi.make("FormRow",{second});
    gridUi.set(a,"label",L"第一字段");gridUi.set(b,"label",L"第二字段");
    auto grid=gridUi.make("FormGrid",{a,b});gridUi.styles()->replace(declarativeTheme());
    grid.widget->setFrame({0,0,800,200});oneui::test_support::RecordingCanvas canvas;grid.widget->paint(canvas);
    CHECK(std::abs(first.widget->frame().y-second.widget->frame().y)<1);CHECK(second.widget->frame().x>first.widget->frame().x+300);
    grid.widget->setFrame({0,0,400,300});grid.widget->paint(canvas);CHECK(second.widget->frame().y>first.widget->frame().y+40);CHECK(std::abs(first.widget->frame().width-400)<1);
    bool bad=false;try{gridUi.set(grid,"min-column-width",0);}catch(const std::invalid_argument&){bad=true;}CHECK(bad);
    // Replayed hover events must not erase a keyboard choice in native Select.
    Select select;select.setItems({L"Comfortable",L"Compact"});select.setFrame({0,0,180,40});
    MouseEvent pointer;pointer.position={10,60};select.onMouseMove(pointer);
    KeyEvent key;key.key=Key::Enter;select.onKeyDown(key);
    key.key=Key::Down;select.onKeyDown(key);select.onMouseMove(pointer);
    key.key=Key::Enter;select.onKeyDown(key);CHECK(select.selectedIndex()==1);
    key.key=Key::Enter;select.onKeyDown(key);key.key=Key::Home;select.onKeyDown(key);
    pointer.position={500,500};select.onMouseMove(pointer);key.key=Key::Enter;select.onKeyDown(key);CHECK(select.selectedIndex()==0);
    bad=false;try{gridUi.make("FormGrid",{first});}catch(const std::invalid_argument&){bad=true;}CHECK(bad);
    samples.scene.set(0);ui.flush();root.widget->setFrame({0,0,1000,800});root.widget->paint(canvas);
    root.as<View>()->requestFocus(input.get());input->setCaretIndex(2);input->setSelectionRange(1,2);input->setTextComposition(L"中文",1);
    const auto name=samples.name.get();
    for(int i=0;i<40;++i){auto density=i%2?Density::Compact:Density::Comfortable;ui.styles()->replace(declarativeTheme(i%3==0,density),density);root.widget->paint(canvas);}
    CHECK(input->focused());CHECK(input->caretIndex()==2);CHECK(input->hasSelection());CHECK(input->hasTextComposition());CHECK(samples.name.get()==name);
    CHECK(ui.diagnostics().subscriptions==subscriptions);CHECK(ui.styles()->size()==styles);
    samples.name.set(L"");samples.validate.execute();ui.flush();CHECK(!samples.error.get().empty());CHECK(samples.tone.get()==L"error");
    samples.name.set(L"中文修正");samples.validate.execute();ui.flush();CHECK(samples.error.get().empty());CHECK(samples.tone.get()==L"success");
    samples.scene.set(1);ui.flush();ui.styles()->replace(declarativeTheme());root.widget->paint(canvas);samples.selected.set(L"40");ui.flush();table->setScrollOffset(44*30);
    ui.styles()->replace(declarativeTheme(false,Density::Compact),Density::Compact);CHECK(samples.selected.get()==L"40");CHECK(table->selectedIndex()==39);CHECK(std::abs(table->scrollOffset()-36*30)<1);
    samples.query.set(L"does not exist");ui.flush();CHECK(samples.empty.get());samples.clear.execute();ui.flush();CHECK(samples.rows.get().size()==1000);
    samples.toggleLoading.execute();ui.flush();CHECK(samples.loading.get());samples.toggleLoading.execute();ui.flush();CHECK(!samples.loading.get());
    std::cout<<"Gallery: "<<cases<<" theme/density/width/scenes; responsive geometry, state feedback, focus/composition, stable subscriptions and table anchor passed.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
