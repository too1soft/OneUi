#include "oneui/controls/image_view.h"
#include "platform/shared/skia_canvas.h"
#include "include/codec/SkCodec.h"
#include "include/core/SkBitmap.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkData.h"
#include "include/core/SkStream.h"
#include "include/encode/SkPngEncoder.h"
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
std::string uri(const SkData& data) {
    constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const auto* bytes = static_cast<const uint8_t*>(data.data());
    std::string result = "data:image/png;base64,";
    for (size_t i = 0; i < data.size(); i += 3) {
        const auto count = std::min(size_t(3), data.size() - i);
        const uint32_t value = uint32_t(bytes[i]) << 16 |
            (count > 1 ? uint32_t(bytes[i + 1]) << 8 : 0) | (count > 2 ? bytes[i + 2] : 0);
        result += alphabet[value >> 18];
        result += alphabet[(value >> 12) & 63];
        result += count > 1 ? alphabet[(value >> 6) & 63] : '=';
        result += count > 2 ? alphabet[value & 63] : '=';
    }
    return result;
}
SkBitmap paint(oneui::ImageView& image, int width, int height) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(width, height);
    bitmap.eraseColor(SK_ColorTRANSPARENT);
    SkCanvas target(bitmap);
    auto canvas = oneui::rendering::makeSkiaCanvas(target);
    image.setFrame({0, 0, float(width), float(height)});
    image.paint(*canvas);
    return bitmap;
}
void exercise(int width, int height, SkAlphaType alpha) {
    SkBitmap bitmap;
    bitmap.allocPixels(SkImageInfo::Make(width, height, kRGBA_8888_SkColorType, alpha));
    auto* pixels = static_cast<uint8_t*>(bitmap.getPixels());
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
        auto* pixel = pixels + y * bitmap.rowBytes() + x * 4;
        pixel[0] = uint8_t(x * 31); pixel[1] = uint8_t(y * 37); pixel[2] = 123;
        pixel[3] = alpha == kOpaque_SkAlphaType ? 255 : uint8_t(x * 19 + y * 17);
    }
    SkDynamicMemoryWStream output;
    check(SkPngEncoder::Encode(&output, bitmap.pixmap(), {}), "PNG fixture encoding failed");
    auto png = output.detachAsData();
    auto reference = SkCodec::MakeFromData(png);
    check(reference != nullptr, "Generic PNG decoder failed");
    const auto info = SkImageInfo::Make(width, height, kRGBA_8888_SkColorType, kUnpremul_SkAlphaType);
    std::vector<uint8_t> decoded(size_t(width) * height * 4);
    check(reference->getPixels(info, decoded.data(), width * 4) == SkCodec::kSuccess, "Generic decode failed");
    oneui::ImageView original, candidate;
    check(original.setRgbaPixels(decoded.data(), decoded.size(), width, height, width * 4), "Reference image failed");
    check(candidate.setPngDataUri(uri(*png)), "PNG data URI failed");
    const auto expected = paint(original, width, height), actual = paint(candidate, width, height);
    check(expected.computeByteSize() == actual.computeByteSize() &&
              std::memcmp(expected.getPixels(), actual.getPixels(), actual.computeByteSize()) == 0,
          "PNG decoder specialization changed rendered pixels");
    for (const std::string invalid : {"", "data:image/jpeg;base64,AAAA", "data:image/png;base64,!!!!",
                                      "data:image/png;base64,AA=A", "data:image/png;base64,A==="}) {
        check(!candidate.setPngDataUri(invalid) && !candidate.hasImage(), "Malformed input accepted or old image retained");
    }
    check(!candidate.setPngDataUri(std::string(384 * 1024 + 1, 'A')), "Oversized URI accepted");
}
} // namespace

int main() {
    try {
        exercise(13, 11, kUnpremul_SkAlphaType);
        exercise(16, 12, kOpaque_SkAlphaType);
        exercise(1024, 1, kUnpremul_SkAlphaType);
        std::cout << "PNG specialization matches generic decoder pixels; invalid input rejected\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
