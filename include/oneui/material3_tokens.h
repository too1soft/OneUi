#pragma once

#include "oneui/color.h"
#include "oneui/export.h"
#include "oneui/style_sheet.h"

namespace oneui {

enum class MaterialState {
    Normal,
    Hovered,
    Focused,
    Pressed,
    Disabled
};

enum class MaterialElevationLevel {
    Level0,
    Level1,
    Level2,
    Level3
};

struct Material3StateOpacity {
    float hover = 0.08f;
    float focus = 0.10f;
    float pressed = 0.10f;
    float dragged = 0.16f;
    float disabledContent = 0.38f;
    float disabledContainer = 0.12f;
};

struct Material3DarkColorScheme {
    Color primary{208, 188, 255};
    Color onPrimary{56, 30, 114};
    Color primaryContainer{79, 55, 139};
    Color onPrimaryContainer{234, 221, 255};
    Color secondary{204, 194, 220};
    Color surface{20, 18, 24};
    Color surfaceContainerLowest{15, 13, 19};
    Color surfaceContainerLow{29, 27, 32};
    Color surfaceContainer{33, 31, 38};
    Color surfaceContainerHigh{43, 41, 48};
    Color surfaceContainerHighest{54, 52, 59};
    Color onSurface{230, 225, 229};
    Color onSurfaceVariant{202, 196, 208};
    Color outline{147, 143, 153};
    Color outlineVariant{73, 69, 79};
    Color error{242, 184, 181};
};

ONEUI_API Color material3Blend(Color base, Color overlay, float opacity);
ONEUI_API Color material3StateLayer(Color base, Color stateColor, MaterialState state);
ONEUI_API StyleShadow material3ElevationShadow(MaterialElevationLevel level);

} // namespace oneui
