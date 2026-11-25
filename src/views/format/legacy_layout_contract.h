#pragma once
#include "component_contract.h"
namespace oneview {
// Exact pre-direction contracts at f73dfd79. Preserve serialized field order,
// flags and versions; never derive historical descriptors from registry().
inline const std::vector<ComponentContract>& legacyLayoutContracts() {
    static const auto contracts = [] {
      std::vector<ComponentContract> layouts = {
        {"Column", "many", 3, 0, {
            {1,4,1,"align",""}, {2,3,3,"basis",""}, {3,4,3,"class",""},
            {4,4,3,"description",""}, {5,1,3,"disabled",""}, {6,3,1,"grow",""},
            {7,4,1,"justify",""}, {8,3,1,"max",""}, {9,3,1,"min",""},
            {10,4,3,"name",""}, {11,3,1,"shrink",""}, {12,1,3,"visible",""},
            {13,1,3,"wrap",""}
        }, {}},
        {"Row", "many", 2, 0, {
            {1,4,1,"align",""}, {2,3,1,"basis",""}, {3,4,3,"class",""},
            {4,4,3,"description",""}, {5,1,3,"disabled",""}, {6,3,1,"grow",""},
            {7,4,1,"justify",""}, {8,3,1,"max",""}, {9,3,1,"min",""},
            {10,4,3,"name",""}, {11,3,1,"shrink",""}, {12,1,3,"visible",""},
            {13,1,3,"wrap",""}
        }, {}},
        {"VirtualTemplateList", "virtual-rows", 1, 0, {
            {1,3,1,"basis",""}, {2,4,1,"class",""}, {3,4,3,"description",""},
            {4,1,3,"disabled",""}, {5,3,1,"grow",""}, {6,3,1,"max",""},
            {7,3,1,"min",""}, {8,4,3,"name",""}, {9,3,1,"shrink",""},
            {10,1,3,"visible",""}, {11,3,3,"estimated-height",""}, {12,3,3,"overscan",""},
            {13,1,3,"follow-end",""}, {14,3,3,"scroll-offset",""}
        }, {}}
      };
      // Direction was the only change to the preceding layouts in 6c76705.
      for (size_t i = 0; i < 2; ++i) {
          auto withDirection = layouts[i];
          ++withDirection.version;
          withDirection.properties.push_back({14,4,3,"direction",""});
          layouts.push_back(std::move(withDirection));
      }
      return layouts;
    }();
    return contracts;
}
}
