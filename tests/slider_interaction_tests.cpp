#include "oneui/controls/slider.h"
#include "oneui/view.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

using namespace oneui;
namespace {
void check(bool value, const char* message) {
    if (!value) { throw std::runtime_error(message); }
}
struct Event { SliderInteraction phase; double value; };
void gestures() {
    View root;
    root.setFrame({0, 0, 400, 100});
    auto slider = std::make_shared<Slider>();
    slider->setFrame({10, 10, 220, 32});
    slider->setRange(0, 100);
    slider->setStep(0.1);
    slider->setValue(20);
    root.add(slider);
    std::vector<Event> events;
    slider->setOnInteraction([&](auto phase, double value) { events.push_back({phase, value}); });
    check(!slider->onMouseDown({{40, 25}, MouseButton::Right}), "right click cannot seek");
    root.onMouseDown({{80, 25}});
    check(slider->dragging(), "capture begins");
    slider->setRange(0, 100);
    check(slider->dragging(), "unchanged range cannot cancel an ongoing drag");
    for (int x = 81; x < 360; ++x) { root.onMouseMove({{static_cast<float>(x), 25}}); }
    check(events.front().phase == SliderInteraction::Begin && events.front().value == 20, "begin original value");
    for (std::size_t i = 1; i < events.size(); ++i) {
        check(events[i].phase == SliderInteraction::Update, "drag previews cannot commit");
    }
    root.onMouseUp({{380, 25}});
    check(events.back().phase == SliderInteraction::Commit && events.back().value == 100, "release outside clamps and commits");
    check(!slider->dragging(), "release clears capture state");
    auto count = events.size();
    root.onMouseUp({{380, 25}});
    check(events.size() == count, "no double commit");
    slider->setValue(40);
    check(events.size() == count, "programmatic value is not input");

    for (int reason = 0; reason < 4; ++reason) {
        slider->setVisible(true);
        slider->setDisabled(false);
        slider->setValue(40);
        events.clear();
        slider->onMouseDown({{140, 25}});
        if (reason == 0) { slider->onKeyDown(KeyEvent{Key::Escape}); }
        if (reason == 1) { slider->setVisible(false); }
        if (reason == 2) { slider->setDisabled(true); }
        if (reason == 3) { root.clearInteractionState(); }
        check(!slider->dragging() && slider->value() == 40, "cancel restores original value");
        check(events.back().phase == SliderInteraction::Cancel, "cancel observable");
    }
}
void keyboardAndReentry() {
    Slider slider;
    slider.setFrame({0, 0, 220, 32});
    slider.setRange(0, 10);
    slider.setStep(1);
    std::vector<Event> events;
    slider.setOnInteraction([&](auto phase, double value) { events.push_back({phase, value}); });
    for (auto key : {Key::Right, Key::End, Key::Home}) {
        events.clear();
        slider.onKeyDown(KeyEvent{key});
        check(events.size() == 3 && events.front().phase == SliderInteraction::Begin
            && events.back().phase == SliderInteraction::Commit, "keyboard lifecycle");
    }
    slider.setValue(4);
    KeyEvent release{Key::Right}; release.pressed = false;
    slider.onKeyDown(release);
    check(slider.value() == 4, "key up is not another step");
    slider.setOnInteraction([&](auto phase, double) {
        if (phase == SliderInteraction::Begin) { slider.setDisabled(true); }
    });
    slider.onMouseDown({{170, 16}});
    check(!slider.dragging() && slider.value() == 4, "begin callback can cancel synchronously");
    slider.setDisabled(false);
    slider.setOnInteraction([&](auto, double) { slider.setOnInteraction({}); });
    slider.onKeyDown(KeyEvent{Key::Right});
    check(slider.value() == 5, "callback can disconnect itself safely");
}
void invalidNumbers() {
    Slider slider;
    slider.setRange(0, 604800);
    slider.setStep(0.001);
    slider.setValue(123.456);
    slider.setValue(std::numeric_limits<double>::quiet_NaN());
    slider.setRange(0, std::numeric_limits<double>::infinity());
    slider.setRange(1e300, 1e300);
    slider.setStep(std::numeric_limits<double>::infinity());
    check(std::abs(slider.value() - 123.456) < 0.00001, "invalid inputs retain valid state");
    slider.setValue(1e9);
    check(slider.value() == 604800, "duration endpoint clamps");
    int invalidations = 0;
    slider.setInvalidator([&] { ++invalidations; });
    for (int i = 0; i < 100; ++i) {
        slider.setValue(604800);
        slider.setRange(0, 604800);
        slider.setStep(0.001);
    }
    check(invalidations == 0, "unchanged playback values must not repaint while idle");
}
}
int main() {
    try { gestures(); keyboardAndReentry(); invalidNumbers(); }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
    std::cout << "Slider interaction tests passed\n";
}
