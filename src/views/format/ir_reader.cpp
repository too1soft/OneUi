#include "ir.h"
#include "legacy_contract_normalize.h"
#include <algorithm>
#include <set>

namespace oneview
{
Image readImage(const Bytes &b)
{
    require(b.size() >= 128 && b.size() <= max_file, "BAD_IR/LIMIT: file size");
    Cursor h(b, 0, 128);
    require(h.get(8) == 0x0057454956454e4full, "BAD_IR: magic");
    require(h.get(2) == 1, "VERSION: major");
    const auto minor = h.get(2);
    auto header = h.u32();
    require(header >= 128 && header <= b.size(), "BAD_IR: header length");
    require(h.get(8) == b.size(), "BAD_IR: file length");
    auto dir = h.get(8);
    auto nd = h.u32();
    require(h.u32() == 0 && h.u32() == 1, "VERSION: flags/hash algorithm");
    Image image;
    image.root = h.u32();
    Hash schemaHash{}, componentHash{};
    for (auto &v : schemaHash)
        v = uint8_t(h.get(1));
    for (auto &v : componentHash)
        v = uint8_t(h.get(1));
    require(h.u32() == 1, "VERSION: protocol");
    auto root = h.u32();
    require(h.get(8) == 0, "VERSION: reserved header");
    require(dir == header && nd <= 1024 && uint64_t(nd) * 32 <= b.size() - dir, "BAD_IR: directory range");
    for (size_t i = 128; i < header; ++i)
        require(b[i] == 0, "VERSION: header extension");
    Cursor d(b, size_t(dir), size_t(nd) * 32);
    struct Entry
    {
        uint32_t kind, flags, count, stride;
        uint64_t offset, length;
    };
    std::vector<Entry> entries;
    std::set<uint32_t> seen;
    for (uint32_t i = 0; i < nd; ++i)
    {
        Entry e;
        e.kind = d.u32();
        e.flags = d.u32();
        e.offset = d.get(8);
        e.length = d.get(8);
        e.count = d.u32();
        e.stride = d.u32();
        require(e.flags <= 1 && seen.insert(e.kind).second, "BAD_IR: duplicate kind/flags");
        require(e.offset % 8 == 0 && e.offset >= dir + uint64_t(nd) * 32 && e.offset <= b.size() &&
                    e.length <= b.size() - e.offset,
                "BAD_IR: section range/alignment");
        if (e.kind < STRS || e.kind > WSC1)
            require(!e.flags, "VERSION: unknown required section");
        else
        {
            require(e.stride == strides[e.kind] && e.flags == (e.kind == SMAP ? 0u : 1u),
                    "BAD_IR: stride/required flag");
            if (e.stride)
                require(uint64_t(e.count) * e.stride == e.length && e.count <= 100000, "BAD_IR/LIMIT: section count");
        }
        entries.push_back(e);
    }
    for (uint32_t k = STRS; k <= HOST; ++k)
        require(k == SMAP || seen.count(k), "BAD_IR: missing required section");
    auto byOffset = entries;
    std::sort(byOffset.begin(), byOffset.end(),
              [](auto a, auto c) { return a.offset < c.offset || (a.offset == c.offset && a.length < c.length); });
    uint64_t end = dir + uint64_t(nd) * 32;
    for (auto e : byOffset)
    {
        require(e.offset >= end, "BAD_IR: overlapping sections");
        for (auto i = end; i < e.offset; ++i)
            require(b[size_t(i)] == 0, "BAD_IR: nonzero padding");
        end = e.offset + e.length;
    }
    require(end == b.size(), "BAD_IR: undeclared trailing data");
    for (auto e : entries)
    {
        if (e.kind < STRS || e.kind > WSC1)
            continue;
        Cursor c(b, size_t(e.offset), size_t(e.length));
        if (e.kind == STRS)
        {
            require(e.count > 0 && e.count <= 100000 && (uint64_t(e.count) + 1) * 8 <= e.length,
                    "BAD_IR: string offsets");
            auto blob = e.offset + (uint64_t(e.count) + 1) * 8, length = e.length - (uint64_t(e.count) + 1) * 8;
            require(length <= 16 * 1024 * 1024, "LIMIT: strings");
            uint64_t previous = c.get(8);
            require(previous == 0, "BAD_IR: string origin");
            image.strings.clear();
            for (uint32_t i = 0; i < e.count; ++i)
            {
                auto next = c.get(8);
                require(next >= previous && next <= length && next - previous <= 1024 * 1024, "BAD_IR: string offset");
                std::string s(b.begin() + size_t(blob + previous), b.begin() + size_t(blob + next));
                require(utf8(s), "BAD_IR: UTF-8");
                image.strings.push_back(s);
                previous = next;
            }
            require(previous == length && image.strings[0].empty(), "BAD_IR: string sentinel");
        }
        else if (e.kind == SCHM || e.kind == COMP)
        {
            Bytes data(b.begin() + size_t(e.offset), b.begin() + size_t(e.offset + e.length));
            require(sha256(data) == (e.kind == SCHM ? schemaHash : componentHash), "BAD_IR: contract hash");
            if (e.kind == SCHM)
            {
                image.schema = decodeSchema(data);
                require(image.schema.models.size() == e.count && image.schema.root == root, "BAD_IR: model count/root");
            }
            else
            {
                image.components = decodeComponents(data);
                require(image.components.size() == e.count, "BAD_IR: component count");
            }
        }
        else
            for (uint32_t i = 0; i < e.count; ++i)
            {
                Record record;
                for (uint32_t j = 0; j < e.stride / 4; ++j)
                    record.push_back(c.u32());
                image.records[e.kind].push_back(record);
            }
    }
    validateImage(image);
    for (const auto& slot : image.records[SLOT])
        require(!(slot[3] & slotSingleRoot) || minor >= 1, "VERSION: single-root slot requires minor 1");
    validateGraph(image);
    normalizeLegacyContracts(image);
    return image;
}
} // namespace oneview
