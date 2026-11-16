#include <oneui/controls/button.h>
#include <oneui/controls/text_field.h>
#include <oneui/layout/overlay_host.h>
#include <oneui/view.h>
#include <iostream>
#include <stdexcept>

using namespace oneui;
namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void click(Widget& root, Point point) {
    require(root.onMouseDown({point}), "Pointer down was not handled");
    require(root.onMouseUp({point}), "Pointer up was not handled");
}
struct Form {
    std::shared_ptr<View> root = std::make_shared<View>();
    std::shared_ptr<View> body = std::make_shared<View>();
    std::shared_ptr<View> footer = std::make_shared<View>();
    std::shared_ptr<TextField> field = std::make_shared<TextField>();
    std::shared_ptr<Button> action = std::make_shared<Button>(L"Send");
    int clicks = 0;
    Form() {
        root->setFrame({0, 0, 300, 200}); body->setFrame({0, 0, 300, 100});
        field->setFrame({0, 0, 280, 48}); body->add(field);
        footer->setFrame({0, 110, 300, 70}); action->setFrame({0, 110, 280, 48});
        action->setOnClick([this] { ++clicks; }); footer->add(action);
        root->add(body); root->add(footer);
        require(root->requestFocus(field.get(), false), "Editor focus unavailable");
    }
};
void nestedAuxiliaryAction() {
    Form form;
    form.action->setFocusOnPointer(false);
    auto editing = form.root->textInputState();
    click(*form.root, {100, 130});
    require(form.clicks == 1, "Preserving focus suppressed click");
    require(form.field->focused(), "Nested action blurred editor");
    require(form.root->textInputState().session == editing.session, "Pointer changed focus generation");
    require(form.root->onTextInput(L'x') && form.field->text() == L"x", "Typing stopped after action");
    require(form.root->requestFocus(form.action.get(), true), "Action cannot receive keyboard focus");
    require(form.action->focused(), "Action was removed from keyboard focus");
    require(form.root->onKeyDown({Key::Enter}) && form.clicks == 2, "Keyboard activation stopped working");
}
void defaultActionFocus() {
    Form form;
    click(*form.root, {100, 130});
    require(form.action->focused() && !form.field->focused(), "Default pointer focus behavior changed");
}
void cancelledAuxiliaryPress() {
    Form form; form.action->setFocusOnPointer(false);
    require(form.root->onMouseDown({{100, 130}}), "Auxiliary press missing");
    form.root->onMouseUp({{299, 199}});
    require(form.clicks == 0 && form.field->focused(), "Cancelled press activated or blurred editor");
}
void overlayActionFocus() {
    auto root = std::make_shared<OverlayHost>(); root->setFrame({0, 0, 300, 200});
    auto body = std::make_shared<View>(); body->setFrame({0, 0, 300, 200});
    auto field = std::make_shared<TextField>(); field->setFrame({10, 10, 100, 48}); body->add(field);
    root->setContent(body);
    auto action = std::make_shared<Button>(L"Auxiliary"); action->setFrame({150, 100, 100, 48});
    action->setFocusOnPointer(false); int clicks = 0; action->setOnClick([&] { ++clicks; });
    root->addOverlay(action, OverlayOptions::modeless());
    require(root->requestFocus(field.get(), false), "Overlay editor focus unavailable");
    click(*root, {180, 120});
    require(clicks == 1 && field->focused(), "Modeless auxiliary action stole editor focus");
    require(root->onTextInput(L'y') && field->text() == L"y", "Modeless action broke text routing");
    action->setFocusOnPointer(true); click(*root, {180, 120});
    require(action->focused() && !field->focused(), "Ordinary modeless action failed to take focus");
}
}
int main() {
    try {
        nestedAuxiliaryAction(); defaultActionFocus(); cancelledAuxiliaryPress(); overlayActionFocus();
        std::cout << "Pointer focus contracts passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
