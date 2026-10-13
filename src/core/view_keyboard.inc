bool View::hasTextComposition() const {
    auto child = activeFocusChild();
    return child && child->hasTextComposition();
}

// Keyboard dispatch and descendant focus traversal for View.
bool View::onKeyDown(const KeyEvent& event) {
    if (!interactive()) {
        return false;
    }

    const auto child = activeFocusChild();
    if (event.key == Key::Tab) {
        // 深度优先：先让当前聚焦子树在更深层推进（表单里逐个字段切换）；
        // 到本层边界（focusNext 返回 false，不回绕）再冒泡给上层——回绕由焦点作用域
        // （模态 overlay 或窗口内容，见 OverlayHost::onKeyDown）统一负责。
        // 键盘导航即使已经抵达当前作用域的边界，也必须让当前焦点显示焦点环。
        // 这样首次通过 Tab 进入窗口时不会保留鼠标焦点样式。
        if (child) {
            child->setFocusVisible(true);
        }
        const auto life = lifetimeToken();
        if (child && child->onKeyDown(event)) {
            return true;
        }
        if (life.expired()) return true;
        return focusNext(event.shift);
    }

    if (focusedChild_ && !isChildInteractive(focusedChild_)) {
        focusChild(nullptr);
    }

    if (child && child->onKeyDown(event)) {
        return true;
    }

    return false;
}

bool View::onKeyUp(const KeyEvent& event) {
    if (!interactive()) {
        return false;
    }

    if (focusedChild_ && !isChildInteractive(focusedChild_)) {
        focusChild(nullptr);
    }

    const auto child = activeFocusChild();
    return child && child->onKeyUp(event);
}

bool View::onTextInput(wchar_t character) {
    if (!interactive()) {
        return false;
    }

    if (focusedChild_ && !isChildInteractive(focusedChild_)) {
        focusChild(nullptr);
    }
    const auto child = activeFocusChild();
    return child && child->onTextInput(character);
}

bool View::onTextInputText(const std::wstring& text) {
    if (!interactive()) {
        return false;
    }
    if (focusedChild_ && !isChildInteractive(focusedChild_)) {
        focusChild(nullptr);
    }
    const auto child = activeFocusChild();
    return child && child->onTextInputText(text);
}

Rect View::textInputCaretRect() const { auto child = activeFocusChild(); return child ? child->textInputCaretRect() : frame(); }

TextInputState View::textInputState() const { auto child = activeFocusChild(); return child ? child->textInputState() : TextInputState{}; }
bool View::onTextCommitted(const std::wstring& text) { auto child = activeFocusChild(); return child && child->onTextCommitted(text); }
void View::setTextComposition(std::wstring text, std::size_t caret) { if (auto child = activeFocusChild()) child->setTextComposition(std::move(text), caret); }
bool View::replaceTextRange(std::size_t start, std::size_t end, const std::wstring& text) { auto child = activeFocusChild(); return child && child->replaceTextRange(start, end, text); }

bool View::onFocusChanged(bool focused) {
    setFocused(focused);
    if (!focused && focusedChild_) {
        focusedChild_->onFocusChanged(false);
    } else if (focused) {
        if (focusedChild_ && !isChildInteractive(focusedChild_)) {
            focusChild(nullptr);
        }

        if (!focusedChild_) {
            focusNext(false, false);
        } else {
            focusedChild_->onFocusChanged(true);
        }
    }
    return true;
}

bool View::isFocusable() const {
    if (!interactive()) {
        return false;
    }
    // Stop at the first focusable child. Building the full list recursed
    // through every descendant at every level of a focus walk, which made
    // focusing into a freshly mounted pane cost 15-25 ms in a large tree.
    for (const auto& child : children_) {
        if (child->visible() && !child->disabled() && child->tabStop() && child->isFocusable()) {
            return true;
        }
    }
    return false;
}

bool View::requestFocus(Widget* descendant, bool focusVisible) {
    if (!descendant || !descendant->visible() || descendant->disabled()) {
        return false;
    }

    for (const auto& child : children_) {
        if (!isChildInteractive(child.get())) {
            continue;
        }
        // No invalidate() here: widgets whose focus state changes invalidate
        // themselves, and invalidating every ancestor recomputed the paint
        // bounds of its whole subtree (15-40 ms focusing a new split pane).
        if (child.get() == descendant) {
            if (!descendant->isFocusable()) {
                return false;
            }
            focusChild(descendant, focusVisible);
            return true;
        }

        auto* childView = dynamic_cast<View*>(child.get());
        auto* popup = dynamic_cast<Popup*>(child.get());
        if ((childView && childView->requestFocus(descendant, focusVisible)) ||
            (popup && popup->requestFocus(descendant, focusVisible))) {
            focusChild(child.get(), focusVisible);
            return true;
        }
    }
    return false;
}

