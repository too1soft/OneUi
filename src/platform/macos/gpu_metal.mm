#include "platform/shared/gpu_renderer.h"
#include "include/core/SkSurface.h"
#include "include/core/SkColorSpace.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/mtl/GrMtlBackendContext.h"
#include "include/gpu/ganesh/mtl/GrMtlBackendSurface.h"
#include "include/gpu/ganesh/mtl/GrMtlDirectContext.h"
#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <QuartzCore/CATransaction.h>
#include <stdexcept>

namespace oneui::platform {
namespace {
class MetalRenderer final : public GpuRenderer {
public:
    ~MetalRenderer() override {
        frame_.reset();
        if (skia_) skia_->flushAndSubmit(GrSyncCpu::kYes);
        skia_.reset();
        [layer_ removeFromSuperlayer];
    }
    void initialize(NSView* view) {
        device_ = MTLCreateSystemDefaultDevice();
        if (!device_) throw std::runtime_error("Metal device unavailable");
        queue_ = [device_ newCommandQueue];
        if (!queue_) throw std::runtime_error("Metal command queue unavailable");
        GrMtlBackendContext context;
        context.fDevice = sk_ret_cfp((__bridge const void*)device_);
        context.fQueue = sk_ret_cfp((__bridge const void*)queue_);
        skia_ = GrDirectContexts::MakeMetal(context);
        if (!skia_) throw std::runtime_error("Skia Ganesh Metal initialization failed");
        view_ = view;
        view_.wantsLayer = YES;
        layer_ = [CAMetalLayer layer];
        layer_.device = device_;
        layer_.pixelFormat = MTLPixelFormatBGRA8Unorm;
        layer_.framebufferOnly = NO; // Skia may sample the drawable for blending.
        layer_.opaque = YES;
        layer_.displaySyncEnabled = YES;
        layer_.allowsNextDrawableTimeout = YES;
        layer_.autoresizingMask = kCALayerWidthSizable | kCALayerHeightSizable;
        layer_.frame = view_.bounds;
        [view_.layer addSublayer:layer_];
    }
    SkCanvas* beginFrame(Size size, float scale) override {
        if (!skia_ || skia_->abandoned() || size.width <= 0 || size.height <= 0 ||
            size.width > 32768 || size.height > 32768) return nullptr;
        const auto drawableSize = CGSizeMake(size.width, size.height);
        if (!CGRectEqualToRect(layer_.frame, view_.bounds) || layer_.contentsScale != scale ||
            !CGSizeEqualToSize(layer_.drawableSize, drawableSize)) {
            [CATransaction begin];
            [CATransaction setDisableActions:YES];
            layer_.frame = view_.bounds;
            layer_.contentsScale = scale;
            layer_.drawableSize = drawableSize;
            [CATransaction commit];
        }
        drawable_ = [layer_ nextDrawable];
        if (!drawable_) return nullptr;
        GrMtlTextureInfo info;
        info.fTexture = sk_ret_cfp((__bridge const void*)drawable_.texture);
        auto target = GrBackendRenderTargets::MakeMtl(size.width, size.height, info);
        frame_ = SkSurfaces::WrapBackendRenderTarget(skia_.get(), target, kTopLeft_GrSurfaceOrigin,
            kBGRA_8888_SkColorType, nullptr, nullptr);
        return frame_ ? frame_->getCanvas() : nullptr;
    }
    bool present() override {
        if (!frame_ || !drawable_) return false;
        // Flush this surface before the drawable is accessed outside Skia.
        skia_->flushAndSubmit(frame_.get());
        frame_.reset();
        id<MTLCommandBuffer> command = [queue_ commandBuffer];
        if (!command || skia_->abandoned()) { drawable_ = nil; return false; }
        [command presentDrawable:drawable_];
        [command commit];
        drawable_ = nil;
        return true;
    }
    RenderBackend backend() const override { return RenderBackend::Metal; }
    std::string device() const override { return device_.name.UTF8String ?: "Metal"; }
    bool vsync() const override { return layer_.displaySyncEnabled; }
    RendererMemoryInfo memoryInfo() const override {
        RendererMemoryInfo result;
        if (skia_) {
            int count = 0; size_t bytes = 0;
            skia_->getResourceCacheUsage(&count, &bytes);
            result.gpuCacheAvailable = true; result.gpuResourceCount = count; result.gpuCacheBytes = bytes;
            result.gpuPurgeableBytes = skia_->getResourceCachePurgeableBytes();
            result.gpuCacheLimitBytes = skia_->getResourceCacheLimit();
        }
        return result;
    }
private:
    NSView* view_ = nil;
    id<MTLDevice> device_ = nil;
    id<MTLCommandQueue> queue_ = nil;
    CAMetalLayer* layer_ = nil;
    id<CAMetalDrawable> drawable_ = nil;
    sk_sp<GrDirectContext> skia_;
    sk_sp<SkSurface> frame_;
};
} // namespace
std::unique_ptr<GpuRenderer> makeMetalRenderer(void* view, std::string& failure) {
    try { auto renderer = std::make_unique<MetalRenderer>(); renderer->initialize((__bridge NSView*)view); return renderer; }
    catch (const std::exception& error) { failure = error.what(); return {}; }
}
} // namespace oneui::platform
