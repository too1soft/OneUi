#include "oneui/controls/text_field.h"
namespace oneui {
bool TextField::commitPendingComposition() {
    if (composition_.empty()) return false;
    auto committed=composition_;
    return onTextCommitted(committed);
}
}
