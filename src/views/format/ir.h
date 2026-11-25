#pragma once
#include "component_contract.h"
#include <algorithm>
#include <initializer_list>
#include <iterator>

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

// Every record has a schema-defined stride. Keep its fields with the record
// when parsing an image or copying an installed event or binding.
class Record
{
    static constexpr auto capacity = *std::max_element(std::begin(strides), std::end(strides)) / sizeof(uint32_t);
    std::array<uint32_t, capacity> fields_{};
    uint32_t count_ = 0;

  public:
    Record() = default;
    explicit Record(std::size_t count)
    {
        require(count <= capacity, "internal record stride");
        count_ = static_cast<uint32_t>(count);
    }
    Record(std::initializer_list<uint32_t> fields) : Record(fields.size())
    {
        std::copy(fields.begin(), fields.end(), fields_.begin());
    }
    std::size_t size() const noexcept { return count_; }
    uint32_t* begin() noexcept { return fields_.data(); }
    uint32_t* end() noexcept { return fields_.data() + count_; }
    const uint32_t* begin() const noexcept { return fields_.data(); }
    const uint32_t* end() const noexcept { return fields_.data() + count_; }
    uint32_t& operator[](std::size_t index) noexcept { return fields_[index]; }
    const uint32_t& operator[](std::size_t index) const noexcept { return fields_[index]; }
    uint32_t& back() noexcept { return fields_[count_ - 1]; }
    const uint32_t& back() const noexcept { return fields_[count_ - 1]; }
    uint32_t& at(std::size_t index)
    {
        if (index >= count_)
            throw std::out_of_range("IR record field");
        return fields_[index];
    }
    const uint32_t& at(std::size_t index) const
    {
        if (index >= count_)
            throw std::out_of_range("IR record field");
        return fields_[index];
    }
};
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
inline constexpr uint32_t workspaceHosts = 1, workspaceCollections = 2, workspaceQueries = 4;
inline constexpr uint32_t repeatTransparent = 1u << 16, repeatExplicit = 1u << 17;
// COND mode bits occupy the high half of its packed mode/depth word.
inline constexpr uint32_t conditionInverse = 1, conditionLiteral = 2, conditionMountOnly = 4;
inline constexpr uint32_t mountConditionMinor = 2;
inline bool supportsMountConditionChildren(const ComponentContract& parent) {
    return parent.name == "Column" || parent.name == "Row" || parent.name == "Surface";
}
// SLOT flags (word 3): bit 0 is supply ownership, bit 1 is the root contract.
inline constexpr uint32_t slotSupplied = 1u, slotSingleRoot = 2u;
void requireWorkspace(Image &, uint32_t bits);
Bytes writeImage(const Image &image, const ComponentContracts *known = nullptr);
Image readImage(const Bytes &bytes, const ComponentContracts *known = nullptr);
void validateImage(const Image &image);
void validateGraph(const Image &image);
uint32_t componentRoot(const Image &image, uint32_t node);
} // namespace oneview
