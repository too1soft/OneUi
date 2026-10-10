#include "oneui/layout/container_query.h"
#include <cmath>
namespace oneui {
bool ContainerQuery::matches(Size size) const {
    if(!std::isfinite(size.width)||!std::isfinite(size.height)||size.width<0||size.height<0)return false;
    for(const auto& term:terms) {
        double left=term.axis==ContainerQueryTerm::Axis::Height?size.height:size.width;
        double right=term.numerator;
        if(term.axis==ContainerQueryTerm::Axis::Aspect) {
            if(size.height<=0)return false;
            left*=term.denominator;right*=size.height;
        }
        using C=ContainerQueryTerm::Compare;
        bool match=term.compare==C::Less?left<right:term.compare==C::LessEqual?left<=right:term.compare==C::Equal?left==right:term.compare==C::GreaterEqual?left>=right:left>right;
        if(!match)return false;
    }
    return true;
}
} // namespace oneui
