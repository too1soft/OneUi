#include "appearance_page.hpp"

namespace lab::mobile {
class AppearancePage final : public ContentPage {
    oneui::mobile::Appearance &appearance_;
    std::shared_ptr<oneui::Label> result_;

  public:
    explicit AppearancePage(oneui::mobile::Appearance &appearance) : appearance_(appearance) {
        result_ = text(L"读取系统外观…");
        result_->setTextWrapping(true);
        body_->setFlex(result_, {0, 0, 84, 84});
        action(L"跟随系统", [this] { theme(ONEUI_MOBILE_THEME_SYSTEM); });
        action(L"使用浅色", [this] { theme(ONEUI_MOBILE_THEME_LIGHT); });
        action(L"使用深色", [this] { theme(ONEUI_MOBILE_THEME_DARK); });
        text(L"系统栏与标题区", 20);
        auto explanation = text(
            L"顶部包含时间和电量；底部为系统导航。图标明暗根据背景自动选择。标题区在本示例中与顶部"
            L"配色同步。");
        explanation->setTextWrapping(true);
        body_->setFlex(explanation, {0, 0, 100, 100});
        action(L"恢复主题配色", [this] { colors(0, 0); });
        action(L"绿色顶部 · 浅色底部", [this] { colors(0xff28613d, 0xfff5f7f4); });
        action(L"浅色顶部 · 深色底部", [this] { colors(0xfff5f7f4, 0xff171e21); });
        auto notes = text(
            L"系统切换主题后立即更新。旋转、切页、前后台切换保留当前选择；退出进程后恢复默认跟随系"
            L"统。语言切换和系统字号适配仍待接入。");
        notes->setTextWrapping(true);
        body_->setFlex(notes, {0, 0, 120, 120});
        refresh();
    }
    void theme(int value) {
        OneUiMobileAppearanceStateV1 s{};
        appearance_.state(s);
        s.options.theme_mode = value;
        appearance_.setOptions(s.options);
    }
    void colors(uint32_t status, uint32_t navigation) {
        OneUiMobileAppearanceStateV1 s{};
        appearance_.state(s);
        s.options.custom_bar_colors = status != 0;
        s.options.status_argb = status;
        s.options.navigation_argb = navigation;
        appearance_.setOptions(s.options);
    }
    void refresh() {
        OneUiMobileAppearanceStateV1 s{};
        appearance_.state(s);
        const wchar_t *system = s.system_theme == 2   ? L"深色"
                                : s.system_theme == 1 ? L"浅色"
                                                      : L"未知";
        const wchar_t *mode = s.options.theme_mode == 0   ? L"跟随系统"
                              : s.options.theme_mode == 1 ? L"固定浅色"
                                                          : L"固定深色";
        const wchar_t *applied = !s.host_available                  ? L"当前宿主未接入系统栏"
                                 : s.applied_revision == s.revision ? L"系统栏样式已应用"
                                                                    : L"系统栏样式待应用";
        result_->setText(std::wstring(L"系统：") + system + L" · 应用：" +
                         (s.effective_theme == 2 ? L"深色" : L"浅色") + L"\n模式：" + mode + L"\n" +
                         applied);
    }
};
std::unique_ptr<DemoPage> createAppearancePage(oneui::mobile::Appearance &appearance) {
    return std::make_unique<AppearancePage>(appearance);
}
void refreshAppearancePage(DemoPage *page) {
    if (auto *appearance = dynamic_cast<AppearancePage *>(page))
        appearance->refresh();
}
} // namespace lab::mobile
