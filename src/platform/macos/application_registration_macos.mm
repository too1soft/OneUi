#include "internal/unicode.h"
#include "platform/shared/notification_backend.h"
#import <Cocoa/Cocoa.h>
#import <CoreServices/CoreServices.h>

namespace oneui::platform {
NotificationResult registerDesktopApplication(const ApplicationOptions &options,
                                              const std::wstring &executable) {
    const auto path = unicode::toUtf8(executable);
    auto *url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:path.c_str()]];
    // executable is <bundle>.app/Contents/MacOS/<name>.
    auto *root = url.URLByDeletingLastPathComponent.URLByDeletingLastPathComponent
                     .URLByDeletingLastPathComponent;
    auto *bundle = [NSBundle bundleWithURL:root];
    if (![root.pathExtension isEqual:@"app"] ||
        ![bundle.bundleIdentifier isEqual:[NSString stringWithUTF8String:options.id.c_str()]] ||
        ![bundle.executableURL.path isEqual:url.path])
        return {NotificationStatus::Failed, {}, "Executable must belong to a matching .app bundle"};
    const auto status = LSRegisterURL((__bridge CFURLRef)root, true);
    return {status == noErr ? NotificationStatus::Accepted : NotificationStatus::Failed,
            {},
            status == noErr ? "Application bundle registered with Launch Services"
                            : "Launch Services registration failed"};
}
NotificationResult unregisterDesktopApplication(const ApplicationOptions &) {
    // There is no public Launch Services unregister API. The package uninstaller
    // removes its own .app; Launch Services then reconciles that removed bundle.
    return {NotificationStatus::Unsupported,
            {},
            "Remove the installed .app bundle through its package uninstaller"};
}
} // namespace oneui::platform
