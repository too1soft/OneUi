#pragma once
#include "oneui/mobile/application.h"
#include "include/core/SkSurface.h"
#include <vector>

namespace oneui::mobile::harmony {
class ApplicationSurface final {
public:
    explicit ApplicationSurface(const ApplicationHost::Factory &factory);
    ApplicationHost core;
    bool viewport(const OneUiMobileViewportV1 &viewport);
    bool paint();
    int width() const { return width_; }
    int height() const { return height_; }
    const std::vector<unsigned char> &pixels() const { return pixels_; }
private:
    int width_ = 0, height_ = 0;
    float scale_ = 1;
    std::vector<unsigned char> pixels_;
    sk_sp<SkSurface> surface_;
};
uint64_t clockNs();
}
