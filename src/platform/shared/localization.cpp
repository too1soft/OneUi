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

constexpr std::size_t textCount = 22;
std::mutex textMutex;
std::array<std::optional<std::wstring>, textCount> overrides;
constexpr std::array<const wchar_t*, textCount> english{
    L"Minimize window", L"Maximize window", L"Restore window", L"Close window", L"OK", L"Cancel",
    L"Show main window", L"Quit", L"Loading…",
    L"Find",
    L"Replace",
    L"No results",
    L"Invalid pattern",
    L"{0} of {1}",
    L"Replace",
    L"Replace all",
    L"Match case",
    L"Use regular expression",
    L"Match whole word",
    L"Previous match",
    L"Next match",
    L"Close"
};
constexpr std::array<const wchar_t*, textCount> chinese{
    L"最小化窗口", L"最大化窗口", L"还原窗口", L"关闭窗口", L"确定", L"取消",
    L"显示主界面", L"退出程序", L"正在加载…",
    L"查找",
    L"替换",
    L"无结果",
    L"表达式无效",
    L"{0}/{1}",
    L"替换",
    L"全部替换",
    L"区分大小写",
    L"使用正则表达式",
    L"全字匹配",
    L"上一个匹配",
    L"下一个匹配",
    L"关闭"
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
