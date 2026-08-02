#pragma once

#include "oneui/export.h"
#include "oneui/layout/overlay_host.h"
#include "oneui/reactive.h"
#include "oneui/style.h"
#include "oneui/widget.h"

#include <memory>
#include <optional>

namespace oneui {

enum class PopupPreferredPlacement {
    BottomStart,
    BottomEnd,
    TopStart,
    TopEnd,
    LeftStart,
    RightStart
};

enum class PopupOutsidePointerPolicy {
    PassThrough,
    Close,
    Block
};

enum class PopupInteractionMode {
    Modeless,
    LightDismiss,
    Modal
};

struct PopupPlacementRequest {
    Rect anchor;
    Size preferredSize;
    Rect viewport;
    PopupPreferredPlacement preferredPlacement = PopupPreferredPlacement::BottomStart;
    float offset = 0.0f;
};

struct PopupPlacementResult {
    Rect rect;
    PopupPreferredPlacement placement = PopupPreferredPlacement::BottomStart;
    bool flipped = false;
};

class ONEUI_API PopupPlacement {
public:
    static PopupPlacementResult resolve(const PopupPlacementRequest& request);
};

class ONEUI_API Popup final : public Widget {
public:
    Popup();

    void setAnchor(std::shared_ptr<Widget> anchor);
    std::shared_ptr<Widget> anchor() const;
    void setContent(std::shared_ptr<Widget> content);
    std::shared_ptr<Widget> content() const;

    void setOpen(bool open);
    bool isOpen() const;
    void bindOpen(State<bool>& state);
    void setPreferredPlacement(PopupPreferredPlacement placement);
    PopupPreferredPlacement preferredPlacement() const;
    void setViewport(std::optional<Rect> viewport);
    void clearViewport();
    void setAnchorRect(std::optional<Rect> rect);
    void clearAnchorRect();
    void setCloseOnOutsideClick(bool close);
    void setInteractionMode(PopupInteractionMode mode);
    PopupInteractionMode interactionMode() const;
    OverlayOptions overlayOptions(int layer = 0) const;
    void setOutsidePointerPolicy(PopupOutsidePointerPolicy policy);
    PopupOutsidePointerPolicy outsidePointerPolicy() const;
    void setCloseOnEscape(bool close);
    void setStyleOverride(PopupStyleOverride style);
    void clearStyleOverride();
    PopupStyle resolvedStyle() const;
    Rect resolvedContentRect() const;

    void setInvalidator(std::function<void()> invalidator) override;
    void paint(Canvas& canvas) override;
    bool onMouseMove(const MouseEvent& event) override;
    bool onMouseDown(const MouseEvent& event) override;
    bool onMouseUp(const MouseEvent& event) override;
    bool onMouseWheel(const MouseWheelEvent& event) override;
    bool onKeyDown(const KeyEvent& event) override;
    bool onTextInput(wchar_t character) override;
    bool onTextInputText(const std::wstring& text) override;
    bool onFocusChanged(bool focused) override;
    bool isFocusable() const override;
    bool hitTest(Point point) const override;
    bool paintsAboveSiblings() const override;
    void setFocusVisible(bool visible) override;

private:
    Rect resolvedAnchorRect() const;
    Rect resolvedViewport() const;
    void layoutAnchor();
    void layoutContent();
    void focusChild(Widget* child, bool focusVisible = false);
    Widget* focusedChild() const;
    static bool isInteractive(const Widget* child);
    void resetInteractionState() override;

    std::shared_ptr<Widget> anchor_;
    std::shared_ptr<Widget> content_;
    bool open_ = false;
    Binding<bool> openBinding_;
    PopupPreferredPlacement preferredPlacement_ = PopupPreferredPlacement::BottomStart;
    std::optional<Rect> viewport_;
    std::optional<Rect> anchorRect_;
    std::optional<PopupStyleOverride> styleOverride_;
    PopupInteractionMode interactionMode_ = PopupInteractionMode::LightDismiss;
    PopupOutsidePointerPolicy outsidePointerPolicy_ = PopupOutsidePointerPolicy::Close;
    bool closeOnEscape_ = true;
    Widget* focusedChild_ = nullptr;
    Widget* pressedChild_ = nullptr;
};

} // namespace oneui
