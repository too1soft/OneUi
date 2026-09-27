#pragma once
#include "oneui/oneui_c_api.h"
#ifdef __cplusplus
extern "C" {
#endif

/* All rectangles are local logical pixels. Arrays are copied synchronously.
 * Paths use a 16x16 view box; verbs: 0 move, 1 line, 2 close. At most 64 commands.
 * Buttons are ordered minimize, maximize/restore, close regardless of position. */
typedef struct OneUiCaptionPathCommand { int verb; float x, y; } OneUiCaptionPathCommand;
typedef struct OneUiCaptionPath {
    const OneUiCaptionPathCommand* commands;
    size_t count;
} OneUiCaptionPath;
typedef struct OneUiCaptionButtonPresentation {
    OneUiRect frame, visual, icon;
    OneUiCaptionPath glyph, maximized_glyph;
    float stroke_width, corner_radius; /* negative radius inherits CSS */
    int ellipse, glyph_on_group_hover, glyph_uses_foreground;
    OneUiColor fill, pressed_fill, glyph_color;
} OneUiCaptionButtonPresentation;
typedef struct OneUiTitleBarPresentation {
    size_t struct_size;
    OneUiRect logo, logo_icon, title, leading, accessory;
    float logo_stroke_width, title_font_size;
    int hide_logo_border;
    OneUiCaptionButtonPresentation buttons[3];
} OneUiTitleBarPresentation;

/* Returns 1 on success, 0 on invalid input/type. Invalid input is atomic.
 * NULL restores default geometry. Size changes require resubmission on UI thread. */
ONEUI_API int oneui_title_bar_set_presentation(OneUiWidget* title_bar, const OneUiTitleBarPresentation* presentation);
#ifdef __cplusplus
}
#endif
