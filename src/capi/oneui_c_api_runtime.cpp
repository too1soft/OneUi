#include "oneui/oneui_c_api.h"
#include "oneui_c_api_error.h"
#include "../internal/runtime_error.h"

static_assert(static_cast<int>(oneui::ErrorCode::None) == OneUiErrorNone);
static_assert(static_cast<int>(oneui::ErrorCode::InvalidArgument) == OneUiErrorInvalidArgument);
static_assert(static_cast<int>(oneui::ErrorCode::OutOfMemory) == OneUiErrorOutOfMemory);
static_assert(static_cast<int>(oneui::ErrorCode::OperationFailed) == OneUiErrorOperationFailed);
static_assert(static_cast<int>(oneui::ErrorCode::UnknownException) == OneUiErrorUnknownException);
namespace oneui::capi {
void recordError(OneUiErrorCode code, const char* operation, const char* message) noexcept {
    detail::recordError(static_cast<ErrorCode>(code), operation, message);
}
void recordCurrentException(const char* operation) noexcept { detail::recordCurrentException(operation); }
}
const char* oneui_last_error(void) { return oneui::lastError(); }
OneUiErrorCode oneui_last_error_code(void) { return static_cast<OneUiErrorCode>(oneui::lastErrorCode()); }
void oneui_clear_error(void) { oneui::clearError(); }

const char* oneui_version(void) {
    return "0.1.0";
}

unsigned int oneui_utf8_abi_version(void) {
    return ONEUI_UTF8_ABI_VERSION;
}
