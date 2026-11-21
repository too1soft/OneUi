// Standalone CPU benchmark; does not create a window or bypass style validation.
#include "oneui/ui_template_support.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>

int main(int argc, char** argv) {
    if (argc != 2 && argc != 3) return 2;
    std::ifstream input(argv[1], std::ios::binary);
    if (!input) return 3;
    const std::string source((std::istreambuf_iterator<char>(input)), {});
    try {
        std::string expected;
        for (int run = 0; run != 15; ++run) {
            const auto started = std::chrono::steady_clock::now();
            auto output = oneui::ui::syntax::css(source);
            const double ms = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - started).count();
            if (run == 0) expected = output;
            if (output != expected) return 4;
            std::cout << run << ',' << source.size() << ',' << output.size() << ',' << ms << '\n';
        }
        if (argc == 3) std::ofstream(argv[2], std::ios::binary) << expected;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 5;
    }
}
