#pragma once
#include "oneui/widget.h"
#include <map>
#include <memory>
#include <optional>
#include <vector>

namespace oneui::platform {
struct AccessibleText {
    std::wstring value;
    std::size_t caret = 0, start = 0, end = 0;
    bool multiline = false;
};
struct AccessibleNumber {
    double value = 0, minimum = 0, maximum = 1, step = 0;
    bool writable = false;
};
struct AccessibleNode {
    std::uint64_t id = 0, parent = 0;
    std::weak_ptr<Widget> widget;
    AccessibilityInfo info;
    Rect frame;
    bool focusable = false, sensitive = false;
    std::optional<AccessibleText> text;
    std::optional<AccessibleNumber> number;
};
// Native proxies keep this context, not widgets or the owning window, alive.
// All operations run on the window's UI thread and revalidate tree membership.
class AccessibilityContext {
public:
    std::function<bool()> valid, exposed;
    std::function<std::shared_ptr<Widget>()> root;
    std::function<bool(Widget*)> focus;
    std::function<std::wstring()> title;
    std::function<std::optional<Rect>(Rect, bool)> bounds;
    std::vector<AccessibleNode> snapshot();
    std::optional<AccessibleNode> resolve(std::uint64_t id);
    bool focusNode(std::uint64_t id);
    bool press(std::uint64_t id);
    bool setValue(std::uint64_t id, const std::wstring& value);
    bool selectText(std::uint64_t id, std::size_t start, std::size_t end);
    bool replaceText(std::uint64_t id, std::size_t start, std::size_t end, const std::wstring& value);
    bool setNumber(std::uint64_t id, double value);
    bool stepNumber(std::uint64_t id, bool increase);
    static bool pressable(AccessibilityRole role);
private:
    std::map<std::weak_ptr<Widget>, std::uint64_t, std::owner_less<std::weak_ptr<Widget>>> ids_;
    std::uint64_t next_ = 1;
};
class NativeAccessibility {
public:
    virtual ~NativeAccessibility() = default;
    virtual void sync() = 0;
};
std::unique_ptr<NativeAccessibility> makeNativeAccessibility(std::shared_ptr<AccessibilityContext>, void* nativeView);
} // namespace oneui::platform
