// Opt-in QA entry points share the interaction adapter's validated UTF-8 helper.
#include "../internal/ui_clock.h"
extern "C" {
int oneui_widget_test_freeze_caret(OneUiWidget* root, int enabled) {
#if defined(ONEUI_ENABLE_TEST_FRAME_CAPTURE) && ONEUI_ENABLE_TEST_FRAME_CAPTURE
    if (!root || !root->widget) return 0;
    oneui::internal::freezeCarets = enabled != 0;
    return 1;
#else
    return 0;
#endif
}
int oneui_widget_test_click(OneUiWidget* root, float x, float y) {
#if defined(ONEUI_ENABLE_TEST_FRAME_CAPTURE) && ONEUI_ENABLE_TEST_FRAME_CAPTURE
    try {
        if (!root || !root->widget || !std::isfinite(x) || !std::isfinite(y)) return 0;
        auto widget = root->widget;
        widget->onMouseMove({{x, y}});
        widget->onMouseDown({{x, y}, oneui::MouseButton::Left});
        return widget->onMouseUp({{x, y}, oneui::MouseButton::Left});
    } catch (...) { oneui::capi::recordCurrentException(__func__); }
#endif
    return 0;
}
int oneui_widget_test_wheel(OneUiWidget* root, float x, float y, float delta) {
#if defined(ONEUI_ENABLE_TEST_FRAME_CAPTURE) && ONEUI_ENABLE_TEST_FRAME_CAPTURE
    try {
        if (!root || !root->widget || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(delta)) return 0;
        return root->widget->onMouseWheel({{x, y}, delta});
    } catch (...) { oneui::capi::recordCurrentException(__func__); }
#endif
    return 0;
}
int oneui_widget_test_text_utf8(OneUiWidget* root, OneUiUtf8String text, int replace) {
#if defined(ONEUI_ENABLE_TEST_FRAME_CAPTURE) && ONEUI_ENABLE_TEST_FRAME_CAPTURE
    try {
        if (!root || !root->widget) return 0;
        const auto value = oneui::unicode::fromUtf8(string(text));
        if (replace) root->widget->onKeyDown({oneui::Key::A, false, true});
        if (replace && value.empty()) return root->widget->onKeyDown({oneui::Key::Backspace});
        return root->widget->onTextCommitted(value);
    } catch (...) { oneui::capi::recordCurrentException(__func__); }
#endif
    return 0;
}
}
