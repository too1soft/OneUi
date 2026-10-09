#pragma once

#include "oneui/oneui_c_api.h"

namespace oneui::capi {

// No allocation, logging, or callbacks: safe even while handling bad_alloc.
void recordError(OneUiErrorCode code, const char* operation, const char* message) noexcept;

// Call only inside an active exception handler, before performing any cleanup.
void recordCurrentException(const char* operation) noexcept;

} // namespace oneui::capi
