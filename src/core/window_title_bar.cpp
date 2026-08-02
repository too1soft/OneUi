#include "oneui/controls/window_title_bar.h"

#include "oneui/icon.h"

#include <algorithm>
#include <chrono>
#include <utility>

namespace oneui {
namespace {

StyleSheet defaultTitleBarSheet() {
    StyleSheet sheet;
    std::string error;
    // 默认标题栏采用 OneUI 设计语言里的浅色中性 token（见 docs/03-style.md）：
    // surface/panel 白、text #202124、border #d8dbe0、primary #2563eb。
    // 之前默认写死深色 #111114，与框架自身的浅色 token 不一致，会和浅色应用割裂。
    sheet.addRulesFromCss(R"css(
        .titlebar { background: #ffffff; border-color: #d8dbe0; border-width: 1px; color: #202124; }
        .titlebar-icon { background: #2563eb; border-radius: 6px; color: #ffffff; border-width: 1.6px; }
        .window-button { background: #ffffff; border-color: #ffffff; border-width: 1px; border-radius: 6px; color: #666a70; transition: all 120ms ease-out; }
        .window-button:hover { background: #eef0f3; border-color: #eef0f3; color: #202124; }
        .window-button:active { background: #e2e5ea; border-color: #e2e5ea; }
        .window-button.close:hover { background: #dc2626; border-color: #dc2626; color: #ffffff; }

        .titlebar.chrome-dark { background: rgba(0,0,0,0); border-width: 0px; color: #e6ecf7; }
        .titlebar-icon.chrome-dark { background: rgba(0,0,0,0); color: #4a9bff; border-width: 2px; }
        .window-button.chrome-dark { background: rgba(0,0,0,0); border-color: rgba(0,0,0,0); border-width: 1px; border-radius: 6px; color: #9aa7c2; transition: all 120ms ease-out; }
        .window-button.chrome-dark:hover { background: #1b2740; border-color: #1b2740; color: #ffffff; }
        .window-button.chrome-dark:active { background: #16203a; border-color: #16203a; }
        .window-button.chrome-dark.close:hover { background: #dc2626; border-color: #dc2626; color: #ffffff; }

        /* Compact dark product chrome. This variant gives desktop tools a
           distinct title band without requiring an app-specific title bar. */
        .titlebar.chrome-product { background: #2e2e3d; border-color: #393947; border-width: 1px; color: #b9bbcc; }
        .titlebar-icon.chrome-product { background: #050507; border-color: #050507; border-width: 1px; border-radius: 4px; color: #f6f7fb; }
        .window-button.chrome-product { background: rgba(0,0,0,0); border-color: rgba(0,0,0,0); border-width: 1px; border-radius: 6px; color: #a8a9b8; transition: all 120ms ease-out; }
        .window-button.chrome-product:hover { background: #3b3b4c; border-color: #3b3b4c; color: #ffffff; }
        .window-button.chrome-product:active { background: #343443; border-color: #343443; }
        .window-button.chrome-product.close:hover { background: #dc2626; border-color: #dc2626; color: #ffffff; }

        .titlebar.chrome-flat { background: rgba(0,0,0,0); border-width: 0px; color: #202124; }
        .window-button.chrome-flat { background: rgba(0,0,0,0); border-color: rgba(0,0,0,0); border-width: 1px; border-radius: 6px; color: #666a70; transition: all 120ms ease-out; }
        .window-button.chrome-flat:hover { background: #e9ebef; border-color: #e9ebef; color: #202124; }
        .window-button.chrome-flat:active { background: #dfe2e7; border-color: #dfe2e7; }
        .window-button.chrome-flat.close:hover { background: #dc2626; border-color: #dc2626; color: #ffffff; }
    )css", &error);
    return sheet;
}

const StyleSheet& fallbackSheet() {
    static const StyleSheet sheet = defaultTitleBarSheet();
    return sheet;
}

Rect offset(Rect rect, Point point) {
    rect.x += point.x;
    rect.y += point.y;
    return rect;
}

ProductWindowChromeLayout offsetChrome(ProductWindowChromeLayout layout, Point point) {
    layout.frame = offset(layout.frame, point);
    layout.titleBar = offset(layout.titleBar, point);
    layout.caption = offset(layout.caption, point);
    layout.minimizeButton = offset(layout.minimizeButton, point);
    layout.maximizeButton = offset(layout.maximizeButton, point);
    layout.closeButton = offset(layout.closeButton, point);
    layout.content = offset(layout.content, point);
    return layout;
}

std::size_t buttonIndex(TitleBarButtonId id) {
    switch (id) {
    case TitleBarButtonId::Minimize:
        return 0;
    case TitleBarButtonId::Maximize:
        return 1;
    case TitleBarButtonId::Close:
    case TitleBarButtonId::None:
        return 2;
    }
    return 2;
}

Color styleColorOr(std::optional<Color> color, Color fallback) {
    return color.value_or(fallback);
}

double currentTimeMs() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration<double, std::milli>(now).count();
}

TransitionSpec transitionSpecFrom(const StyleBox& box) {
    TransitionSpec spec;
    if (box.transitionDurationMs) {
        spec.durationMs = *box.transitionDurationMs;
    }
    if (box.transitionEasing) {
        spec.easing = *box.transitionEasing;
    }
    return spec;
}

} // namespace

WindowTitleBar::WindowTitleBar(std::wstring title)
    : title_(std::move(title)) {
    setPreferredSize(Size{0.0f, 34.0f});
    setAccessibleRole(AccessibilityRole::Window);
}

void WindowTitleBar::setTitle(std::wstring title) {
    title_ = std::move(title);
    invalidate();
}

void WindowTitleBar::setIconSymbol(IconSymbol symbol) {
    if (iconSymbol_ == symbol) {
        return;
    }
    iconSymbol_ = symbol;
    invalidate();
}

void WindowTitleBar::setMaximized(bool maximized) {
    if (maximized_ == maximized) {
        return;
    }
    const auto previous = titleBarLayout();
    maximized_ = maximized;
    beginButtonTransitions(previous, titleBarLayout());
    invalidate();
}

void WindowTitleBar::setVariant(std::string variant) {
    if (variant_ == variant) {
        return;
    }
    const auto previous = titleBarLayout();
    variant_ = std::move(variant);
    beginButtonTransitions(previous, titleBarLayout());
    invalidate();
}

void WindowTitleBar::setStyleSheet(std::shared_ptr<StyleSheet> sheet) {
    const auto previous = titleBarLayout();
    styleSheet_ = std::move(sheet);
    beginButtonTransitions(previous, titleBarLayout());
    invalidate();
}

void WindowTitleBar::setOnMinimize(std::function<void()> callback) {
    onMinimize_ = std::move(callback);
}

void WindowTitleBar::setOnMaximize(std::function<void()> callback) {
    onMaximize_ = std::move(callback);
}

void WindowTitleBar::setOnClose(std::function<void()> callback) {
    onClose_ = std::move(callback);
}

CursorKind WindowTitleBar::cursor(Point point) const {
    if (!disabled() && hitTestTitleBarButton(titleBarLayout(), point) != TitleBarButtonId::None) {
        return CursorKind::Pointer;
    }
    return Widget::cursor(point);
}

ProductWindowChromeLayout WindowTitleBar::chromeLayout() const {
    const Rect bounds = frame();
    // 标题栏高度以控件实际 frame 为准：默认 metrics 是 34，若外部把标题栏设成
    // 44 等高度，仍按 34 排版会让 logo/标题/窗口按钮整体偏上、不居中。
    ProductShellMetrics metrics;
    metrics.windowTitleBarHeight = bounds.height;
    return offsetChrome(computeProductWindowChromeLayout(Size{bounds.width, bounds.height}, metrics), Point{bounds.x, bounds.y});
}

TitleBarBridgeLayout WindowTitleBar::titleBarLayout() const {
    const StyleSheet& sheet = styleSheet_ ? *styleSheet_ : fallbackSheet();
    TitleBarBridgeConfig config;
    config.chrome = chromeLayout();
    config.maximized = maximized_;
    config.hoveredButton = hoveredButton_;
    config.pressedButton = pressedButton_;
    std::vector<std::string> titleBarClasses{"titlebar"};
    std::vector<std::string> logoClasses{"titlebar-icon"};
    std::vector<std::string> buttonClasses{"window-button"};
    std::vector<std::string> closeClasses{"window-button", "close"};
    if (!variant_.empty()) {
        // 追加统一变体标记类（如 chrome-dark），各子节点共用，供样式表按 .<node>.chrome-<variant> 换肤。
        // 用单连字符、单独标记类，避免 `--` 触发 CSS 解析器的自定义属性语义、以及等特异性平局。
        const std::string mark = "chrome-" + variant_;
        titleBarClasses.push_back(mark);
        logoClasses.push_back(mark);
        buttonClasses.push_back(mark);
        closeClasses.push_back(mark);
    }
    config.titleBarNode = StyleNode{"titlebar", titleBarClasses, StyleStateNone};
    config.logoNode = StyleNode{"icon", logoClasses, StyleStateNone};
    config.buttonNode = StyleNode{"button", buttonClasses, StyleStateNone};
    config.closeButtonNode = StyleNode{"button", closeClasses, StyleStateNone};
    return computeTitleBarBridgeLayout(sheet, config);
}

void WindowTitleBar::paint(Canvas& canvas) {
    const auto layout = titleBarLayout();
    paintStyleBox(canvas, frame(), layout.titleBarStyle);
    paintStyleBox(canvas, layout.logo, layout.logoStyle);

    const Color logoColor = layout.logoStyle.foreground.value_or(Color{17, 17, 20});
    const Color logoAccent = layout.logoStyle.background.color.value_or(Color{123, 212, 198});
    paintIcon(canvas, iconSymbol_, layout.logoIcon, logoColor, logoAccent, layout.logoStyle.borderWidth.value_or(1.5f));
    const Color titleColor = layout.titleBarStyle.foreground.value_or(Color{32, 33, 36});
    canvas.drawText(title_, layout.title, titleColor, 12.0f, TextAlign::Left);

    for (const auto& button : layout.buttons) {
        const StyleBox style = visualButtonStyle(button.id, button.style);
        paintStyleBox(canvas, button.visual, style);
        const Color iconColor = style.foreground.value_or(button.iconColor);
        paintIcon(canvas, button.symbol, button.icon, iconColor, Color{0, 0, 0, 0}, 1.5f);
    }
}

bool WindowTitleBar::onMouseMove(const MouseEvent& event) {
    const auto next = hitTestTitleBarButton(titleBarLayout(), event.position);
    if (next == hoveredButton_) {
        return false;
    }
    const auto previous = titleBarLayout();
    hoveredButton_ = next;
    beginButtonTransitions(previous, titleBarLayout());
    invalidate();
    return true;
}

bool WindowTitleBar::onMouseDown(const MouseEvent& event) {
    const auto button = hitTestTitleBarButton(titleBarLayout(), event.position);
    if (button == TitleBarButtonId::None) {
        return false;
    }
    const auto previous = titleBarLayout();
    pressedButton_ = button;
    beginButtonTransitions(previous, titleBarLayout());
    invalidate();
    return true;
}

bool WindowTitleBar::onMouseUp(const MouseEvent& event) {
    if (pressedButton_ == TitleBarButtonId::None) {
        return false;
    }
    const auto previousLayout = titleBarLayout();
    const auto pressed = pressedButton_;
    const auto released = hitTestTitleBarButton(titleBarLayout(), event.position);
    pressedButton_ = TitleBarButtonId::None;
    beginButtonTransitions(previousLayout, titleBarLayout());
    invalidate();
    if (pressed != released) {
        return true;
    }
    if (pressed == TitleBarButtonId::Minimize && onMinimize_) {
        onMinimize_();
    } else if (pressed == TitleBarButtonId::Maximize && onMaximize_) {
        onMaximize_();
    } else if (pressed == TitleBarButtonId::Close && onClose_) {
        onClose_();
    }
    return true;
}

bool WindowTitleBar::tickAnimations(double nowMs) {
    bool running = false;
    for (std::size_t index = 0; index < buttonBackgroundTransitions_.size(); ++index) {
        running = buttonBackgroundTransitions_[index].tick(nowMs) || running;
        running = buttonForegroundTransitions_[index].tick(nowMs) || running;
        running = buttonBorderTransitions_[index].tick(nowMs) || running;
    }
    if (running) {
        invalidate();
    }
    return running;
}

StyleBox WindowTitleBar::visualButtonStyle(TitleBarButtonId id, StyleBox target) const {
    const std::size_t index = buttonIndex(id);
    if (!buttonVisualInitialized_[index]) {
        return target;
    }

    target.background.color = buttonBackgroundTransitions_[index].value();
    target.background.gradientStart.reset();
    target.background.gradientEnd.reset();
    target.foreground = buttonForegroundTransitions_[index].value();
    target.borderColor = buttonBorderTransitions_[index].value();
    return target;
}

void WindowTitleBar::beginButtonTransitions(const TitleBarBridgeLayout& from, const TitleBarBridgeLayout& target) {
    if (!hasAnimationScheduler()) {
        buttonVisualInitialized_ = {false, false, false};
        return;
    }

    bool anyRunning = false;
    for (const auto& targetButton : target.buttons) {
        const std::size_t index = buttonIndex(targetButton.id);
        const auto found = std::find_if(from.buttons.begin(), from.buttons.end(), [&](const TitleBarBridgeButton& button) {
            return button.id == targetButton.id;
        });
        const StyleBox& fromStyle = found == from.buttons.end() ? targetButton.style : found->style;
        const Color fromBackground = styleColorOr(fromStyle.background.color, Color{0, 0, 0, 0});
        const Color fromForeground = styleColorOr(fromStyle.foreground, targetButton.iconColor);
        const Color fromBorder = styleColorOr(fromStyle.borderColor, Color{0, 0, 0, 0});
        const Color targetBackground = styleColorOr(targetButton.style.background.color, Color{0, 0, 0, 0});
        const Color targetForeground = styleColorOr(targetButton.style.foreground, targetButton.iconColor);
        const Color targetBorder = styleColorOr(targetButton.style.borderColor, Color{0, 0, 0, 0});

        if (!buttonVisualInitialized_[index]) {
            buttonBackgroundTransitions_[index].reset(fromBackground);
            buttonForegroundTransitions_[index].reset(fromForeground);
            buttonBorderTransitions_[index].reset(fromBorder);
            buttonVisualInitialized_[index] = true;
        }

        const TransitionSpec spec = transitionSpecFrom(targetButton.style);
        const double nowMs = currentTimeMs();
        buttonBackgroundTransitions_[index].animateTo(targetBackground, nowMs, spec);
        buttonForegroundTransitions_[index].animateTo(targetForeground, nowMs, spec);
        buttonBorderTransitions_[index].animateTo(targetBorder, nowMs, spec);
        anyRunning = buttonBackgroundTransitions_[index].running()
            || buttonForegroundTransitions_[index].running()
            || buttonBorderTransitions_[index].running()
            || anyRunning;
    }
    if (anyRunning) {
        requestAnimationFrame();
    }
}

bool WindowTitleBar::hasInteractionState() const {
    return hoveredButton_ != TitleBarButtonId::None || pressedButton_ != TitleBarButtonId::None;
}

void WindowTitleBar::resetInteractionState() {
    hoveredButton_ = TitleBarButtonId::None;
    pressedButton_ = TitleBarButtonId::None;
    const auto target = titleBarLayout();
    for (const auto& button : target.buttons) {
        const std::size_t index = buttonIndex(button.id);
        buttonBackgroundTransitions_[index].reset(styleColorOr(button.style.background.color, Color{0, 0, 0, 0}));
        buttonForegroundTransitions_[index].reset(styleColorOr(button.style.foreground, button.iconColor));
        buttonBorderTransitions_[index].reset(styleColorOr(button.style.borderColor, Color{0, 0, 0, 0}));
        buttonVisualInitialized_[index] = true;
    }
}

} // namespace oneui
