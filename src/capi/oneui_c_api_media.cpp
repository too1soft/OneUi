#include "oneui_c_api_controls_internal.h"
#include "oneui_c_api_error.h"
#include "oneui/controls/image_view.h"
#include "oneui/controls/realtime_frame_view.h"
#include "oneui/controls/remote_input_region.h"
#include <algorithm>
#include <cmath>
#include <climits>
#include <cstring>
#include <cwchar>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace oneui::capi;

namespace {

oneui::PixelFormat toPixelFormat(OneUiPixelFormat format) {
    switch (format) {
    case OneUiPixelFormatRgba8888:
        return oneui::PixelFormat::Rgba8888;
    case OneUiPixelFormatNv12:
        return oneui::PixelFormat::Nv12;
    case OneUiPixelFormatBgra8888:
    default:
        return oneui::PixelFormat::Bgra8888;
    }
}

oneui::ScaleMode toScaleMode(OneUiVideoScaleMode mode) {
    switch (mode) {
    case OneUiVideoScaleModeActualSize:
        return oneui::ScaleMode::ActualSize;
    case OneUiVideoScaleModeFill:
        return oneui::ScaleMode::Fill;
    case OneUiVideoScaleModeStretch:
        return oneui::ScaleMode::Stretch;
    case OneUiVideoScaleModeFit:
    default:
        return oneui::ScaleMode::Fit;
    }
}

oneui::RemoteInputScaleMode toRemoteInputScaleMode(OneUiVideoScaleMode mode) {
    switch (mode) {
    case OneUiVideoScaleModeActualSize:
        return oneui::RemoteInputScaleMode::ActualSize;
    case OneUiVideoScaleModeFill:
        return oneui::RemoteInputScaleMode::Fill;
    case OneUiVideoScaleModeStretch:
        return oneui::RemoteInputScaleMode::Stretch;
    case OneUiVideoScaleModeFit:
    default:
        return oneui::RemoteInputScaleMode::Fit;
    }
}

} // namespace

OneUiWidget* oneui_image_view_create() {
    return wrap(std::make_shared<oneui::ImageView>());
}

int oneui_image_view_set_rgba(
    OneUiWidget* image,
    const unsigned char* pixels,
    size_t length,
    int width,
    int height,
    int stride) {
    auto* nativeImage = asWidget<oneui::ImageView>(image);
    if (!nativeImage) {
        return 0;
    }
    return nativeImage->setRgbaPixels(pixels, length, width, height, stride) ? 1 : 0;
}

void oneui_image_view_clear(OneUiWidget* image) {
    auto* nativeImage = asWidget<oneui::ImageView>(image);
    if (nativeImage) {
        nativeImage->clearImage();
    }
}

void oneui_image_view_set_content_mode(OneUiWidget* image, int content_mode) {
    auto* nativeImage = asWidget<oneui::ImageView>(image);
    if (!nativeImage) {
        return;
    }
    const int clamped = std::clamp(content_mode, 0, 3);
    nativeImage->setContentMode(static_cast<oneui::ImageContentMode>(clamped));
}

void oneui_image_view_set_content_alignment(
    OneUiWidget* image,
    int horizontal_alignment,
    int vertical_alignment) {
    auto* nativeImage = asWidget<oneui::ImageView>(image);
    if (!nativeImage) {
        return;
    }
    nativeImage->setContentAlignment(
        static_cast<oneui::ImageContentAlignment>(std::clamp(horizontal_alignment, 0, 2)),
        static_cast<oneui::ImageContentAlignment>(std::clamp(vertical_alignment, 0, 2)));
}

void oneui_image_view_set_corner_radius(OneUiWidget* image, float radius) {
    auto* nativeImage = asWidget<oneui::ImageView>(image);
    if (nativeImage) {
        nativeImage->setCornerRadius(radius);
    }
}

void oneui_image_view_set_background(
    OneUiWidget* image,
    unsigned char r,
    unsigned char g,
    unsigned char b,
    unsigned char a) {
    auto* nativeImage = asWidget<oneui::ImageView>(image);
    if (nativeImage) {
        nativeImage->setBackground(oneui::Color{r, g, b, a});
    }
}

OneUiWidget* oneui_realtime_frame_view_create(void) {
    auto* wrapper = wrap(std::make_shared<oneui::RealtimeFrameView>());
    if (wrapper) {
        wrapper->tag = "realtime-frame-view";
    }
    return wrapper;
}

void oneui_realtime_frame_view_set_scale_mode(OneUiWidget* frame_view, OneUiVideoScaleMode scale_mode) {
    auto* nativeFrameView = asWidget<oneui::RealtimeFrameView>(frame_view);
    if (!nativeFrameView) {
        return;
    }
    nativeFrameView->setScaleMode(toScaleMode(scale_mode));
}

void oneui_realtime_frame_view_set_content_alignment(
    OneUiWidget* frame_view,
    int horizontal_alignment,
    int vertical_alignment) {
    auto* nativeFrameView = asWidget<oneui::RealtimeFrameView>(frame_view);
    if (!nativeFrameView) {
        return;
    }
    nativeFrameView->setContentAlignment(
        static_cast<oneui::ImageContentAlignment>(std::clamp(horizontal_alignment, 0, 2)),
        static_cast<oneui::ImageContentAlignment>(std::clamp(vertical_alignment, 0, 2)));
}

void oneui_realtime_frame_view_set_background(OneUiWidget* frame_view, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    auto* nativeFrameView = asWidget<oneui::RealtimeFrameView>(frame_view);
    if (!nativeFrameView) {
        return;
    }
    nativeFrameView->setBackgroundColor(oneui::Color{r, g, b, a});
}

void oneui_realtime_frame_view_submit_frame(
    OneUiWidget* frame_view,
    const void* pixels,
    int width,
    int height,
    int stride,
    OneUiPixelFormat pixel_format,
    unsigned long long frame_id,
    unsigned long long timestamp_us) {
    auto* nativeFrameView = asWidget<oneui::RealtimeFrameView>(frame_view);
    if (!nativeFrameView) {
        return;
    }
    oneui::VideoFrame frame;
    frame.data = pixels;
    frame.width = width;
    frame.height = height;
    frame.stride = stride;
    frame.format = toPixelFormat(pixel_format);
    frame.frameId = static_cast<std::uint64_t>(frame_id);
    frame.timestampUs = static_cast<std::uint64_t>(timestamp_us);
    nativeFrameView->submitFrame(frame);
}

int oneui_realtime_frame_view_submit_frame_owned(
    OneUiWidget* frame_view,
    const void* pixels,
    size_t pixel_bytes,
    int width,
    int height,
    int stride,
    OneUiPixelFormat pixel_format,
    unsigned long long frame_id,
    unsigned long long timestamp_us,
    OneUiFrameReleaseCallback release_callback,
    void* user_data) {
    if (!release_callback) {
        return 0;
    }

    std::shared_ptr<const void> owner(
        pixels,
        [release_callback, user_data](const void* releasedPixels) {
            release_callback(releasedPixels, user_data);
        });
    auto* nativeFrameView = asWidget<oneui::RealtimeFrameView>(frame_view);
    if (!nativeFrameView) {
        return 0;
    }

    oneui::VideoFrame frame;
    frame.data = pixels;
    frame.width = width;
    frame.height = height;
    frame.stride = stride;
    frame.format = toPixelFormat(pixel_format);
    frame.frameId = static_cast<std::uint64_t>(frame_id);
    frame.timestampUs = static_cast<std::uint64_t>(timestamp_us);
    return nativeFrameView->submitOwnedFrame(frame, pixel_bytes, std::move(owner)) ? 1 : 0;
}

int oneui_realtime_frame_view_submit_damage(
    OneUiWidget* frame_view,
    int frame_width,
    int frame_height,
    OneUiPixelFormat pixel_format,
    const OneUiVideoFramePatch* patches,
    size_t patch_count,
    unsigned long long frame_id,
    unsigned long long timestamp_us) {
    auto* nativeFrameView = asWidget<oneui::RealtimeFrameView>(frame_view);
    if (!nativeFrameView || !patches || patch_count == 0 || patch_count > 64) {
        return 0;
    }
    std::vector<oneui::VideoFramePatch> nativePatches;
    nativePatches.reserve(patch_count);
    for (size_t index = 0; index < patch_count; ++index) {
        const auto& patch = patches[index];
        nativePatches.push_back(oneui::VideoFramePatch{
            patch.pixels,
            patch.pixel_bytes,
            patch.x,
            patch.y,
            patch.width,
            patch.height,
            patch.stride});
    }
    return nativeFrameView->submitDamage(
        frame_width,
        frame_height,
        toPixelFormat(pixel_format),
        nativePatches.data(),
        nativePatches.size(),
        static_cast<std::uint64_t>(frame_id),
        static_cast<std::uint64_t>(timestamp_us)) ? 1 : 0;
}

OneUiWidget* oneui_remote_input_region_create(void) {
    auto* wrapper = wrap(std::make_shared<oneui::RemoteInputRegion>());
    if (wrapper) {
        wrapper->tag = "remote-input-region";
        applyCurrentStyleSheet(wrapper);
    }
    return wrapper;
}

void oneui_remote_input_region_set_remote_size(OneUiWidget* region, float width, float height) {
    if (auto* nativeRegion = asWidget<oneui::RemoteInputRegion>(region)) {
        nativeRegion->setRemoteSize(oneui::Size{width, height});
    }
}

void oneui_remote_input_region_set_scale_mode(OneUiWidget* region, OneUiVideoScaleMode scale_mode) {
    if (auto* nativeRegion = asWidget<oneui::RemoteInputRegion>(region)) {
        nativeRegion->setScaleMode(toRemoteInputScaleMode(scale_mode));
    }
}

void oneui_remote_input_region_set_cursor_mode(OneUiWidget* region, OneUiRemoteCursorMode cursor_mode) {
    if (auto* nativeRegion = asWidget<oneui::RemoteInputRegion>(region)) {
        oneui::RemoteCursorMode mode = oneui::RemoteCursorMode::Default;
        if (cursor_mode == OneUiRemoteCursorModeHidden) {
            mode = oneui::RemoteCursorMode::Hidden;
        } else if (cursor_mode == OneUiRemoteCursorModeBitmap) {
            mode = oneui::RemoteCursorMode::Bitmap;
        }
        nativeRegion->setRemoteCursorMode(mode);
    }
}

void oneui_remote_input_region_set_cursor_position(OneUiWidget* region, float remote_x, float remote_y) {
    if (auto* nativeRegion = asWidget<oneui::RemoteInputRegion>(region)) {
        nativeRegion->setRemoteCursorPosition(oneui::Point{remote_x, remote_y});
    }
}

int oneui_remote_input_region_set_cursor_bitmap_rgba(
    OneUiWidget* region,
    const void* pixels,
    size_t pixel_bytes,
    int width,
    int height,
    int stride,
    int hotspot_x,
    int hotspot_y) {
    if (auto* nativeRegion = asWidget<oneui::RemoteInputRegion>(region)) {
        return nativeRegion->setRemoteCursorBitmap(
            static_cast<const std::uint8_t*>(pixels),
            pixel_bytes,
            width,
            height,
            stride,
            hotspot_x,
            hotspot_y) ? 1 : 0;
    }
    return 0;
}

void oneui_remote_input_region_set_on_pointer(
    OneUiWidget* region,
    OneUiRemotePointerCallback callback,
    void* user_data) {
    if (auto* nativeRegion = asWidget<oneui::RemoteInputRegion>(region)) {
        if (!callback) {
            nativeRegion->setOnPointer(nullptr);
            return;
        }
        nativeRegion->setOnPointer([callback, user_data](const oneui::RemotePointerEvent& event) {
            OneUiRemotePointerEvent cEvent{};
            cEvent.window_x = event.windowPosition.x;
            cEvent.window_y = event.windowPosition.y;
            cEvent.content_x = event.contentPosition.x;
            cEvent.content_y = event.contentPosition.y;
            cEvent.normalized_x = event.normalizedPosition.x;
            cEvent.normalized_y = event.normalizedPosition.y;
            cEvent.remote_x = event.remotePosition.x;
            cEvent.remote_y = event.remotePosition.y;
            cEvent.button = toCButton(event.button);
            cEvent.pressed = event.pressed ? 1 : 0;
            cEvent.wheel_delta_x = event.wheelDeltaX;
            cEvent.wheel_delta_y = event.wheelDeltaY;
            callback(&cEvent, user_data);
        });
    }
}

void oneui_remote_input_region_set_on_raw_key(
    OneUiWidget* region,
    OneUiRawKeyCallback callback,
    void* user_data) {
    if (auto* nativeRegion = asWidget<oneui::RemoteInputRegion>(region)) {
        if (!callback) {
            nativeRegion->setOnRawKey(nullptr);
            return;
        }
        nativeRegion->setOnRawKey([callback, user_data](const oneui::RawKeyEvent& event) {
            OneUiRawKeyEvent cEvent{};
            cEvent.virtual_key = event.virtualKey;
            cEvent.scan_code = event.scanCode;
            cEvent.pressed = event.pressed ? 1 : 0;
            cEvent.repeat = event.repeat ? 1 : 0;
            cEvent.extended = event.extended ? 1 : 0;
            cEvent.alt = event.alt ? 1 : 0;
            cEvent.ctrl = event.ctrl ? 1 : 0;
            cEvent.shift = event.shift ? 1 : 0;
            cEvent.win = event.win ? 1 : 0;
            callback(&cEvent, user_data);
        });
    }
}

void oneui_remote_input_region_set_on_text_input_utf8(
    OneUiWidget* region,
    OneUiUtf8TextCallback callback,
    void* user_data) {
    if (auto* nativeRegion = asWidget<oneui::RemoteInputRegion>(region)) {
        if (!callback) {
            nativeRegion->setOnTextInput(nullptr);
            return;
        }
        nativeRegion->setOnTextInput([callback, user_data](const std::wstring& text) {
            const std::string utf8 = utf8FromWide(text);
            callback(utf8.data(), utf8.size(), user_data);
        });
    }
}

void oneui_remote_input_region_release_all_inputs(OneUiWidget* region) {
    if (auto* nativeRegion = asWidget<oneui::RemoteInputRegion>(region)) {
        nativeRegion->releaseAllInputs();
    }
}
