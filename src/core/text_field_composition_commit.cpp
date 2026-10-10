#include "oneui/controls/text_field.h"
namespace oneui {
bool TextField::commitTextComposition() {
    if(composition_.empty())return false;
    auto committed=composition_;
    setTextComposition({},0);
    return onTextCommitted(committed);
}
} // namespace oneui
