#pragma once
#include "oneui/export.h"
#include "oneui/geometry.h"
#include <string>
#include <vector>
namespace oneui {
struct ContainerQueryTerm {
    enum class Axis {Width,Height,Aspect};
    enum class Compare {Less,LessEqual,Equal,GreaterEqual,Greater};
    Axis axis;
    Compare compare;
    double numerator=0,denominator=1;
};
struct ContainerQuery {
    std::string name;
    std::vector<ContainerQueryTerm> terms;
    ONEUI_API bool matches(Size) const;
};
ONEUI_API ContainerQuery parseContainerQuery(const std::string&);
} // namespace oneui
