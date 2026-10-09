#pragma once

#include "oneui/export.h"

namespace oneui {

enum class ErrorCode {
    None = 0,
    InvalidArgument = 1,
    OutOfMemory = 2,
    OperationFailed = 3,
    UnknownException = 4,
};

// Thread-local, bounded diagnostics shared by core controls and C bindings.
// Success leaves the last failure intact until explicitly cleared. Copy the
// borrowed UTF-8 message before another operation on the same thread.
ONEUI_API const char* lastError() noexcept;
ONEUI_API ErrorCode lastErrorCode() noexcept;
ONEUI_API void clearError() noexcept;

} // namespace oneui
