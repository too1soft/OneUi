#include "oneui/ui_theme.h"
#include "oneui/ui_native_theme.h"
#include "oneui/layout/theme_scope.h"
#include "oneui/controls/font_picker.h"
#include "oneui/controls/media_picker.h"
#include "../../src/views/data_menu.h"
#include "oneui/controls/popup.h"
#include "../../src/core/picker_styles.h"
#include "../../src/internal/ui_clock.h"
#include "../support/recording_canvas.h"
#include <iostream>
#include <stdexcept>
#include <thread>
#include <fstream>
#include <filesystem>
using namespace oneui;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
void fonts() {
    StyleSheet partial;
    partial.setCustomProperty("--surface", "#18201b");
    partial.setCustomProperty("--ink", "#e3eae5");
    auto palette = pickerColors(partial);
    check(palette.foreground && palette.foreground->r == 227 &&
          palette.background.color && palette.background.color->r == 24,
          "partial dark palette retains colors without selection/line tokens");
    const auto native = ui::nativePalette(partial);
    check(native.surface.r == 24 && native.ink.r == 227,
          "native palette retains dark colors when optional tokens are absent");
    for (const bool dark : {false, true}) {
        StyleSheet sheet;
        sheet.setCustomProperty("--surface", dark ? "#18201b" : "#ffffff");
        sheet.setCustomProperty("--ink", dark ? "#e3eae5" : "#21302b");
        sheet.setCustomProperty("--selection", dark ? "#35453b" : "#e7eee8");
        Tabs tabs;
        tabs.setItems({L"Active", L"Inactive"});
        tabs.setFrame({0, 0, 300, 34});
        ui::applyNativeTheme(tabs, sheet, false);
        test_support::RecordingCanvas painted;
        tabs.paint(painted);
        check(!painted.fillRects.empty() && painted.fillRects.front().color.r == (dark ? 24 : 255),
              "Tabs strip uses resolved light/dark surface through native theme adapter");
        check(painted.fillRects.size() > 1 && painted.fillRects[1].color.r == (dark ? 53 : 231),
              "Tabs selected item uses light/dark selection token");
    }
    View root;root.setFrame({0,0,900,700});auto picker=std::make_shared<FontPicker>();root.add(picker);picker->setFrame({20,20,300,36});
    FontPickerConfig config;config.selected="missing";
    for(int i=0;i<1000;++i)config.items.push_back({"font"+std::to_string(i),L"Consolas",L"Font "+std::to_wstring(i),i!=0});
    picker->setConfig(config);std::string selected;picker->setOnSelectionRequested([&](auto key){selected=key;});
    root.onMouseDown({{50,35}});root.onMouseUp({{50,35}});check(picker->open(),"button opens picker");
    test_support::RecordingCanvas canvas;picker->paint(canvas);check(picker->paintedRows()<=6&&picker->filteredCount()==1000,"virtual font rows bounded");
    KeyEvent enter;enter.key=Key::Enter;root.onKeyDown(enter);check(selected.empty()&&picker->open(),"unavailable family cannot select");
    KeyEvent down;down.key=Key::Down;root.onKeyDown(down);root.onKeyDown(enter);
    check(selected=="font1"&&picker->config().selected=="missing","selection only requests VM mutation");
    picker->setOpen(true);picker->setQuery(L"Font 999");check(picker->filteredCount()==1,"font filter");
    auto search=std::dynamic_pointer_cast<TextField>(picker->children()[1]);search->setTextComposition(L"字",1);
    config.loading=true;picker->setConfig(config);check(search->hasTextComposition()&&picker->query()==L"Font 999","VM loading update retains search IME");
    search->setTextComposition({},0);KeyEvent escape;escape.key=Key::Escape;root.onKeyDown(escape);check(!picker->open(),"Esc closes without selecting");
    config.loading=false;config.selected="font1";picker->setConfig(config);auto label=picker->accessibilityInfo().value;
    config.items.clear();config.error=L"Font service unavailable";picker->setConfig(config);check(picker->accessibilityInfo().value==label,"missing selected font retains prior label");
    bool rejected=false;try{config.size=-1;picker->setConfig(config);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"invalid preview size rejected");
}
void mediaGeneration() {
    auto preview=std::make_shared<MediaPreview>();auto image=preview->image();
    std::vector<MediaPreview::Completion> completions;std::vector<std::shared_ptr<std::atomic<bool>>> cancels;
    preview->setLoader([&](auto,auto cancel,auto done){cancels.push_back(cancel);completions.push_back(done);});
    int errors=0;preview->setOnLoadFailed([&](auto,auto,auto){++errors;});
    preview->setSource("a.png");preview->setSource("b.png");check(*cancels[0]&&!(*cancels[1]),"source replacement cancels generation");
    auto good=std::make_shared<MediaPixels>();good->width=good->height=1;good->rgba={255,0,0,255};
    completions[0](good);preview->tickAnimations(0);check(!image->hasImage()&&preview->loading(),"late completion cannot replace new source");
    std::thread worker([&]{completions[1](good);});worker.join();check(!image->hasImage(),"worker completion does not touch UI");
    preview->tickAnimations(1);check(image->hasImage()&&!preview->loading()&&preview->image()==image,"UI drain keeps image object");
    preview->setFit("cover");preview->setPosition("bottom-right");preview->setOpacity(.5f);preview->setFrame({0,0,100,100});
    test_support::RecordingCanvas canvas;preview->paint(canvas);check(!canvas.opacityLayers.empty()&&canvas.opacityLayers.back().opacity==.5f,"opacity composes pixels");
    preview->setSource("broken");auto bad=std::make_shared<MediaPixels>();bad->code="format";bad->message="bad image";completions[2](bad);preview->tickAnimations(2);
    check(errors==1&&preview->errorCode()=="format"&&!image->hasImage(),"typed decode failure clears stale pixels");
    preview->setSource("later");preview.reset();check(*cancels.back(),"destruction cancels worker");completions.back()(good);check(errors==1,"late destroyed completion silent");
    MediaPicker picker;std::vector<MediaPickerAction> actions;picker.setOnIntent([&](auto a){actions.push_back(a);});picker.setFrame({0,0,360,240});picker.paint(canvas);
    picker.onMouseDown({{20,220}});picker.onMouseUp({{20,220}});check(actions.size()==1&&actions[0]==MediaPickerAction::Choose,"empty picker choose intent");
    picker.onMouseDown({{150,220}});picker.onMouseUp({{150,220}});check(actions.size()==1,"empty picker cannot clear");
    MediaPickerConfig config;config.loading=true;picker.setConfig(config);picker.onMouseDown({{280,220}});picker.onMouseUp({{280,220}});
    check(actions.back()==MediaPickerAction::Cancel&&picker.config().loading,"cancel request does not preempt VM state");
}
void localDecode() {
    const unsigned char png[]={137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,1,0,0,0,1,8,6,0,0,0,31,21,196,137,0,0,0,13,73,68,65,84,120,156,99,248,207,192,240,31,0,5,0,1,255,137,153,61,29,0,0,0,0,73,69,78,68,174,66,96,130};
    const auto path=std::filesystem::current_path()/"wp4-media-test.png";
    struct Cleanup {std::filesystem::path path;~Cleanup(){std::error_code error;std::filesystem::remove(path,error);}} cleanup{path};
    {std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(png),sizeof(png));}
    MediaPreview decoded;decoded.setSource(path.string());
    for(int i=0;i<1000&&decoded.loading();++i){decoded.tickAnimations(i);std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    std::filesystem::remove(path);
    check(decoded.errorCode().empty()&&decoded.image()->imageWidth()==1&&decoded.image()->imageHeight()==1,"real local PNG asynchronously decoded");
    MediaPreview preview;std::string code;preview.setOnLoadFailed([&](auto,auto c,auto){code=c;});
    preview.setSource("https://example.invalid/picture.png");
    for(int i=0;i<1000&&preview.loading();++i){preview.tickAnimations(i);std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    check(code=="unsupported"&&!preview.loading(),"default loader rejects remote without network");
    code.clear();preview.setSource("build/capture/this-image-does-not-exist.png");
    for(int i=0;i<1000&&preview.loading();++i){preview.tickAnimations(i);std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    check(code=="not-found","local missing resource reports error");
}
void themes() {
    auto styles=std::make_shared<ui::StyleRegistry>();styles->replace(ui::declarativeTheme());ui::Mount mount({},styles,{});
    std::vector<ui::Element> scopes;std::vector<std::shared_ptr<Label>> labels;
    for(int i=0;i<12;++i) {
        auto label=mount.make("Text");mount.set(label,"text",L"Preview");labels.push_back(label.as<Label>());
        auto card=mount.make("Card",{label});auto scope=mount.make("ThemeScope",{card});scopes.push_back(scope);
        scope.as<ThemeScope>()->setTokens({{"surface",int64_t(0x102030ffu+i*0x10100u)},{"ink",int64_t(0xe0e0e0ffu-i*0x1010000u)}});
        scope.widget->setFrame({float(i*100),0,95,90});
    }
    test_support::RecordingCanvas canvas;for(auto& scope:scopes)scope.widget->paint(canvas);
    check(canvas.texts.size()==12&&canvas.texts[0].color.r!=canvas.texts[1].color.r,"12 simultaneous palettes");
    int siblingInvalidations=0;scopes[1].widget->setInvalidator([&]{++siblingInvalidations;});
    auto first=scopes[0].as<ThemeScope>();auto revision=first->revision();first->setTokens({{"ink",0xff0000ff}});
    check(first->revision()==revision+1&&siblingInvalidations==0,"one theme delta does not restyle sibling");
    for(auto tokens:{std::vector<ThemeColorToken>{{"unknown",0}},{{"ink",-1}},{{"ink",0x100000000ll}},{{"ink",1},{"ink",2}}}) {
        bool rejected=false;try{first->setTokens(tokens);}catch(const std::invalid_argument&){rejected=true;}
        check(rejected&&first->tokens()[0].rgba==0xff0000ff,"invalid token batch preserves old theme");
    }
    auto plain=mount.make("Text");mount.set(plain,"text",L"Global");plain.widget->setFrame({0,0,100,30});canvas.texts.clear();plain.widget->paint(canvas);
    check(canvas.texts.back().color.r==28,"global theme unchanged");
    first->setTokens({});canvas.texts.clear();scopes[0].widget->paint(canvas);check(canvas.texts.back().color.r==28,"empty local tokens inherit parent");
}
#include "settings_controls.inc"
#include "font_confirmation.inc"
#include "data_menu_tests.inc"
#include "declarative_popup_tests.inc"
}
int main(){try{fonts();mediaGeneration();localDecode();themes();settingsControls();fontConfirmation();dataMenus();declarativePopups();std::cout<<"pickers, menus and popups: 8 scenario groups passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
