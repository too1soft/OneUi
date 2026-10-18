#include "ir.h"
#include <algorithm>

namespace oneview
{
uint32_t componentRoot(const Image &image, uint32_t node)
{
    for (unsigned depth = 0; depth < 64; ++depth)
    {
        require(node < image.count(NODE), "BAD_IR: overlay node");
        const auto &n = image.records[NODE][node];
        if (n[0] == 1)
            return node;
        require(n[0] == 3 && n[9] < image.count(CALL), "CALL_ROOT: a component root is required");
        auto target = image.records[CALL][n[9]][1];
        require(target < image.count(TMPL), "BAD_IR: overlay template");
        node = image.records[TMPL][target][1];
    }
    throw Error("BAD_IR: overlay root cycle/depth");
}
uint32_t Image::sid(const std::string &text)
{
    require(utf8(text) && text.size() <= 1024 * 1024, "UTF8/LIMIT: string");
    if (stringIds.empty())
        for (uint32_t i = 0; i < strings.size(); ++i)
            stringIds.emplace(strings[i], i);
    auto i = stringIds.find(text);
    if (i != stringIds.end())
        return i->second;
    require(strings.size() < 100000, "LIMIT: string count");
    strings.push_back(text);
    stringIds.emplace(text, uint32_t(strings.size() - 1));
    return uint32_t(strings.size() - 1);
}
uint32_t Image::add(Section section, Record record)
{
    require(record.size() * 4 == strides[section], "internal record stride");
    require(records[section].size() < 100000, "LIMIT: record count");
    records[section].push_back(std::move(record));
    return count(section) - 1;
}
void requireWorkspace(Image &image, uint32_t bits) {
    if (!image.count(WSC1)) image.add(WSC1, {1, bits, 0, 0});
    else image.records[WSC1][0][1] |= bits;
}
Bytes writeImage(const Image &image)
{
    std::array<Bytes, 20> sections;
    Bytes strings;
    uint64_t offset = 0;
    for (const auto &s : image.strings)
    {
        put(sections[STRS], offset, 8);
        offset += s.size();
        strings.insert(strings.end(), s.begin(), s.end());
    }
    put(sections[STRS], offset, 8);
    sections[STRS].insert(sections[STRS].end(), strings.begin(), strings.end());
    sections[SCHM] = image.schema.encode();
    sections[COMP] = encodeComponents(image.components);
    const uint32_t last = image.count(WSC1) ? WSC1 : HOST;
    for (uint32_t k = TMPL; k <= last; ++k)
        for (auto &r : image.records[k])
            for (auto v : r)
                put(sections[k], v);
    Bytes b(128 + last * 32, 0);
    const char magic[] = "ONEVIEW";
    std::copy(magic, magic + 8, b.begin());
    patch(b, 8, 1, 2);
    const bool singleRoot = std::any_of(image.records[SLOT].begin(), image.records[SLOT].end(),
        [](const auto& slot) { return slot[3] & slotSingleRoot; });
    patch(b, 10, singleRoot ? 1 : 0, 2);
    patch(b, 12, 128);
    patch(b, 24, 128, 8);
    patch(b, 32, last);
    patch(b, 40, 1);
    patch(b, 44, image.root);
    auto sh = sha256(sections[SCHM]), ch = sha256(sections[COMP]);
    std::copy(sh.begin(), sh.end(), b.begin() + 48);
    std::copy(ch.begin(), ch.end(), b.begin() + 80);
    patch(b, 112, 1);
    patch(b, 116, image.schema.root);
    for (uint32_t k = STRS; k <= last; ++k)
    {
        align(b);
        auto d = 128 + (k - 1) * 32;
        patch(b, d, k);
        patch(b, d + 4, k == SMAP ? 0 : 1);
        patch(b, d + 8, b.size(), 8);
        patch(b, d + 16, sections[k].size(), 8);
        auto count = k == STRS   ? image.strings.size()
                     : k == SCHM ? image.schema.models.size()
                     : k == COMP ? image.components.size()
                                 : image.records[k].size();
        patch(b, d + 24, count);
        patch(b, d + 28, strides[k]);
        b.insert(b.end(), sections[k].begin(), sections[k].end());
    }
    patch(b, 16, b.size(), 8);
    require(b.size() <= max_file, "LIMIT: file");
    // The public writer cannot publish an image the independent loader rejects.
    readImage(b);
    return b;
}
} // namespace oneview
