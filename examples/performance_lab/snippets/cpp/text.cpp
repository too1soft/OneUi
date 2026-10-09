#include <memory>
#include <oneui/controls/text_field.h>
#include <oneui/fonts.h>
#include <oneui/platform/window.h>

std::shared_ptr<oneui::TextField> example_text() {
    auto input = std::make_shared<oneui::TextField>(L"输入说明");
    input->setMultiline(true);
    input->setClipboard(std::make_shared<oneui::SystemClipboard>());
    input->setText(L"中文 · 👩‍💻 · العربية\n第二行");
    input->setOnChanged([](const std::wstring &value) {
        // Save the draft here, on the UI thread.
        (void)value;
    });
    return input;
}

// Call this on a worker; publish the returned snapshot through the UI dispatcher.
std::vector<oneui::FontFamilyInfo> example_system_fonts() { return oneui::enumerateSystemFonts(); }
