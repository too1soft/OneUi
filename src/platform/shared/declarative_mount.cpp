#include "oneui/ui_declarative.h"
#include "oneui/platform/window.h"

namespace oneui::ui {
std::shared_ptr<Clipboard> defaultMountClipboard() {
    return std::make_shared<SystemClipboard>();
}
} // namespace oneui::ui
