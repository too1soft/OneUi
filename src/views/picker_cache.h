#pragma once
#include "runtime.h"
namespace oneui::views {
struct Instance::PickerCache {
    FontPickerConfig font;
    MediaPickerConfig media;
    std::string source,fit="contain",position="center";
    double opacity=1,minimum=0,maximum=1,step=.01,value=0;
    std::vector<ThemeColorToken> colors;
};
} // namespace oneui::views
