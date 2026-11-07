#include "oneui/json.h"
#include "yyjson.h"
#include <charconv>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <unordered_set>

namespace oneui::json {
namespace {
void validate(yyjson_val *value, size_t depth) {
    if (depth > 64) throw std::invalid_argument("JSON nesting limit exceeded");
    if (yyjson_is_arr(value)) {
        const auto count = yyjson_arr_size(value);
        for (size_t i = 0; i < count; ++i) validate(yyjson_arr_get(value, i), depth + 1);
    } else if (yyjson_is_obj(value)) {
        std::unordered_set<std::string_view> names;
        auto iterator = yyjson_obj_iter_with(value);
        while (auto *key = yyjson_obj_iter_next(&iterator)) {
            if (!names.emplace(yyjson_get_str(key), yyjson_get_len(key)).second)
                throw std::invalid_argument("Duplicate JSON key");
            validate(yyjson_obj_iter_get_val(key), depth + 1);
        }
    }
}
}
Value parse(std::string_view text) {
    if (text.empty() || text.size() > 1024 * 1024) throw std::invalid_argument("JSON size limit exceeded");
    auto *document = yyjson_read_opts(const_cast<char *>(text.data()), text.size(), 0, nullptr, nullptr);
    if (!document) throw std::invalid_argument("Invalid JSON");
    std::shared_ptr<void> owner(document, [](void *value) { yyjson_doc_free(static_cast<yyjson_doc *>(value)); });
    auto *root = yyjson_doc_get_root(document);
    validate(root, 0);
    return Value(std::move(owner), root);
}
bool Value::isNull() const { return !value_ || yyjson_is_null(static_cast<yyjson_val *>(value_)); }
bool Value::isObject() const { return value_ && yyjson_is_obj(static_cast<yyjson_val *>(value_)); }
bool Value::isArray() const { return value_ && yyjson_is_arr(static_cast<yyjson_val *>(value_)); }
size_t Value::size() const {
    auto *value = static_cast<yyjson_val *>(value_);
    return isObject() ? yyjson_obj_size(value) : isArray() ? yyjson_arr_size(value) : 0;
}
Value Value::operator[](std::string_view key) const {
    return Value(document_, isObject() ? yyjson_obj_getn(static_cast<yyjson_val *>(value_), key.data(), key.size()) : nullptr);
}
Value Value::at(size_t index) const { return Value(document_, isArray() ? yyjson_arr_get(static_cast<yyjson_val *>(value_), index) : nullptr); }
std::string Value::string(std::string fallback) const {
    auto *value = static_cast<yyjson_val *>(value_);
    return value && yyjson_is_str(value) ? std::string(yyjson_get_str(value), yyjson_get_len(value)) : std::move(fallback);
}
int64_t Value::integer(int64_t fallback) const {
    auto *value = static_cast<yyjson_val *>(value_);
    if (value && yyjson_is_sint(value)) return yyjson_get_sint(value);
    if (value && yyjson_is_uint(value) && yyjson_get_uint(value) <= uint64_t(std::numeric_limits<int64_t>::max())) return int64_t(yyjson_get_uint(value));
    if (value && yyjson_is_str(value)) {
        const auto text = string();
        int64_t number = 0;
        const auto result = std::from_chars(text.data(), text.data() + text.size(), number);
        if (result.ec == std::errc{} && result.ptr == text.data() + text.size()) return number;
    }
    return fallback;
}
bool Value::boolean(bool fallback) const {
    auto *value = static_cast<yyjson_val *>(value_);
    return value && yyjson_is_bool(value) ? yyjson_get_bool(value) : fallback;
}
std::string quote(std::string_view text) {
    if (text.size() > 1024 * 1024) throw std::invalid_argument("JSON string limit exceeded");
    auto *document = yyjson_mut_doc_new(nullptr);
    if (!document) throw std::bad_alloc();
    std::unique_ptr<yyjson_mut_doc, decltype(&yyjson_mut_doc_free)> owner(document, yyjson_mut_doc_free);
    auto *value = yyjson_mut_strncpy(document, text.data(), text.size());
    if (!value) throw std::bad_alloc();
    yyjson_mut_doc_set_root(document, value);
    size_t length = 0;
    std::unique_ptr<char, decltype(&std::free)> result(yyjson_mut_write(document, 0, &length), std::free);
    if (!result) throw std::invalid_argument("Invalid UTF-8 JSON string");
    return std::string(result.get(), length);
}
std::string object(std::initializer_list<std::pair<std::string_view, std::string_view>> fields) {
    if (fields.size() > 256) throw std::invalid_argument("JSON object limit exceeded");
    std::unordered_set<std::string_view> names;
    std::string result = "{";
    for (const auto &field : fields) {
        if (!names.insert(field.first).second) throw std::invalid_argument("Duplicate JSON key");
        if (result.size() > 1) result += ',';
        result += quote(field.first) + ':' + quote(field.second);
        if (result.size() > 1024 * 1024) throw std::invalid_argument("JSON object limit exceeded");
    }
    return result + '}';
}
}
