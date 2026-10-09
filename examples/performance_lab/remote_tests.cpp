#include "remote_scene.hpp"
#include <iostream>
#include <stdexcept>

using namespace oneui;
void remotePageLifecycle();
namespace {
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
} // namespace
int main() {
    try {
        lab::RemoteScene scene;
        for (int i = 0; i < 60; ++i) {
            scene.submit(lab::RemoteScene::FrameKind(i % 3));
            auto frame = scene.frame->latestFrame();
            require(frame && frame->width == 640 && frame->height == 360 &&
                        frame->pixels.size() == 921600,
                    "Synthetic frame dimensions or storage mismatch");
            const bool bgra = i % 3 == 1;
            require(frame->format == (bgra ? PixelFormat::Bgra8888 : PixelFormat::Rgba8888),
                    "Frame format was not retained");
            auto owner = scene.ownedBuffer();
            if (i % 3 == 2)
                require(!owner.expired(), "Owned frame was released before presentation");
            require(scene.damage(), "Compatible partial update rejected");
            const auto updated = scene.frame->latestFrame();
            require(updated && updated->frameId > frame->frameId &&
                        updated->pixels != frame->pixels,
                    "Partial update did not change the retained frame");
            require(owner.expired(), "Copy-on-write retained obsolete external frame owner");
            require(!scene.probeNv12(),
                    "NV12 support changed; update the capability demonstration");
            require(scene.frame->latestFrame()->pixels == updated->pixels,
                    "Unsupported owned frame damaged the current image");
        }
        scene.input->setFrame({0, 0, 800, 600});
        int releases = 0, keyReleases = 0;
        Point mapped{};
        scene.input->setOnPointer([&](const RemotePointerEvent &event) {
            mapped = event.remotePosition;
            if (event.button == PointerButton::Left && !event.pressed)
                ++releases;
        });
        scene.input->setOnRawKey([&](const RawKeyEvent &event) {
            if (!event.pressed)
                ++keyReleases;
        });
        require(scene.input->dispatchPointer({400, 300}, PointerButton::Left, true),
                "Input dispatch failed");
        require(mapped.x == 320 && mapped.y == 180,
                "Letterboxed input was not mapped to remote pixels");
        RawKeyEvent key;
        key.virtualKey = 17;
        key.pressed = true;
        key.ctrl = true;
        scene.input->dispatchRawKey(key);
        scene.input->releaseAllInputs();
        scene.input->releaseAllInputs();
        require(releases == 1 && keyReleases == 1, "Held input release was lost or duplicated");
        scene.cursor(RemoteCursorMode::Bitmap);
        require(scene.input->remoteCursorMode() == RemoteCursorMode::Bitmap,
                "Bitmap cursor rejected");
        scene.cursor(RemoteCursorMode::Hidden);
        scene.cursor(RemoteCursorMode::Default);
        remotePageLifecycle();
        std::cout << "PASS: 60 full/damage/owner cycles, rejected NV12 preserves frame, mapped "
                     "input, release and cursor.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
