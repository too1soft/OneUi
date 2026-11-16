#pragma once
#include "oneui/mobile/pixel_conversion.h"
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#include <mutex>
#include <vector>

namespace oneui::mobile {
// One retained frame, independent of codec/network and usable by every host.
class FrameMailbox final {
  public:
    struct Frame {
        std::shared_ptr<std::vector<uint8_t>> pixels;
        int width = 0, height = 0, stride = 0;
        uint64_t sequence = 0;
    };
    bool submit(const void *data, size_t bytes, int width, int height, int stride) {
        if (!data || !validFrameDimensions(width,height) ||
            stride < width * 4 || stride > maxFrameSide * 4 || size_t(stride) * height > bytes)
            return false;
        std::shared_ptr<std::vector<uint8_t>> pixels;
        try { pixels = std::make_shared<std::vector<uint8_t>>(size_t(width) * height * 4); }
        catch(const std::bad_alloc &) { return false; }
        for (int row = 0; row < height; ++row)
            std::memcpy(pixels->data() + size_t(row) * width * 4,
                        static_cast<const uint8_t *>(data) + size_t(row) * stride,
                        size_t(width) * 4);
        return submitOwned(std::move(pixels), width, height, width * 4);
    }
    bool submitNv12(const void *data, size_t bytes, int width, int height, int stride) {
        if (!data || width < 2 || height < 2 || !validFrameDimensions(width,height) || width % 2 ||
            height % 2 || stride < width || stride > maxFrameSide ||
            size_t(stride) * (height + height / 2) > bytes)
            return false;
        std::shared_ptr<std::vector<uint8_t>> pixels;
        try { pixels = std::make_shared<std::vector<uint8_t>>(size_t(width) * height * 4); }
        catch(const std::bad_alloc &) { return false; }
        if (!nv12ToRgba(static_cast<const uint8_t *>(data), bytes, width, height, stride,
                        pixels->data(), pixels->size(), width * 4))
            return false;
        return submitOwned(std::move(pixels), width, height, width * 4);
    }
    // The producer relinquishes writes; the consumer retains these exact pixels.
    bool submitOwned(std::shared_ptr<std::vector<uint8_t>> pixels, int width, int height, int stride) {
        if (!pixels || !validFrameDimensions(width,height) || stride < width * 4 ||
            stride > maxFrameSide * 4 || size_t(stride) * height > pixels->size())
            return false;
        std::lock_guard<std::mutex> lock(mutex_);
        latest_ = {std::move(pixels), width, height, stride, ++sequence_};
        return true;
    }
    Frame take() {
        std::lock_guard<std::mutex> lock(mutex_);
        auto frame = std::move(latest_);
        latest_ = {};
        return frame;
    }
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        latest_ = {};
    }
    uint64_t sequence() {
        std::lock_guard<std::mutex> lock(mutex_);
        return sequence_;
    }
    // A queued lifecycle notification may arrive after newer pixels were submitted.
    void clearThrough(uint64_t sequence) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (latest_.sequence <= sequence)
            latest_ = {};
    }

  private:
    std::mutex mutex_;
    Frame latest_;
    uint64_t sequence_ = 0;
};
} // namespace oneui::mobile
