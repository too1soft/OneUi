#include "internal/runtime_error.h"
#include <cstddef>
#include <exception>
#include <new>
#include <stdexcept>

namespace {
struct ErrorState {
    oneui::ErrorCode code = oneui::ErrorCode::None;
    char message[1024]{};
};
thread_local ErrorState currentError;

// Exception messages are not required to be UTF-8. Replace invalid bytes and
// truncate only between Unicode scalars, without allocating another string.
void appendUtf8(char* output, size_t capacity, size_t& used, const char* input) noexcept {
    if (!input) return;
    while (*input && used + 1 < capacity) {
        const auto first = static_cast<unsigned char>(*input);
        size_t width = first < 0x80 ? 1 : first >= 0xc2 && first <= 0xdf ? 2
            : first >= 0xe0 && first <= 0xef ? 3 : first >= 0xf0 && first <= 0xf4 ? 4 : 0;
        for (size_t i = 1; i < width; ++i) {
            const auto next = static_cast<unsigned char>(input[i]);
            if (next < 0x80 || next > 0xbf) { width = 0; break; }
        }
        if (width >= 3) {
            const auto second = static_cast<unsigned char>(input[1]);
            if ((first == 0xe0 && second < 0xa0) || (first == 0xed && second >= 0xa0)
                || (first == 0xf0 && second < 0x90) || (first == 0xf4 && second >= 0x90)) width = 0;
        }
        if (!width) { output[used++] = '?'; ++input; continue; }
        if (width >= capacity - used) break;
        for (size_t i = 0; i < width; ++i) output[used++] = *input++;
    }
    output[used] = '\0';
}
}

namespace oneui::detail {
void recordError(oneui::ErrorCode code, const char* operation, const char* message) noexcept {
    currentError.code = code;
    currentError.message[0] = '\0';
    size_t used = 0;
    appendUtf8(currentError.message, sizeof(currentError.message), used, operation);
    appendUtf8(currentError.message, sizeof(currentError.message), used, ": ");
    appendUtf8(currentError.message, sizeof(currentError.message), used, message);
}

void recordCurrentException(const char* operation) noexcept {
    try {
        throw;
    } catch (const std::bad_alloc&) {
        recordError(oneui::ErrorCode::OutOfMemory, operation, "Memory allocation failed");
    } catch (const std::invalid_argument& error) {
        recordError(oneui::ErrorCode::InvalidArgument, operation, error.what());
    } catch (const std::out_of_range& error) {
        recordError(oneui::ErrorCode::InvalidArgument, operation, error.what());
    } catch (const std::exception& error) {
        recordError(oneui::ErrorCode::OperationFailed, operation, error.what());
    } catch (...) {
        recordError(oneui::ErrorCode::UnknownException, operation, "Unknown native exception");
    }
}
}

namespace oneui {
const char* lastError() noexcept { return currentError.message; }
ErrorCode lastErrorCode() noexcept { return currentError.code; }
void clearError() noexcept { currentError.code = ErrorCode::None; currentError.message[0] = '\0'; }
} // namespace oneui
