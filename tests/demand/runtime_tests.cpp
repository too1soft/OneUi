#include "oneui/ui_declarative.h"
#include "oneui/controls/label.h"
#include <iostream>

int main() {
    try {
        oneui::ui::Mount mount;
        auto text = mount.make("Text");
        mount.set(text, "text", std::wstring(L"中文 العربية 👋"));
        if (text.as<oneui::Label>()->text() != L"中文 العربية 👋") return 1;
        for (const auto* missing : {"CodeEditor", "TimeSeriesChart", "Typo"}) {
            try {
                mount.make(missing);
                throw std::runtime_error("unlinked component silently accepted");
            } catch (const std::invalid_argument& error) {
                if (std::string(error.what()).find("Component not linked") == std::string::npos)
                    throw;
            }
        }
        std::cout << "Selected components work; omitted and unknown components fail explicitly.\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
