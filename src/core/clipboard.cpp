#include "oneui/clipboard.h"

#include <utility>

namespace oneui {

void MemoryClipboard::setText(std::wstring text) {
    text_ = std::move(text);
}

std::wstring MemoryClipboard::text() const {
    return text_;
}

} // namespace oneui
