#include "text_page.hpp"
#include <cmath>
#include <sstream>

namespace lab::mobile {
using namespace oneui;
class TextPage final : public ContentPage {
  public:
    std::shared_ptr<TextField> single = std::make_shared<TextField>();
    std::shared_ptr<TextField> password = std::make_shared<TextField>();
    std::shared_ptr<TextArea> multiple = std::make_shared<TextArea>();
    std::shared_ptr<Label> result, metrics;
    float scale = 1, size = 16;
    unsigned changes = 0;
    TextPage() {
        text(L"单行输入");
        single->setPlaceholder(L"点这里，试试中文或 emoji");
        add(single, 56);
        text(L"多行笔记");
        multiple->setPlaceholder(L"输入多行文字；键盘弹出后可滚动");
        add(multiple, 160);
        result = text(L"已提交文字：0 次变化");
        result->setTextWrapping(true);
        addActions();
        text(L"密码输入");
        password->setPasswordMode(true);
        password->setPlaceholder(L"仅用于试验，不要输入真实密码");
        add(password, 56);
        action(L"全选密码", [this] {
            password->selectAll();
            scroll_->requestFocus(password.get());
        });
        action(L"清空密码", [this] { password->setText({}); });
        metrics = text(L"系统字号：100%");
        metrics->setTextWrapping(true);
        body_->setFlex(metrics, {0, 0, 80, 80});
        auto note =
            text(L"切页保留已提交文字。系统字号会调整本页输入框和说明文字；其他页面仍在逐项适配。"
                 L"普通输入框可点击定位、长按选词，并拖动光标或两端选区手柄；菜单支持复制、剪切和粘贴。"
                 L"密码始终遮罩且不可复制，暂不显示手柄。读屏仍待接入。");
        note->setTextWrapping(true);
        body_->setFlex(note, {0, 0, 160, 160});
        auto changed = [this](const std::wstring &) {
            ++changes;
            result->setText(L"已提交文字：" + std::to_wstring(changes) + L" 次变化");
        };
        single->setOnChanged(changed);
        multiple->setOnChanged(changed);
    }
    void setTheme(bool dark, bool compact) override {
        ContentPage::setTheme(dark, compact);
        if (metrics)
            typography(scale, size);
    }
    void addActions() {
        action(L"全选单行文字", [this] {
            single->selectAll();
            scroll_->requestFocus(single.get());
        });
        action(L"撤销单行编辑", [this] { single->undo(); });
        action(L"重做单行编辑", [this] { single->redo(); });
    }
    void typography(float fontScale, float bodySize) {
        if (!std::isfinite(fontScale) || !std::isfinite(bodySize) || fontScale <= 0 ||
            bodySize <= 0)
            return;
        scale = fontScale;
        size = bodySize;
        password->setFontSize(size);
        single->setFontSize(size);
        multiple->setFontSize(size);
        const float singleHeight = std::max(56.f, size * 1.5f + 24);
        const float multipleHeight = std::max(160.f, size * 4 + 32);
        body_->setFlex(password, {0, 0, singleHeight, singleHeight});
        body_->setFlex(single, {0, 0, singleHeight, singleHeight});
        body_->setFlex(multiple, {0, 0, multipleHeight, multipleHeight});
        for (const auto &label : labels_)
            label->setFontSize(size);
        metrics->setText(L"系统字号：" + std::to_wstring(int(std::round(scale * 100))) +
                         L"%\n正文：16 sp → " + std::to_wstring(int(std::round(size))) + L" dp");
        body_->setFlex(result, {0, 0, size * 2.8f, size * 2.8f});
        body_->setFlex(metrics, {0, 0, size * 4, size * 4});
        for (const auto &label : labels_)
            if (label != metrics && label != result)
                body_->setFlex(label, {0, 0, label == labels_.back() ? size * 12 : size * 2,
                                       label == labels_.back() ? size * 12 : size * 2});
    }
};
std::unique_ptr<DemoPage> createTextPage() { return std::make_unique<TextPage>(); }
void textPageMetrics(DemoPage *page, float scale, float size) {
    if (auto *text = dynamic_cast<TextPage *>(page))
        text->typography(scale, size);
}
std::string textPageDiagnostics(DemoPage *page) {
    auto *text = dynamic_cast<TextPage *>(page);
    if (!text)
        return {};
    std::ostringstream out;
    out << " fontScale=" << text->scale << " inputFont=" << text->size
        << " textChanges=" << text->changes;
    // Lengths only. Never emit user input in diagnostic logs.
    int i = 0;
    for (auto field :
         {text->single, std::static_pointer_cast<TextField>(text->multiple), text->password}) {
        const auto b = field->frame();
        out << " editorComposing" << i << '=' << field->hasTextComposition();
        out << " editor" << i++ << '=' << b.x << ',' << b.y << ',' << b.width << ',' << b.height
            << ',' << field->text().size() << ',' << field->selectionStart() << ','
            << field->selectionEnd();
    }
    out << " editorFocus0=" << text->single->focused()
        << " editorFocus1=" << text->multiple->focused()
        << " editorFocus2=" << text->password->focused();
    return out.str();
}
} // namespace lab::mobile
