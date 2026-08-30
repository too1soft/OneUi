#include "oneui/controls/remote_input_region.h"

#include <cmath>
#include <iostream>
#include <vector>

namespace {

int failures = 0;

class NullCanvas final : public oneui::Canvas {
public:
    void save() override {}
    void restore() override {}
    void clipRect(oneui::Rect) override {}
    void clear(oneui::Color) override {}
    void fillRect(oneui::Rect, oneui::Color, float = 0.0f) override {}
    void strokeRect(oneui::Rect, oneui::Color, float, float = 1.0f) override {}
    void fillEllipse(oneui::Rect, oneui::Color) override {}
    void strokeEllipse(oneui::Rect, oneui::Color, float = 1.0f) override {}
    void drawLine(oneui::Point, oneui::Point, oneui::Color, float = 1.0f) override {}
    void drawText(const std::wstring&, oneui::Rect, oneui::Color, float, oneui::TextAlign = oneui::TextAlign::Center) override {}
};

void expectEqual(const char* name, int actual, int expected) {
    if (actual != expected) {
        std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
        ++failures;
    }
}

void expectNear(const char* name, float actual, float expected) {
    if (std::fabs(actual - expected) > 0.001f) {
        std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
        ++failures;
    }
}

void expectRect(const char* name, oneui::Rect actual, oneui::Rect expected) {
    expectNear((std::string(name) + " x").c_str(), actual.x, expected.x);
    expectNear((std::string(name) + " y").c_str(), actual.y, expected.y);
    expectNear((std::string(name) + " width").c_str(), actual.width, expected.width);
    expectNear((std::string(name) + " height").c_str(), actual.height, expected.height);
}

void testFitMappingLetterboxesAndMapsRemoteCoordinates() {
    oneui::RemoteInputRegion region;
    region.setFrame(oneui::Rect{0.0f, 0.0f, 800.0f, 600.0f});
    region.setRemoteSize(oneui::Size{1920.0f, 1080.0f});
    region.setScaleMode(oneui::RemoteInputScaleMode::Fit);

    expectRect("RemoteInputRegion fit content rect", region.contentRect(), oneui::Rect{0.0f, 75.0f, 800.0f, 450.0f});

    std::vector<oneui::RemotePointerEvent> events;
    region.setOnPointer([&](const oneui::RemotePointerEvent& event) {
        events.push_back(event);
    });

    expectEqual("RemoteInputRegion pointer handled", region.dispatchPointer(oneui::Point{400.0f, 300.0f}, oneui::PointerButton::Left, true) ? 1 : 0, 1);
    expectEqual("RemoteInputRegion pointer count", static_cast<int>(events.size()), 1);
    expectNear("RemoteInputRegion normalized x", events.back().normalizedPosition.x, 0.5f);
    expectNear("RemoteInputRegion normalized y", events.back().normalizedPosition.y, 0.5f);
    expectNear("RemoteInputRegion remote x", events.back().remotePosition.x, 960.0f);
    expectNear("RemoteInputRegion remote y", events.back().remotePosition.y, 540.0f);
}

void testStretchMappingUsesFullFrame() {
    oneui::RemoteInputRegion region;
    region.setFrame(oneui::Rect{10.0f, 20.0f, 300.0f, 200.0f});
    region.setRemoteSize(oneui::Size{1200.0f, 800.0f});
    region.setScaleMode(oneui::RemoteInputScaleMode::Stretch);

    expectRect("RemoteInputRegion stretch content rect", region.contentRect(), oneui::Rect{10.0f, 20.0f, 300.0f, 200.0f});

    std::vector<oneui::RemotePointerEvent> events;
    region.setOnPointer([&](const oneui::RemotePointerEvent& event) {
        events.push_back(event);
    });

    region.dispatchPointer(oneui::Point{310.0f, 220.0f}, oneui::PointerButton::None, false);
    expectNear("RemoteInputRegion stretch clamps remote x", events.back().remotePosition.x, 1200.0f);
    expectNear("RemoteInputRegion stretch clamps remote y", events.back().remotePosition.y, 800.0f);
}

void testPointerIgnoredUntilRemoteSizeKnown() {
    oneui::RemoteInputRegion region;
    region.setFrame(oneui::Rect{0.0f, 0.0f, 800.0f, 600.0f});

    std::vector<oneui::RemotePointerEvent> events;
    region.setOnPointer([&](const oneui::RemotePointerEvent& event) {
        events.push_back(event);
    });

    expectEqual("RemoteInputRegion no-size pointer handled", region.dispatchPointer(oneui::Point{400.0f, 300.0f}, oneui::PointerButton::Left, true) ? 1 : 0, 0);
    expectEqual("RemoteInputRegion no-size pointer count", static_cast<int>(events.size()), 0);

    region.setRemoteSize(oneui::Size{1920.0f, 1080.0f});
    expectEqual("RemoteInputRegion sized pointer handled", region.dispatchPointer(oneui::Point{400.0f, 300.0f}, oneui::PointerButton::Left, true) ? 1 : 0, 1);
    expectEqual("RemoteInputRegion sized pointer count", static_cast<int>(events.size()), 1);
}

void testRawKeyAndReleaseAllInputs() {
    oneui::RemoteInputRegion region;
    region.setFrame(oneui::Rect{0.0f, 0.0f, 800.0f, 600.0f});
    region.setRemoteSize(oneui::Size{1920.0f, 1080.0f});
    std::vector<oneui::RawKeyEvent> keys;
    std::vector<oneui::RemotePointerEvent> pointers;
    region.setOnRawKey([&](const oneui::RawKeyEvent& event) {
        keys.push_back(event);
    });
    region.setOnPointer([&](const oneui::RemotePointerEvent& event) {
        pointers.push_back(event);
    });

    oneui::RawKeyEvent key;
    key.virtualKey = 0x41;
    key.scanCode = 0x1e;
    key.pressed = true;
    key.ctrl = true;
    expectEqual("RemoteInputRegion raw key down handled", region.dispatchRawKey(key) ? 1 : 0, 1);
    region.dispatchPointer(oneui::Point{10.0f, 10.0f}, oneui::PointerButton::Right, true);

    region.releaseAllInputs();

    expectEqual("RemoteInputRegion raw key event count", static_cast<int>(keys.size()), 2);
    expectEqual("RemoteInputRegion release key pressed false", keys.back().pressed ? 1 : 0, 0);
    expectEqual("RemoteInputRegion release key virtual", static_cast<int>(keys.back().virtualKey), 0x41);
    expectEqual("RemoteInputRegion pointer event count", static_cast<int>(pointers.size()), 2);
    expectEqual("RemoteInputRegion release pointer pressed false", pointers.back().pressed ? 1 : 0, 0);
    expectEqual("RemoteInputRegion release pointer button", static_cast<int>(pointers.back().button), static_cast<int>(oneui::PointerButton::Right));
}

void testFocusLossReleasesPressedInputs() {
    oneui::RemoteInputRegion region;
    region.setFrame(oneui::Rect{0.0f, 0.0f, 800.0f, 600.0f});
    region.setRemoteSize(oneui::Size{1920.0f, 1080.0f});

    std::vector<oneui::RawKeyEvent> keys;
    std::vector<oneui::RemotePointerEvent> pointers;
    region.setOnRawKey([&](const oneui::RawKeyEvent& event) {
        keys.push_back(event);
    });
    region.setOnPointer([&](const oneui::RemotePointerEvent& event) {
        pointers.push_back(event);
    });

    oneui::RawKeyEvent key;
    key.virtualKey = 0x31;
    key.scanCode = 0x02;
    key.pressed = true;
    region.dispatchRawKey(key);
    region.dispatchPointer(oneui::Point{30.0f, 30.0f}, oneui::PointerButton::Left, true);

    expectEqual("RemoteInputRegion focus loss changed", region.onFocusChanged(false) ? 1 : 0, 1);
    expectEqual("RemoteInputRegion focus loss key event count", static_cast<int>(keys.size()), 2);
    expectEqual("RemoteInputRegion focus loss key up", keys.back().pressed ? 1 : 0, 0);
    expectEqual("RemoteInputRegion focus loss key virtual", static_cast<int>(keys.back().virtualKey), 0x31);
    expectEqual("RemoteInputRegion focus loss pointer event count", static_cast<int>(pointers.size()), 2);
    expectEqual("RemoteInputRegion focus loss pointer up", pointers.back().pressed ? 1 : 0, 0);

    expectEqual("RemoteInputRegion second focus loss no change", region.onFocusChanged(false) ? 1 : 0, 0);
    expectEqual("RemoteInputRegion second focus loss no extra key", static_cast<int>(keys.size()), 2);
    expectEqual("RemoteInputRegion second focus loss no extra pointer", static_cast<int>(pointers.size()), 2);
}

void testCommittedTextAvoidsPrintableRawKeyDuplication() {
    oneui::RemoteInputRegion region;
    region.setFrame(oneui::Rect{10.0f, 20.0f, 800.0f, 600.0f});
    region.setRemoteSize(oneui::Size{1920.0f, 1080.0f});

    std::vector<oneui::RawKeyEvent> keys;
    std::vector<std::wstring> text;
    region.setOnRawKey([&](const oneui::RawKeyEvent& event) {
        keys.push_back(event);
    });
    region.setOnTextInput([&](const std::wstring& value) {
        text.push_back(value);
    });

    oneui::KeyEvent printable;
    printable.virtualKey = 0x41;
    printable.scanCode = 0x1e;
    expectEqual("RemoteInputRegion printable down handled", region.onKeyDown(printable) ? 1 : 0, 1);
    expectEqual("RemoteInputRegion printable raw key suppressed", static_cast<int>(keys.size()), 0);
    expectEqual("RemoteInputRegion committed text handled", region.onTextInputText(L"中文") ? 1 : 0, 1);
    expectEqual("RemoteInputRegion committed text count", static_cast<int>(text.size()), 1);
    expectEqual("RemoteInputRegion committed text length", static_cast<int>(text.front().size()), 2);
    expectEqual("RemoteInputRegion printable up handled", region.onKeyUp(printable) ? 1 : 0, 1);
    expectEqual("RemoteInputRegion printable raw key up suppressed", static_cast<int>(keys.size()), 0);

    oneui::KeyEvent modified = printable;
    modified.control = true;
    expectEqual("RemoteInputRegion modified down handled", region.onKeyDown(modified) ? 1 : 0, 1);
    expectEqual("RemoteInputRegion modified raw key forwarded", static_cast<int>(keys.size()), 1);
    region.onKeyUp(modified);
    expectEqual("RemoteInputRegion modified raw key up forwarded", static_cast<int>(keys.size()), 2);
}

} // namespace

int main() {
    testFitMappingLetterboxesAndMapsRemoteCoordinates();
    testStretchMappingUsesFullFrame();
    testPointerIgnoredUntilRemoteSizeKnown();
    testRawKeyAndReleaseAllInputs();
    testFocusLossReleasesPressedInputs();
    testCommittedTextAvoidsPrintableRawKeyDuplication();

    if (failures != 0) {
        std::cerr << failures << " remote input region behavior test(s) failed.\n";
        return 1;
    }

    return 0;
}
