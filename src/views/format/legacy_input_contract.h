#pragma once
#include "component_contract.h"

namespace oneview {
// Frozen Input v4 COMP record (accessible-name, ID 17) emitted before
// placeholder-when-focused (ID 18) was appended. Never derive historical rows
// from the mutable current registry.
inline const std::vector<ComponentContract>& legacyInputContracts() {
    static const std::vector<ComponentContract> contracts = {
        {"Input", "leaf", 4, 11, {
            {1, 3, 1, "basis", ""},
            {2, 4, 1, "class", ""},
            {3, 4, 3, "description", ""},
            {4, 1, 3, "disabled", ""},
            {5, 3, 1, "grow", ""},
            {6, 3, 1, "max", ""},
            {7, 3, 1, "min", ""},
            {8, 4, 3, "name", ""},
            {9, 4, 3, "placeholder", ""},
            {10, 3, 1, "shrink", ""},
            {11, 4, 7, "text", ""},
            {12, 1, 3, "visible", ""},
            {13, 1, 3, "password", ""},
            {14, 1, 3, "read-only", ""},
            {15, 4, 3, "password-mask", ""},
            {16, 4, 3, "prefix-icon", ""},
            {17, 4, 3, "accessible-name", ""},
        }, {
            {1, "submit", {}},
        }},
    };
    return contracts;
}
} // namespace oneview
