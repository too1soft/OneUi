#include <memory>
#include <oneui/controls/image_view.h>

std::shared_ptr<oneui::ImageView> example_image() {
    auto image = std::make_shared<oneui::ImageView>();
    const std::uint8_t rgba[] = {196, 237, 135, 255, 48, 98, 78, 255};
    // Pixels are copied; the local array need not outlive the widget.
    image->setRgbaPixels(rgba, sizeof(rgba), 2, 1, 8);
    image->setContentMode(oneui::ImageContentMode::Contain);
    image->setCornerRadius(12);
    return image;
}

// Custom Canvas painting currently has a C++ API; ImageView is available in C/Rust.
class ExampleDrawing final : public oneui::Widget {
  public:
    void paint(oneui::Canvas &canvas) override {
        using namespace oneui;
        const Rect bounds = frame().inset(Insets{12});
        canvas.save();
        canvas.clipRect(bounds);
        canvas.fillLinearGradient(bounds, {40, 97, 61}, {196, 237, 135}, 35, 6);
        CanvasPath path;
        path.moveTo({bounds.x, (bounds.y + bounds.height)});
        path.cubicTo({bounds.x, bounds.y}, {(bounds.x + bounds.width), (bounds.y + bounds.height)},
                     {(bounds.x + bounds.width), bounds.y});
        canvas.strokePath(path, {229, 236, 232}, 3);
        canvas.restore();
    }
};

std::shared_ptr<oneui::Widget> example_drawing() { return std::make_shared<ExampleDrawing>(); }
