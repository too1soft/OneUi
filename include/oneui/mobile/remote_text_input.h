#pragma once

#include "oneui/mobile/presentation.h"
#include <functional>

namespace oneui::mobile {
// A visible IME capture field for a remote editor. Preedit stays local in
// TextField; committed text is consumed once without retaining remote history.
// The consumer owns transport, permissions and release on session changes.
class RemoteTextInput final : public TextField {
  public:
    using TextSink = std::function<void(const std::wstring &)>;
    using KeySink = std::function<void(Key)>;
    RemoteTextInput(TextSink text, KeySink key)
        : TextField(L"直接输入到电脑"), text_(std::move(text)), key_(std::move(key)) {
        setMultiline(true);
    }
    bool replaceTextRange(std::size_t, std::size_t, const std::wstring &value) override {
        if (!value.empty() && text_) text_(value);
        return true;
    }
    bool onTextCommitted(const std::wstring &value) override {
        setTextComposition({}, 0);
        return replaceTextRange(0, 0, value);
    }
    bool onTextInput(wchar_t value) override { return onTextCommitted(std::wstring(1, value)); }
    void setTabPresentation(ControlPalette palette, float scale) {
        palette_=palette;scale_=scale;tabPresentation_=true;invalidate();
    }
    void setTextComposition(std::wstring value, std::size_t caret) override {
        preedit_=value;
        TextField::setTextComposition(std::move(value),caret);
    }
    void paint(Canvas &canvas) override {
        if(!tabPresentation_){TextField::paint(canvas);return;}
        const auto r=frame();
        canvas.fillRect(r,palette_.pressed,8);
        if(focusVisible())canvas.strokeRect(r,palette_.tint,8,1.5f);
        const float height=22*scale_;
        const Rect text{r.x+4,r.y+(r.height-height)/2,std::max(0.f,r.width-8),height};
        // The capture remains a real visible editor, but empty remote history
        // presents the same centered caption as adjacent mode buttons.
        canvas.drawTextStyledEllipsized(preedit_.empty()?L"手机键盘":preedit_,text,
            palette_.ink,13*scale_,TextAlign::Center,600);
        if(!preedit_.empty())canvas.drawLine({text.x,text.y+height-1},
            {text.x+text.width,text.y+height-1},palette_.tint,1);
    }
    Rect textInputCaretRect() const override {
        if(!tabPresentation_)return TextField::textInputCaretRect();
        const auto r=frame();const float height=22*scale_;
        return {r.x+r.width/2,r.y+(r.height-height)/2,1,height};
    }
    bool onKeyDown(const KeyEvent &event) override {
        switch (event.key) {
        case Key::Backspace: case Key::Delete: case Key::Enter:
        case Key::Left: case Key::Right: case Key::Up: case Key::Down:
        case Key::Tab: case Key::Escape:
            if (hasTextComposition()) return TextField::onKeyDown(event);
            if (key_) key_(event.key);
            return true;
        default: return TextField::onKeyDown(event);
        }
    }
  private:
    TextSink text_;
    KeySink key_;
    ControlPalette palette_=controlPalette(false);
    std::wstring preedit_;
    float scale_=1;
    bool tabPresentation_=false;
};
} // namespace oneui::mobile
