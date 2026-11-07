#include "oneui/json.h"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

void require(bool value) { if (!value) throw std::runtime_error("JSON boundary assertion failed"); }
int main() {
    using namespace oneui::json;
    try {
        const std::string text = "a\"\\\n\xF0\x9F\x8C\x90";
        auto nested = parse("{\"entries\":[{\"revision\":\"9223372036854775807\",\"ok\":true}]}")["entries"].at(0);
        require(nested["revision"].integer() == INT64_MAX && nested["ok"].boolean());
        require(nested["missing"]["nested"].isNull());
        require(parse(quote(text)).string() == text);
        require(parse(object({{"name", text}}))["name"].string() == text);
        require(parse("18446744073709551615").integer(-1) == -1);
        for (const auto &invalid : {std::string("{\"name\":1,\"name\":2}"), std::string("[] trailing"),
                                   std::string("\"\xFF\""), std::string(66, '[') + std::string(66, ']'), std::string(1024 * 1024 + 1, ' ')}) {
            bool rejected = false;
            try { (void)parse(invalid); } catch (const std::invalid_argument &) { rejected = true; }
            require(rejected);
        }
        std::cout << "JSON ownership, Unicode, integer precision and malformed-input boundaries passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return EXIT_FAILURE; }
}
