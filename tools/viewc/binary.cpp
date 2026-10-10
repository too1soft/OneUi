#include "binary.h"
#include <iomanip>
#include <sstream>

namespace oneview
{
bool utf8(const std::string &s)
{
    for (size_t i = 0; i < s.size();)
    {
        uint32_t c = uint8_t(s[i++]), min = 0;
        unsigned count = 0;
        if (!c)
            return false;
        if (c < 128)
            continue;
        if (c >= 0xc2 && c <= 0xdf)
        {
            count = 1;
            c &= 31;
            min = 0x80;
        }
        else if (c >= 0xe0 && c <= 0xef)
        {
            count = 2;
            c &= 15;
            min = 0x800;
        }
        else if (c >= 0xf0 && c <= 0xf4)
        {
            count = 3;
            c &= 7;
            min = 0x10000;
        }
        else
            return false;
        if (count > s.size() - i)
            return false;
        while (count--)
        {
            auto d = uint8_t(s[i++]);
            if ((d & 0xc0) != 0x80)
                return false;
            c = (c << 6) | (d & 63);
        }
        if (c < min || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff))
            return false;
    }
    return true;
}
Hash sha256(const Bytes &input)
{
    static constexpr uint32_t k[] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
    uint32_t h[] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    Bytes b = input;
    b.push_back(0x80);
    while (b.size() % 64 != 56)
        b.push_back(0);
    auto bits = uint64_t(input.size()) * 8;
    for (int i = 7; i >= 0; --i)
        b.push_back(uint8_t(bits >> (8 * i)));
    auto rotr = [](uint32_t v, unsigned n) { return (v >> n) | (v << (32 - n)); };
    for (size_t offset = 0; offset < b.size(); offset += 64)
    {
        uint32_t w[64];
        for (unsigned i = 0; i < 16; ++i)
            w[i] = (uint32_t(b[offset + i * 4]) << 24) | (uint32_t(b[offset + i * 4 + 1]) << 16) |
                   (uint32_t(b[offset + i * 4 + 2]) << 8) | b[offset + i * 4 + 3];
        for (unsigned i = 16; i < 64; ++i)
        {
            auto x = w[i - 15], y = w[i - 2];
            w[i] =
                w[i - 16] + (rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3)) + w[i - 7] + (rotr(y, 17) ^ rotr(y, 19) ^ (y >> 10));
        }
        uint32_t a = h[0], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], z = h[7], v = h[1];
        for (unsigned i = 0; i < 64; ++i)
        {
            auto t1 = z + (rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)) + ((e & f) ^ (~e & g)) + k[i] + w[i];
            auto t2 = (rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)) + ((a & v) ^ (a & c) ^ (v & c));
            z = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = v;
            v = a;
            a = t1 + t2;
        }
        h[0] += a;
        h[1] += v;
        h[2] += c;
        h[3] += d;
        h[4] += e;
        h[5] += f;
        h[6] += g;
        h[7] += z;
    }
    Hash out{};
    for (unsigned i = 0; i < 8; ++i)
        for (unsigned j = 0; j < 4; ++j)
            out[i * 4 + j] = uint8_t(h[i] >> (24 - j * 8));
    return out;
}
std::string hex(const Hash &hash)
{
    std::ostringstream s;
    for (auto b : hash)
        s << std::hex << std::setfill('0') << std::setw(2) << unsigned(b);
    return s.str();
}
} // namespace oneview
