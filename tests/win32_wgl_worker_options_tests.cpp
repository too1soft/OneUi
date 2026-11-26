#include "platform/win32/window_win32_wgl_worker_options.h"

#include <array>
#include <iostream>
#include <stdexcept>

namespace {

using oneui::win32::WglWorkerOptions;

void require(bool result, const char* reason) {
    if (!result) throw std::runtime_error(reason);
}

void set(const wchar_t* key, const wchar_t* value) {
    if (value) require(SetEnvironmentVariableW(key, value) != FALSE, "could not set fixture flag");
    else SetEnvironmentVariableW(key, nullptr);
}

bool allOptions(const WglWorkerOptions& options, bool expected) {
    return options.enabled == expected && options.afterFirstFrame == expected &&
        options.prewarm == expected && options.deferCensus == expected &&
        options.scratch == expected && options.retainDamage == expected &&
        options.presentLatest == expected && options.softwareHandoff == expected &&
        options.rehearsePresents == expected && options.foldVirtualRowDamage == expected;
}

void verifyStartupDefaults() {
    constexpr std::array keys{
        L"ONEUI_WIN32_WGL_STARTUP_WORKER", L"ONEUI_WIN32_DEFER_OPENGL",
        L"ONEUI_WIN32_GL_WORKER", L"ONEUI_WIN32_GL_WORKER_AFTER_FIRST_FRAME",
        L"ONEUI_WIN32_GL_WORKER_PREWARM", L"ONEUI_WIN32_GL_WORKER_DEFER_CENSUS",
        L"ONEUI_WIN32_GL_WORKER_SCRATCH", L"ONEUI_WIN32_GL_WORKER_DAMAGE",
        L"ONEUI_WIN32_GL_WORKER_PRESENT_LATEST", L"ONEUI_WIN32_WGL_SOFTWARE_HANDOFF",
        L"ONEUI_WIN32_GL_WORKER_REHEARSE_PRESENTS",
        L"ONEUI_WIN32_FOLD_FIRST_VIRTUAL_ROW_DAMAGE_WORKER",
    };
    for (const auto* key : keys) set(key, nullptr);
    require(allOptions(WglWorkerOptions::fromEnvironment(true), false), "SDK default changed without opt-in");
    set(L"ONEUI_WIN32_WGL_STARTUP_WORKER", L"1");
    require(allOptions(WglWorkerOptions::fromEnvironment(true), true), "product defaults are incomplete");
    require(allOptions(WglWorkerOptions::fromEnvironment(false), false), "ANGLE acquired WGL defaults");

    set(L"ONEUI_WIN32_WGL_SOFTWARE_HANDOFF", L"0");
    const auto noHandoff = WglWorkerOptions::fromEnvironment(true);
    require(noHandoff.enabled && !noHandoff.softwareHandoff, "explicit feature opt-out ignored");
    set(L"ONEUI_WIN32_WGL_SOFTWARE_HANDOFF", nullptr);

    set(L"ONEUI_WIN32_FOLD_FIRST_VIRTUAL_ROW_DAMAGE_WORKER", L"0");
    const auto separateRowPaint = WglWorkerOptions::fromEnvironment(true);
    require(separateRowPaint.enabled && !separateRowPaint.foldVirtualRowDamage,
        "explicit virtual row damage opt-out ignored");
    set(L"ONEUI_WIN32_FOLD_FIRST_VIRTUAL_ROW_DAMAGE_WORKER", nullptr);

    set(L"ONEUI_WIN32_DEFER_OPENGL", L"1");
    require(allOptions(WglWorkerOptions::fromEnvironment(true), false), "deferred WGL startup was replaced");
    set(L"ONEUI_WIN32_DEFER_OPENGL", nullptr);

    set(L"ONEUI_WIN32_GL_WORKER", L"0");
    require(allOptions(WglWorkerOptions::fromEnvironment(true), false), "explicit direct WGL was replaced");
    set(L"ONEUI_WIN32_GL_WORKER", L"1");
    set(L"ONEUI_WIN32_GL_WORKER_DAMAGE", L"1");
    const auto legacy = WglWorkerOptions::fromEnvironment(true);
    require(legacy.enabled && legacy.retainDamage && !legacy.afterFirstFrame &&
        !legacy.scratch && !legacy.softwareHandoff && !legacy.rehearsePresents && !legacy.foldVirtualRowDamage,
        "legacy worker gained unrequested defaults");

    set(L"ONEUI_WIN32_GL_WORKER", nullptr);
    set(L"ONEUI_WIN32_GL_WORKER_DAMAGE", nullptr);
    set(L"ONEUI_WIN32_WGL_STARTUP_WORKER", L"0");
    require(allOptions(WglWorkerOptions::fromEnvironment(true), false), "startup worker opt-out ignored");
}

} // namespace

int main() {
    try {
        verifyStartupDefaults();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
