#include "internal/runtime_config.h"
#include "internal/build_features.h"
#include "internal/unicode.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cstdlib>
#endif

namespace oneui::internal {
namespace {
std::wstring environment(const char* name) {
#ifdef _WIN32
    const auto wideName = unicode::fromUtf8(name);
    const DWORD required = GetEnvironmentVariableW(wideName.c_str(), nullptr, 0);
    if (!required) return {};
    std::wstring value(required, L'\0');
    const DWORD length = GetEnvironmentVariableW(wideName.c_str(), value.data(), required);
    if (!length || length >= required) return {};
    value.resize(length);
    return value;
#else
    const char* value = std::getenv(name);
    return value ? unicode::fromUtf8(value) : std::wstring{};
#endif
}
}

RuntimeConfig RuntimeConfig::fromEnvironment() {
    RuntimeConfig result;
    const auto gpu = environment("ONEUI_ENABLE_GPU");
    result.gpuEnabled = (gpu.empty() || gpu.front() != L'0')
        && environment("ONEUI_RENDERER") != L"software";
#if ONEUI_DEVELOPER_TOOLS
    result.developerTools = environment("ONEUI_DEVTOOLS") == L"1";
    const auto trace = environment("ONEUI_RENDER_TRACE");
    result.renderTrace = !trace.empty() && trace.front() != L'0';
    if (result.renderTrace) result.renderTraceFile = environment("ONEUI_RENDER_TRACE_FILE");
#endif
    return result;
}
} // namespace oneui::internal
