#include "internal/build_features.h"
#include "internal/runtime_config.h"
#include "internal/unicode.h"
#include "oneui/platform/window.h"
#include <cstdlib>
#include <iostream>
#include <optional>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {
void require(bool value, const char* reason) { if (!value) throw std::runtime_error(reason); }
class Environment {
public:
    explicit Environment(const char* name) : name_(name) {
#ifdef _WIN32
        const auto key = oneui::unicode::fromUtf8(name_);
        const auto length = GetEnvironmentVariableW(key.c_str(), nullptr, 0);
        if (length) {
            old_.emplace(length, L'\0');
            old_->resize(GetEnvironmentVariableW(key.c_str(), old_->data(), length));
        }
#else
        if (const auto* value = std::getenv(name_.c_str())) old_ = oneui::unicode::fromUtf8(value);
#endif
    }
    ~Environment() { set(old_ ? old_->c_str() : nullptr); }
    void set(const wchar_t* value) {
#ifdef _WIN32
        SetEnvironmentVariableW(oneui::unicode::fromUtf8(name_).c_str(), value);
#else
        if (value) setenv(name_.c_str(), oneui::unicode::toUtf8(value).c_str(), 1);
        else unsetenv(name_.c_str());
#endif
    }
private:
    std::string name_;
    std::optional<std::wstring> old_;
};
}

int main() {
    try {
        Environment tools("ONEUI_DEVTOOLS"), gpu("ONEUI_ENABLE_GPU"), renderer("ONEUI_RENDERER"),
            trace("ONEUI_RENDER_TRACE"), traceFile("ONEUI_RENDER_TRACE_FILE");
        tools.set(L"1"); gpu.set(L"1"); renderer.set(L"software");
        trace.set(L"1"); traceFile.set(L"诊断/trace.log");
        const auto config = oneui::internal::RuntimeConfig::fromEnvironment();
        require(!config.gpuEnabled, "software renderer overrides GPU enable");
#if ONEUI_DEVELOPER_TOOLS
        require(config.developerTools && config.renderTrace && config.renderTraceFile == L"诊断/trace.log", "QA config preserves Unicode path and flags");
#else
        require(!config.developerTools && !config.renderTrace && config.renderTraceFile.empty(), "production ignores diagnostic environment variables");
#endif
        renderer.set(nullptr); gpu.set(L"0");
        require(!oneui::internal::RuntimeConfig::fromEnvironment().gpuEnabled, "GPU can be disabled independently");
        gpu.set(nullptr);
        require(oneui::internal::RuntimeConfig::fromEnvironment().gpuEnabled, "GPU enabled by default");
        gpu.set(L"0"); trace.set(L"0");

        oneui::WindowOptions options; options.visible = false;
        auto window = oneui::Window::create(options);
        window->initialize();
        const auto initial = window->developerSnapshot();
#if ONEUI_DEVELOPER_TOOLS
        require(initial.supported && initial.enabled, "QA environment activates recorder");
        require(window->setDeveloperTools({false, false, 16}), "QA API can disable tools");
        require(!window->developerSnapshot().enabled, "disable releases recorder");
        require(window->setDeveloperTools({true, false, 16}), "QA API can enable tools");
        require(window->developerSnapshot().enabled, "re-enable starts recorder");
#else
        require(!initial.supported && !initial.enabled, "Release window ignores environment activation");
        require(!window->setDeveloperTools({true, true, 16}), "Release API cannot enable tools");
        require(!window->developerSnapshot().enabled, "Release recorder remains absent");
        // Empty path is never valid for file creation; no test output is created.
        require(!window->exportDeveloperReport(L""), "Release export is unavailable");
#ifdef _WIN32
        bool receivedF12 = false;
        window->setRawKeyHandler([&](const oneui::KeyEvent& event) {
            receivedF12 = event.virtualKey == VK_F12; return true;
        });
        SendMessageW(static_cast<HWND>(window->nativeHandle()), WM_KEYDOWN, VK_F12, 0);
        require(receivedF12, "Release forwards F12 to application");
#endif
#endif
        window.reset();
        std::cout << "Runtime configuration and developer tool activation policy passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
