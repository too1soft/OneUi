#include "oneui/controls/text_field.h"
#include "oneui/mobile/text_input.h"
#include "oneui/view.h"
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace oneui;
using namespace oneui::mobile;
void testSelectionGeometry();
void check(bool value, const char *what) {
    if (!value)
        throw std::runtime_error(what);
}
int main() {
    try {
        testSelectionGeometry();
        auto root = std::make_shared<View>();
        auto field = std::make_shared<TextField>();
        auto second = std::make_shared<TextArea>();
        root->add(field);
        root->add(second);
        root->requestFocus(field.get());
        field->setText(L"ab");
        field->setSelectionRange(1, 1);
        TextInputSession input(root);
        auto s = input.snapshot();
        check(s.session != 0, "focused editor unavailable");
        s.text = u"a中b";
        s.anchor = s.caret = 2;
        s.composingStart = 1;
        s.composingEnd = 2;
        check(input.apply(s) && field->text() == L"ab", "preedit must not commit");
        s.text = u"a中文b";
        s.anchor = s.caret = 3;
        s.composingEnd = 3;
        check(input.apply(s) && field->text() == L"ab", "repeated composition must not commit");
        s.composingStart = s.composingEnd = -1;
        check(input.apply(s) && field->text() == L"a中文b", "atomic Chinese commit");
        field->undo();
        check(field->text() == L"ab", "preedit created undo entries");
        check(!input.apply(s), "external undo must invalidate old IME session");
        s = input.snapshot();
        s.text = u"a\U0001f600b";
        s.anchor = 1;
        s.caret = 3;
        check(input.apply(s), "valid emoji selection");
        check(field->selectedText() == L"\U0001f600", "UTF-16 / wchar_t conversion");
        auto invalid = s;
        invalid.caret = 2;
        check(!input.apply(invalid), "surrogate midpoint accepted");
        invalid = s;
        invalid.text = std::u16string{char16_t(0xd800)};
        invalid.anchor = invalid.caret = 1;
        check(!input.apply(invalid) && input.snapshot().text == s.text,
              "malformed UTF-16 changed state");
        s = input.snapshot();
        s.text = u"ab";
        s.anchor = s.caret = 1;
        check(input.apply(s) && field->text() == L"ab", "emoji delete");
        auto old = s;
        root->requestFocus(second.get());
        s = input.snapshot();
        check(s.session != old.session && s.multiline, "focus identity and multiline");
        check(!input.apply(old) && second->text().empty(), "stale callback wrote new editor");
        s.text = u"第一行\nsecond";
        s.anchor = s.caret = int(s.text.size());
        check(input.apply(s) && second->text() == L"第一行\nsecond", "multiline commit");
        input.reset();
        check(!input.apply(s), "closed session accepted");
        s = input.snapshot();
        bool wrongThread = false;
        std::thread worker([&] {
            try {
                input.snapshot();
            } catch (const std::logic_error &) {
                wrongThread = true;
            }
        });
        worker.join();
        check(wrongThread, "UI thread guard absent");
        root->requestFocus(field.get());
        s = input.snapshot();
        s.text = u"draft";
        s.anchor = s.caret = 5;
        s.composingStart = 0;
        s.composingEnd = 5;
        check(input.apply(s), "start new composition");
        input.reset();
        check(input.snapshot().text == u"ab", "cancel committed composition");
        field->setReadOnly(true);
        check(input.snapshot().session == 0, "read-only editor exposed");
        field->setReadOnly(false);
        field->setPasswordMode(true);
        field->setText(L"a\U0001f600b");
        field->setCaretIndex(field->text().size());
        s = input.snapshot();
        check(s.sensitive && s.text == u"••••" && s.caret == 4,
              "password snapshot leaked or wrong UTF16 length");
        check(!input.apply(s), "masked whole-buffer apply must be rejected");
        const auto secretSession = s.session;
        auto edit = [&](SensitiveEdit op, const std::u16string &value, int a, int b = 0) {
            return input.editSensitive(secretSession, op, value, a, b);
        };
        check(edit(SensitiveEdit::Select, {}, 1, 3), "password emoji select");
        check(edit(SensitiveEdit::Compose, u"中文", 1) && field->text() == L"a\U0001f600b",
              "password preedit committed");
        check(input.snapshot().text == u"••••", "password composition leaked");
        check(edit(SensitiveEdit::Commit, u"中文", 1) && field->text() == L"a中文b",
              "password compose/commit");
        check(edit(SensitiveEdit::DeleteCodePoints, {}, 1, 0) && field->text() == L"a中b",
              "password codepoint delete");
        check(edit(SensitiveEdit::Select, {}, 1, 2) &&
                  edit(SensitiveEdit::Commit, u"\U0001f600", 1),
              "password emoji replace");
        check(!edit(SensitiveEdit::Select, {}, 2, 2), "password surrogate selection accepted");
        check(!edit(SensitiveEdit::DeleteUnits, {}, 1, 0),
              "password unit deletion split surrogate");
        check(edit(SensitiveEdit::DeleteCodePoints, {}, 1, 0) && field->text() == L"ab",
              "password emoji codepoint deletion");
        check(!edit(SensitiveEdit::Commit, std::u16string{char16_t(0xd800)}, 1),
              "malformed secret committed");
        check(edit(SensitiveEdit::Commit, u"\U0001f600", 1), "password insert");
        check(edit(SensitiveEdit::Move, {}, -1, 1) && input.snapshot().anchor == 3 &&
                  input.snapshot().caret == 1,
              "secure shift-arrow split emoji");
        check(edit(SensitiveEdit::Commit, u"Z", 1) && field->text() == L"aZb",
              "reverse secret selection replace");
        field->setText(L"external");
        check(!edit(SensitiveEdit::Commit, u"stale", 1),
              "external secret edit did not revoke session");
        s = input.snapshot();
        field->setPasswordMode(false);
        check(!input.editSensitive(s.session, SensitiveEdit::Commit, u"stale", 1),
              "security mode change did not revoke session");
        std::cout << "PASS: preedit/undo, Chinese, UTF16 emoji, invalid input, stale sessions, "
                     "multiline, thread guard, cancel/read-only, password commands/privacy\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
