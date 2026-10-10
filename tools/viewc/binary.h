#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace oneview
{
using Bytes = std::vector<uint8_t>;
using Hash = std::array<uint8_t, 32>;
constexpr uint32_t none = 0xffffffffu;
constexpr uint64_t max_file = 64ull * 1024 * 1024;
struct Error : std::runtime_error
{
    using std::runtime_error::runtime_error;
};
inline void require(bool ok, const std::string &message)
{
    if (!ok)
        throw Error(message);
}
inline void put(Bytes &b, uint64_t value, unsigned size = 4)
{
    for (unsigned i = 0; i < size; ++i)
        b.push_back(uint8_t(value >> (8 * i)));
}
inline void patch(Bytes &b, size_t at, uint64_t value, unsigned size = 4)
{
    require(at <= b.size() && size <= b.size() - at, "internal patch range");
    for (unsigned i = 0; i < size; ++i)
        b[at + i] = uint8_t(value >> (8 * i));
}
inline void string(Bytes &b, const std::string &s)
{
    require(s.size() <= 1024 * 1024, "LIMIT: string");
    put(b, s.size());
    b.insert(b.end(), s.begin(), s.end());
}
inline void words(Bytes &b, std::initializer_list<uint32_t> values)
{
    for (auto v : values)
        put(b, v);
}
inline void align(Bytes &b)
{
    while (b.size() % 8)
        b.push_back(0);
}
bool utf8(const std::string &s);
Hash sha256(const Bytes &bytes);
std::string hex(const Hash &hash);
struct Cursor
{
    const Bytes &bytes;
    size_t pos = 0, end;
    explicit Cursor(const Bytes &b) : bytes(b), end(b.size())
    {
    }
    Cursor(const Bytes &b, size_t start, size_t length) : bytes(b), pos(start), end(start)
    {
        require(start <= b.size() && length <= b.size() - start, "BAD_IR: range");
        end += length;
    }
    uint64_t get(unsigned size = 4)
    {
        require(size <= end - pos, "BAD_IR: truncated record");
        uint64_t v = 0;
        for (unsigned i = 0; i < size; ++i)
            v |= uint64_t(bytes[pos++]) << (8 * i);
        return v;
    }
    uint32_t u32()
    {
        return uint32_t(get());
    }
    std::string str()
    {
        auto n = u32();
        require(n <= 1024 * 1024 && n <= end - pos, "BAD_IR: string length");
        std::string s(bytes.begin() + pos, bytes.begin() + pos + n);
        pos += n;
        require(utf8(s), "BAD_IR: UTF-8");
        return s;
    }
    void done() const
    {
        require(pos == end, "BAD_IR: trailing bytes");
    }
};
} // namespace oneview
