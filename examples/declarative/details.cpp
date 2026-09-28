#include <oneui/ui_declarative_app.h>
#include <oneui/ui_theme.h>

int main() {
    using namespace oneui;
    using namespace oneui::ui;
    DeclarativeApp app(L"OneUI · 详情页布局", 960, 760);
    State<std::wstring> name{L"上海研发节点"}, host{L"127.0.0.1"};
    State<std::wstring> note{L"本地模拟配置，不发起网络连接。缩窄窗口可以看到字段自动换行。"};
    VmCommand changeTheme;
    bool dark=false;
    auto& ui=app.mount();

    // Reuse the same field layout for text, inputs, switches and selects.
    auto field=[&](const wchar_t* label,auto& value) {
        auto text=ui.make("Text");ui.bind(text,"text",value);
        auto row=ui.make("FormRow",{text});ui.set(row,"label",label);
        return row;
    };
    auto grid=ui.make("FormGrid",{field(L"连接名称",name),field(L"主机地址",host)});
    auto section=ui.make("Section",{grid,field(L"备注",note)});
    ui.set(section,"title",L"连接信息");
    auto theme=ui.make("Button");ui.set(theme,"text",L"切换浅色 / 深色");
    changeTheme.setAction([&]{dark=!dark;applyTheme(ui,dark);});
    ui.click(theme,changeTheme);
    auto body=ui.make("DetailPage",{section,ui.make("ActionBar",{theme})});
    ui.set(body,"title",L"连接详情");
    auto page=ui.make("Page",{body});
    applyTheme(ui);
    return app.run(page);
}
