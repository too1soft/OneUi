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

void sharedRulesDetachWithoutSharingThemeState() {
    StyleSheet original;
    parse(original, theme);
    (void)original.resolve(button); // Warm the source's derived cache first.
    StyleSheet local = original;
    require(local.rules().data() == original.rules().data(),
        "control palette copies must share rule storage instead of duplicating it");
    local.setCustomProperty("--ink", "#abcdef");
    require(local.rules().data() == original.rules().data(),
        "token-only theme edits must keep the common immutable rules");
    require(local.resolve(button).foreground->r == 0xab &&
        original.resolve(button).foreground->r == 0x12,
        "local theme caches must resolve their own tokens after a warm copy");
    parse(local, "button.cached { height: 61px; max-width: 95px; }");
    require(local.rules().data() != original.rules().data() && original.rules().size() == 1,
        "rule edits must detach before changing the shared rule sequence");
    require(original.resolve(button).height == 32 && local.resolve(button).height == 61 &&
        !original.resolveLayout(button).maxWidth && local.resolveLayout(button).maxWidth == 95,
        "detached visual and layout rules preserve both cascades");
    auto survivor = local;
    local = StyleSheet{};
    require(survivor.resolve(button).height == 61, "a copy owns its rules after the source is reset");
    StyleSheet moved = std::move(survivor);
    parse(survivor, theme);
    require(moved.resolve(button).height == 61 && survivor.resolve(button).height == 32,
        "moved-from sheets remain reusable and independent");
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

void copiedComponentStylesStayIndependent() {
    StyleSheet base;
    parse(base, theme);
    (void)base.resolve(button); // Populate a cache before component snapshots.
    auto dark = base;
    StyleSheet light;
    light = base;
    dark.setCustomProperty("--ink", "#eeeeee");
    light.setCustomProperty("--ink", "#222222");
    require(base.resolve(button).foreground->r == 0x12 &&
        dark.resolve(button).foreground->r == 0xee &&
        light.resolve(button).foreground->r == 0x22,
        "component tokens and resolved caches remain local to each snapshot");
    parse(dark, "button.cached {height: 61px; max-width: 73px;}");
    require(dark.resolve(button).height == 61 && dark.resolveLayout(button).maxWidth == 73 &&
        base.resolve(button).height == 32 && light.resolve(button).height == 32 &&
        !light.resolveLayout(button).maxWidth, "appending rules detaches only the edited snapshot");
    oneui::StyleRule programmatic;
    programmatic.selector = "button.cached";
    programmatic.box.height = 86;
    light.addRule(programmatic);
    require(light.resolve(button).height == 86 && dark.resolve(button).height == 61,
        "programmatic rule edits preserve sibling snapshots too");
    const auto before = dark.version();
    std::string error;
    require(!dark.addRulesFromCss(":root {--ink:#000000;} button.cached {height:wrong;}", &error) &&
        dark.version() == before && dark.resolve(button).foreground->r == 0xee,
        "failed edits preserve a shared snapshot and its cached palette");
    auto responsive = base.forContainers({});
    responsive.setCustomProperty("--ink", "#abcdef");
    require(responsive.resolve(button).foreground->r == 0xab && base.resolve(button).foreground->r == 0x12,
        "query-free responsive copies retain independent tokens");
    auto moved = std::move(responsive);
    parse(responsive, "button.cached {height: 19px;}");
    require(responsive.resolve(button).height == 19 && moved.resolve(button).height == 32,
        "moved-from sheets remain usable without changing their moved snapshot");
    base = StyleSheet{};
    require(dark.resolve(button).height == 61 && light.resolve(button).height == 86,
        "component snapshots outlive the original sheet");
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

void transientNodeScopesDoNotAffectCascadeAfterEviction() {
    StyleSheet sheet;
    parse(sheet, theme + " button.cached { max-width: 90px; }");
    for (int i = 0; i != 10000; ++i) {
        const oneui::StyleNode scoped{"button", {"cached", "mount-" + std::to_string(i)},
                                    i % 2 ? oneui::StyleStateHover : 0u};
        require(sheet.resolve(scoped).foreground->r == 0x12, "scope churn preserves declarations");
        require(sheet.resolveLayout(scoped).maxWidth == 90, "scope churn preserves layout");
    }
    sheet.setCustomProperty("--ink", "#abcdef");
    require(sheet.resolve(button).foreground->r == 0xab, "token edits invalidate cached declarations");
    parse(sheet, "button.cached { height: 49px; }");
    require(sheet.resolve(button).height == 49, "rule edits invalidate cached declarations");
    const oneui::StyleNode later{"button", {"latent", "cached", "cached"}, 0};
    require(sheet.resolve(later).height == 49, "unmatched classes preserve the original cascade");
    parse(sheet, "button.latent { height: 71px; }");
    require(sheet.resolve(later).height == 71 && sheet.resolve(button).height == 49,
            "a newly referenced class must invalidate canonical node keys");
    StyleSheet generic;
    parse(generic, ".cached { color: #123456; max-width: 90px; }");
    for (int i = 0; i != 3000; ++i) {
        const oneui::StyleNode node{"tag-" + std::to_string(i), {"cached"}, 0};
        require(generic.resolve(node).foreground->r == 0x12 && generic.resolveLayout(node).maxWidth == 90,
                "evicting distinct node tags preserves paint and layout results");
    }
}

void copiedTokenTablesStaySharedUntilAnActualEdit() {
    StyleSheet base;
    parse(base, theme);
    std::vector<StyleSheet> snapshots(1000, base);
    for (auto& copy : snapshots) {
        require(&copy.customProperties() == &base.customProperties(), "component copies share the token table");
        copy.setCustomProperty("--ink", *base.customProperty("--ink"));
        require(&copy.customProperties() == &base.customProperties(), "unchanged tokens do not detach storage");
        require(copy.resolve(button).foreground->r == 0x12, "sharing preserves token resolution");
    }
    snapshots.front().setCustomProperty("--ink", "#abcdef");
    require(&snapshots.front().customProperties() != &base.customProperties(), "token edit detaches only its snapshot");
    require(snapshots.front().resolve(button).foreground->r == 0xab &&
            snapshots.back().resolve(button).foreground->r == 0x12, "edited and retained palettes stay independent");
    StyleSheet moved = std::move(snapshots.front());
    require(snapshots.front().customProperties().empty() && moved.resolve(button).foreground->r == 0xab,
            "moved-from token storage remains valid");
}
}

int main() {
    try {
        independentTokensAndRules();
        sharedRulesDetachWithoutSharingThemeState();
        preserveCallerEnvironment();
        transientPalettesBelongToEachMount();
        failureIsTransactionalAndRepeated();
        copiedComponentStylesStayIndependent();
        queryAndLayoutCopiesSurviveOtherThemes();
        transientNodeScopesDoNotAffectCascadeAfterEviction();
        copiedTokenTablesStaySharedUntilAnActualEdit();
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
