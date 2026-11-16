#include "platform/android/picture_raster_worker.h"
#include "platform/shared/skia_canvas.h"
#include "oneui/controls/realtime_frame_view.h"
#include "oneui/pixel_image.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkPictureRecorder.h"
#include <atomic>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace oneui;
using namespace oneui::mobile;

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

template<class Predicate> void waitFor(Predicate done) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!done()) {
        if (std::chrono::steady_clock::now() >= deadline) throw std::runtime_error("worker did not progress");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

struct BackendState {
    std::mutex mutex;
    std::condition_variable changed;
    bool block = false, entered = false, fail = false, throwRender = false;
    bool destroyed = false;
    unsigned rendered = 0, attaches = 0, resets = 0;
    std::thread::id thread;
    std::vector<unsigned> pictures;
};

class TestBackend final : public PictureRasterBackend {
public:
    explicit TestBackend(std::shared_ptr<BackendState> state) : state_(std::move(state)) {}
    ~TestBackend() override {
        std::lock_guard<std::mutex> lock(state_->mutex);
        checkThread();
        state_->destroyed = true;
    }
    bool attach(ANativeWindow*, int width, int height) override {
        std::lock_guard<std::mutex> lock(state_->mutex);
        checkThread();
        ++state_->attaches;
        return width > 0 && height > 0;
    }
    void reset() override {
        std::lock_guard<std::mutex> lock(state_->mutex);
        checkThread();
        ++state_->resets;
    }
    bool render(const SkPicture& picture) override {
        std::unique_lock<std::mutex> lock(state_->mutex);
        checkThread();
        state_->entered = true;
        state_->changed.notify_all();
        state_->changed.wait(lock, [&] { return !state_->block; });
        ++state_->rendered;
        state_->pictures.push_back(picture.uniqueID());
        if (state_->throwRender) throw std::runtime_error("controlled raster failure");
        return !state_->fail;
    }
    std::string renderer() const override { return "controlled"; }
    std::string error() const override { return "controlled raster failure"; }
    std::uint64_t contextCount() const override { return state_->attaches; }
    std::uint64_t resizeCount() const override { return 0; }
private:
    void checkThread() {
        if (state_->thread == std::thread::id{}) state_->thread = std::this_thread::get_id();
        require(state_->thread == std::this_thread::get_id(), "backend crossed thread ownership");
    }
    std::shared_ptr<BackendState> state_;
};

RecordedFrame frame(std::uint64_t generation, std::uint64_t number,
                    std::function<void()> release = {}) {
    SkPictureRecorder recorder;
    auto* target = recorder.beginRecording(2, 2);
    if (release) {
        auto pixels = std::shared_ptr<const void>(new std::uint8_t[16]{},
            [release = std::move(release)](const void* address) {
                delete[] static_cast<const std::uint8_t*>(address);
                release();
            });
        auto image = PixelImage::makeOwned(static_cast<const std::uint8_t*>(pixels.get()),
            16, 2, 2, 8, CanvasPixelFormat::Rgba8888, pixels);
        auto canvas = rendering::makeSkiaCanvas(*target);
        drawPixelImage(*canvas, {0, 0, 2, 2}, image);
    } else target->clear(SK_ColorRED);
    FramePaintTrace trace;
    trace.note(17, number);
    return {recorder.finishRecordingAsPicture(), trace, generation};
}

void unblock(const std::shared_ptr<BackendState>& state) {
    std::lock_guard<std::mutex> lock(state->mutex);
    state->block = false;
    state->changed.notify_all();
}

struct ReleaseBlockedRaster {
    std::shared_ptr<BackendState> state;
    ~ReleaseBlockedRaster() { unblock(state); }
};

void latestPictureAndActualSwaps() {
    auto state = std::make_shared<BackendState>();
    std::atomic<int> releases{0};
    {
        PictureRasterWorker worker(std::make_unique<TestBackend>(state));
        ReleaseBlockedRaster releaseOnFailure{state};
        require(worker.attach(nullptr, 2, 2), "attach failed");
        const auto generation = worker.statistics().generation;
        { std::lock_guard<std::mutex> lock(state->mutex); state->block = true; }
        require(worker.submit(frame(generation, 1)), "first picture rejected");
        waitFor([&] { std::lock_guard<std::mutex> lock(state->mutex); return state->entered; });
        unsigned lastPicture = 0;
        for (unsigned number = 2; number <= 100; ++number) {
            auto value = frame(generation, number, [&] { (void)worker.statistics(); ++releases; });
            lastPicture = value.picture->uniqueID();
            require(worker.submit(std::move(value)), "pending picture rejected");
        }
        require(worker.statistics().submitted == 100 && worker.statistics().coalesced == 98,
                "pending queue is not bounded to the latest picture");
        require(releases == 98, "superseded picture owners leaked or released under the queue lock");
        require(worker.statistics().swapSuccess == 0, "queue acceptance counted as presentation");
        unblock(state);
        waitFor([&] { return worker.statistics().swapSuccess == 2; });
        require(worker.statistics().lastFrame == 100 && worker.statistics().sourceFrameChangesSwapped == 2,
                "actual swapped source identities differ");
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            require(state->pictures.size() == 2 && state->pictures.back() == lastPicture,
                    "superseded pictures were rendered");
        }
        require(worker.submit(frame(generation, 100)), "duplicate repaint rejected");
        waitFor([&] { return worker.statistics().swapSuccess == 3; });
        require(worker.statistics().sourceFrameChangesSwapped == 2 && worker.statistics().frameBearingSwaps == 3,
                "duplicate repaint counted as a changed video frame");
        { std::lock_guard<std::mutex> lock(state->mutex); state->throwRender = true; }
        require(worker.submit(frame(generation, 101)), "failure picture rejected before execution");
        waitFor([&] { return worker.statistics().swapFailure == 1; });
        require(worker.statistics().lastFrame == 100 && !worker.statistics().attached,
                "failed presentation acknowledged the new source frame");
        require(!worker.submit(frame(generation, 102)), "latched failure accepted another picture");
        worker.reset();
        require(!worker.statistics().attached && worker.statistics().swapSuccess == 0,
                "reset retained a previous generation's presentation receipts");
        require(!worker.submit(frame(generation, 103)), "retired surface accepted a stale picture");
        require(worker.attach(nullptr, 2, 2), "reattach failed");
        require(!worker.submit(frame(generation, 104)), "new generation accepted an old picture");
        { std::lock_guard<std::mutex> lock(state->mutex); state->throwRender = false; }
        require(worker.submit(frame(worker.statistics().generation, 105)), "new generation frame rejected");
        waitFor([&] { return worker.statistics().swapSuccess == 1; });
        require(worker.statistics().lastFrame == 105, "reattach did not render a fresh frame");
        for (unsigned source = 1; source <= 32; ++source) {
            auto value = frame(worker.statistics().generation, 200);
            value.sources.stamps[0].source = source;
            require(worker.submit(std::move(value)), "new source rejected");
            waitFor([&] { return worker.statistics().swapSuccess == source + 1; });
        }
        require(!worker.statistics().traceOverflow, "successive views incorrectly overflowed a per-picture trace");
    }
    require(state->destroyed && state->thread != std::this_thread::get_id(), "backend destruction left its owner thread");
    require(releases == 99, "pending picture owner remained alive after completion");
}

void resetDrainsInFlightAndDiscardsPending() {
    auto state = std::make_shared<BackendState>();
    PictureRasterWorker worker(std::make_unique<TestBackend>(state));
    require(worker.attach(nullptr, 2, 2), "attach failed");
    const auto generation = worker.statistics().generation;
    { std::lock_guard<std::mutex> lock(state->mutex); state->block = true; }
    require(worker.submit(frame(generation, 1)), "in-flight frame rejected");
    waitFor([&] { std::lock_guard<std::mutex> lock(state->mutex); return state->entered; });
    std::atomic<bool> released{false};
    require(worker.submit(frame(generation, 2, [&] { (void)worker.statistics(); released = true; })), "pending frame rejected");
    std::thread release([&] { std::this_thread::sleep_for(std::chrono::milliseconds(50)); unblock(state); });
    worker.reset();
    release.join();
    require(released && !worker.statistics().attached && worker.statistics().rendered == 0,
            "reset did not isolate the retired generation");
    { std::lock_guard<std::mutex> lock(state->mutex); require(state->rendered == 1, "reset rendered a discarded pending frame"); }
}

void traceUsesActuallyPaintedFrame() {
    RealtimeFrameView view;
    view.setFrame({0, 0, 2, 2});
    std::vector<std::uint8_t> pixels(16, 255);
    view.submitFrame({pixels.data(), 2, 2, 8, PixelFormat::Rgba8888, 41, 0});
    SkPictureRecorder recorder;
    auto canvas = rendering::makeSkiaCanvas(*recorder.beginRecording(2, 2));
    FramePaintTrace outer, inner;
    outer.note(1, 0);
    require(outer.size == 0, "static image counted as a stream frame");
    {
        FramePaintScope scope(outer);
        view.paint(*canvas);
        view.paint(*canvas);
        { FramePaintScope nested(inner); view.paint(*canvas); }
        require(outer.size == 1 && inner.size == 1 && outer.stamps[0].frame == 41,
                "repaints or nested traces changed source identity");
        const auto source = outer.stamps[0].source;
        view.submitFrame({pixels.data(), 2, 2, 8, PixelFormat::Rgba8888, 42, 0});
        view.paint(*canvas);
        require(outer.size == 1 && outer.stamps[0].source == source && outer.stamps[0].frame == 42,
                "replacing a frame replaced its source identity");
        FramePaintTrace hidden;
        { FramePaintScope clipped(hidden); canvas->clipRect({4, 4, 1, 1}); view.paint(*canvas); }
        require(hidden.size == 0, "fully clipped video generated a paint stamp");
    }
    for (unsigned source = 1; source <= 17; ++source) inner.note(source, source);
    require(inner.size == 16 && inner.overflow, "trace overflow was hidden or unbounded");
}
}

int main() {
    try {
        latestPictureAndActualSwaps();
        resetDrainsInFlightAndDiscardsPending();
        traceUsesActuallyPaintedFrame();
        std::cout << "Raster ownership, latest-picture bounds, real swaps, faults, epochs, releases and paint trace passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
