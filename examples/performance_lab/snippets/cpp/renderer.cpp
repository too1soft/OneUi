#include <oneui/platform/window.h>

struct ExampleRendererSample {
    oneui::RendererInfo renderer;
    oneui::RendererMemoryInfo caches;
    oneui::Size physicalSize;
};
ExampleRendererSample example_renderer(oneui::Window &window) {
    auto size = window.clientSize();
    const auto scale = window.dpiScale();
    // Counters are cumulative. Difference two samples over the same interval.
    // paintMs is CPU wall time, not GPU time or actual display presentation FPS.
    return {window.rendererInfo(),
            window.rendererMemoryInfo(),
            {size.width * scale, size.height * scale}};
}
