#include "platform/shared/gpu_renderer.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkSurface.h"
#ifdef ONEUI_TEST_CIRCLE_MESH
#include "platform/shared/skia_canvas.h"
#include "platform/shared/circle_mesh_experiment.h"
#endif
#import <Cocoa/Cocoa.h>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <vector>

namespace {
void scene(SkCanvas* canvas, int width, int height, int frame) {
    canvas->clear(frame % 2 ? SK_ColorWHITE : SK_ColorBLACK);
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(SkColorSetARGB(180, 40, 170, 230));
    canvas->drawRoundRect(SkRect::MakeXYWH(20, 20, width-40, height-40), 12, 12, paint);
    paint.setColor(SK_ColorRED);
    canvas->drawCircle(width*.5f, height*.5f, 15, paint);
    SkPathBuilder path;
    path.moveTo(0, height*.5f);
    for (int i=1; i<1200; ++i) {
        float x=float(i)*width/1199;
        path.lineTo(x, height*(.5f+.25f*std::sin(x*.04f+frame)));
    }
    paint.setColor(SK_ColorGREEN);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(2.5f);
    canvas->drawPath(path.detach(), paint);
}
}

int main() {
    @autoreleasepool {
        [NSApplication sharedApplication];
        NSWindow* window=[[NSWindow alloc] initWithContentRect:NSMakeRect(0,0,320,200)
            styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:NO];
        window.releasedWhenClosed=NO;
        [window orderFront:nil];
        std::string failure;
        auto renderer=oneui::platform::makeMetalRenderer((__bridge void*)window.contentView, failure);
        if (!renderer) { std::cerr<<failure<<'\n'; return 1; }
        for (int frame=0; frame<12; ++frame) {
            // Alternate drawable size and scale, including reuse at the same size.
            int width=frame<4?320:frame<8?640:240, height=frame<4?200:frame<8?400:160;
            auto* gpu=renderer->beginFrame({float(width),float(height)},frame<4?1.f:2.f);
            if (!gpu) return 2;
            const auto info=SkImageInfo::MakeN32Premul(width,height);
            auto raster=SkSurfaces::Raster(info);
            scene(gpu,width,height,frame);
            scene(raster->getCanvas(),width,height,frame);
            std::vector<unsigned char> actual(width*height*4), expected(actual.size());
            if (!gpu->readPixels(info,actual.data(),width*4,0,0) ||
                !raster->readPixels(info,expected.data(),width*4,0,0)) return 3;
            double error=0;
            for (size_t i=0;i<actual.size();++i) error+=std::abs(int(actual[i])-int(expected[i]));
            error/=actual.size();
            // Coverage-AA and MSAA differ at edges; interiors and successive clears must agree.
            const size_t corner=(size_t(5)*width+5)*4;
            if (error>3.0 || actual[corner]!=expected[corner] || actual[corner+3]!=255) {
                std::cerr<<"Metal/raster mismatch frame="<<frame<<" mean_channel_error="<<error<<'\n';
                return 4;
            }
            if (!renderer->present()) return 5;
            std::cout<<"frame="<<frame<<" mean_channel_error="<<error<<'\n';
        }
#ifdef ONEUI_TEST_CIRCLE_MESH
        for (int clipCase : {0,1,2}) for (float scale : {1.f,1.25f,2.f}) for (int count : {1,300,9000}) {
            const bool fractionalClip = clipCase != 0;
            const bool interior = clipCase == 2;
            const int width=320, height=240;
            const auto info=SkImageInfo::MakeN32Premul(width,height);
            std::vector<oneui::RoundedRectFill> circles;
            for (int i=0;i<count;++i) {
                const float size=i%7==0?6.f:3.2f;
                circles.push_back({{float((i*37)%(interior?100:300))+(interior?20.f:.25f),float((i*19)%(interior?70:220))+(interior?20.f:.5f),size,size},
                    {static_cast<unsigned char>(i%255),180,90,static_cast<unsigned char>(i%3?255:120)},size/2});
            }
            std::vector<unsigned char> expected(width*height*4), actual(expected.size());
            for (int mesh=0;mesh<2;++mesh) {
                auto* gpu=renderer->beginFrame({float(width),float(height)},scale);
                if (!gpu) return 6;
                gpu->clear(SK_ColorWHITE); gpu->save(); gpu->scale(scale,scale);
                auto canvas=oneui::rendering::makeSkiaCanvas(*gpu);
                canvas->clipRect(fractionalClip ? oneui::Rect{2.25f,3.5f,260,180} : oneui::Rect{4,8,260,180});
                if (mesh) {
                    const auto result=oneui::rendering::experimental::tryCircleMesh(*canvas,circles.data(),circles.size());
                    if (result.drawn != (!fractionalClip || interior)) { std::cerr<<result.reason<<'\n'; return 7; }
                    if (!result.drawn) canvas->fillRoundedRects(circles.data(),circles.size());
                } else canvas->fillRoundedRects(circles.data(),circles.size());
                canvas.reset(); gpu->restore();
                if (!gpu->readPixels(info,(mesh?actual:expected).data(),width*4,0,0)) return 8;
                if (!renderer->present()) return 9;
            }
            double error=0; int maximum=0;
            for(size_t i=0;i<actual.size();++i) {
                const int delta=std::abs(int(actual[i])-int(expected[i]));
                error+=delta; maximum=std::max(maximum,delta);
            }
            error/=actual.size();
            std::cout<<"mesh clip="<<clipCase<<" count="<<count<<" scale="<<scale<<" mean="<<error<<" max="<<maximum<<'\n';
            if(error>0.01 || maximum>1) return 10;
        }
#endif
        renderer.reset();
        [window close];
    }
    return 0;
}
