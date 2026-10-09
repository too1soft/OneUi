#include "internal/unicode.h"
#include "platform/shared/notification_backend.h"
#include <algorithm>
#include <set>

namespace oneui::platform {
namespace {
bool text(const std::string &value, std::size_t limit) {
    if (value.size() > limit || value.find('\0') != std::string::npos)
        return false;
    try {
        return unicode::toUtf8(unicode::fromUtf8(value)) == value;
    } catch (...) {
        return false;
    }
}
bool wide(const std::wstring &value, std::size_t limit) {
    if (value.size() > limit || value.find(L'\0') != std::wstring::npos)
        return false;
    try {
        return unicode::fromUtf8(unicode::toUtf8(value)) == value;
    } catch (...) {
        return false;
    }
}
std::string hex(const std::string &value) {
    static const char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(value.size() * 2);
    for (unsigned char c : value) {
        result += digits[c >> 4];
        result += digits[c & 15];
    }
    return result;
}
bool unhex(const std::string &value, std::string &result) {
    if (value.size() % 2)
        return false;
    const auto digit = [](char c) {
        return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
    };
    for (std::size_t i = 0; i < value.size(); i += 2) {
        int a = digit(value[i]), b = digit(value[i + 1]);
        if (a < 0 || b < 0)
            return false;
        result += char((a << 4) | b);
    }
    return text(result, 4096);
}
} // namespace
bool validApplicationId(const std::string &value) {
    if (value.empty() || value.size() > 128 || value.find('.') == std::string::npos)
        return false;
    bool first = true;
    for (unsigned char c : value) {
        if (c == '.') {
            if (first)
                return false;
            first = true;
            continue;
        }
        const bool letter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        if (!letter && c != '_' && (first || (c != '-' && (c < '0' || c > '9'))))
            return false;
        first = false;
    }
    return !first;
}
bool validNotification(const Notification &value, std::string &error) {
    if (value.id.empty() || !text(value.id, 256))
        error = "ID must contain 1–256 UTF-8 bytes";
    else if (!wide(value.title, 256) || !wide(value.body, 4096))
        error = "Invalid or oversized notification text";
    else if (!text(value.parameter, 4096))
        error = "Invalid or oversized application parameter";
    else if (value.actions.size() > 2)
        error = "At most two notification actions are portable";
    else {
        std::set<std::string> ids;
        for (const auto &action : value.actions) {
            if (action.id.empty() || !text(action.id, 128) || action.title.empty() ||
                !wide(action.title, 80) || !ids.insert(action.id).second) {
                error = "Action IDs must be nonempty, unique UTF-8 strings with a visible title";
                return false;
            }
        }
        return true;
    }
    return false;
}
std::string encodeActivation(const ApplicationActivation &value) {
    return "oneui1:" + hex(value.notificationId) + ":" + hex(value.actionId) + ":" +
           hex(value.parameter);
}
bool decodeActivation(const std::string &payload, ApplicationActivation &value) {
    if (payload.size() > 9000 || payload.rfind("oneui1:", 0) != 0)
        return false;
    const auto first = payload.find(':', 7),
               second = payload.find(':', first == std::string::npos ? first : first + 1);
    if (first == std::string::npos || second == std::string::npos ||
        payload.find(':', second + 1) != std::string::npos)
        return false;
    ApplicationActivation decoded;
    if (!unhex(payload.substr(7, first - 7), decoded.notificationId) ||
        !unhex(payload.substr(first + 1, second - first - 1), decoded.actionId) ||
        !unhex(payload.substr(second + 1), decoded.parameter))
        return false;
    if (decoded.notificationId.size() > 256 || decoded.actionId.size() > 128)
        return false;
    value = std::move(decoded);
    return true;
}
} // namespace oneui::platform
