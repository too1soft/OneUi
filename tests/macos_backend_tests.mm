#import <Cocoa/Cocoa.h>
#include "oneui/controls/text_field.h"
#include "oneui/controls/button.h"
#include "oneui/controls/slider.h"
#include "oneui/controls/progress_bar.h"
#include "oneui/platform/window.h"
#include "oneui/oneui_c_api.h"
#include "platform/shared/desktop_window.h"
#include "platform/shared/native_desktop_services.h"
#include <iostream>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <thread>
#include <vector>

// Deliberately calls the real NSView input protocol on the process main
// thread. These tests require AppKit; portable offset tests run on every OS.
namespace {
int failures = 0;
void check(bool value, const char* name) {
    if (!value) { std::cerr << name << '\n'; ++failures; }
}
NSEvent* modifierEvent(NSWindow* window, unsigned short key, NSEventModifierFlags flags) {
    return [NSEvent keyEventWithType:NSEventTypeFlagsChanged location:NSZeroPoint
                     modifierFlags:flags timestamp:0 windowNumber:window.windowNumber
                           context:nil characters:@"" charactersIgnoringModifiers:@""
                         isARepeat:NO keyCode:key];
}
void testApplicationMenu() {
    auto window=oneui::Window::create(L"Menu regression",320,200);
    window->initialize();window->show();
    NSMenu* previous=NSApp.mainMenu;
    oneui::NativeMenuItem checkItem{41,L"Checked"};checkItem.check=oneui::MenuCheck::Check;checkItem.checked=true;
    checkItem.shortcut={"s",oneui::KeyModifierPrimary};
    oneui::NativeMenuItem radio{42,L"Radio"};radio.check=oneui::MenuCheck::Radio;radio.checked=true;radio.radioGroup=1;
    oneui::NativeMenuItem disabled{43,L"Disabled",false};
    oneui::NativeMenuItem nested{0,L"Nested"};nested.children={{44,L"Child"}};
    oneui::NativeMenuItem file{0,L"File"};file.children={checkItem,radio,disabled,nested};
    int action=0;window->setOnMenuAction([&](int id){action=id;});
    check(window->setMenuBar({file}),"Cocoa accepts shared menu tree");
    NSMenu* root=NSApp.mainMenu;
    check(root.numberOfItems==2,"Cocoa inserts one system application menu");
    if(root.numberOfItems<2)return;
    NSMenu* menu=[root itemAtIndex:1].submenu;
    check(menu.numberOfItems==4 && [menu itemAtIndex:0].state==NSControlStateValueOn,"Cocoa preserves controlled check state");
    check([[menu itemAtIndex:0].keyEquivalent isEqualToString:@"s"] && [menu itemAtIndex:0].keyEquivalentModifierMask==NSEventModifierFlagCommand,"Primary maps to native Command shortcut");
    [menu performActionForItemAtIndex:0];check(action==41 && [menu itemAtIndex:0].state==NSControlStateValueOn,"Native action reports ID without toggling state");
    [menu performActionForItemAtIndex:2];check(action==41,"Disabled native action blocked");
    [[menu itemAtIndex:3].submenu performActionForItemAtIndex:0];check(action==44,"Nested native action reports leaf ID");
    auto* desktop=dynamic_cast<oneui::platform::DesktopWindow*>(window.get());desktop->enterNativeModal();
    [menu performActionForItemAtIndex:0];check(action==44,"Native modal blocks application menu action");desktop->leaveNativeModal();
    window->setOnMenuAction([&](int id){action=id;file.children[0].checked=false;check(window->setMenuBar({file}),"Native menu can be replaced inside its action");});
    [menu performActionForItemAtIndex:0];
    check([NSApp.mainMenu itemAtIndex:1].submenu.itemArray[0].state==NSControlStateValueOff,"Native replacement updates check state");
    check(window->setTrayIcon({2,1,{255,0,0,255,0,255,0,128},true}),"Cocoa accepts template RGBA icon");
    check(window->setTrayMenu({file}) && window->setTrayEnabled(true),"Cocoa tray uses shared recursive menu");
    check(window->setTrayIcon({}) && window->setTrayEnabled(false),"Cocoa tray icon can be reset and removed");
    {
        auto second=oneui::Window::create(L"Second menu window",320,200);second->initialize();
        auto secondMenu=file;secondMenu.text=L"Second";
        check(second->setMenuBar({secondMenu}),"Second window owns a separate menu");
        dynamic_cast<oneui::platform::DesktopWindow*>(second.get())->focus(true);
        check([[NSApp.mainMenu itemAtIndex:1].title isEqualToString:@"Second"],"Activating second window selects its menu");
        desktop->focus(true);
        check([[NSApp.mainMenu itemAtIndex:1].title isEqualToString:@"File"],"Reactivating first window restores its menu");
    }
    check(window->setMenuBar({}) && NSApp.mainMenu==previous,"Removing native bar restores previous application menu");
    window->setMenuBar({file});menu=[NSApp.mainMenu itemAtIndex:1].submenu;
    window->setOnMenuAction([&](int){window.reset();});
    [menu performActionForItemAtIndex:0];check(!window,"Native menu callback can destroy its owner");
    [menu performActionForItemAtIndex:0];check(NSApp.mainMenu==previous,"Retained old native action is safe after owner destruction");
}
void testNativeAccessibility() {
    auto window = oneui::Window::create(L"Accessibility regression", 320, 200);
    window->initialize(); window->show(); window->setContentScale(1.5f);
    auto* desktop = dynamic_cast<oneui::platform::DesktopWindow*>(window.get());
    NSWindow* native = (__bridge NSWindow*)window->nativeHandle();
    NSView* view = native.contentView;
    auto root = std::make_shared<oneui::View>();
    auto button = std::make_shared<oneui::Button>(L"Action");
    auto field = std::make_shared<oneui::TextField>(L"Password");
    field->setPasswordMode(true); field->setText(L"secret"); field->setAccessibleValue(L"secret");
    root->add(button); root->add(field); window->setContent(root); desktop->render();
    button->setFrame({10, 10, 100, 30}); field->setFrame({10, 50, 100, 30}); desktop->dispatchWork();
    NSArray* children = view.accessibilityChildren;
    check(children.count == 2, "Native view exposes semantic controls");
    if (children.count != 2) { window->close(); return; }
    id action = children[0], secret = children[1];
    int clicks = 0; button->setOnClick([&] { ++clicks; });
    check([[action accessibilityRole] isEqualToString:NSAccessibilityButtonRole], "Native button role is correct");
    check([action accessibilityPerformPress] && clicks == 1, "AppKit press reaches OneUI callback");
    check([view accessibilityFocusedUIElement] == action, "Native focus query reaches the focused leaf");
    check([[secret accessibilitySubrole] isEqualToString:NSAccessibilitySecureTextFieldSubrole] && ![secret accessibilityValue],
          "Password proxy never exposes plaintext");
    [secret setAccessibilityValue:@"updated"];
    check(field->text() == L"updated", "AppKit text replacement reaches the actual field");
    field->setReadOnly(true); [secret setAccessibilityValue:@"forbidden"];
    check(field->text() == L"updated", "Read-only field rejects AppKit writes");
    const NSRect bounds = [action accessibilityFrame];
    check(std::abs(bounds.size.width - 150) < 0.1 && std::abs(bounds.size.height - 45) < 0.1,
          "Accessibility uses Cocoa screen points and content zoom, not backing pixels");
    desktop->enterNativeModal(); desktop->dispatchWork();
    check(view.accessibilityChildren.count == 0 && ![action accessibilityPerformPress], "Cocoa modal isolates retained background proxies");
    desktop->leaveNativeModal(); desktop->dispatchWork();
    button->setDisabled(true);
    check(![action accessibilityPerformPress] && clicks == 1, "Disabled proxy rejects native actions");
    root->reconcileChildren({field}); desktop->dispatchWork();
    check(![action isAccessibilityElement] && ![action accessibilityPerformPress], "Detached native proxies become defunct");
    root->reconcileChildren({button}); button->setDisabled(false);
    button->setOnClick([&] { window.reset(); }); desktop->dispatchWork();
    id closingAction = view.accessibilityChildren.firstObject;
    check([closingAction accessibilityPerformPress] && !window, "AppKit action can destroy its own window safely");
    check(![secret isAccessibilityElement] && ![secret isAccessibilityEnabled], "Retained proxy survives owner destruction safely");
}
void testNativeAccessibleTextAndValue() {
    auto window = oneui::Window::create(L"Text and value accessibility", 420, 320);
    window->initialize(); window->show(); window->setContentScale(1.5f);
    auto* desktop = dynamic_cast<oneui::platform::DesktopWindow*>(window.get());
    NSView* view = ((__bridge NSWindow*)window->nativeHandle()).contentView;
    auto root = std::make_shared<oneui::View>();
    auto field = std::make_shared<oneui::TextField>(); field->setMultiline(true); field->setText(L"A\U0001f600B\nSecond");
    auto slider = std::make_shared<oneui::Slider>(); slider->setRange(-5, 10); slider->setStep(0.5); slider->setValue(2);
    auto progress = std::make_shared<oneui::ProgressBar>(); progress->setValue(0.25);
    root->add(field); root->add(slider); root->add(progress); window->setContent(root); desktop->render();
    field->setFrame({10, 10, 220, 90}); slider->setFrame({10, 110, 200, 30}); progress->setFrame({10, 150, 200, 10});
    desktop->dispatchWork(); NSArray* children = view.accessibilityChildren;
    check(children.count == 3, "AppKit exposes editor, slider and progress");
    if (children.count != 3) { window->close(); return; }
    id edit = children[0], number = children[1], meter = children[2];
    check([edit accessibilityNumberOfCharacters] == 11, "AppKit character count is UTF-16 units");
    check([[edit accessibilityStringForRange:NSMakeRange(2, 1)] isEqualToString:@"\U0001f600"], "Range queries never return a broken surrogate pair");
    check(NSEqualRanges([edit accessibilityRangeForIndex:2], NSMakeRange(1, 2)), "Emoji index resolves to its full UTF-16 range");
    [edit setAccessibilitySelectedTextRange:NSMakeRange(1, 2)];
    check(field->selectedText() == L"\U0001f600" && NSEqualRanges([edit accessibilitySelectedTextRange], NSMakeRange(1, 2)), "Native selection round trips UTF-16 correctly");
    const NSRect glyph = [edit accessibilityFrameForRange:NSMakeRange(1, 2)];
    check(glyph.size.width > 0 && glyph.size.height > 0, "Native text range uses shaped glyph geometry");
    NSRange hit = [edit accessibilityRangeForPosition:NSMakePoint(NSMinX(glyph) + glyph.size.width / 4, NSMidY(glyph))];
    check(hit.location == 1 || hit.location == 3, "Cocoa screen point maps back through the flipped view");
    check([edit accessibilityLineForIndex:5] == 1 && [edit accessibilityRangeForLine:1].location == 5, "Native lines follow the text layout");
    [edit setAccessibilitySelectedText:@"\u4e2d"];
    check(field->text() == L"A\u4e2dB\nSecond" && field->undo() && field->text() == L"A\U0001f600B\nSecond", "Native replacement preserves undo");
    field->setReadOnly(true); [edit setAccessibilitySelectedText:@"blocked"];
    check(field->text() == L"A\U0001f600B\nSecond", "Read-only native text rejects replacement");
    field->setPasswordMode(true);
    check(![edit accessibilityStringForRange:NSMakeRange(0, 1)] && [edit accessibilityNumberOfCharacters] == 0 &&
          [edit accessibilitySelectedTextRange].location == NSNotFound, "Cached text proxies cannot read passwords");
    check([[number accessibilityMinValue] doubleValue] == -5 && [[number accessibilityMaxValue] doubleValue] == 10, "Numeric bounds match slider configuration");
    [number setAccessibilityValue:@4.5];
    check(slider->value() == 4.5 && [number accessibilityPerformIncrement] && slider->value() == 5, "Native numeric editing respects step");
    slider->setDisabled(true); [number setAccessibilityValue:@7];
    check(slider->value() == 5 && ![number accessibilityPerformDecrement], "Disabled native numeric mutation fails");
    [meter setAccessibilityValue:@0.75];
    check([[meter accessibilityValue] doubleValue] == 0.25 && progress->value() == 0.25, "Progress is read-only");
    progress->setIndeterminate(true); check(![meter accessibilityValue], "Indeterminate progress has no fabricated percentage");
    slider->setDisabled(false); slider->setOnChanged([&](double) { window.reset(); });
    [number setAccessibilityValue:@6];
    check(!window && ![number isAccessibilityElement], "Native numeric callback can destroy its owner safely");
}
void testNativeInput() {
    oneui::WindowOptions options;
    options.visible = false;
    auto window = oneui::Window::create(options);
    window->initialize();
    NSWindow* native = (__bridge NSWindow*)window->nativeHandle();
    NSView<NSTextInputClient>* view = (NSView<NSTextInputClient>*)native.contentView;
    std::vector<oneui::KeyEvent> keys;
    window->setRawKeyHandler([&](const oneui::KeyEvent& e) { keys.push_back(e); return false; });
    [view flagsChanged:modifierEvent(native, 56, NSEventModifierFlagShift)];
    [view flagsChanged:modifierEvent(native, 56, 0)];
    [view flagsChanged:modifierEvent(native, 55, NSEventModifierFlagCommand)];
    [view flagsChanged:modifierEvent(native, 55, 0)];
    [view flagsChanged:modifierEvent(native, 63, NSEventModifierFlagFunction)];
    check(keys.size() == 4, "Fn must not become a fake Command event");
    if (keys.size() == 4) {
        check(keys[0].pressed && keys[0].shift && !keys[0].repeat && !keys[1].pressed,
              "Shift flagsChanged must route without reading key-only event properties");
        check(keys[2].pressed && keys[2].win && !keys[2].control && !keys[3].pressed,
              "Command flags retain their physical meaning");
    }
    window->setRawKeyHandler({});

    auto field = std::make_shared<oneui::TextField>();
    window->setContent(field);
    window->requestFocus(field.get());
    int commits = 0;
    field->setOnChanged([&](const std::wstring&) { ++commits; });
    [view setMarkedText:@"\u4e2d\u6587" selectedRange:NSMakeRange(2, 0)
      replacementRange:NSMakeRange(NSNotFound, 0)];
    check(field->text().empty() && commits == 0, "Preedit is provisional");
    [view unmarkText];
    check(field->text() == L"\u4e2d\u6587" && commits == 1 && !view.hasMarkedText,
          "AppKit unmark accepts the preedit exactly once");
    [view unmarkText];
    check(commits == 1, "Unmark without preedit is a no-op");
    check(field->undo() && field->text().empty(), "Accepted preedit has one undo entry");

    field->setText(L"A\U0001f600B");
    [view insertText:@"X" replacementRange:NSMakeRange(2, 1)];
    check(field->text() == L"AXB", "Replacement inside a surrogate pair preserves the surrounding document");
    field->setText(L"A\U0001f600B");
    NSRange actual = NSMakeRange(9, 9);
    NSAttributedString* snippet = [view attributedSubstringForProposedRange:NSMakeRange(2, 1) actualRange:&actual];
    check([snippet.string isEqualToString:@"\U0001f600"] && NSEqualRanges(actual, NSMakeRange(1, 2)),
          "Surrounding-text queries never return a broken surrogate pair");
    snippet = [view attributedSubstringForProposedRange:NSMakeRange(NSNotFound, 10) actualRange:&actual];
    check(snippet == nil && actual.location == NSNotFound, "Unavailable ranges report NSNotFound");
    [view insertText:@"Z" replacementRange:NSMakeRange(3, std::numeric_limits<NSUInteger>::max())];
    check(field->text() == L"A\U0001f600Z", "Overflowing replacement lengths clamp safely");

    [view setMarkedText:@"old" selectedRange:NSMakeRange(3, 0)
      replacementRange:NSMakeRange(NSNotFound, 0)];
    auto next = std::make_shared<oneui::TextField>();
    window->setContent(next);
    window->requestFocus(next.get());
    [view unmarkText];
    check(next->text().empty() && !view.hasMarkedText, "Stale IME unmark cannot commit into another editor");

    [view setMarkedText:@"cancel" selectedRange:NSMakeRange(6, 0)
      replacementRange:NSMakeRange(NSNotFound, 0)];
    [native.delegate windowDidResignKey:[NSNotification notificationWithName:NSWindowDidResignKeyNotification object:native]];
    check(next->text().empty() && !view.hasMarkedText, "Native focus loss cancels without committing");

    bool rejected = false;
    std::thread worker([&] {
        try { window->setTitle(L"wrong thread"); }
        catch (const std::logic_error&) { rejected = true; }
    });
    worker.join();
    check(rejected, "Cocoa mutations reject non-UI threads before entering AppKit");
    window->close();
}
void testNativeServices() {
    auto window = oneui::Window::create(L"Cocoa tray regression", 320, 200);
    window->initialize();
    check(window->setTrayEnabled(true) && window->trayIconVisible(), "NSStatusItem registration failed");
    check(window->setTrayMenu({{42, L"\u4e2d\u6587 \U0001f600"}, {43, L"Disabled", false}}), "Native tray menu rejected");
    window->setCloseToTray(true); window->close();
    check(window->nativeHandle() && window->trayIconVisible(), "Close-to-tray destroyed the owner");
    window->setCloseToTray(false); window->setTrayEnabled(false); window->close();
    check(!window->trayIconVisible(), "Closing left an orphan status item");

    OneUiWindowOptions options{}; options.title = L"Cocoa modal regression"; options.width = 320; options.height = 200;
    auto* handle = oneui_window_create(&options);
    check(handle && oneui_window_initialize_checked(handle), "Modal owner initialization failed");
    if (!handle) return;
    oneui_window_show(handle);
    NSWindow* owner = (__bridge NSWindow*)oneui_window_native_handle(handle);
    NSTimer* cancel = [NSTimer scheduledTimerWithTimeInterval:0.05 repeats:YES block:^(NSTimer* timer) {
        if (owner.attachedSheet) {
            [owner endSheet:owner.attachedSheet returnCode:NSAlertSecondButtonReturn];
            [timer invalidate];
        }
    }];
    wchar_t result[32]{};
    check(!oneui_window_prompt_text(handle, L"Cancel", L"Secret", L"secret", L"", 1, result, 32) && !result[0], "Cancelled Cocoa prompt returned data");
    [cancel invalidate];
    oneui_window_destroy(handle);
}
void testModalLifetimeAndScaling() {
    auto window = oneui::Window::create(L"Cocoa modal lifetime", 400, 300);
    window->setMinimumClientSize({120, 80});
    window->setContentScale(1.5f);
    window->initialize();
    NSWindow* native = (__bridge NSWindow*)window->nativeHandle();
    check(NSEqualSizes(native.contentMinSize, NSMakeSize(180, 120)), "Minimum size applies zoom set before initialization");
    window->setContentScale(2.f);
    check(NSEqualSizes(native.contentMinSize, NSMakeSize(240, 160)), "Minimum size follows content zoom");
    window->show();
    auto* desktop = dynamic_cast<oneui::platform::DesktopWindow*>(window.get());
    auto field = std::make_shared<oneui::TextField>(L"original");
    field->setText(L"original");
    window->setContent(field);
    window->requestFocus(field.get());
    NSView<NSTextInputClient>* view = (NSView<NSTextInputClient>*)native.contentView;
    desktop->enterNativeModal();
    [view setMarkedText:@"blocked" selectedRange:NSMakeRange(7, 0) replacementRange:NSMakeRange(0, 8)];
    [view insertText:@"blocked" replacementRange:NSMakeRange(0, 8)];
    check(!view.hasMarkedText && field->text() == L"original", "A sheet blocks direct owner IME replacement and preedit");
    desktop->leaveNativeModal();

    window->post([] { throw std::runtime_error("modal callback regression"); });
    bool threw = false;
    try { oneui::platform::nativeConfirm(window.get(), L"Callback", L"Unwind the sheet"); }
    catch (const std::runtime_error&) { threw = true; }
    check(threw && !desktop->nativeModal() && !native.attachedSheet, "A throwing callback unwinds native modal state");

    window->post([&] {
        if (native.attachedSheet) [native endSheet:native.attachedSheet returnCode:NSAlertSecondButtonReturn];
        window.reset();
    });
    check(!oneui::platform::nativeConfirm(window.get(), L"Lifetime", L"Destroy owner while accepting"),
          "Owner destruction wins over a pending accepted result");
    check(!window, "Modal callback destroyed its owner safely");
}
} // namespace

int main() {
    @autoreleasepool {
        @try { testApplicationMenu(); testNativeInput(); testNativeServices(); testModalLifetimeAndScaling(); testNativeAccessibility(); testNativeAccessibleTextAndValue(); }
        @catch (NSException* error) {
            std::cerr << "AppKit exception: " << error.reason.UTF8String << '\n';
            return 1;
        }
    }
    return failures ? 1 : 0;
}
