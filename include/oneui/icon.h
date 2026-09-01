#pragma once

#include "oneui/color.h"
#include "oneui/export.h"
#include "oneui/geometry.h"

#include <array>
#include <vector>

namespace oneui {

enum class IconSymbol {
    BrandBloom,
    Search,
    RemoteAssist,
    Monitor,
    Device,
    Toolbox,
    Compass,
    Settings,
    Bell,
    Minimize,
    Maximize,
    Restore,
    Close,
    Heart,
    Desktop,
    File,
    Sparkle,
    RadioOn,
    RadioOff,
    ToggleOn,
    KeyDots,
    Copy,
    ChevronDown,
    ChevronUp,
    Plus,
    User,
    Globe,
    Play,
    Check,
    BrandMark,
    CheckCircle,
    Terminal,
    Server,
    LayoutGrid,
    List,
    Refresh,
    Upload,
    Download,
    Sliders,
    Code,
    Database,
    Cube,
    Notebook,
    Edit,
    Trash,
    ChevronLeft,
    ChevronRight,
    Folder,
    Headset,
    OpenInNew
};

enum class IconPrimitiveKind {
    Line,
    Rect,
    RoundRect,
    Circle,
    Polyline,
    Polygon
};

struct IconPrimitive {
    IconPrimitiveKind kind = IconPrimitiveKind::Line;
    Rect rect{};
    Point from{};
    Point to{};
    std::array<Point, 6> points{};
    int pointCount = 0;
    Color color{255, 255, 255, 255};
    float strokeWidth = 1.5f;
    float radius = 0.0f;
    bool filled = false;
    bool closed = false;
};

ONEUI_API std::vector<IconPrimitive> buildIconPrimitives(
    IconSymbol symbol,
    Rect rect,
    Color color,
    Color accent = Color{0, 0, 0, 0},
    float strokeWidth = 1.5f);

class Canvas;

// Paints one vector icon into the supplied rectangle. Components should use
// this shared renderer instead of duplicating the primitive traversal loop.
ONEUI_API void paintIcon(
    Canvas& canvas,
    IconSymbol symbol,
    Rect rect,
    Color color,
    Color accent = Color{0, 0, 0, 0},
    float strokeWidth = 1.5f);

} // namespace oneui
