#include "platform/shared/native_desktop_services.h"
#include "platform/shared/desktop_window.h"
#include "internal/unicode.h"
#include "oneui/localization.h"
#import <objc/runtime.h>
#include <cstring>
#include <algorithm>
#import <Cocoa/Cocoa.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <UserNotifications/UserNotifications.h>
#include <atomic>
#include <filesystem>
#include <type_traits>

@interface OneUiStatusAction : NSObject
{ @public std::function<void(int)> callback; }
- (void)invoke:(id)sender;
@end
@implementation OneUiStatusAction
- (void)invoke:(id)sender {
    const auto action = callback;
    if (action) action(static_cast<int>([sender tag]));
}
@end

namespace oneui::platform {
namespace {
NSString* native(const std::wstring& value) {
    const auto bytes = unicode::toUtf8(value);
    return [[NSString alloc] initWithBytes:bytes.data() length:bytes.size() encoding:NSUTF8StringEncoding];
}
std::wstring wide(NSString* value) {
    NSData* bytes = [value dataUsingEncoding:NSUTF8StringEncoding];
    return unicode::fromUtf8({static_cast<const char*>(bytes.bytes), bytes.length});
}
template<class Panel>
NSModalResponse modal(Window* owner, Panel* panel) {
    auto* desktop = dynamic_cast<DesktopWindow*>(owner);
    if (desktop && desktop->isClosed()) return NSModalResponseCancel;
    const auto weak = desktop ? desktop->nativeLifetime() : std::weak_ptr<std::atomic<bool>>{};
    if (desktop) desktop->enterNativeModal();
    NSWindow* parent = owner ? (__bridge NSWindow*)owner->nativeHandle() : nil;
    NSWindow* sheet;
    if constexpr (std::is_same_v<Panel, NSAlert>) sheet = panel.window;
    else sheet = panel;
    NSTimer* timer = nil;
    auto ownerAlive = [&] {
        const auto alive = weak.lock();
        return !desktop || (alive && *alive);
    };
    @try {
        if (desktop && parent && !parent.attachedSheet) {
            auto response = std::make_shared<std::atomic<NSModalResponse>>(NSModalResponseContinue);
            [panel beginSheetModalForWindow:parent completionHandler:^(NSModalResponse value) { *response = value; }];
            while (*response == NSModalResponseContinue) {
                if (!ownerAlive()) return NSModalResponseCancel;
                desktop->pumpNativeEvents(20);
            }
            // The pump may both complete the sheet and destroy its owner.
            return ownerAlive() ? response->load() : NSModalResponseCancel;
        }
        timer = desktop ? [NSTimer timerWithTimeInterval:0.016 repeats:YES block:^(NSTimer*) {
            const auto alive = weak.lock();
            if (!alive || !*alive) { [NSApp abortModal]; return; }
            desktop->dispatchWork();
        }] : nil;
        if (timer) [NSRunLoop.mainRunLoop addTimer:timer forMode:NSModalPanelRunLoopMode];
        const auto response = [panel runModal];
        return ownerAlive() ? response : NSModalResponseCancel;
    }
    @finally {
        [timer invalidate];
        // Also unwind sheet state and input suppression when a posted callback
        // throws. The native parent is retained without retaining its C++ owner.
        if (parent && sheet.sheetParent == parent)
            [parent endSheet:sheet returnCode:NSModalResponseCancel];
        [sheet orderOut:nil];
        if (desktop && ownerAlive()) desktop->leaveNativeModal();
    }
}
NSString* menuKey(const std::string& value) {
    auto key=value;for(auto& c:key)if(c>='A'&&c<='Z')c+='a'-'A';
    if(key.size()==1)return [NSString stringWithUTF8String:key.c_str()];
    const std::pair<const char*,unichar> names[]={{"enter",'\r'},{"escape",27},{"space",' '},{"tab",'\t'},{"backspace",NSBackspaceCharacter},{"delete",NSDeleteFunctionKey},{"left",NSLeftArrowFunctionKey},{"right",NSRightArrowFunctionKey},{"up",NSUpArrowFunctionKey},{"down",NSDownArrowFunctionKey},{"home",NSHomeFunctionKey},{"end",NSEndFunctionKey},{"pageup",NSPageUpFunctionKey},{"pagedown",NSPageDownFunctionKey}};
    for(auto pair:names)if(key==pair.first)return [NSString stringWithCharacters:&pair.second length:1];
    for(int f=1;f<=24;++f)if(key=="f"+std::to_string(f)){unichar c=NSF1FunctionKey+f-1;return [NSString stringWithCharacters:&c length:1];}
    return @"";
}
NSMenu* cocoaMenu(const std::vector<NativeMenuItem>& items,const std::function<void(int)>& callback,bool parentEnabled=true) {
    NSMenu* menu=[[NSMenu alloc] initWithTitle:@""];menu.autoenablesItems=NO;
    NSMutableArray* targets=[NSMutableArray array];
    for(const auto& item:items){
        if(item.separator){[menu addItem:NSMenuItem.separatorItem];continue;}
        NSMenuItem* entry=[[NSMenuItem alloc] initWithTitle:native(item.text) action:nil keyEquivalent:menuKey(item.shortcut.key)];
        entry.enabled=parentEnabled&&item.enabled;entry.state=item.checked?NSControlStateValueOn:NSControlStateValueOff;
        if(item.check==MenuCheck::Radio)entry.onStateImage=[NSImage imageWithSystemSymbolName:@"circle.fill" accessibilityDescription:@"Selected"];
        auto mods=item.shortcut.modifiers;if(mods&KeyModifierPrimary)mods=(mods&~KeyModifierPrimary)|KeyModifierMeta;
        entry.keyEquivalentModifierMask=((mods&KeyModifierShift)?NSEventModifierFlagShift:0)|((mods&KeyModifierControl)?NSEventModifierFlagControl:0)|((mods&KeyModifierAlt)?NSEventModifierFlagOption:0)|((mods&KeyModifierMeta)?NSEventModifierFlagCommand:0);
        if(!item.children.empty())entry.submenu=cocoaMenu(item.children,callback,entry.enabled);
        else {
            OneUiStatusAction* target=[OneUiStatusAction new];
            target->callback=[callback,id=item.id,enabled=bool(entry.enabled)](int){if(enabled&&callback)callback(id);};
            entry.target=target;entry.action=@selector(invoke:);[targets addObject:target];
        }
        [menu addItem:entry];
    }
    // NSMenuItem.target does not retain targets; retain them with this exact
    // menu snapshot, including while an old menu is still tracking.
    static char targetKey;objc_setAssociatedObject(menu,&targetKey,targets,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    return menu;
}
class CocoaMenuBar final : public NativeMenuBar {
public:
    CocoaMenuBar(Window* owner,std::function<void(int)> action,std::function<void()> quit):owner_(owner),action_(std::move(action)),quit_(std::move(quit)) { if(bars().empty()) previousMenu()=NSApp.mainMenu; bars().push_back(this); }
    ~CocoaMenuBar() override {
        auto& all=bars();all.erase(std::remove(all.begin(),all.end(),this),all.end());
        if(NSApp.mainMenu==menu_){NSApp.mainMenu=previousMenu();for(auto* bar:all)if(bar->menu_){bar->activate();break;}}
        if(all.empty())previousMenu()=nil;
    }
    void setMenu(const std::vector<NativeMenuItem>& items) override {
        const bool wasActive=NSApp.mainMenu==menu_;
        menu_=cocoaMenu(items,action_);
        NSMenuItem* app=[[NSMenuItem alloc] initWithTitle:NSProcessInfo.processInfo.processName action:nil keyEquivalent:@""];
        std::vector<NativeMenuItem> commands={{1,L"About"},{2,L"Hide"},{3,L"Quit"}};
        commands[1].shortcut={"h",KeyModifierPrimary};commands[2].shortcut={"q",KeyModifierPrimary};
        auto quit=quit_;
        app.submenu=cocoaMenu(commands,[quit](int id){if(id==1)[NSApp orderFrontStandardAboutPanel:nil];else if(id==2)[NSApp hide:nil];else if(quit)quit();});
        [menu_ insertItem:app atIndex:0];
        NSWindow* window=(__bridge NSWindow*)owner_->nativeHandle();
        if(wasActive || bars().size()==1 || !NSApp.mainMenu || window.isKeyWindow)activate();
    }
    void activate() override { if(menu_)NSApp.mainMenu=menu_; }
private:
    static NSMenu* __strong& previousMenu(){static NSMenu* value=nil;return value;}
    static std::vector<CocoaMenuBar*>& bars(){static std::vector<CocoaMenuBar*> value;return value;}
    Window* owner_;std::function<void(int)> action_;std::function<void()> quit_;
    NSMenu* menu_=nil;
};
class CocoaTray final : public NativeTray {
public:
    explicit CocoaTray(std::function<void(int)> action) {
        target_ = [OneUiStatusAction new];
        target_->callback = std::move(action);
        setMenu({});
    }
    ~CocoaTray() override { hide(); }
    bool show() override {
        if (![NSThread isMainThread]) return false;
        if (!item_) {
            item_ = [NSStatusBar.systemStatusBar statusItemWithLength:NSSquareStatusItemLength];
            if (!item_) return false;
            NSImage* icon = [NSImage imageWithSystemSymbolName:@"square.stack.3d.up" accessibilityDescription:@"OneUI"];
            item_.button.image = icon_ ?: icon;
            item_.button.toolTip = tooltip_;
            item_.menu = menu_;
        }
        item_.visible = YES;
        return item_.visible;
    }
    void hide() override {
        if (item_) [NSStatusBar.systemStatusBar removeStatusItem:item_];
        item_ = nil;
    }
    bool visible() const override { return item_ && item_.visible; }
    void setTooltip(const std::wstring& text) override {
        tooltip_ = native(text);
        item_.button.toolTip = tooltip_;
    }
    void setMenu(const std::vector<TrayMenuItem>& items) override {
        const std::vector<NativeMenuItem> defaults={{int(kTrayCommandShow),builtinText(BuiltinText::ShowMainWindow)},{0,{},true,true},{int(kTrayCommandExit),builtinText(BuiltinText::QuitApplication)}};
        menu_=cocoaMenu(items.empty()?defaults:items,target_->callback);
        item_.menu=menu_;
    }
    bool setIcon(const TrayIcon& icon) override {
        if(!validateTrayIcon(icon))return false;
        NSImage* image=nil;
        if(!icon.rgba.empty()){
            NSBitmapImageRep* rep=[[NSBitmapImageRep alloc] initWithBitmapDataPlanes:nil pixelsWide:icon.width pixelsHigh:icon.height bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace bitmapFormat:NSBitmapFormatAlphaNonpremultiplied bytesPerRow:icon.width*4 bitsPerPixel:32];
            if(!rep)return false;std::memcpy(rep.bitmapData,icon.rgba.data(),icon.rgba.size());
            image=[[NSImage alloc] initWithSize:NSMakeSize(18,18)];[image addRepresentation:rep];[image setTemplate:icon.templateImage];
        }
        icon_=image;item_.button.image=icon_?:[NSImage imageWithSystemSymbolName:@"square.stack.3d.up" accessibilityDescription:@"OneUI"];
        return true;
    }
    bool notify(const std::wstring& title, const std::wstring& message) override {
        // UserNotifications requires a bundle identity. Failure is observable;
        // an unbundled command-line consumer cannot silently claim delivery.
        if (!NSBundle.mainBundle.bundleIdentifier || ![NSThread isMainThread]) return false;
        auto result = std::make_shared<std::atomic<int>>(0);
        UNUserNotificationCenter* center = UNUserNotificationCenter.currentNotificationCenter;
        UNMutableNotificationContent* content = [UNMutableNotificationContent new];
        content.title = native(title);
        content.body = native(message);
        [center requestAuthorizationWithOptions:UNAuthorizationOptionAlert | UNAuthorizationOptionSound
                            completionHandler:^(BOOL granted, NSError* error) {
            if (!granted || error) { *result = -1; return; }
            auto* request = [UNNotificationRequest requestWithIdentifier:NSUUID.UUID.UUIDString content:content trigger:nil];
            [center addNotificationRequest:request withCompletionHandler:^(NSError* failure) { *result = failure ? -1 : 1; }];
        }];
        const auto deadline = DesktopWindow::nowMs() + 5000;
        while (!result->load() && DesktopWindow::nowMs() < deadline)
            [NSRunLoop.currentRunLoop runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
        return result->load() == 1; // Accepted by notification service, not proof the user saw it.
    }
private:
    NSStatusItem* item_ = nil;
    NSImage* icon_ = nil;
    NSMenu* menu_ = nil;
    OneUiStatusAction* target_ = nil;
    NSString* tooltip_ = @"";
};
} // namespace
std::unique_ptr<NativeMenuBar> makeNativeMenuBar(Window* owner,std::function<void(int)> action,std::function<void()> quit) {
    if(!owner || ![NSThread isMainThread])return {};
    [NSApplication sharedApplication];
    return std::make_unique<CocoaMenuBar>(owner,std::move(action),std::move(quit));
}
bool nativeDialogsAvailable() { return [NSThread isMainThread]; }
bool nativeTrayAvailable() { return [NSThread isMainThread]; }
unsigned int nativeDoubleClickInterval() { return std::max(1u, static_cast<unsigned int>(NSEvent.doubleClickInterval * 1000)); }
bool nativeAnimationsEnabled() { return !NSWorkspace.sharedWorkspace.accessibilityDisplayShouldReduceMotion; }
void dispatchNativeServices() {}
std::shared_ptr<NativeTray> makeNativeTray(std::function<void(int)> action) {
    if (![NSThread isMainThread]) return {};
    [NSApplication sharedApplication];
    return std::make_shared<CocoaTray>(std::move(action));
}
NativeFileResult nativeFileDialog(Window* owner, const NativeFileRequest& request) {
    if (![NSThread isMainThread] || request.mode < 0 || request.mode > 2 || (request.multiple && request.mode == 1)) return {};
    @autoreleasepool {
        NSSavePanel* panel = request.mode == 1 ? [NSSavePanel savePanel] : [NSOpenPanel openPanel];
        panel.title = native(request.title);
        panel.canCreateDirectories = YES;
        if (!request.directory.empty()) panel.directoryURL = [NSURL fileURLWithPath:native(request.directory) isDirectory:YES];
        if (!request.name.empty()) panel.nameFieldStringValue = native(request.name);
        if (request.mode != 1) {
            NSOpenPanel* open = (NSOpenPanel*)panel;
            open.canChooseFiles = request.mode == 0;
            open.canChooseDirectories = request.mode == 2;
            open.allowsMultipleSelection = request.multiple;
        }
        NSMutableArray<UTType*>* types = [NSMutableArray array];
        bool anyType = request.filters.empty();
        for (const auto& filter : request.filters) {
            for (NSString* pattern in [native(filter.pattern) componentsSeparatedByString:@";"]) {
                if ([pattern isEqualToString:@"*"] || [pattern isEqualToString:@"*.*"]) { anyType = true; break; }
                if ([pattern hasPrefix:@"*."]) {
                    UTType* type = [UTType typeWithFilenameExtension:[pattern substringFromIndex:2]];
                    if (type && ![types containsObject:type]) [types addObject:type];
                }
            }
        }
        if (!request.extension.empty()) {
            NSString* extension = native(request.extension);
            if ([extension hasPrefix:@"."]) extension = [extension substringFromIndex:1];
            UTType* type = [UTType typeWithFilenameExtension:extension];
            if (type && ![types containsObject:type]) [types insertObject:type atIndex:0];
        }
        if (request.mode != 2 && types.count) panel.allowedContentTypes = types;
        panel.allowsOtherFileTypes = anyType;
        const auto response = modal(owner, panel);
        if (response != NSModalResponseOK) return {0, {}};
        NativeFileResult result{1, {}};
        if (request.mode == 1) result.paths.push_back(wide(panel.URL.path));
        else for (NSURL* url in [(NSOpenPanel*)panel URLs]) if (url.isFileURL) result.paths.push_back(wide(url.path));
        if (result.paths.empty()) result.status = -1;
        return result;
    }
}
bool nativeConfirm(Window* owner, const std::wstring& title, const std::wstring& message) {
    if (![NSThread isMainThread]) return false;
    @autoreleasepool {
        NSAlert* alert = [NSAlert new];
        alert.messageText = native(title);
        alert.informativeText = native(message);
        [alert addButtonWithTitle:@"Cancel"];
        [alert addButtonWithTitle:@"Confirm"];
        return modal(owner, alert) == NSAlertSecondButtonReturn;
    }
}
bool nativePrompt(Window* owner, const std::wstring& title, const std::wstring& message, const std::wstring& initial,
                  const std::wstring& placeholder, bool password, std::wstring& result) {
    if (![NSThread isMainThread]) return false;
    @autoreleasepool {
        NSAlert* alert = [NSAlert new];
        alert.messageText = native(title);
        alert.informativeText = native(message);
        [alert addButtonWithTitle:@"OK"];
        [alert addButtonWithTitle:@"Cancel"];
        NSTextField* field = password ? [[NSSecureTextField alloc] initWithFrame:NSMakeRect(0, 0, 320, 26)] : [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 320, 26)];
        field.stringValue = native(initial);
        field.placeholderString = native(placeholder);
        alert.accessoryView = field;
        [alert.window makeFirstResponder:field];
        const bool accepted = modal(owner, alert) == NSAlertFirstButtonReturn;
        if (accepted) result = wide(field.stringValue);
        field.stringValue = @"";
        return accepted;
    }
}
} // namespace oneui::platform
