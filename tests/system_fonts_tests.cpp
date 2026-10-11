#include "oneui/fonts.h"
#include "oneui/controls/label.h"
#include <cmath>
#include <limits>
#include "oneui/oneui_c_api.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <thread>

int main() {
    try {
        for (const std::string text : {"", "Operating system", "主机分组", "مرحبا", "👩‍💻"}) {
            float width = -1;
            if (oneui_text_measure_width_utf8({text.data(), text.size()}, {nullptr, 0}, 12, 400, 1, &width) != 1 ||
                !std::isfinite(width) || width < 0 || (!text.empty() && width == 0))
                throw std::runtime_error("widget-free text shaping failed");
        }
        const std::string sample = "Operating system";
        float width = 0;
        oneui::Label label(L"Operating system");
        label.setFontSize(12);
        oneui_text_measure_width_utf8({sample.data(), sample.size()}, {nullptr,0},12,400,1,&width);
        if (std::abs(width-label.naturalTextSize().width) > 0.001f)
            throw std::runtime_error("text measurement must match Label without constructing a widget");
        if (oneui_text_measure_width_utf8({"x",1},{nullptr,0},0,400,1,&width) ||
            oneui_text_measure_width_utf8({"x",1},{nullptr,0},12,400,1,nullptr) ||
            oneui_text_measure_width_utf8({"\xff",1},{nullptr,0},12,400,1,&width) ||
            oneui_text_measure_width_utf8({"x",1},{nullptr,0},12,400,std::numeric_limits<float>::quiet_NaN(),&width))
            throw std::runtime_error("invalid text measurement arguments must fail");
        std::vector<oneui::FontFamilyInfo> cpp;
        std::exception_ptr failure;
        std::thread worker([&] { try { cpp = oneui::enumerateSystemFonts(); } catch (...) { failure = std::current_exception(); } });
        worker.join();
        if (failure) std::rethrow_exception(failure);
        std::vector<oneui::FontFamilyInfo> abi;
        const int count = oneui_enumerate_system_fonts([](OneUiUtf8String name, int mono, void* data) {
            static_cast<std::vector<oneui::FontFamilyInfo>*>(data)->push_back({std::string(name.data, name.length), mono != 0});
        }, &abi);
        if (count < 0 || static_cast<std::size_t>(count) != abi.size() || cpp.size() != abi.size())
            throw std::runtime_error("C++ and C ABI snapshots differ");
        for (std::size_t i = 0; i < cpp.size(); ++i) {
            if (cpp[i].family.empty() || cpp[i].family != abi[i].family || cpp[i].monospaced != abi[i].monospaced)
                throw std::runtime_error("C ABI must retain UTF-8 family names and pitch metadata");
            if (i && cpp[i-1].family >= cpp[i].family)
                throw std::runtime_error("snapshot must be sorted and unique");
        }
        if (oneui_enumerate_system_fonts(nullptr, nullptr) != count)
            throw std::runtime_error("null callback must query the count");
#ifdef _WIN32
        if (cpp.empty() || std::none_of(cpp.begin(), cpp.end(), [](const auto& f) { return f.monospaced; }))
            throw std::runtime_error("Windows fixture must expose installed and fixed-pitch fonts");
#endif
        std::cout << "System fonts: " << cpp.size() << "; C++, C ABI and worker-thread snapshot passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
