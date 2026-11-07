#pragma once
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace oneui::json {
// Read-only values retain their document. Parsing is strict UTF-8/JSON, bounded
// to 1 MiB and 64 levels, and rejects duplicate object keys. Missing/wrong-type
// lookups return an empty value; applications choose their explicit fallback.
class Value final {
  public:
    Value() = default;
    bool isNull() const;
    bool isObject() const;
    bool isArray() const;
    size_t size() const;
    Value operator[](std::string_view key) const;
    Value at(size_t index) const;
    std::string string(std::string fallback = {}) const;
    int64_t integer(int64_t fallback = 0) const;
    bool boolean(bool fallback = false) const;
  private:
    std::shared_ptr<void> document_;
    void *value_ = nullptr;
    Value(std::shared_ptr<void> document, void *value) : document_(std::move(document)), value_(value) {}
    friend Value parse(std::string_view text);
};
Value parse(std::string_view text);
std::string quote(std::string_view text);
std::string object(std::initializer_list<std::pair<std::string_view, std::string_view>> fields);
}
