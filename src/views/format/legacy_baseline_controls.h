#pragma once
#include "component_contract.h"

namespace oneview {
// Exact controls from ABI baseline64a5b89; real IR comparison is a freeze gate.
// Frozen serialized descriptors. Never derive historical rows from registry().
inline const std::vector<ComponentContract>& legacyBaselineControls() {
    static const std::vector<ComponentContract> contracts = {
        {"Input", "leaf", 3, 11, {
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
        }, {
            {1, "submit", {}},
        }},
        {"Checkbox", "leaf", 1, 2, {
            {1, 3, 1, "basis", ""},
            {2, 1, 7, "checked", ""},
            {3, 4, 1, "class", ""},
            {4, 4, 3, "description", ""},
            {5, 1, 3, "disabled", ""},
            {6, 3, 1, "grow", ""},
            {7, 3, 1, "max", ""},
            {8, 3, 1, "min", ""},
            {9, 4, 3, "name", ""},
            {10, 3, 1, "shrink", ""},
            {11, 4, 3, "text", ""},
            {12, 1, 3, "visible", ""},
        }, {
        }},
        {"TimeSeriesChart", "leaf", 1, 0, {
            {1, 3, 1, "basis", ""},
            {2, 4, 1, "class", ""},
            {3, 4, 3, "description", ""},
            {4, 1, 3, "disabled", ""},
            {5, 3, 1, "grow", ""},
            {6, 3, 1, "max", ""},
            {7, 3, 1, "min", ""},
            {8, 4, 3, "name", ""},
            {9, 5, 2, "series", "oneui.v1.Series"},
            {10, 3, 1, "shrink", ""},
            {11, 1, 3, "visible", ""},
        }, {
        }},
        {"Scroll", "one", 3, 0, {
            {1, 3, 1, "basis", ""},
            {2, 4, 1, "class", ""},
            {3, 4, 3, "description", ""},
            {4, 1, 3, "disabled", ""},
            {5, 3, 1, "grow", ""},
            {6, 3, 1, "max", ""},
            {7, 3, 1, "min", ""},
            {8, 4, 3, "name", ""},
            {9, 3, 1, "shrink", ""},
            {10, 1, 3, "visible", ""},
            {11, 3, 3, "content-height", ""},
            {12, 3, 3, "wheel-step", ""},
            {13, 1, 3, "chrome-visible", ""},
            {14, 3, 3, "scroll-offset", ""},
            {15, 3, 3, "content-width", ""},
        }, {
        }},
        {"VirtualList", "leaf", 1, 10, {
            {1, 3, 1, "basis", ""},
            {2, 4, 1, "class", ""},
            {3, 4, 3, "description", ""},
            {4, 1, 3, "disabled", ""},
            {5, 3, 1, "grow", ""},
            {6, 5, 2, "items", "@list.item"},
            {7, 3, 1, "max", ""},
            {8, 3, 1, "min", ""},
            {9, 4, 3, "name", ""},
            {10, 2, 7, "selectedIndex", ""},
            {11, 3, 1, "shrink", ""},
            {12, 1, 3, "visible", ""},
            {13, 3, 3, "row-height", ""},
            {14, 3, 3, "scroll-offset", ""},
        }, {
            {1, "activate", {}},
        }},
    };
    return contracts;
}
} // namespace oneview
