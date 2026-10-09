#include "platform/shared/desktop_window.h"
#include "oneui/controls/button.h"
#include "oneui/controls/text_field.h"
#include "oneui/controls/slider.h"
#include "oneui/controls/progress_bar.h"
#include "oneui/layout/overlay_host.h"
#include <iostream>
#include <stdexcept>

int main() {
    using namespace oneui;
    int failures = 0;
    auto check = [&](bool ok, const char* message) { if (!ok) { ++failures; std::cerr << message << '\n'; } };
    WindowOptions options; options.visible = false;
    auto window = Window::create(options); window->initialize();
    auto* desktop = dynamic_cast<platform::DesktopWindow*>(window.get());
    auto root = std::make_shared<View>(); root->setAccessibleName(L"Semantic parent");
    auto button = std::make_shared<Button>(L"Action");
    auto field = std::make_shared<TextField>(L"Password");
    field->setPasswordMode(true); field->setText(L"secret-value"); field->setAccessibleValue(L"secret-value");
    auto hidden = std::make_shared<Button>(L"Hidden"); hidden->setVisible(false);
    root->add(button); root->add(field); root->add(hidden);
    window->setContent(root);
    auto context = desktop->accessibilityContext();
    check(context->snapshot().empty(), "Hidden windows expose no actionable controls");
    window->show(); desktop->render();
    root->setFrame({0, 0, 400, 300}); button->setFrame({10, 10, 100, 30}); field->setFrame({10, 50, 100, 30});
    auto nodes = context->snapshot();
    auto id = [&](Widget* widget) { for (const auto& n : context->snapshot()) if (n.widget.lock().get() == widget) return n.id; return uint64_t(0); };
    const auto buttonId = id(button.get()), fieldId = id(field.get());
    check(buttonId && fieldId && !id(hidden.get()), "Only visible semantic controls are exposed");
    check(context->resolve(fieldId)->sensitive && context->resolve(fieldId)->info.value.empty(), "Password overrides never expose plaintext");
    check(context->focusNode(fieldId) && !context->resolve(id(root.get()))->sensitive &&
          !context->resolve(id(root.get()))->info.state.focused, "Routing parent does not inherit password role or leaf focus");
    int clicks = 0; button->setOnClick([&] { ++clicks; });
    check(context->press(buttonId) && clicks == 1, "Accessible press reaches the actual callback");
    check(context->resolve(buttonId)->info.state.focused, "Accessible action transfers keyboard focus");
    button->setDisabled(true);
    check(!context->press(buttonId) && clicks == 1, "Disabled actions are rejected");
    button->setDisabled(false); root->setDisabled(true);
    check(!context->press(buttonId), "Disabled ancestors suppress descendant actions");
    root->setDisabled(false);
    check(context->setValue(fieldId, L"replacement") && field->text() == L"replacement", "Password editing uses the real text length");
    field->setReadOnly(true);
    check(!context->setValue(fieldId, L"forbidden") && field->text() == L"replacement", "Read-only edit is rejected");
    desktop->enterNativeModal(); desktop->enterNativeModal();
    check(context->snapshot().empty() && !context->press(buttonId), "Native modal isolates background accessibility");
    desktop->leaveNativeModal();
    check(context->snapshot().empty(), "Nested native modal remains isolated");
    desktop->leaveNativeModal();
    check(id(button.get()) == buttonId, "Stable identity survives temporary modal isolation");
    root->reconcileChildren({field, button});
    check(id(button.get()) == buttonId && id(field.get()) == fieldId, "Reordering preserves node identities");
    auto host = std::make_shared<OverlayHost>(); host->setContent(root);
    auto modal = std::make_shared<Button>(L"Modal"); modal->setFrame({0, 0, 90, 30});
    host->addOverlay(modal, OverlayOptions::modal(5));
    window->setContent(host); desktop->render();
    check(id(modal.get()) && !id(button.get()) && !context->press(buttonId), "OneUI focus traps isolate the background tree");
    host->removeOverlay(modal.get());
    check(id(button.get()) == buttonId, "Dismissing a OneUI modal restores stable background nodes");
    field->setReadOnly(false); field->setPasswordMode(false); field->setText(L"A\U0001f600e\u0301B");
    check(context->selectText(fieldId, 2, 4) && field->selectedText() == L"e\u0301", "Selection follows the real Unicode editor");
    field->setTextComposition(L"preedit", 2);
    check(!context->selectText(fieldId, 0, 1) && !context->replaceText(fieldId, 0, 1, L"X") && field->hasTextComposition(), "Accessibility cannot interrupt provisional IME text");
    field->setTextComposition({}, 0);
    auto savedFocus = context->focus;
    context->focus = [&](Widget*) { field->setText(L"different document"); return true; };
    check(!context->replaceText(fieldId, 0, 1, L"X") && field->text() == L"different document", "Focus callbacks cannot redirect a stale text edit into another document");
    context->focus = savedFocus;
    auto slider = std::make_shared<Slider>(); slider->setRange(-5, 10); slider->setStep(0.5); slider->setFrame({10, 150, 200, 30});
    root->add(slider); const auto sliderId = id(slider.get());
    std::vector<SliderInteraction> phases;
    slider->setOnInteraction([&](SliderInteraction phase, double) { phases.push_back(phase); });
    check(context->setNumber(sliderId, 4.5) && slider->value() == 4.5 && phases == std::vector<SliderInteraction>{
        SliderInteraction::Begin, SliderInteraction::Update, SliderInteraction::Commit}, "Accessible numeric input uses the real interaction lifecycle");
    check(context->stepNumber(sliderId, false) && slider->value() == 4, "Accessible decrement respects the actual step");
    for (auto phase : {SliderInteraction::Begin, SliderInteraction::Update, SliderInteraction::Commit}) {
        slider->setOnInteraction([phase](SliderInteraction current, double) { if (phase == current) throw std::runtime_error("input callback"); });
        bool threw = false; try { context->setNumber(sliderId, 5); } catch (...) { threw = true; }
        check(threw && !slider->dragging(), "Throwing native input callback never leaves a phantom drag");
    }
    slider->setOnInteraction({});
    check(context->setNumber(sliderId, 6) && slider->value() == 6, "Numeric input recovers after callback exceptions");
    root->setDisabled(true);
    check(!context->setNumber(sliderId, 5), "Disabled parents also reject numeric input");
    root->setDisabled(false);
    auto replacement = std::make_shared<Button>(L"Replacement");
    window->setContent(replacement); desktop->render();
    check(!context->resolve(buttonId) && !context->press(buttonId), "Detached but still alive widgets cannot be invoked");
    check(id(replacement.get()) != buttonId, "New controls do not inherit stale identities");
    const auto replacementId = id(replacement.get());
    replacement->setOnClick([&] { window.reset(); });
    check(context->press(replacementId) && !window, "An accessibility callback may destroy its own window");
    check(context->snapshot().empty() && !context->press(replacementId), "Retained contexts fail closed after window destruction");
    return failures ? 1 : 0;
}
