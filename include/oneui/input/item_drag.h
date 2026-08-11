#pragma once

#include "oneui/geometry.h"

#include <string>

namespace oneui {

enum class ItemDragPhase {
    Started = 0,
    Updated = 1,
    Dropped = 2,
    Cancelled = 3,
};

struct ItemDragEvent {
    std::wstring sourceId;
    ItemDragPhase phase = ItemDragPhase::Updated;
    Point position;
};

} // namespace oneui
