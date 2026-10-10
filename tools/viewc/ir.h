#pragma once
#include "component_contract.h"

namespace oneview
{
enum Section : uint32_t
{
    STRS = 1,
    SCHM,
    COMP,
    TMPL,
    NODE,
    EDGE,
    VALU,
    PROP,
    BIND,
    EVNT,
    EARG,
    COND,
    REPT,
    CALL,
    SLOT,
    CSS,
    SMAP,
    HOST
};
inline constexpr uint32_t strides[] = {0, 0, 0, 0, 32, 48, 8, 24, 16, 32, 32, 16, 24, 32, 24, 32, 24, 24, 16};
using Record = std::vector<uint32_t>;
struct Image
{
    Schema schema;
    std::vector<ComponentContract> components;
    std::vector<std::string> strings{""};
    std::map<std::string, uint32_t> stringIds;
    std::array<std::vector<Record>, 19> records;
    uint32_t root = 0;
    uint32_t sid(const std::string &text);
    uint32_t add(Section section, Record record);
    uint32_t count(Section section) const
    {
        return uint32_t(records[section].size());
    }
};
Bytes writeImage(const Image &image);
Image readImage(const Bytes &bytes);
void validateImage(const Image &image);
void validateGraph(const Image &image);
} // namespace oneview
