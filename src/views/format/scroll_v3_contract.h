#pragma once
#include "component_contract.h"
namespace oneview {
// Frozen complete legacy wire contract, matched field-for-field. Do not derive
// it from the current registry: future changes must not broaden v3 acceptance.
inline const ComponentContract& scrollV3Contract() {
    static const ComponentContract value{"Scroll","one",3,0,{
        {1,3,1,"basis",""}, {2,4,1,"class",""},
        {3,4,3,"description",""}, {4,1,3,"disabled",""},
        {5,3,1,"grow",""}, {6,3,1,"max",""}, {7,3,1,"min",""},
        {8,4,3,"name",""}, {9,3,1,"shrink",""}, {10,1,3,"visible",""},
        {11,3,3,"content-height",""}, {12,3,3,"wheel-step",""},
        {13,1,3,"chrome-visible",""}, {14,3,3,"scroll-offset",""},
        {15,3,3,"content-width",""}
    },{}};
    return value;
}
} // namespace oneview
