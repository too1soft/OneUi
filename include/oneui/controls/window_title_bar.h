#pragma once

#include "oneui/export.h"
#include "oneui/icon.h"
#include "oneui/layout/title_bar_bridge.h"
#include "oneui/style_sheet.h"
#include "oneui/view.h"

#include <functional>
#include <array>
#include <memory>
#include <string>

namespace oneui {

// Explicit local-coordinate presentation. The application owns responsive
// geometry; painting, hit testing and native window actions share these frames.
// A missing presentation preserves the existing default title bar.
struct TitleBarButtonPresentation {
    Rect frame{}, visual{}, icon{};
    CanvasPath glyph, maximizedGlyph; // 16 x 16 coordinates; empty uses the built-in icon.
    float strokeWidth = 1.5f; // Scaled from the 16-unit glyph to icon.width.
    float cornerRadius = -1.0f; // Negative inherits CSS.
    bool ellipse = false;
    bool glyphOnGroupHover = false;
    bool glyphUsesForeground = true;
    Color fill{0,0,0,0}, pressedFill{0,0,0,0}, glyphColor{0,0,0,255};
};
struct TitleBarPresentation {
    Rect logo{}, logoIcon{}, title{}, leading{}, accessory{};
    float logoStrokeWidth = 1.5f, titleFontSize = 12.0f;
    bool hideLogoBorder = false;
    // Fixed semantic order: minimize, maximize/restore, close. Visual order is free.
    std::array<TitleBarButtonPresentation, 3> buttons{};
};

class ONEUI_API WindowTitleBar final : public View {
public:
    explicit WindowTitleBar(std::wstring title = {});

    void setTitle(std::wstring title);
    void setIconSymbol(IconSymbol symbol);
    void setMaximized(bool maximized);
    // setVariant 给标题栏挂一个皮肤变体（如 "dark"）：非空时在各样式节点上追加
    // "chrome-<variant>" 类，
    // 供样式表按类换肤（如登录页深色标题栏）；空则保持默认皮肤。几何独立配置。
    void setVariant(std::string variant);
    // Copies and validates atomically; invalid input leaves the previous layout.
    // nullptr restores default geometry. Call on the UI thread after size changes.
    bool setPresentation(const TitleBarPresentation* presentation);
    bool hitsWindowButton(Point point) const;
    Rect windowButtonFrame(TitleBarButtonId id) const;
    bool activateWindowButton(TitleBarButtonId id);
    bool maximized() const { return maximized_; }
    void setStyleSheet(std::shared_ptr<StyleSheet> sheet);
    void setLeading(std::shared_ptr<Widget> leading);
    void setAccessory(std::shared_ptr<Widget> accessory);
    void setOnMinimize(std::function<void()> callback);
    void setOnMaximize(std::function<void()> callback);
    void setOnClose(std::function<void()> callback);

    void paint(Canvas& canvas) override;
    bool onMouseMove(const MouseEvent& event) override;
    bool onMouseDown(const MouseEvent& event) override;
    bool onMouseUp(const MouseEvent& event) override;
    CursorKind cursor(Point point) const override;
    bool tickAnimations(double nowMs) override;

private:
    void layoutChildren() override;
    ProductWindowChromeLayout chromeLayout() const;
    TitleBarBridgeLayout titleBarLayout() const;
    StyleBox visualButtonStyle(TitleBarButtonId id, StyleBox target) const;
    void beginButtonTransitions(const TitleBarBridgeLayout& from, const TitleBarBridgeLayout& target);
    bool hasInteractionState() const override;
    void resetInteractionState() override;

    std::wstring title_;
    std::string variant_;
    std::optional<TitleBarPresentation> presentation_;
    IconSymbol iconSymbol_ = IconSymbol::BrandBloom;
    std::shared_ptr<StyleSheet> styleSheet_;
    std::shared_ptr<Widget> leading_;
    std::shared_ptr<Widget> accessory_;
    bool maximized_ = false;
    TitleBarButtonId hoveredButton_ = TitleBarButtonId::None;
    TitleBarButtonId pressedButton_ = TitleBarButtonId::None;
    std::array<ColorTransition, 3> buttonBackgroundTransitions_{
        ColorTransition{Color{0, 0, 0, 0}},
        ColorTransition{Color{0, 0, 0, 0}},
        ColorTransition{Color{0, 0, 0, 0}}};
    std::array<ColorTransition, 3> buttonForegroundTransitions_{
        ColorTransition{Color{178, 184, 196}},
        ColorTransition{Color{178, 184, 196}},
        ColorTransition{Color{178, 184, 196}}};
    std::array<ColorTransition, 3> buttonBorderTransitions_{
        ColorTransition{Color{0, 0, 0, 0}},
        ColorTransition{Color{0, 0, 0, 0}},
        ColorTransition{Color{0, 0, 0, 0}}};
    std::array<bool, 3> buttonVisualInitialized_{false, false, false};
    std::function<void()> onMinimize_;
    std::function<void()> onMaximize_;
    std::function<void()> onClose_;
};

} // namespace oneui
