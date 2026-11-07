#pragma once
#import "oneui/mobile/application_ios.h"
#include "oneui/mobile/application.h"

@interface OneUiApplicationView (Internal)
- (oneui::mobile::ApplicationHost *)oneuiHost;
- (void)oneuiChanged;
@end
@interface OneUiImeBridge : NSObject
- (instancetype)initWithView:(OneUiApplicationView *)view;
- (void)sync:(BOOL)showKeyboard;
- (void)suspend;
@end
