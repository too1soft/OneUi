#import <Cocoa/Cocoa.h>
#include "oneui/application_events.h"
#include "application_events_macos.h"
#include "../../capi/oneui_c_api_error.h"
#include <cstdint>
#include <deque>
#include <string>
#include <utility>

namespace {
OneUiOpenUriCallback callback = nullptr;
void* callbackData = nullptr;
std::deque<std::string> pending;
std::uint64_t subscription = 0;
void deliver(std::string uri) {
    if (uri.empty() || uri.size() > 8192) return;
    if (callback) callback(uri.data(), uri.size(), callbackData);
    else if (pending.size() < 32) pending.push_back(std::move(uri));
}
}

@interface OneUiApplicationUriEvents : NSObject
- (void)handleUrl:(NSAppleEventDescriptor*)event reply:(NSAppleEventDescriptor*)reply;
@end
@implementation OneUiApplicationUriEvents
- (void)handleUrl:(NSAppleEventDescriptor*)event reply:(NSAppleEventDescriptor*)reply {
    (void)reply;
    // kInternetEventClass/kAEGetURL use GURL; keyDirectObject uses ----.
    NSString* uri = [[event paramDescriptorForKeyword:0x2d2d2d2d] stringValue];
    NSData* bytes = [uri dataUsingEncoding:NSUTF8StringEncoding];
    if (bytes.length > 0 && bytes.length <= 8192) {
        try {
            deliver(std::string(static_cast<const char*>(bytes.bytes), bytes.length));
        } catch (...) {
            oneui::capi::recordCurrentException("application.open_uri");
        }
    }
}
@end

namespace oneui::macos {
void installOpenUriEvents() {
    static OneUiApplicationUriEvents* receiver = nil;
    if (receiver || ![NSThread isMainThread]) return;
    receiver = [OneUiApplicationUriEvents new];
    [[NSAppleEventManager sharedAppleEventManager] setEventHandler:receiver
        andSelector:@selector(handleUrl:reply:) forEventClass:0x4755524c andEventID:0x4755524c];
}
}

extern "C" int oneui_macos_set_open_uri_callback(OneUiOpenUriCallback next, void* data) {
    if (![NSThread isMainThread]) return 0;
    try {
    oneui::macos::installOpenUriEvents();
    callback = next;
    callbackData = next ? data : nullptr;
    const auto current = ++subscription;
    if (next) {
        auto waiting = std::move(pending);
        pending.clear();
        for (const auto& uri : waiting) {
            // A callback can unsubscribe/re-subscribe. Do not hand the old
            // subscription's remaining cold-start events to its replacement.
            if (!callback || current != subscription) break;
            callback(uri.data(), uri.size(), callbackData);
        }
    }
    return 1;
    } catch (...) {
        callback = nullptr;
        callbackData = nullptr;
        oneui::capi::recordCurrentException(__func__);
        return 0;
    }
}
