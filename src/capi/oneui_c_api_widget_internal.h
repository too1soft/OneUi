#pragma once
#include "oneui/style_sheet.h"
#include "oneui/widget.h"
#include <memory>
#include <string>
#include <vector>

struct OneUiWidgetStyleBinding {
    std::weak_ptr<oneui::Widget> widget;
    std::weak_ptr<oneui::StyleSheet> styleSheet;
    std::string tag;
    std::vector<std::string> classes;
};
struct OneUiWidget {
    std::shared_ptr<oneui::Widget> widget;
    std::shared_ptr<oneui::StyleSheet> styleSheet;
    std::string tag;
    std::vector<std::string> classes;
    std::shared_ptr<OneUiWidgetStyleBinding> styleBinding;
};
