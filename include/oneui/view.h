#pragma once

#include "oneui/widget.h"

#include <memory>
#include <vector>

namespace oneui {

class ONEUI_API View : public Widget {
public:
    ~View() override;

    void add(std::shared_ptr<Widget> child);
    void clearChildren();
    // Reorders retained children without discarding focus/IME in surviving nodes.
    void reconcileChildren(std::vector<std::shared_ptr<Widget>> children);
    // Preparation allocates and attaches only unpublished children. The owner
    // must outlive the plan. Commit never allocates container storage or throws.
    class ONEUI_API ChildrenUpdate {
    public:
        ~ChildrenUpdate();
        void commit() noexcept;
    private:
        friend class View;
        explicit ChildrenUpdate(View& owner) : owner_(owner) {}
        View& owner_;
        std::vector<std::shared_ptr<Widget>> next_, added_, removed_;
        bool committed_ = false;
    };
    std::shared_ptr<ChildrenUpdate> prepareChildren(std::vector<std::shared_ptr<Widget>> children);
    const std::vector<std::shared_ptr<Widget>>& children() const;
    void setInvalidator(std::function<void()> invalidator) override;
    void setRectInvalidator(std::function<void(Rect)> invalidator) override;
    void setAnimationScheduler(std::function<void()> scheduler) override;
    void setTextEnvironment(std::wstring family, float scale) override;
    std::shared_ptr<Widget> activeFocusChild() const override;
    bool hasTextComposition() const override;

    Rect paintBounds() const override;
    void paint(Canvas& canvas) override;
    bool onMouseMove(const MouseEvent& event) override;
    bool onMouseDown(const MouseEvent& event) override;
    bool onMouseUp(const MouseEvent& event) override;
    bool onMouseWheel(const MouseWheelEvent& event) override;
    bool onKeyDown(const KeyEvent& event) override;
    bool onKeyUp(const KeyEvent& event) override;
    bool onTextInput(wchar_t character) override;
    bool onTextInputText(const std::wstring& text) override;
    bool onTextCommitted(const std::wstring& text) override;
    TextInputState textInputState() const override;
    void setTextComposition(std::wstring text, std::size_t caret) override;
    bool replaceTextRange(std::size_t start, std::size_t end, const std::wstring& text) override;
    Rect textInputCaretRect() const override;
    bool onFocusChanged(bool focused) override;
    bool isFocusable() const override;
    bool focusFirstLeaf() override;
    bool focusLastLeaf() override;
    bool hitTest(Point point) const override;
    bool paintsAboveSiblings() const override;
    // Focuses a visible, enabled descendant and establishes the complete
    // parent focus chain needed for keyboard and text-input routing.
    virtual bool requestFocus(Widget* descendant, bool focusVisible = true);
    CursorKind cursor(Point point) const override;
    const std::wstring* tooltipAt(Point point) const override;
    void setFocusVisible(bool visible) override;
    bool tickAnimations(double nowMs) override;

protected:
    void styleScopeChanged() override;
    virtual void layoutChildren();
    Widget* focusedChild() const;
    // Descendant currently owns a pointer gesture. Container hover affordances
    // must not intercept its moves while crossing their hit regions.
    Widget* pressedChild() const { return pressedChild_; }
    void focusChild(Widget* child, bool focusVisible = false);
    bool focusNext(bool reverse, bool focusVisible = true);
    bool hasInteractionState() const override;
    void resetInteractionState() override;

private:
    void installChildCallbacks(Widget& child);
    static bool isChildInteractive(const Widget* child);
    Widget* hitTestChild(Point point) const;
    bool clearHoveredChildExcept(Widget* child);
    std::vector<Widget*> focusableChildren() const;

    std::vector<std::shared_ptr<Widget>> children_;
    Widget* focusedChild_ = nullptr;
    Widget* pressedChild_ = nullptr;
    Widget* hoveredChild_ = nullptr;
};

} // namespace oneui
