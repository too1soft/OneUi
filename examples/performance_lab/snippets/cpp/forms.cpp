#include <oneui/ui_compose.h>

// Retain Mount and the State while the returned tree is displayed.
oneui::ui::Element example_form(oneui::ui::Mount &mount, oneui::State<std::wstring> &name) {
    oneui::ui::Compose ui(mount);
    return ui.settingsPage({ui.formGrid({ui.field(L"名称", ui.input(name))})}).title(L"设置");
}
