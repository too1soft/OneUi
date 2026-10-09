#pragma once
#include "oneui/application.h"
#include <stdexcept>

struct OneUiApplication {
    std::unique_ptr<oneui::Application> impl;
};
namespace oneui::capi {
inline Application &desktopApplication(OneUiApplication *value) {
    if (!value || !value->impl)
        throw std::invalid_argument("Application is required");
    return *value->impl;
}
} // namespace oneui::capi
