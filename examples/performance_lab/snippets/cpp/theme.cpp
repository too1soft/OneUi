#include <oneui/localization.h>
#include <oneui/ui_native_theme.h>
#include <oneui/ui_theme.h>

void example_theme(oneui::Widget &widget, bool dark) {
    auto sheet = std::make_shared<oneui::StyleSheet>();
    sheet->addRulesFromCss(oneui::ui::declarativeTheme(dark));
    widget.setStyleScope(sheet);
    oneui::ui::applyNativeTheme(widget, *sheet, false);
    oneui::setBuiltinText(oneui::BuiltinText::Accept, L"确认");
    // Restore the OS default with setBuiltinText(BuiltinText::Accept, std::nullopt).
}
