#import "platform/ios/application_view_internal.h"
#include "platform/shared/skia_canvas.h"
#include "include/core/SkSurface.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkImageInfo.h"
#include <cmath>
#include <vector>

@interface OneUiFrameTarget : NSObject
@property(nonatomic, weak) OneUiApplicationView *view;
- (void)pulse:(CADisplayLink *)link;
@end
@interface OneUiApplicationView ()
- (void)pulse:(CADisplayLink *)link;
@end
@implementation OneUiFrameTarget
- (void)pulse:(CADisplayLink *)link { [self.view pulse:link]; }
@end
@implementation OneUiApplicationView {
    std::unique_ptr<oneui::mobile::ApplicationHost> _host;
    std::vector<uint8_t> _pixels;
    sk_sp<SkSurface> _surface;
    CADisplayLink *_clock;
    OneUiImeBridge *_ime;
    CGFloat _keyboard;
    int _width, _height;
    BOOL _active;
    NSMutableSet<NSNumber *> *_inFlight;
}
@synthesize services = _services;
- (instancetype)initWithFrame:(CGRect)frame {
    if ((self = [super initWithFrame:frame])) {
        _host = std::make_unique<oneui::mobile::ApplicationHost>([](auto &services) {
            return std::unique_ptr<oneui::mobile::Application>(oneui_create_mobile_application(&services));
        });
        self.multipleTouchEnabled = YES; self.opaque = YES; self.backgroundColor = UIColor.systemBackgroundColor;
        _inFlight = [NSMutableSet new];
        _ime = [[OneUiImeBridge alloc] initWithView:self];
        NSNotificationCenter *center = NSNotificationCenter.defaultCenter;
        [center addObserver:self selector:@selector(keyboard:) name:UIKeyboardWillChangeFrameNotification object:nil];
        [center addObserver:self selector:@selector(activation:) name:UIApplicationDidBecomeActiveNotification object:nil];
        [center addObserver:self selector:@selector(activation:) name:UIApplicationWillResignActiveNotification object:nil];
        [center addObserver:self selector:@selector(typography:) name:UIContentSizeCategoryDidChangeNotification object:nil];
        [self typography:nil];
    }
    return self;
}
- (oneui::mobile::ApplicationHost *)oneuiHost { return _host.get(); }
- (void)setServices:(id<OneUiApplicationServices>)services { _services = services; [self drain]; }
- (void)drain {
    if (!_host || !self.services) return;
    oneui::mobile::ApplicationRequest request;
    while (_host->services().take(request)) {
        const auto id = request.id;
        if (_inFlight.count >= 16) { _host->reply(id, -2, {}); continue; }
        [_inFlight addObject:@(id)];
        NSString *action = [[NSString alloc] initWithBytes:request.action.data() length:request.action.size() encoding:NSUTF8StringEncoding];
        NSData *payload = [NSData dataWithBytes:request.payload.data() length:request.payload.size()];
        __weak OneUiApplicationView *weakSelf = self;
        if ([action isEqualToString:@"oneui.open-url"]) {
            NSString *raw = [[NSString alloc] initWithData:payload encoding:NSUTF8StringEncoding];
            NSURLComponents *url = raw ? [NSURLComponents componentsWithString:raw] : nil;
            if (payload.length > 4096 || ![url.scheme isEqualToString:@"https"] || !url.host.length || url.user || url.password || !url.URL) {
                [self reply:id code:-4 payload:[NSData data]];
            } else {
                [UIApplication.sharedApplication openURL:url.URL options:@{} completionHandler:^(BOOL success) {
                    [weakSelf reply:id code:success ? 0 : -4 payload:[NSData data]];
                }];
            }
            continue;
        }
        [self.services oneuiRequest:action payload:payload reply:^(int32_t code, NSData *result) {
            NSData *owned = [result copy];
            dispatch_async(dispatch_get_main_queue(), ^{ [weakSelf reply:id code:code payload:owned]; });
        }];
        std::fill(request.payload.begin(), request.payload.end(), '\0');
    }
}
- (void)reply:(uint64_t)request code:(int32_t)code payload:(NSData *)payload {
    if (!_host || payload.length > 1024 * 1024) return;
    if (request && ![_inFlight containsObject:@(request)]) return;
    [_inFlight removeObject:@(request)];
    _host->reply(request, code, std::string(static_cast<const char *>(payload.bytes), payload.length));
    [self oneuiChanged]; [_ime sync:NO]; [self drain];
}
- (void)deliverEvent:(NSData *)payload { [self reply:0 code:0 payload:payload]; }
- (void)oneuiChanged { if (_host) _host->runtime().invalidate(); }
- (void)typography:(NSNotification *)notification {
    UIFont *font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _host->application().typography(font.pointSize / 17.0, font.pointSize); [self oneuiChanged];
}
- (void)traitCollectionDidChange:(UITraitCollection *)previous {
    [super traitCollectionDidChange:previous]; [self observeAppearance];
}
- (void)observeAppearance {
    if (!_host) return;
    _host->application().appearance().observe(self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark ? 2 : 1, self.window != nil);
    _host->syncAppearance(); [self oneuiChanged];
}
- (void)didMoveToWindow { [super didMoveToWindow]; [self activation:nil]; [self observeAppearance]; }
- (void)activation:(NSNotification *)notification {
    const BOOL active = self.window != nil && UIApplication.sharedApplication.applicationState == UIApplicationStateActive &&
        ![notification.name isEqualToString:UIApplicationWillResignActiveNotification];
    _active = active;
    if (_host) _host->lifecycle(active ? ONEUI_MOBILE_ACTIVE : ONEUI_MOBILE_INACTIVE);
    if (!active) { [_ime suspend]; [_clock invalidate]; _clock = nil; }
    else if (!_clock) {
        OneUiFrameTarget *target = [OneUiFrameTarget new]; target.view = self;
        _clock = [CADisplayLink displayLinkWithTarget:target selector:@selector(pulse:)];
        [_clock addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
        [self setNeedsLayout];
    }
}
- (void)keyboard:(NSNotification *)notification {
    CGRect frame = [self convertRect:[notification.userInfo[UIKeyboardFrameEndUserInfoKey] CGRectValue] fromView:nil];
    CGRect intersection = CGRectIntersection(self.bounds, frame);
    _keyboard = CGRectIsNull(intersection) || CGRectGetMaxY(intersection) < CGRectGetMaxY(self.bounds) - 1 ? 0 : intersection.size.height;
    [self setNeedsLayout];
}
- (void)layoutSubviews {
    [super layoutSubviews]; if (!_host) return;
    CGFloat scale = self.window.screen.scale ?: UIScreen.mainScreen.scale;
    const int width = static_cast<int>(std::ceil(self.bounds.size.width * scale));
    const int height = static_cast<int>(std::ceil(self.bounds.size.height * scale));
    if (width < 1 || height < 1 || width > 8192 || height > 8192 || static_cast<int64_t>(width) * height > 32000000) return;
    if (width != _width || height != _height) {
        _host->surface(false); _surface.reset(); _pixels.resize(static_cast<size_t>(width) * height * 4);
        _surface = SkSurfaces::WrapPixels(SkImageInfo::Make(width, height, kBGRA_8888_SkColorType, kPremul_SkAlphaType), _pixels.data(), width * 4);
        _width = width; _height = height;
    }
    UIEdgeInsets safe = self.safeAreaInsets;
    OneUiMobileViewportV1 viewport{static_cast<float>(width), static_cast<float>(height), static_cast<float>(scale),
        static_cast<float>(safe.left * scale), static_cast<float>(safe.top * scale), static_cast<float>(safe.right * scale),
        static_cast<float>(safe.bottom * scale), static_cast<float>(_keyboard * scale)};
    _host->viewport(viewport); _host->surface(_surface != nullptr); [self oneuiChanged];
}
- (void)pulse:(CADisplayLink *)link {
    if (!_active || !_host || !_surface) return;
    std::unique_ptr<oneui::Canvas> canvas;
    auto result = _host->paint([&]() {
        auto *skia = _surface->getCanvas(); skia->save(); skia->clear(SK_ColorWHITE);
        skia->scale(self.window.screen.scale, self.window.screen.scale);
        canvas = oneui::rendering::makeSkiaCanvas(*skia); return canvas.get();
    }, static_cast<uint64_t>(link.timestamp * 1000000000));
    if (canvas) _surface->getCanvas()->restore();
    if (result == ONEUI_MOBILE_OK) [self setNeedsDisplay];
    [_ime sync:NO]; [self drain];
}
- (void)drawRect:(CGRect)rect {
    if (!_surface || _pixels.empty()) return;
    CGDataProviderRef provider = CGDataProviderCreateWithData(nullptr, _pixels.data(), _pixels.size(), nullptr);
    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    CGImageRef image = CGImageCreate(_width, _height, 8, 32, _width * 4, space,
        kCGBitmapByteOrder32Little | kCGImageAlphaPremultipliedFirst, provider, nullptr, false, kCGRenderingIntentDefault);
    CGContextRef context = UIGraphicsGetCurrentContext();
    CGContextSaveGState(context); CGContextTranslateCTM(context, 0, self.bounds.size.height); CGContextScaleCTM(context, 1, -1);
    CGContextDrawImage(context, self.bounds, image); CGContextRestoreGState(context);
    CGImageRelease(image); CGColorSpaceRelease(space); CGDataProviderRelease(provider);
}
- (void)touches:(NSSet<UITouch *> *)touches phase:(int)phase {
    if (!_host) return;
    const CGFloat scale = self.window.screen.scale;
    for (UITouch *touch in touches) {
        CGPoint point = [touch locationInView:self];
        OneUiMobilePointerV1 event{static_cast<int32_t>(reinterpret_cast<uintptr_t>((__bridge void *)touch) & 0x7fffffff), phase,
            touch.type == UITouchTypePencil ? ONEUI_MOBILE_PEN : ONEUI_MOBILE_TOUCH, static_cast<float>(point.x * scale),
            static_cast<float>(point.y * scale), 1, static_cast<uint64_t>(touch.timestamp * 1000000000), 0};
        _host->pointer(event);
    }
    [_ime sync:phase == ONEUI_MOBILE_UP]; [self drain]; [self oneuiChanged];
}
- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event { [self touches:touches phase:ONEUI_MOBILE_DOWN]; }
- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event { [self touches:touches phase:ONEUI_MOBILE_MOVE]; }
- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event { [self touches:touches phase:ONEUI_MOBILE_UP]; }
- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event { if (_host) _host->cancelPointers(); [_ime sync:NO]; }
- (void)dealloc { [NSNotificationCenter.defaultCenter removeObserver:self]; [_clock invalidate]; [_ime suspend]; _host.reset(); }
@end
