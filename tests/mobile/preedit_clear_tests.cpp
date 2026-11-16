#include <oneui/controls/text_field.h>
#include <oneui/mobile/text_input.h>
#include <oneui/view.h>
#include <stdexcept>

void testExternallyClearedPreedit() {
    auto root = std::make_shared<oneui::View>();
    auto field = std::make_shared<oneui::TextArea>();
    root->add(field); root->requestFocus(field.get());
    oneui::mobile::TextInputSession input(root);
    auto draft = input.snapshot();
    draft.text = u"输入法草稿";
    draft.anchor = draft.caret = int(draft.text.size());
    draft.composingStart = 0; draft.composingEnd = draft.caret;
    if (!input.apply(draft) || !field->text().empty())
        throw std::runtime_error("Preedit setup committed too early");
    if (!field->commitTextComposition() || field->text() != L"输入法草稿")
        throw std::runtime_error("Displayed preedit was not committed for submission");
    field->setText(L"");
    const auto empty = input.snapshot();
    if (!empty.text.empty() || empty.composingStart != -1 || empty.session == draft.session)
        throw std::runtime_error("Commit-and-clear restored stale preedit");
    if (input.apply(draft) || !field->text().empty())
        throw std::runtime_error("Stale composing buffer rewrote the sent draft");
    auto next = empty; next.text = u"下一段"; next.anchor = next.caret = int(next.text.size());
    if (!input.apply(next) || field->text() != L"下一段")
        throw std::runtime_error("Typing after clearing preedit failed");
}
