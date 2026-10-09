#include "FormSnippet.g.h"
#include "TerminalSnippet.g.h"
#include <oneui/controls/terminal_view.h>

// Instantiates the generated template; generation alone cannot type-check it.
oneui::ui::Element example_template_check(oneui::ui::Mount &mount) {
    struct EmptyModel {
    } model;
    return build_FormSnippet(model, mount);
}
oneui::ui::Element example_terminal_template(oneui::ui::Mount &mount) {
    struct EmptyModel {
    } model;
    auto terminal = std::make_shared<oneui::TerminalView>();
    mount.registerNative("terminal", terminal);
    return build_TerminalSnippet(model, mount);
}
