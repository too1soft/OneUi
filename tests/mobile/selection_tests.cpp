#include "oneui/controls/text_field.h"
#include "oneui/mobile/text_input.h"
#include "oneui/view.h"
#include <limits>
#include <stdexcept>
using namespace oneui;
using namespace oneui::mobile;
namespace {
void require(bool ok, const char *why) {
    if (!ok)
        throw std::runtime_error(why);
}
Point point(const TextField &field, size_t index) {
    const auto r = field.textRangeRects(index, index).at(0);
    return {r.x + .01f, r.y + r.height * .5f};
}
} // namespace
void testSelectionGeometry() {
    auto root = std::make_shared<View>();
    auto field = std::make_shared<TextArea>();
    root->add(field);
    root->setFrame({0, 0, 400, 250});
    field->setFrame({20, 20, 350, 180});
    field->setText(L"alpha beta\nsecond line");
    root->requestFocus(field.get());
    field->setSelectionRange(0, 5);
    TextInputSession input(root);
    auto s = input.snapshot();
    auto g = input.selectionGeometry();
    require(g.session == s.session && g.anchor == 0 && g.caret == 5, "selection geometry identity");
    require(g.anchorRect.x < g.caretRect.x && g.clip.x == 20, "selection endpoint positions");
    require(input.selectAtPoint(s.session, SelectionEdge::Anchor, point(*field, 9)), "anchor drag");
    auto crossed = input.snapshot();
    require(crossed.anchor == 9 && crossed.caret == 5 && field->selectedText() == L" bet",
            "crossing changed fixed endpoint");
    require(input.selectAtPoint(s.session, SelectionEdge::Caret, point(*field, 14)),
            "multiline endpoint drag");
    require(input.snapshot().anchor == 9 && input.snapshot().caret == 14,
            "multiline selection indices");
    require(input.selectionGeometry().caretRect.y > g.caretRect.y, "multiline caret row");
    require(input.selectAtPoint(s.session, SelectionEdge::Insertion, point(*field, 2)),
            "insertion drag");
    require(input.snapshot().anchor == 2 && input.snapshot().caret == 2,
            "insertion did not collapse");
    require(!input.selectAtPoint(s.session, SelectionEdge::Caret,
                                 {std::numeric_limits<float>::quiet_NaN(), 0}),
            "NaN accepted");
    field->setText(L"a\U0001f600e\u0301 z");
    field->setCaretIndex(0);
    s = input.snapshot();
    const size_t emojiEnd = std::wstring(L"a\U0001f600").size();
    require(input.selectAtPoint(s.session, SelectionEdge::Insertion, point(*field, emojiEnd)),
            "Unicode caret hit");
    require(input.snapshot().caret == 3, "geometry split a UTF16 surrogate");
    require(input.selectAtPoint(s.session, SelectionEdge::Caret, point(*field, emojiEnd + 2)),
            "combining grapheme drag");
    require(field->selectedText() == L"e\u0301", "combining sequence split");
    const auto old = input.snapshot();
    field->setText(L"external");
    require(!input.selectAtPoint(old.session, SelectionEdge::Caret, {30, 30}),
            "stale geometry edited new text");
    field->setPasswordMode(true);
    s = input.snapshot();
    require(!input.selectionGeometry().session &&
                !input.selectAtPoint(s.session, SelectionEdge::Caret, {30, 30}),
            "password geometry exposed");
    field->setPasswordMode(false);
    s = input.snapshot();
    s.text = u"preedit";
    s.anchor = s.caret = 7;
    s.composingStart = 0;
    s.composingEnd = 7;
    require(input.apply(s) && !input.selectionGeometry().session,
            "composition exposed editable handles");
    s.composingStart = s.composingEnd = -1;
    require(input.apply(s), "finish composition");
    field->setFrame({20, 300, 350, 100});
    require(!input.selectionGeometry().session, "fully clipped editor exposed handles");
}
