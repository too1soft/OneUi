#import "platform/ios/application_view_internal.h"
#include <algorithm>
#include <climits>

namespace {
std::u16string utf16(NSString *text) {
    std::u16string result(text.length, u'\0');
    if (text.length) [text getCharacters:reinterpret_cast<unichar *>(result.data()) range:NSMakeRange(0, text.length)];
    return result;
}
NSString *nativeText(const std::u16string &text) {
    return [[NSString alloc] initWithCharacters:reinterpret_cast<const unichar *>(text.data()) length:text.size()];
}
}
@interface OneUiImeBridge () <UITextViewDelegate>
@end
@implementation OneUiImeBridge {
    __weak OneUiApplicationView *_view;
    UITextView *_editor;
    uint64_t _session;
    BOOL _syncing, _sensitive;
}
- (instancetype)initWithView:(OneUiApplicationView *)view {
    if ((self = [super init])) {
        _view = view;
        _editor = [[UITextView alloc] initWithFrame:CGRectMake(0, 0, 1, 1)];
        _editor.delegate = self; _editor.textColor = UIColor.clearColor; _editor.tintColor = UIColor.clearColor;
        _editor.backgroundColor = UIColor.clearColor; _editor.userInteractionEnabled = NO;
        _editor.accessibilityElementsHidden = YES; _editor.autocorrectionType = UITextAutocorrectionTypeNo;
        _editor.autocapitalizationType = UITextAutocapitalizationTypeNone;
        [view addSubview:_editor];
    }
    return self;
}
- (void)suspend {
    [_editor resignFirstResponder]; _syncing = YES; _editor.text = @""; _syncing = NO; _session = 0;
}
- (void)sync:(BOOL)showKeyboard {
    auto *host = [_view oneuiHost];
    if (!host) { [self suspend]; return; }
    auto state = host->input().snapshot();
    if (!state.session) { [self suspend]; return; }
    const BOOL changed = state.session != _session;
    _syncing = YES;
    if (changed) {
        _session = state.session; _sensitive = state.sensitive;
        _editor.secureTextEntry = _sensitive;
        _editor.keyboardType = _sensitive ? UIKeyboardTypeASCIICapable : UIKeyboardTypeDefault;
        _editor.smartDashesType = UITextSmartDashesTypeNo; _editor.smartQuotesType = UITextSmartQuotesTypeNo;
        _editor.smartInsertDeleteType = UITextSmartInsertDeleteTypeNo;
        _editor.textContentType = _sensitive ? UITextContentTypePassword : nil;
    }
    // Never replace UIKit's live marked range during Chinese/Japanese input.
    if (changed || _editor.markedTextRange == nil) {
        NSString *text = nativeText(state.text);
        if (![_editor.text isEqualToString:text]) _editor.text = text;
        const auto first = std::min(state.anchor, state.caret), last = std::max(state.anchor, state.caret);
        if (first >= 0 && last <= static_cast<int>(text.length)) _editor.selectedRange = NSMakeRange(first, last - first);
    }
    _syncing = NO;
    if (showKeyboard && !_editor.isFirstResponder) [_editor becomeFirstResponder];
}
- (BOOL)textView:(UITextView *)textView shouldChangeTextInRange:(NSRange)range replacementText:(NSString *)text {
    if (_syncing || !_sensitive) return YES;
    auto *host = [_view oneuiHost];
    if (!host || range.location > INT_MAX || NSMaxRange(range) > INT_MAX) return NO;
    using oneui::mobile::SensitiveEdit;
    auto &input = host->input();
    if (input.editSensitive(_session, SensitiveEdit::Select, {}, static_cast<int>(range.location), static_cast<int>(NSMaxRange(range))))
        input.editSensitive(_session, SensitiveEdit::Commit, utf16(text), 1);
    [_view oneuiChanged]; [self sync:NO];
    return NO; // UIKit keeps bullets only; the clear text stays at the input boundary.
}
- (void)textViewDidChange:(UITextView *)textView { [self apply:textView]; }
- (void)textViewDidChangeSelection:(UITextView *)textView {
    if (_sensitive && !_syncing) {
        auto *host = [_view oneuiHost];
        NSRange range = textView.selectedRange;
        if (host && NSMaxRange(range) <= INT_MAX)
            host->input().editSensitive(_session, oneui::mobile::SensitiveEdit::Select, {}, static_cast<int>(range.location), static_cast<int>(NSMaxRange(range)));
        [_view oneuiChanged];
    } else [self apply:textView];
}
- (void)apply:(UITextView *)textView {
    if (_syncing || _sensitive || !_session) return;
    auto *host = [_view oneuiHost]; if (!host) return;
    auto state = host->input().snapshot();
    if (state.session != _session || textView.text.length > 1024 * 1024 || NSMaxRange(textView.selectedRange) > INT_MAX) return;
    state.text = utf16(textView.text); state.anchor = static_cast<int>(textView.selectedRange.location);
    state.caret = static_cast<int>(NSMaxRange(textView.selectedRange));
    UITextRange *marked = textView.markedTextRange;
    state.composingStart = marked ? static_cast<int>([textView offsetFromPosition:textView.beginningOfDocument toPosition:marked.start]) : -1;
    state.composingEnd = marked ? static_cast<int>([textView offsetFromPosition:textView.beginningOfDocument toPosition:marked.end]) : -1;
    host->input().apply(state); [_view oneuiChanged];
}
- (void)dealloc { _editor.delegate = nil; [_editor removeFromSuperview]; }
@end
