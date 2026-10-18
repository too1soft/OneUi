#include "oneui/style_adapter.h"
#include "oneui/style_sheet.h"
#include "oneui/ui_template_support.h"
#include "support/recording_canvas.h"
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void verify() {
    using namespace oneui;
    // Both .one compilation and ViewMount::setStyles use this typed CSS contract.
    const auto mapped = ui::syntax::css(
        "Button {background:#12345680;background-opacity:0.5;color:#ffffff;}"
        "SearchInput {background:#123456;background-opacity:0.28;}"
        "Select {background-opacity:0.28;content-background-opacity:0.96;}");
    StyleSheet declarative;
    std::string mappedError;
    require(declarative.addRulesFromCss(mapped, &mappedError), "typed material CSS parses");
    const auto material = declarative.resolve({"button", {"one-Button"}, 0});
    require(material.background.color->a == 64 && material.foreground->a == 255,
            "typed material changes fill only, preserving text opacity");
    require(declarative.resolve({"input", {"one-SearchInput"}, 0}).background.color->a == 71,
            "SearchInput material maps to the native input");
    try {
        ui::syntax::css("Icon {background-opacity:0.5;}");
        require(false, "components without background must still reject material CSS");
    } catch (const std::runtime_error& error) {
        require(std::string(error.what()).find("not consumed") != std::string::npos,
                "unsupported material CSS has the existing diagnostic");
    }
    StyleSheet sheet;
    std::string error;
    sheet.setCustomProperty("--surface-alpha", "50%");
    require(sheet.addRulesFromCss(R"(
        * { background-opacity: var(--surface-alpha); content-background-opacity: 75%; }
        .material { background: #12345680; content-background: #ffffff;
            check-background: #ffffff;
            color: #ffffff; border-color: #abcdef; border-width: 1px;
            outline-color: #abcdef; outline-width: 1px; }
        .material:hover { background: #123456; }
        .material:disabled { background: #222222; }
        .material:selected { background: #1155aa; }
        .opaque { background-opacity: 100%; }
    )", &error), "parse multipliers");
    StyleNode node{"button", {"material"}, StyleStateNone};
    const auto normal = sheet.resolve(node);
    require(normal.background.color->a == 64, "multiply existing alpha once");
    require(normal.content.backgroundColor->a == 191, "independent content alpha");
    require(normal.foreground->a == 255 && normal.borderColor->a == 255 && normal.outlineColor->a == 255, "keep ink and focus opaque");
    require(sheet.resolve(node).background.color->a == 64, "cache must not compound alpha");
    for (auto state : {StyleStateHover, StyleStateActive, StyleStateDisabled, StyleStateSelected, StyleStateFocus}) {
        node.state = state;
        const auto resolved = sheet.resolve(node);
        const auto expected = (state == StyleStateActive || state == StyleStateFocus) ? 64 : 127;
        require(resolved.background.color->a == expected, "state cascade must preserve multiplier");
    }
    node.state = StyleStateNone;
    const auto button = buttonStyleOverrideFromStyleSheet(sheet, node);
    require(button.normal->background->a == 64 && button.hovered->background->a == 127, "button adapters");
    require(textFieldStyleOverrideFromStyleSheet(sheet, node).normal->background->a == 64, "text field adapter");
    require(selectStyleOverrideFromStyleSheet(sheet, node).normal->popupBackground->a == 191, "select popup adapter");
    require(listStyleOverrideFromStyleSheet(sheet, node).normal->rowBackground->a == 191, "list content adapter");
    require(tableStyleOverrideFromStyleSheet(sheet, node).headerBackground->a == 191, "table header adapter");
    require(tableStyleOverrideFromStyleSheet(sheet, node).checkBackground->a == 127, "table checkbox background");
    require(popupStyleOverrideFromStyleSheet(sheet, node).background->a == 64, "popup adapter");
    require(switchStyleOverrideFromStyleSheet(sheet, node).normal->trackBackground->a == 64, "switch adapter");
    sheet.setCustomProperty("--surface-alpha", "0%");
    require(sheet.resolve(node).background.color->a == 0, "live token changes invalidate cache, including zero");
    node.classes.push_back("opaque");
    require(sheet.resolve(node).background.color->a == 128, "per-control opt out");

    StyleBox direct;
    direct.background.color = Color{20, 40, 60, 255};
    direct.backgroundOpacity = 0.5f;
    direct.content.backgroundColor = Color{20, 40, 60, 128};
    direct.contentBackgroundOpacity = 0.25f;
    test_support::RecordingCanvas canvas;
    paintStyleBox(canvas, {0,0,100,100}, direct);
    require(canvas.fillRects.size() == 2 && canvas.fillRects[0].color.a == 127 && canvas.fillRects[1].color.a == 32, "programmatic paint parity");
    require(buttonStateStyleOverrideFromStyleBox(direct).background->a == 127, "programmatic adapter parity");
    StyleSheet gradient;
    require(gradient.addRulesFromCss("panel { background: linear-gradient(90deg, #12345680, #ffffff); background-opacity: .5; }", &error), "gradient parse");
    const auto grad = gradient.resolve({"panel",{},0});
    require(grad.background.gradient->stops.front().color.a == 64 && grad.background.gradient->stops.back().color.a == 127, "gradient stop alpha");
    for (const auto* value : {"nan", "inf", "%", "nope"}) {
        StyleSheet invalid;
        require(!invalid.addRulesFromCss(std::string("panel { background-opacity: ") + value + "; }", &error), "reject invalid opacity");
    }
}
}
int main() {
    try { verify(); std::cout << "background opacity contracts passed\n"; return 0; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
