#include "oneui/view.h"
#include "oneui/layout/overlay_host.h"
#include "oneui/controls/popup.h"
#include "oneui/style_sheet.h"
#include "oneui/style_adapter.h"
#include <iostream>
#include <stdexcept>

namespace {
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void verify() {
    using namespace oneui;
    auto dark = std::make_shared<StyleSheet>();
    auto light = std::make_shared<StyleSheet>();
    auto root = std::make_shared<View>();
    auto branch = std::make_shared<View>();
    auto leaf = std::make_shared<View>();
    root->add(branch); branch->add(leaf);
    root->setStyleScope(dark);
    require(leaf->styleScope() == dark, "existing descendants inherit");
    branch->setStyleScope(light);
    root->setStyleScope({});
    require(leaf->styleScope() == light, "explicit nested scope survives parent removal");
    auto later = std::make_shared<View>(); branch->add(later);
    require(later->styleScope() == light, "new descendants inherit before paint");
    branch->clearChildren();
    require(!leaf->styleScope() && !later->styleScope(), "detach clears inherited scope");
    root->setStyleScope(dark); root->add(leaf);
    require(leaf->styleScope() == dark, "reparent receives new material");
    branch->setStyleScope({});
    require(branch->styleScope() == dark, "clearing explicit scope restores inherited one");
    int calls = 0; leaf->setOnStyleScopeChanged([&] { ++calls; });
    root->setStyleScope(dark);
    require(calls == 0, "unchanged scope does not re-enter style callback");
    auto popup = std::make_shared<Popup>();
    popup->setAnchor(leaf); popup->setContent(later); popup->setStyleScope(light);
    require(leaf->styleScope() == dark && later->styleScope() == light, "popup styles content, never page anchor");
    OverlayHost host; host.setStyleScope(dark); host.setContent(root); host.addOverlay(popup);
    require(root->styleScope() == dark && later->styleScope() == light, "overlay preserves local boundary");
    host.clearOverlays();
    require(popup->styleScope() == light, "detaching transient retains its explicit palette");
    std::string error;
    require(dark->addRulesFromCss("select { color:#eeeeee; background:#222222; content-background:#333333; }", &error), "dark CSS");
    require(light->addRulesFromCss("select { color:#222222; background:#fafafa; content-background:#ffffff; } select:hover { color:#111111; }", &error), "light CSS");
    dark->setTransientStyleSheet(light);
    const auto select = selectStyleOverrideFromStyleSheet(*dark, {"select", {}, 0});
    require(select.normal->foreground->r == 238 && select.normal->popupBackground->r == 255 && select.normal->optionForeground->r == 34, "select keeps light trigger ink and light popup with dark ink");
    require(select.hovered->optionForeground->r == 17, "popup hover follows transient palette");
    dark->setTransientStyleSheet({});
    require(selectStyleOverrideFromStyleSheet(*dark, {"select", {}, 0}).normal->optionForeground->r == 238, "clear transient restores base palette");
    // Retained descendants can outlive an owner without calling its invalidator.
    auto retained = std::make_shared<View>();
    { auto owner = std::make_shared<View>(); owner->setStyleScope(light); owner->add(retained); }
    require(!retained->styleScope(), "owner destruction clears inherited scope safely");
}
}
int main() { try { verify(); std::cout << "style scopes passed\n"; return 0; }
catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; } }
