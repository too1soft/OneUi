#pragma once
#include <oneui/canvas.h>

namespace oneui::mobile {
inline void paintKeyboardIcon(Canvas &canvas, Rect r, Color color) {
    const float s = r.width / 24;
    const auto point = [r, s](float x, float y) { return Point{r.x + x * s, r.y + y * s}; };
    canvas.strokeRect({r.x + 1 * s, r.y + 4 * s, 22 * s, 16 * s}, color, 3 * s, 1.5f * s);
    for (int row = 0; row < 2; row++)
        for (int column = 0; column < 5; column++)
            canvas.drawLine(point(4.5f + column * 3.75f, 8.f + row * 4),
                            point(5.f + column * 3.75f, 8.f + row * 4), color, 1.5f * s);
    canvas.drawLine(point(7, 16), point(17, 16), color, 1.5f * s);
}

inline void paintTouchpadIcon(Canvas &canvas, Rect r, Color color) {
    const float s = r.width / 24;
    canvas.strokeRect({r.x + 3 * s, r.y + 2 * s, 18 * s, 20 * s}, color, 3 * s, 1.5f * s);
    canvas.drawLine({r.x + 3 * s, r.y + 16 * s}, {r.x + 21 * s, r.y + 16 * s}, color, 1.5f * s);
    canvas.drawLine({r.x + 12 * s, r.y + 16 * s}, {r.x + 12 * s, r.y + 22 * s}, color, 1.5f * s);
}

inline void paintPointerIcon(Canvas &canvas, Rect r, Color color) {
    const float s = r.width / 24;
    const Point points[] = {{4, 2}, {20, 13}, {13, 14}, {10, 21}, {4, 2}};
    for (int i = 1; i < 5; i++)
        canvas.drawLine({r.x + points[i - 1].x * s, r.y + points[i - 1].y * s},
                        {r.x + points[i].x * s, r.y + points[i].y * s}, color, 1.5f * s);
}
} // namespace oneui::mobile
