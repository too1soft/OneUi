#pragma once
#include "oneui/platform/window.h"
#include "oneui/style_sheet.h"

struct OneUiWindow {
    std::unique_ptr<oneui::Window> window;
    std::shared_ptr<oneui::StyleSheet> styleSheet;
    std::shared_ptr<oneui::Widget> rootContent;
    std::string pendingLayoutSnapshot;
    bool hasPendingLayoutSnapshot = false;
};
