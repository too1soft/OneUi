#include <oneui/oneui.h>

std::shared_ptr<oneui::OverlayHost> example_remote() {
    auto frame = std::make_shared<oneui::RealtimeFrameView>();
    const std::uint8_t pixel[] = {196, 237, 135, 255};
    frame->submitFrame({pixel, 1, 1, 4, oneui::PixelFormat::Rgba8888, 1, 0});
    auto input = std::make_shared<oneui::RemoteInputRegion>();
    input->setRemoteSize({1, 1}); // Match the frame dimensions.
    input->setOnPointer([](const oneui::RemotePointerEvent &event) {
        // event.remotePosition contains mapped remote coordinates.
        (void)event;
    });
    auto host = std::make_shared<oneui::OverlayHost>();
    host->setContent(frame);
    host->addOverlay(input);
    // Retain input and call releaseAllInputs() when leaving the page/session.
    return host;
}
