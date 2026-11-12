#include "picture_raster_worker.h"
#include <chrono>
#include <stdexcept>
#include <utility>

namespace oneui::mobile {
namespace {
std::uint64_t nowNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
}

PictureRasterWorker::PictureRasterWorker(std::unique_ptr<PictureRasterBackend> backend) {
    if (!backend) throw std::invalid_argument("Missing raster backend");
    thread_ = std::thread([this, value = std::move(backend)]() mutable { run(std::move(value)); });
}

PictureRasterWorker::~PictureRasterWorker() {
    std::unique_ptr<RecordedFrame> retired;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
        retired = std::move(pending_);
    }
    retired.reset();
    wake_.notify_one();
    thread_.join();
}

bool PictureRasterWorker::control(std::function<bool(PictureRasterBackend&)> operation, bool attached) {
    auto promise = std::make_shared<std::promise<bool>>();
    auto result = promise->get_future();
    std::unique_ptr<RecordedFrame> retired;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_ || control_) return false;
        const auto generation = stats_.generation + 1;
        stats_ = {};
        stats_.generation = generation;
        retired = std::move(pending_);
        control_ = [this, operation = std::move(operation), promise, attached](PictureRasterBackend& backend) {
            bool success = false;
            try { success = operation(backend); } catch (...) { backend.reset(); }
            auto renderer = backend.renderer();
            auto error = success ? std::string{} : backend.error();
            const auto contexts = backend.contextCount(), resizes = backend.resizeCount();
            {
                std::lock_guard<std::mutex> lock(mutex_);
                stats_.attached = success && attached;
                stats_.renderer = std::move(renderer);
                stats_.error = std::move(error);
                if (!success && stats_.error.empty()) stats_.error = "Raster lifecycle operation failed";
                stats_.contexts = contexts;
                stats_.resizes = resizes;
                swapped_ = {};
            }
            promise->set_value(success);
        };
    }
    retired.reset();
    wake_.notify_one();
    return result.get();
}

bool PictureRasterWorker::attach(ANativeWindow* acquiredWindow, int width, int height) {
    return control([=](PictureRasterBackend& backend) { return backend.attach(acquiredWindow, width, height); }, true);
}

void PictureRasterWorker::reset() {
    (void)control([](PictureRasterBackend& backend) { backend.reset(); return true; }, false);
}

bool PictureRasterWorker::submit(RecordedFrame frame) {
    if (!frame.picture) return false;
    auto value = std::make_unique<RecordedFrame>(std::move(frame));
    std::unique_ptr<RecordedFrame> retired;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_ || !stats_.attached || !stats_.error.empty() || value->generation != stats_.generation) return false;
        retired = std::exchange(pending_, std::move(value));
        ++stats_.submitted;
        if (retired) ++stats_.coalesced;
    }
    // A picture can own application pixel-release callbacks.
    retired.reset();
    wake_.notify_one();
    return true;
}

RenderStatistics PictureRasterWorker::statistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

void PictureRasterWorker::rendered(const RecordedFrame& frame, bool success, std::uint64_t durationUs,
                                   const PictureRasterBackend& backend) {
    auto error = success ? std::string{} : backend.error();
    std::unique_ptr<RecordedFrame> retired;
    std::unique_lock<std::mutex> lock(mutex_);
    if (frame.generation != stats_.generation) return;
    ++stats_.rendered;
    stats_.rasterWallUs += durationUs;
    if (!success) {
        ++stats_.swapFailure;
        stats_.attached = false;
        stats_.error = std::move(error);
        if (stats_.error.empty()) stats_.error = "Raster presentation failed";
        retired = std::move(pending_);
        lock.unlock();
        retired.reset();
        return;
    }
    ++stats_.swapSuccess;
    stats_.lastSwapTimeNs = nowNs();
    stats_.traceOverflow = stats_.traceOverflow || frame.sources.overflow;
    if (frame.sources.size) ++stats_.frameBearingSwaps;
    for (std::size_t index = 0; index < frame.sources.size; ++index) {
        const auto stamp = frame.sources.stamps[index];
        bool duplicate = false;
        for (std::size_t previous = 0; previous < swapped_.size; ++previous) {
            if (swapped_.stamps[previous].source == stamp.source) {
                duplicate = swapped_.stamps[previous].frame == stamp.frame;
                break;
            }
        }
        if (!duplicate) ++stats_.sourceFrameChangesSwapped;
        stats_.lastSource = stamp.source;
        stats_.lastFrame = stamp.frame;
    }
    swapped_ = frame.sources;
}

void PictureRasterWorker::run(std::unique_ptr<PictureRasterBackend> backend) {
    for (;;) {
        std::unique_ptr<RecordedFrame> frame;
        std::function<void(PictureRasterBackend&)> operation;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            wake_.wait(lock, [&] { return stopping_ || control_ || pending_; });
            if (stopping_) break;
            operation = std::exchange(control_, {});
            if (!operation) frame = std::move(pending_);
        }
        if (operation) { operation(*backend); continue; }
        if (frame) {
            const auto started = nowNs();
            bool success = false;
            try { success = backend->render(*frame->picture); } catch (...) { }
            rendered(*frame, success, (nowNs() - started) / 1000, *backend);
        }
    }
    backend->reset();
    backend.reset();
}

} // namespace oneui::mobile
