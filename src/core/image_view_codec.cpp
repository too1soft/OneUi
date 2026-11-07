#include "include/codec/SkCodec.h"
#include "include/core/SkData.h"
#include "oneui/controls/image_view.h"
#include <cstring>
#include <string_view>

namespace oneui {
bool ImageView::setPngDataUri(const std::string &uri) {
    constexpr std::string_view prefix = "data:image/png;base64,";
    clearImage();
    if (uri.size() > 384 * 1024 || uri.compare(0, prefix.size(), prefix) != 0)
        return false;
    std::string_view encoded(uri.data() + prefix.size(), uri.size() - prefix.size());
    if (encoded.empty() || encoded.size() % 4)
        return false;
    std::vector<uint8_t> bytes;
    bytes.reserve(encoded.size() / 4 * 3);
    auto digit = [](char ch) -> int {
        if (ch >= 'A' && ch <= 'Z')
            return ch - 'A';
        if (ch >= 'a' && ch <= 'z')
            return ch - 'a' + 26;
        if (ch >= '0' && ch <= '9')
            return ch - '0' + 52;
        return ch == '+' ? 62 : ch == '/' ? 63 : -1;
    };
    for (size_t i = 0; i < encoded.size(); i += 4) {
        const int a = digit(encoded[i]), b = digit(encoded[i + 1]);
        const bool pad2 = encoded[i + 2] == '=', pad3 = encoded[i + 3] == '=';
        const int c = pad2 ? 0 : digit(encoded[i + 2]), d = pad3 ? 0 : digit(encoded[i + 3]);
        if (a < 0 || b < 0 || c < 0 || d < 0 || (pad2 && !pad3) ||
            ((pad2 || pad3) && i + 4 != encoded.size()) || (pad2 && (b & 15)) || (pad3 && !pad2 && (c & 3)))
            return false;
        bytes.push_back(uint8_t((a << 2) | (b >> 4)));
        if (!pad2)
            bytes.push_back(uint8_t((b << 4) | (c >> 2)));
        if (!pad3)
            bytes.push_back(uint8_t((c << 6) | d));
    }
    constexpr uint8_t magic[]{137, 80, 78, 71, 13, 10, 26, 10};
    if (bytes.size() < sizeof(magic) || std::memcmp(bytes.data(), magic, sizeof(magic)))
        return false;
    auto codec = SkCodec::MakeFromData(SkData::MakeWithCopy(bytes.data(), bytes.size()));
    if (!codec || codec->getFrameCount() != 1)
        return false;
    const auto info = codec->getInfo();
    const int width = info.width(), height = info.height();
    if (width <= 0 || height <= 0 || width > 1024 || height > 1024 || size_t(width) * height > 1024 * 1024)
        return false;
    std::vector<uint8_t> pixels(size_t(width) * height * 4);
    const auto target = SkImageInfo::Make(width, height, kRGBA_8888_SkColorType, kUnpremul_SkAlphaType);
    if (codec->getPixels(target, pixels.data(), size_t(width) * 4) != SkCodec::kSuccess)
        return false;
    return setRgbaPixels(pixels.data(), pixels.size(), width, height, width * 4);
}
} // namespace oneui
