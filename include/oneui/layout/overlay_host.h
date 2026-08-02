#pragma once

#include "oneui/view.h"

#include <memory>
#include <vector>

namespace oneui {

struct OverlayEntry {
    std::shared_ptr<Widget> child;
    int layer = 0;
    bool trapsFocus = false;
    bool blocksOutsidePointer = false;
    Color backdrop{0, 0, 0, 0};
    Size size{};
    Insets margin{};
    int horizontalAlignment = 2;
    int verticalAlignment = 0;
    bool anchored = false;
};

struct OverlayOptions {
    int layer = 0;
    bool trapsFocus = false;
    bool blocksOutsidePointer = false;
    Color backdrop{0, 0, 0, 0};

    static OverlayOptions modeless(int layer = 0) {
        return OverlayOptions{layer, false, false};
    }

    static OverlayOptions modal(int layer = 0) {
        // A modal surface always dims the inactive context. The color lives in
        // OneUI rather than product code so every native dialog behaves alike.
        return OverlayOptions{layer, true, true, Color{0, 0, 0, 104}};
    }
};

class ONEUI_API OverlayHost : public View {
public:
    void setContent(std::shared_ptr<Widget> child);
    void addOverlay(std::shared_ptr<Widget> child, int layer = 0);
    void addOverlay(std::shared_ptr<Widget> child, OverlayOptions options);
    void addAnchoredOverlay(
        std::shared_ptr<Widget> child,
        OverlayOptions options,
        Size size,
        Insets margin,
        int horizontalAlignment,
        int verticalAlignment);
    bool updateAnchoredOverlay(
        const Widget* child,
        Size size,
        Insets margin,
        int horizontalAlignment,
        int verticalAlignment);
    bool removeOverlay(const Widget* child);
    void clearOverlays();
    const std::vector<OverlayEntry>& overlays() const;
    void setInvalidator(std::function<void()> invalidator) override;
    void setRectInvalidator(std::function<void(Rect)> invalidator) override;
    void setAnimationScheduler(std::function<void()> scheduler) override;

    void paint(Canvas& canvas) override;
    bool onMouseMove(const MouseEvent& event) override;
    bool onMouseDown(const MouseEvent& event) override;
    bool onMouseUp(const MouseEvent& event) override;
    bool onMouseWheel(const MouseWheelEvent& event) override;
    bool onKeyDown(const KeyEvent& event) override;
    bool onKeyUp(const KeyEvent& event) override;
    bool onTextInput(wchar_t character) override;
    bool onTextInputText(const std::wstring& text) override;
    bool onFocusChanged(bool focused) override;
    bool isFocusable() const override;
    bool focusFirstLeaf() override;
    bool focusLastLeaf() override;
    bool requestFocus(Widget* descendant, bool focusVisible = true) override;
    CursorKind cursor(Point point) const override;
    void setFocusVisible(bool visible) override;
    bool tickAnimations(double nowMs) override;

protected:
    void resetInteractionState() override;

private:
    struct OverlayFocusRecord {
        Widget* child = nullptr;
        bool focusVisible = false;
    };

    std::vector<std::size_t> paintOrder() const;
    std::vector<std::size_t> hitOrder() const;
    void layoutAnchoredOverlays();
    Widget* focusedOverlay() const;
    void focusOverlay(Widget* child, bool focusVisible = false);
    bool focusNextOverlay(bool reverse, bool focusVisible = true);
    std::vector<Widget*> focusableOverlays() const;
    bool restorePreviousOverlayFocus();
    bool containsOverlay(const Widget* child) const;
    bool hasActiveFocusTrap() const;
    bool isFocusAllowed(const Widget* child) const;
    void clearOverlayReferences(Widget* child, bool restorePreviousFocus = true);
    void installOverlayHostCallbacks(Widget& child);
    static bool isInteractive(const Widget* child);

    std::vector<OverlayEntry> overlays_;
    std::shared_ptr<Widget> content_;
    std::vector<OverlayFocusRecord> overlayFocusHistory_;
    Widget* focusedOverlay_ = nullptr;
    Widget* pressedOverlay_ = nullptr;
    Widget* pressedContent_ = nullptr;
    Widget* previousFocusedChild_ = nullptr;
    bool previousFocusVisible_ = false;
};

} // namespace oneui
