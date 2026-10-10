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
    HOST,
    WSC1
};
// PROP/BIND/EVNT end with owner_kind: 0 component NODE, 1 CALL NODE.
inline constexpr uint32_t strides[] = {0, 0, 0, 0, 32, 48, 8, 24, 20, 36, 36, 16, 24, 32, 24, 32, 24, 24, 16, 16};
using Record = std::vector<uint32_t>;
struct Image
{
    Schema schema;
    std::vector<ComponentContract> components;
    std::vector<std::string> strings{""};
    std::map<std::string, uint32_t> stringIds;
    std::array<std::vector<Record>, 20> records;
    uint32_t root = 0;
    uint32_t sid(const std::string &text);
    uint32_t add(Section section, Record record);
    uint32_t count(Section section) const
    {
        return uint32_t(records[section].size());
    }
};
inline constexpr uint32_t workspaceHosts = 1, workspaceCollections = 2;
inline constexpr uint32_t repeatTransparent = 1u << 16, repeatExplicit = 1u << 17;
void requireWorkspace(Image &, uint32_t bits);
Bytes writeImage(const Image &image);
Image readImage(const Bytes &bytes);
void validateImage(const Image &image);
void validateGraph(const Image &image);
uint32_t componentRoot(const Image &image, uint32_t node);
} // namespace oneview
