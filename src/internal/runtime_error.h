#pragma once

#include "oneui/error.h"

namespace oneui::detail {
// No allocation, logging or callbacks, including while handling bad_alloc.
void recordError(ErrorCode code, const char* operation, const char* message) noexcept;
// Only call from an active exception handler.
void recordCurrentException(const char* operation) noexcept;
} // namespace oneui::detail
