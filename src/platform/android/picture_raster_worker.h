#pragma once

#include "oneui/frame_paint_trace.h"
#include "oneui/mobile/render_statistics.h"
#include "include/core/SkPicture.h"
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>

struct ANativeWindow;

namespace oneui::mobile {

struct RecordedFrame {
    sk_sp<SkPicture> picture;
    FramePaintTrace sources;
    std::uint64_t generation = 0;
};

// All backend operations, including destruction, execute on the raster thread.
class PictureRasterBackend {
public:
    virtual ~PictureRasterBackend() = default;
    virtual bool attach(ANativeWindow* acquiredWindow, int width, int height) = 0;
    virtual void reset() = 0;
    virtual bool render(const SkPicture& picture) = 0;
    virtual std::string renderer() const = 0;
    virtual std::string error() const = 0;
    virtual std::uint64_t contextCount() const = 0;
    virtual std::uint64_t resizeCount() const = 0;
};

class PictureRasterWorker final {
public:
    // Control/submission calls are serialized by the owning UI thread.
    // statistics() is safe on any thread, including pixel-release callbacks.
    explicit PictureRasterWorker(std::unique_ptr<PictureRasterBackend> backend);
    ~PictureRasterWorker();
    PictureRasterWorker(const PictureRasterWorker&) = delete;
    PictureRasterWorker& operator=(const PictureRasterWorker&) = delete;
    bool attach(ANativeWindow* acquiredWindow, int width, int height);
    void reset();
    bool submit(RecordedFrame frame);
    RenderStatistics statistics() const;

private:
    bool control(std::function<bool(PictureRasterBackend&)> operation, bool attached);
    void run(std::unique_ptr<PictureRasterBackend> backend);
    void rendered(const RecordedFrame& frame, bool success, std::uint64_t durationUs,
                  const PictureRasterBackend& backend);
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::unique_ptr<RecordedFrame> pending_;
    std::function<void(PictureRasterBackend&)> control_;
    RenderStatistics stats_;
    FramePaintTrace swapped_;
    bool stopping_ = false;
    std::thread thread_;
};

std::unique_ptr<PictureRasterBackend> makeEglPictureBackend();

} // namespace oneui::mobile
