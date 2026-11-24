#pragma once
#include "format/ir.h"

namespace oneui::views {
// Compact per-image index. Keep record order and use offsets rather than
// pointers so the index can move with its image during transactional reload.
class OwnerRecords {
    struct Index {
        std::vector<uint32_t> starts, records;
    };
    std::array<Index, 4> indices;
    bool mountConditions_ = false;
    static constexpr oneview::Section sections[] = {
        oneview::PROP, oneview::BIND, oneview::COND, oneview::EVNT};
public:
    explicit OwnerRecords(const oneview::Image& image) {
        for (size_t k = 0; k < indices.size(); ++k) {
            auto& index = indices[k];
            const auto& records = image.records[sections[k]];
            index.starts.resize(image.count(oneview::NODE) + 1);
            for (const auto& record : records) {
                ++index.starts.at(record[0] + 1);
                if (sections[k] == oneview::COND && ((record[3] >> 16) & oneview::conditionMountOnly))
                    mountConditions_ = true;
            }
            for (size_t i = 1; i < index.starts.size(); ++i)
                index.starts[i] += index.starts[i - 1];
            index.records.resize(records.size());
            auto cursor = index.starts;
            for (uint32_t i = 0; i < records.size(); ++i)
                index.records[cursor[records[i][0]]++] = i;
        }
    }
    struct Iterator {
        const std::vector<oneview::Record>* records;
        std::vector<uint32_t>::const_iterator position;
        const oneview::Record& operator*() const { return (*records)[*position]; }
        Iterator& operator++() { ++position; return *this; }
        bool operator!=(const Iterator& other) const { return position != other.position; }
    };
    struct Range {
        Iterator first, last;
        Iterator begin() const { return first; }
        Iterator end() const { return last; }
        size_t size() const { return size_t(last.position - first.position); }
    };
    bool hasMountConditions() const { return mountConditions_; }
    Range get(const oneview::Image& image, oneview::Section section, uint32_t owner) const {
        const auto found = std::find(std::begin(sections), std::end(sections), section);
        const auto& index = indices.at(size_t(found - std::begin(sections)));
        const auto* records = &image.records[section];
        return {{records, index.records.begin() + index.starts.at(owner)},
                {records, index.records.begin() + index.starts.at(owner + 1)}};
    }
};
} // namespace oneui::views
