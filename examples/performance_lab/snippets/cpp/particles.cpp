#include <oneui/canvas.h>
#include <oneui/widget.h>

class ExampleParticles final : public oneui::Widget {
  public:
    void paint(oneui::Canvas &canvas) override {
        const auto bounds = frame();
        for (int i = 0; i < 100; ++i) {
            const float x = bounds.x + float(i % 10) * 16;
            const float y = bounds.y + float(i / 10) * 16;
            canvas.fillEllipse({x, y, 8, 8}, {40, 97, 61});
        }
    }
};
// Animation owners request a frame only while active. Do not invalidate from
// paint(), which would keep an otherwise idle page repainting continuously.
