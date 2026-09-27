#include <oneui/ui_declarative_app.h>
#include <oneui/ui_theme.h>

int main() {
    oneui::ui::DeclarativeApp app(L"我的第一个 OneUI 页面", 760, 540);
    oneui::State<std::wstring> name{L"工作空间"};
    auto& ui = app.mount();

    auto input = ui.make("Input");
    ui.model(input, name); // 输入与状态双向同步。
    auto row = ui.make("FormRow", {input});
    ui.set(row, "label", L"名称");
    ui.set(row, "hint", L"修改输入，下面的文字会跟着变化。");
    auto preview = ui.make("Text");
    ui.bind(preview, "text", name);
    auto page = ui.make("SettingsPage", {row, preview});
    ui.set(page, "title", L"偏好设置");
    oneui::ui::applyTheme(ui);
    return app.run(page);
}
