#pragma once
#include "connection_editor.hpp"

namespace connection_demo {
struct Samples {
    State<int> scene{0}, protocol{0};
    State<int> workspaceMode{0},visualPreset{0},appearanceIndex{2};
    State<bool> detailOpen{false};
    State<std::wstring> savedName{L"上海研发中心 · 主连接"};
    State<bool> savedEnabled{true};
    State<std::vector<std::wstring>> visualPresets{{L"标准外观",L"柔和外观"}},appearances{{L"平面",L"描边",L"浮起",L"柔和强调"}};
    State<int> strength{1}, timing{1}, motion{0}, gradient{0};
    State<bool> expanded{true}, reducedMotion{false};
    State<std::vector<std::wstring>> strengths{{L"轻 · 6px",L"中 · 12px",L"深 · 20px"}}, timings{{L"快速 · 120ms",L"标准 · 220ms",L"从容 · 400ms"}}, motions{{L"展开 / 收起",L"淡入 / 淡出"}}, gradients{{L"线性 · 三色",L"径向 · 三色"}};
    State<bool> enabled{true}, attempted{false}, loading{false};
    State<std::wstring> name{L"上海研发中心 · 主连接"}, address{L"127.0.0.1"}, note{L"长中文会随可用宽度换行，输入内容在切换主题和密度后保留。"};
    State<std::wstring> query, selected, message{L"编辑示例字段，然后点“检查表单”。"}, tone{L"neutral"};
    State<std::vector<std::wstring>> scenes{{L"表单与操作",L"表格与选择",L"反馈与空状态",L"材质与动效",L"完整页面组合"}};
    State<std::vector<TableColumn>> columns{{{L"模拟连接",0},{L"位置",0},{L"状态",72}}};
    const std::vector<TableRow> records=[] {
        std::vector<TableRow> seed;seed.reserve(1000);
        for(int i=1;i<=1000;++i)seed.push_back({std::to_wstring(i),{L"研发节点 "+std::to_wstring(i),i%2?L"上海 · 研发中心":L"杭州 · 测试中心",i%3?L"在线":L"离线"}});
        return seed;
    }();
    std::function<void(bool)> navigatePane;
    VmCommand showWorkspaceList,showWorkspaceSettings,openDetail,closeDetail,applySettings;
    Computed<bool> workspaceVisible{[this]{return scene.get()==4;},scene};
    Computed<bool> workspaceList{[this]{return workspaceMode.get()==0;},workspaceMode};
    Computed<bool> workspaceSettings{[this]{return workspaceMode.get()==1;},workspaceMode};
    Computed<std::wstring> listVariant{[this]{return std::wstring(workspaceMode.get()==0?L"primary":L"normal");},workspaceMode};
    Computed<std::wstring> settingsVariant{[this]{return std::wstring(workspaceMode.get()==1?L"primary":L"normal");},workspaceMode};
    Computed<std::wstring> surfaceAppearance{[this]{const wchar_t* values[]={L"flat",L"outlined",L"raised",L"tinted"};return std::wstring(values[std::clamp(appearanceIndex.get(),0,3)]);},appearanceIndex};
    Computed<bool> noSelection{[this]{return selected.get().empty();},selected};
    Computed<bool> hasSelection{[this]{return !selected.get().empty();},selected};
    Computed<std::wstring> nodeName{[this]{return selected.get().empty()?std::wstring{}:L"研发节点 "+selected.get();},selected};
    Computed<std::wstring> saveStatus{[this]{return std::wstring(name.get()!=savedName.get() || enabled.get()!=savedEnabled.get()?L"有未应用的修改":L"本地设置已应用");},name,enabled,savedName,savedEnabled};
    VmCommand back, validate, clear, toggleLoading, toggleExpanded;
    Computed<bool> effectsVisible{[this]{return scene.get()==3;},scene};
    Computed<std::wstring> effectPreset{[this]{return std::wstring(motion.get()==0?L"expand":L"fade");},motion};
    Computed<std::wstring> expandText{[this]{return std::wstring(expanded.get()?L"收起说明":L"展开说明");},expanded};
    std::string effectCss() const {
        const int blur= strength.get()==0?6:strength.get()==1?12:20;
        const int duration=timing.get()==0?120:timing.get()==1?220:400;
        return syntax::css("Section.effect-shadow { box-shadow: 0px 4px "+std::to_string(blur)+"px #00000026; }\n"
          "Input.effect-inset { box-shadow: inset 0px 2px "+std::to_string(blur/2)+"px #00000040; }\n"
          "Section.effect-gradient { background: "+std::string(gradient.get()==0?"linear-gradient(110deg, #d9f5e5 0%, #9ddfd0 48%, #c3e5f5 100%)":"radial-gradient(80% at 25% 30%, #e8f7cf 0%, #9ddfd0 48%, #c3e5f5 100%)")+"; }\n"
          "Reveal.effect-reveal { transition-duration: "+std::to_string(duration)+"ms; }","scope_Gallery");
    }
    Computed<bool> formVisible{[this]{return scene.get()==0;},scene};
    Computed<bool> tableVisible{[this]{return scene.get()==1;},scene};
    Computed<bool> statesVisible{[this]{return scene.get()==2;},scene};
    Computed<std::wstring> error{[this]{return attempted.get() && name.get().find_first_not_of(L" \t")==std::wstring::npos?std::wstring(L"请填写连接名称，再检查表单。"):std::wstring{};},attempted,name};
    Computed<std::vector<TableRow>> rows{[this]{std::vector<TableRow> result;for(auto& r:records)if(query.get().empty() || r.cells[0].find(query.get())!=std::wstring::npos)result.push_back(r);return result;},query};
    Computed<bool> empty{[this]{return rows.get().empty();},rows};
    Computed<bool> hasRows{[this]{return !rows.get().empty();},rows};
    Computed<std::wstring> count{[this]{return L"显示 "+std::to_wstring(rows.get().size())+L" / 1,000 条模拟数据";},rows};
    Computed<std::wstring> selection{[this]{return selected.get().empty()?std::wstring(L"尚未选择 · 支持方向键和滚轮"):L"已选择 ID："+selected.get();},selected};
    Computed<std::wstring> loadingText{[this]{return std::wstring(loading.get()?L"完成模拟加载":L"开始模拟加载");},loading};
    Samples() {
        showWorkspaceList.setAction([this]{workspaceMode.set(0);});
        showWorkspaceSettings.setAction([this]{workspaceMode.set(1);});
        openDetail.setAction([this]{if(!selected.get().empty()){detailOpen.set(true);if(navigatePane)navigatePane(true);}});
        closeDetail.setAction([this]{detailOpen.set(false);if(navigatePane)navigatePane(false);});
        applySettings.setAction([this]{validate.execute();if(error.get().empty()){savedName.set(name.get());savedEnabled.set(enabled.get());}});
        toggleExpanded.setAction([this]{expanded.set(!expanded.get());});
        validate.setAction([this]{attempted.set(true);const bool valid=name.get().find_first_not_of(L" \t")!=std::wstring::npos;tone.set(valid?L"success":L"error");message.set(valid?L"检查通过 · 示例不会保存或访问网络。":L"名称尚未填写，请修正标出的字段。");});
        clear.setAction([this]{query.set({});});
        toggleLoading.setAction([this]{loading.set(!loading.get());});
    }
};
struct GalleryModel { VM& settings; Samples& sample; };
}
#include "Gallery.g.h"
#include "Recipes.g.h"
#include "recipe_code.hpp"
namespace connection_demo {
inline Element buildGallery(Mount& ui,VM& settings,Samples& samples,bool codeRecipes=false) {
    GalleryModel model{settings,samples};auto root=build_Gallery(model,ui);
    auto built=std::make_shared<bool>(false);
    ui.watch(samples.scene,[&ui,&settings,&samples,built,codeRecipes](int scene){
        if(scene!=4 || *built)return;*built=true;GalleryModel model{settings,samples};
        auto page=codeRecipes?buildRecipeCode(ui,settings,samples):build_Recipes(model,ui);
        std::weak_ptr<AdaptivePanes> panes=std::dynamic_pointer_cast<AdaptivePanes>(ui.find("recipePanes"));
        samples.navigatePane=[panes](bool detail){if(auto live=panes.lock())live->activatePane(detail);};
        auto host=std::dynamic_pointer_cast<Stack>(ui.find("workspaceHost"));host->add(page.widget);host->setFlex(page.widget,page.flex);
    });
    return root;
}
}
