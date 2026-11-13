#include "oneui/style_sheet.h"
#include <iostream>
#include <exception>
#include <stdexcept>
#include <thread>

namespace {
using oneui::StyleSheet;
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void parse(StyleSheet& sheet, const std::string& css) {
    std::string error;
    if (!sheet.addRulesFromCss(css, &error)) throw std::runtime_error(error);
}
const oneui::StyleNode button{"button", {"cached"}, 0};
const std::string theme =
    ":root { --ink: #123456; } button.cached { color: var(--ink); height: 32px; }";

void independentTokensAndRules() {
    StyleSheet first;
    parse(first, theme);
    first.setCustomProperty("--ink", "#abcdef");
    parse(first, "button.cached { height: 45px; }");
    require(first.resolve(button).foreground->r == 0xab, "first theme edits apply");
    StyleSheet second;
    parse(second, theme);
    const auto original = second.resolve(button);
    require(original.foreground->r == 0x12 && original.height == 32,
        "later independent mounts must not inherit another view's edits");
    require(first.resolve(button).height == 45, "a later mount cannot rewrite the first view");
}

void preserveCallerEnvironment() {
    const std::string css = "button.cached { color: var(--ink, #010203); }";
    StyleSheet plain;
    parse(plain, css);
    require(plain.resolve(button).foreground->r == 1, "fallback in independent sheet");
    for (const auto ink : {"#445566", "#778899"}) {
        StyleSheet supplied;
        supplied.setCustomProperty("--ink", ink);
        parse(supplied, css);
        require(supplied.resolve(button).foreground->r == (ink[1] == '4' ? 0x44 : 0x77),
            "appended CSS must resolve against the caller's tokens");
    }
    StyleSheet appended;
    parse(appended, "button.cached { height: 71px; }");
    parse(appended, theme);
    require(appended.rules().size() == 2 && appended.resolve(button).height == 32,
        "append retains earlier rules and the original cascade order");
}

void transientPalettesBelongToEachMount() {
    auto light = std::make_shared<StyleSheet>();
    auto dark = std::make_shared<StyleSheet>();
    StyleSheet first;
    first.setTransientStyleSheet(light);
    parse(first, theme);
    StyleSheet second;
    second.setTransientStyleSheet(dark);
    parse(second, theme);
    StyleSheet none;
    parse(none, theme);
    require(first.transientStyleSheet() == light && second.transientStyleSheet() == dark &&
        !none.transientStyleSheet(), "cached parsing cannot share a window's transient palette");
    light.reset();
    require(!first.transientStyleSheet(), "parse cache cannot retain a caller-owned palette");
}

void failureIsTransactionalAndRepeated() {
    const std::string invalid = ":root { --ink: #fedcba; }\nbutton.cached { height: bogus; }";
    for (int repeat = 0; repeat != 2; ++repeat) {
        StyleSheet sheet;
        parse(sheet, theme);
        const auto before = sheet.version();
        std::string error;
        require(!sheet.addRulesFromCss(invalid, &error) && !error.empty(),
            "invalid appended rules still fail with a diagnostic");
        require(sheet.version() == before && sheet.resolve(button).foreground->r == 0x12,
            "failed append cannot publish partial root tokens");
        StyleSheet empty;
        error.clear();
        require(!empty.addRulesFromCss(invalid, &error) && empty.rules().empty() &&
            empty.customProperties().empty() && !error.empty(),
            "a failed independent parse cannot seed a successful result");
    }
}

void queryAndLayoutCopiesSurviveOtherThemes() {
    const std::string css = "button.cached { max-width: 90px; } "
        "@container pane (width >= 100px) { button.cached { height: 48px; } }";
    for (int repeat = 0; repeat != 2; ++repeat) {
        StyleSheet sheet;
        parse(sheet, css);
        require(sheet.resolveLayout(button).maxWidth == 90, "layout extension survives copying");
        require(!sheet.forContainers({{"pane", {80, 120}}}).resolve(button).height,
            "query stays inactive under its threshold");
        require(sheet.forContainers({{"pane", {120, 120}}}).resolve(button).height == 48,
            "query remains active in a later independent mount");
    }
    for (int i = 0; i != 20; ++i) {
        StyleSheet other;
        parse(other, "button.cached { height: " + std::to_string(i + 10) + "px; }");
        require(other.resolve(button).height == i + 10, "different CSS cannot reuse the wrong parse");
    }
    StyleSheet restored;
    parse(restored, theme);
    require(restored.resolve(button).foreground->r == 0x12, "eviction cannot change subsequent parsing");
}
}

int main() {
    try {
        independentTokensAndRules();
        preserveCallerEnvironment();
        transientPalettesBelongToEachMount();
        failureIsTransactionalAndRepeated();
        queryAndLayoutCopiesSurviveOtherThemes();
        std::exception_ptr workerError;
        std::thread worker([&] {
            try { independentTokensAndRules(); }
            catch (...) { workerError = std::current_exception(); }
        });
        worker.join();
        if (workerError) std::rethrow_exception(workerError);
        std::cout << "style parse isolation passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
