#include <oneui/ui_declarative_app.h>
#include <oneui/ui_compose.h>
#include <oneui/ui_theme.h>

int main() {
    oneui::ui::DeclarativeApp app(L"我的第一个 OneUI 页面", 760, 540);
    oneui::State<std::wstring> name{L"工作空间"};
    oneui::ui::Compose ui(app.mount());
    auto page = ui.settingsPage({
        ui.field(L"名称", ui.input(name))
            .hint(L"修改输入，下面的文字会跟着变化。"),
        ui.text(name)
    }).title(L"偏好设置");
    oneui::ui::applyTheme(app.mount());
    return app.run(page);
}
