#pragma once
#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN
typedef void (^OneUiApplicationReply)(int32_t code, NSData *payload);
@protocol OneUiApplicationServices <NSObject>
- (void)oneuiRequest:(NSString *)action payload:(NSData *)payload reply:(OneUiApplicationReply)reply;
@end
// UIKit surface/lifecycle/IME for the shared C++ Application factory. Service
// implementations supply business commands; no platform plumbing is required.
@interface OneUiApplicationView : UIView
@property(nonatomic, weak, nullable) id<OneUiApplicationServices> services;
- (void)deliverEvent:(NSData *)payload;
@end
NS_ASSUME_NONNULL_END
