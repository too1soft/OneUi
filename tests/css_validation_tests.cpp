#include "oneui/ui_template_support.h"
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
using oneui::ui::syntax::css;
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
std::string error(const std::string& source, const std::string& file = "test.one",
                  int line = 1, const std::string& scope = {}, bool query = false,
                  const std::set<std::string>& external = {}) {
    try { css(source, scope, file, line, query, external); }
    catch (const std::runtime_error& e) { return e.what(); }
    throw std::runtime_error("Expected invalid CSS: " + source);
}
void contracts() {
    const std::string source = "Button.primary:hover {color:#ffffff;}";
    require(css(source) == "button.one-Button.primary:hover {color:#ffffff;}\n", "typed selector");
    require(css(source, "page") == "button.one-Button.primary.page:hover {color:#ffffff;}\n", "scope is part of cache identity");
    require(css(source) == css(source, {}, "other.one", 40), "valid output is location independent");
    require(css("Extension {color:#ffffff;}", {}, "a", 1, false, {"Extension"}) ==
            "widget.one-Extension {color:#ffffff;}\n", "external component accepted");
    require(error("Extension {color:#ffffff;}").find("Unknown CSS component") != std::string::npos,
            "external components cannot leak between mounts");
    css(":root {--accent:#123456;}");
    require(error(":root {--accent:#123456;}", "a", 1, "page").find("Scoped :root") != std::string::npos,
            "cached global tokens cannot bypass scope validation");
    css("stack {container-name:main;container-type:size;}");
    require(error("stack {container-name:main;container-type:size;}", "a", 1, {}, true).find("CONTAINER_DEPENDENCY") != std::string::npos,
            "cache cannot bypass query dependency validation");
    css("@container main (width > 100px) {Button {width:40px;}}");
    require(error("@container main (width > 100px) {Button {width:40px;}}", "a", 1, {}, true).find("nested queries") != std::string::npos,
            "cached query cannot be nested");
    for (int i = 0; i != 3; ++i) {
        require(error("Button {invalid:1;}", "fresh.one", 70).rfind("fresh.one:70:1:", 0) == 0,
                "invalid CSS keeps current diagnostic location");
        require(error("Button {invalid:1;}", "next.one", 3).rfind("next.one:3:1:", 0) == 0,
                "failed validation is not remembered");
        require(error("/* unclosed") == "Unclosed CSS comment", "comments still validated");
    }
    require(css("/* comment */\nButton {width:10px;}") == "button.one-Button {width:10px;}\n", "comment normalization");
    const auto failure = error("Button {color:#fff;}\n\nInput {width:-1px;}", "lines.one", 10);
    require(failure.rfind("lines.one:10:1:", 0) == 0, "preserve existing rule diagnostic position");
}
// The same corpus can be compiled with the previous header and compared byte
// for byte. It covers all native tags/properties as well as rejected dialects.
void corpus() {
    std::vector<std::string> sources = {
        "", " \n\t", "/* unclosed", "Button {color:;} tail", "Unknown {color:red;}",
        ":root {--accent:#fff;}", ":root {color:red;}", ".part {gap:4px;}",
        "Button:hover {background:#123456;}", "Input:pressed {color:red;}",
        "Button {background-color:linear-gradient(red,blue);}", "Button, Input {color:red;}",
        "Button {color:red;}\n\nInput {width:-1px;}",
        "@container main (width > 200px) {Button {width:20px;}}",
        "@container main (width > 200px) {stack {container-type:size;}}",
        "@container main (width > 200px) {@container main (height > 2px) {Button {color:red;}}}",
        "Button {display:grid;}", "stack {flex-wrap:wrap;flex-direction:row;}",
        "Button {order:1000001;}", "Button {order:-2;flex-basis:20%;}",
        "DataTable {table-column-widths:auto 30px;table-selection-width:24px;}",
        "DataTable {table-selection-width:23px;}", "DataTable:selected {color:red;}"
    };
    for (const auto& component : oneui::ui::syntax::components()) {
        for (const auto& property : oneui::ui::syntax::cssProperties(component.second)) {
            const std::string value = property == "table-selection-width" ? "32px" :
                property == "table-column-widths" ? "auto 32px" : "1px";
            sources.push_back(component.first + " {" + property + ":" + value + ";}");
        }
        sources.push_back(component.first + " {unsupported:1;}");
    }
    for (const auto& source : sources)
        for (const auto& scope : {std::string{}, std::string{"scope"}})
            for (bool query : {false, true}) {
                try { const auto output = css(source, scope, "corpus.one", 17, query);
                    std::cout << "OK " << output.size() << ':' << output << '\n'; }
                catch (const std::exception& e) { std::cout << "ERROR " << e.what() << '\n'; }
            }
}
}
int main(int argc, char**) {
    try { contracts(); if (argc > 1) corpus();
        else std::cout << "CSS validation contracts passed\n"; return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
