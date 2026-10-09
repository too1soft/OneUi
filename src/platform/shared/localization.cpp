#include "oneui/localization.h"

#include <array>
#include <mutex>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace oneui {
namespace {

constexpr std::size_t textCount = 9;
std::mutex textMutex;
std::array<std::optional<std::wstring>, textCount> overrides;
constexpr std::array<const wchar_t*, textCount> english{
    L"Minimize window", L"Maximize window", L"Restore window", L"Close window", L"OK", L"Cancel",
    L"Show main window", L"Quit", L"Loading…"
};
constexpr std::array<const wchar_t*, textCount> chinese{
    L"最小化窗口", L"最大化窗口", L"还原窗口", L"关闭窗口", L"确定", L"取消",
    L"显示主界面", L"退出程序", L"正在加载…"
};

bool systemUsesChinese() {
#ifdef _WIN32
    return PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_CHINESE;
#else
    return false;
#endif
}

} // namespace

bool setBuiltinText(BuiltinText key, std::optional<std::wstring> value) {
    const auto index = static_cast<std::size_t>(key);
    if (index >= textCount || (value && (value->empty() || value->size() > 1024))) return false;
    std::lock_guard<std::mutex> guard(textMutex);
    overrides[index] = std::move(value);
    return true;
}

std::wstring builtinText(BuiltinText key) {
    const auto index = static_cast<std::size_t>(key);
    if (index >= textCount) return {};
    std::lock_guard<std::mutex> guard(textMutex);
    if (overrides[index]) return *overrides[index];
    return systemUsesChinese() ? chinese[index] : english[index];
}

} // namespace oneui
