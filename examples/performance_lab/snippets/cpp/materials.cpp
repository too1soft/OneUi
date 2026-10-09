#include <oneui/canvas.h>

void example_material(oneui::Canvas &canvas, oneui::Rect bounds) {
    canvas.fillLinearGradient(bounds, {196, 237, 135}, {48, 98, 78}, 45, 12);
    canvas.drawText(L"中文 · Gradient", bounds, {20, 35, 27}, 18);
}
