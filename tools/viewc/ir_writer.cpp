#include "ir.h"
#include <algorithm>

namespace oneview
{
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
Bytes writeImage(const Image &image)
{
    std::array<Bytes, 19> sections;
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
    for (uint32_t k = TMPL; k <= HOST; ++k)
        for (auto &r : image.records[k])
            for (auto v : r)
                put(sections[k], v);
    Bytes b(128 + 18 * 32, 0);
    const char magic[] = "ONEVIEW";
    std::copy(magic, magic + 8, b.begin());
    patch(b, 8, 1, 2);
    patch(b, 12, 128);
    patch(b, 24, 128, 8);
    patch(b, 32, 18);
    patch(b, 40, 1);
    patch(b, 44, image.root);
    auto sh = sha256(sections[SCHM]), ch = sha256(sections[COMP]);
    std::copy(sh.begin(), sh.end(), b.begin() + 48);
    std::copy(ch.begin(), ch.end(), b.begin() + 80);
    patch(b, 112, 1);
    patch(b, 116, image.schema.root);
    for (uint32_t k = STRS; k <= HOST; ++k)
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
