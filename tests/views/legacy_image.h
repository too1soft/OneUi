#pragma once
#include "fixture.h"
#include <algorithm>
#include <filesystem>
namespace legacytest {
using viewtest::expect;
inline oneview::Bytes bytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    expect(bool(input), "missing frozen legacy IR");
    return {std::istreambuf_iterator<char>(input), {}};
}
inline std::vector<oneview::ComponentContract> frozenContracts(const oneview::Bytes& data) {
    oneview::Cursor header(data, 24, data.size() - 24);
    const auto directory = header.get(8);
    const auto count = header.u32();
    for (uint32_t i = 0; i < count; ++i) {
        oneview::Cursor row(data, size_t(directory) + i * 32, 32);
        const auto kind = row.u32();
        row.u32();
        const auto offset = row.get(8), length = row.get(8);
        if (kind == oneview::COMP)
            return oneview::decodeComponents({data.begin() + size_t(offset),
                                             data.begin() + size_t(offset + length)});
    }
    throw std::runtime_error("missing frozen COMP");
}
} // namespace legacytest
