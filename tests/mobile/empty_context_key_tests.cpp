#include "oneui/controls/text_field.h"
#include "oneui/mobile/text_input.h"
#include "oneui/view.h"
#include <stdexcept>
#include <vector>

namespace {
class EmptyContextEditor final : public oneui::TextField {
public:
    std::vector<oneui::Key> received;
    bool onKeyDown(const oneui::KeyEvent &event) override {
        received.push_back(event.key);
        return true;
    }
};
void expect(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}
}

void testEmptyContextKeys() {
    auto root = std::make_shared<oneui::View>();
    auto remote = std::make_shared<EmptyContextEditor>();
    auto ordinary = std::make_shared<oneui::TextField>();
    root->add(remote);
    root->add(ordinary);
    root->requestFocus(remote.get());
    oneui::mobile::TextInputSession input(root);
    const auto session = input.snapshot().session;
    expect(session != 0, "Empty editor has no input session");
    const std::vector<oneui::Key> keys{oneui::Key::Backspace, oneui::Key::Delete,
                                     oneui::Key::Left, oneui::Key::Right};
    for (auto key : keys)
        expect(input.key(session, key), "Empty-context key was not delivered");
    expect(remote->received == keys, "Key delivery duplicated or reordered");
    expect(input.snapshot().text.empty(), "Key delivery invented local text");
    expect(!input.key(0, oneui::Key::Backspace), "Zero session accepted a key");
    root->requestFocus(ordinary.get());
    const auto next = input.snapshot().session;
    expect(next != 0 && next != session, "Focus change reused the input session");
    expect(!input.key(session, oneui::Key::Backspace), "Stale session accepted a key");
    expect(remote->received == keys, "Stale key reached the previous editor");
    ordinary->setText(L"ab");
    ordinary->setSelectionRange(2, 2);
    const auto editing = input.snapshot();
    expect(input.key(editing.session, oneui::Key::Backspace), "Ordinary deletion failed");
    expect(ordinary->text() == L"a", "Ordinary deletion bypassed the focused editor");
}
