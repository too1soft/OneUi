#include <oneui/ui_declarative_app.h>
#include <oneui/ui_compose.h>
#include <oneui/ui_theme.h>

int main() {
    using namespace oneui;
    using namespace oneui::ui;
    DeclarativeApp app(L"OneUI · 详情页布局", 960, 760);
    State<std::wstring> name{L"上海研发节点"}, host{L"127.0.0.1"};
    State<std::wstring> note{L"本地模拟配置，不发起网络连接。缩窄窗口可以看到字段自动换行。"};
    VmCommand changeTheme;
    bool dark=false;
    Compose ui(app.mount());
    changeTheme.setAction([&]{dark=!dark;applyTheme(app.mount(),dark);});
    auto body=ui.detailPage({
        ui.section({
            ui.formGrid({ui.field(L"连接名称",ui.text(name)),ui.field(L"主机地址",ui.text(host))}),
            ui.field(L"备注",ui.text(note))
        }).title(L"连接信息"),
        ui.actions({ui.button(L"切换浅色 / 深色",changeTheme)})
    }).title(L"连接详情");
    applyTheme(app.mount());
    return app.run(ui.page({body}));
}
