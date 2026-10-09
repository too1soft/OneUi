#include "platform/shared/accessibility.h"
#include "internal/unicode.h"
#include "platform/shared/text_input.h"
#include "oneui/controls/text_field.h"
#import <Cocoa/Cocoa.h>
#include <map>

using oneui::AccessibilityRole;
using oneui::platform::AccessibilityContext;
using oneui::platform::AccessibleNode;
@class OneUiAccessibleElement;
// The bridge owns the element cache. Elements only retain the independent
// context and weakly reference this owner, so neither widgets nor windows form
// retain cycles with accessibility clients holding stale elements.
@interface OneUiAccessibilityOwner : NSObject {
@public
    std::shared_ptr<AccessibilityContext> context;
    NSMutableDictionary<NSNumber*, OneUiAccessibleElement*>* elements;
    __weak NSView* view;
}
- (OneUiAccessibleElement*)element:(uint64_t)identifier;
- (NSArray*)children:(uint64_t)identifier;
@end
@interface OneUiAccessibleElement : NSAccessibilityElement {
@public
    std::shared_ptr<AccessibilityContext> context;
    __weak OneUiAccessibilityOwner* owner;
    uint64_t identifier;
}
@end
namespace {
NSString* text(const std::wstring& value) {
    const auto utf8 = oneui::unicode::toUtf8(value);
    return [[NSString alloc] initWithBytes:utf8.data() length:utf8.size() encoding:NSUTF8StringEncoding] ?: @"";
}
NSString* role(AccessibilityRole role) {
    switch (role) {
    case AccessibilityRole::Button: return NSAccessibilityButtonRole;
    case AccessibilityRole::Text: return NSAccessibilityStaticTextRole;
    case AccessibilityRole::TextBox: return NSAccessibilityTextFieldRole;
    case AccessibilityRole::CheckBox: return NSAccessibilityCheckBoxRole;
    case AccessibilityRole::RadioButton: return NSAccessibilityRadioButtonRole;
    case AccessibilityRole::RadioGroup: return NSAccessibilityRadioGroupRole;
    case AccessibilityRole::ComboBox: return NSAccessibilityComboBoxRole;
    case AccessibilityRole::Slider: return NSAccessibilitySliderRole;
    case AccessibilityRole::ProgressBar: return NSAccessibilityProgressIndicatorRole;
    case AccessibilityRole::Tab: return NSAccessibilityRadioButtonRole;
    case AccessibilityRole::TabList: return NSAccessibilityTabGroupRole;
    case AccessibilityRole::List: return NSAccessibilityListRole;
    case AccessibilityRole::Table: return NSAccessibilityTableRole;
    case AccessibilityRole::Row: return NSAccessibilityRowRole;
    case AccessibilityRole::Cell: return NSAccessibilityCellRole;
    default: return NSAccessibilityGroupRole;
    }
}
}
@implementation OneUiAccessibilityOwner
- (OneUiAccessibleElement*)element:(uint64_t)key {
    // Callers have already resolved membership in their current snapshot.
    if (!context->valid() || !context->exposed()) return nil;
    OneUiAccessibleElement* element = elements[@(key)];
    if (!element) {
        element = [OneUiAccessibleElement new];
        element->context = context; element->owner = self; element->identifier = key;
        elements[@(key)] = element;
    }
    return element;
}
- (NSArray*)children:(uint64_t)key {
    NSMutableArray* result = [NSMutableArray new];
    for (const auto& node : context->snapshot()) if (node.parent == key) {
        OneUiAccessibleElement* element = [self element:node.id];
        if (element) [result addObject:element];
    }
    return result;
}
@end
@implementation OneUiAccessibleElement
- (BOOL)isAccessibilityElement { return context && context->resolve(identifier).has_value(); }
- (NSString*)accessibilityRole {
    auto node = context->resolve(identifier);
    if (node && node->text && node->text->multiline) return NSAccessibilityTextAreaRole;
    return node ? role(node->info.role) : NSAccessibilityUnknownRole;
}
- (NSString*)accessibilitySubrole {
    auto node = context->resolve(identifier); return node && node->sensitive ? NSAccessibilitySecureTextFieldSubrole : nil;
}
- (NSString*)accessibilityLabel {
    auto node = context->resolve(identifier); return node ? text(node->info.name) : nil;
}
- (NSString*)accessibilityHelp {
    auto node = context->resolve(identifier); return node ? text(node->info.description) : nil;
}
- (id)accessibilityValue {
    auto node = context->resolve(identifier);
    if (!node || node->sensitive) return nil;
    if (node->info.role == AccessibilityRole::CheckBox || node->info.role == AccessibilityRole::RadioButton)
        return @(node->info.state.checked);
    if (node->info.role == AccessibilityRole::Tab) return @(node->info.state.selected);
    if (node->number) return @(node->number->value);
    if (node->text) return text(node->text->value);
    if (node->info.role == AccessibilityRole::ProgressBar) return nil;
    if (node->info.role == AccessibilityRole::Text) return text(node->info.value.empty() ? node->info.name : node->info.value);
    return text(node->info.value);
}
- (void)setAccessibilityValue:(id)value {
    auto keep = context;
    if ([value isKindOfClass:NSNumber.class]) {
        try { keep->setNumber(identifier, [value doubleValue]); } catch (...) {}
        return;
    }
    if (![value isKindOfClass:NSString.class]) return;
    NSData* data = [(NSString*)value dataUsingEncoding:NSUTF8StringEncoding];
    try { keep->setValue(identifier, oneui::unicode::fromUtf8({static_cast<const char*>(data.bytes), data.length})); }
    catch (...) {} // Never unwind application callbacks through AppKit.
}
- (BOOL)isAccessibilityEnabled {
    auto node = context->resolve(identifier); return node && !node->info.state.disabled;
}
- (BOOL)isAccessibilityFocused {
    auto node = context->resolve(identifier); return node && node->info.state.focused;
}
- (void)setAccessibilityFocused:(BOOL)focused {
    auto keep = context;
    try { if (focused) keep->focusNode(identifier); } catch (...) {}
}
- (BOOL)isAccessibilitySelected {
    auto node = context->resolve(identifier); return node && node->info.state.selected;
}
- (BOOL)isAccessibilityExpanded {
    auto node = context->resolve(identifier); return node && node->info.state.expanded;
}
- (BOOL)isAccessibilitySelectorAllowed:(SEL)selector {
    auto node = context->resolve(identifier);
    if (!node) return NO;
    if (selector == @selector(accessibilityPerformPress))
        return !node->info.state.disabled && AccessibilityContext::pressable(node->info.role);
    if (selector == @selector(setAccessibilityFocused:)) return node->focusable && !node->info.state.disabled;
    if (selector == @selector(setAccessibilityValue:))
        return (node->number && node->number->writable) ||
            (node->info.role == AccessibilityRole::TextBox && !node->info.state.disabled && !node->info.state.readOnly);
    if (selector == @selector(accessibilityPerformIncrement) || selector == @selector(accessibilityPerformDecrement))
        return node->number && node->number->writable;
    if (selector == @selector(accessibilityMinValue) || selector == @selector(accessibilityMaxValue)) return node->number.has_value();
    if (selector == @selector(setAccessibilitySelectedText:))
        return node->text && !node->info.state.disabled && !node->info.state.readOnly;
    if (selector == @selector(setAccessibilitySelectedTextRange:)) return node->text && !node->info.state.disabled;
    if (selector == @selector(accessibilityNumberOfCharacters) || selector == @selector(accessibilitySelectedTextRange) ||
        selector == @selector(accessibilitySelectedText) || selector == @selector(accessibilityStringForRange:) ||
        selector == @selector(accessibilityRangeForIndex:) || selector == @selector(accessibilityRangeForLine:) ||
        selector == @selector(accessibilityLineForIndex:) || selector == @selector(accessibilityInsertionPointLineNumber) ||
        selector == @selector(accessibilityFrameForRange:) || selector == @selector(accessibilityRangeForPosition:)) return node->text.has_value();
    return [super isAccessibilitySelectorAllowed:selector];
}
- (BOOL)accessibilityPerformPress {
    auto keep = context;
    try { return keep->press(identifier); } catch (...) { return NO; }
}
- (id)accessibilityMinValue {
    auto node = context->resolve(identifier); return node && node->number ? @(node->number->minimum) : nil;
}
- (id)accessibilityMaxValue {
    auto node = context->resolve(identifier); return node && node->number ? @(node->number->maximum) : nil;
}
- (BOOL)accessibilityPerformIncrement {
    auto keep = context; try { return keep->stepNumber(identifier, true); } catch (...) { return NO; }
}
- (BOOL)accessibilityPerformDecrement {
    auto keep = context; try { return keep->stepNumber(identifier, false); } catch (...) { return NO; }
}
- (NSInteger)accessibilityNumberOfCharacters {
    auto node = context->resolve(identifier);
    return node && node->text ? oneui::platform::wideToUtf16Offset(node->text->value, node->text->value.size()) : 0;
}
- (NSRange)accessibilitySelectedTextRange {
    auto node = context->resolve(identifier);
    if (!node || !node->text) return NSMakeRange(NSNotFound, 0);
    const auto& t = *node->text;
    const auto start = oneui::platform::wideToUtf16Offset(t.value, t.start);
    return NSMakeRange(start, oneui::platform::wideToUtf16Offset(t.value, t.end) - start);
}
- (void)setAccessibilitySelectedTextRange:(NSRange)range {
    auto keep = context; auto node = keep->resolve(identifier);
    if (!node || !node->text || range.location == NSNotFound) return;
    const auto& t = node->text->value;
    const auto size = oneui::platform::wideToUtf16Offset(t, t.size());
    if (range.location > size || range.length > size - range.location) return;
    const auto offsets = oneui::platform::utf16ReplacementRange(t, range.location, range.length);
    try { keep->selectText(identifier, offsets.first, offsets.second); } catch (...) {}
}
- (NSString*)accessibilitySelectedText {
    auto node = context->resolve(identifier);
    if (!node || !node->text) return nil;
    const auto& t = *node->text; return text(t.value.substr(t.start, t.end - t.start));
}
- (void)setAccessibilitySelectedText:(NSString*)value {
    auto keep = context; auto node = keep->resolve(identifier);
    if (!node || !node->text || ![value isKindOfClass:NSString.class]) return;
    NSData* data = [value dataUsingEncoding:NSUTF8StringEncoding];
    try { keep->replaceText(identifier, node->text->start, node->text->end,
        oneui::unicode::fromUtf8({static_cast<const char*>(data.bytes), data.length})); } catch (...) {}
}
- (NSString*)accessibilityStringForRange:(NSRange)range {
    auto node = context->resolve(identifier);
    if (!node || !node->text || range.location == NSNotFound) return nil;
    const auto& t = node->text->value;
    const auto size = oneui::platform::wideToUtf16Offset(t, t.size());
    if (range.location > size || range.length > size - range.location) return nil;
    const auto offsets = oneui::platform::utf16ReplacementRange(t, range.location, range.length);
    return text(t.substr(offsets.first, offsets.second - offsets.first));
}
- (NSRange)accessibilityRangeForIndex:(NSInteger)index {
    auto node = context->resolve(identifier);
    if (!node || !node->text || index < 0) return NSMakeRange(NSNotFound, 0);
    const auto& t = node->text->value;
    if (std::size_t(index) >= oneui::platform::wideToUtf16Offset(t, t.size())) return NSMakeRange(NSNotFound, 0);
    const auto start = oneui::platform::utf16ToWideOffset(t, index);
    const auto a = oneui::platform::wideToUtf16Offset(t, start);
    return NSMakeRange(a, oneui::platform::wideToUtf16Offset(t, oneui::unicode::next(t, start)) - a);
}
- (NSRange)accessibilityRangeForLine:(NSInteger)line {
    auto node = context->resolve(identifier);
    if (!node || !node->text || line < 0) return NSMakeRange(NSNotFound, 0);
    auto field = std::dynamic_pointer_cast<oneui::TextField>(node->widget.lock());
    if (!field) return NSMakeRange(NSNotFound, 0);
    const auto lines = field->textLineRanges();
    if (std::size_t(line) >= lines.size()) return NSMakeRange(NSNotFound, 0);
    const auto& t = node->text->value;
    const auto start = oneui::platform::wideToUtf16Offset(t, lines[line].first);
    return NSMakeRange(start, oneui::platform::wideToUtf16Offset(t, lines[line].second) - start);
}
- (NSInteger)accessibilityLineForIndex:(NSInteger)index {
    auto node = context->resolve(identifier);
    if (!node || !node->text || index < 0) return NSNotFound;
    const auto& t = node->text->value;
    if (std::size_t(index) > oneui::platform::wideToUtf16Offset(t, t.size())) return NSNotFound;
    auto field = std::dynamic_pointer_cast<oneui::TextField>(node->widget.lock());
    if (!field) return NSNotFound;
    const auto offset = oneui::platform::utf16ToWideOffset(t, index);
    const auto lines = field->textLineRanges();
    for (std::size_t i = 0; i < lines.size(); ++i)
        if (offset >= lines[i].first && (i + 1 == lines.size() || offset < lines[i + 1].first)) return i;
    return NSNotFound;
}
- (NSInteger)accessibilityInsertionPointLineNumber {
    auto node = context->resolve(identifier);
    return node && node->text ? [self accessibilityLineForIndex:oneui::platform::wideToUtf16Offset(node->text->value, node->text->caret)] : NSNotFound;
}
- (NSRect)accessibilityFrameForRange:(NSRange)range {
    auto node = context->resolve(identifier);
    if (!node || !node->text || range.location == NSNotFound) return NSZeroRect;
    const auto& t = node->text->value;
    const auto size = oneui::platform::wideToUtf16Offset(t, t.size());
    if (range.location > size || range.length > size - range.location) return NSZeroRect;
    auto field = std::dynamic_pointer_cast<oneui::TextField>(node->widget.lock());
    if (!field) return NSZeroRect;
    const auto offsets = oneui::platform::utf16ReplacementRange(t, range.location, range.length);
    NSRect result = NSZeroRect; bool first = true;
    for (const auto& r : field->textRangeRects(offsets.first, offsets.second)) {
        auto screen = context->bounds(r, true); if (!screen) return NSZeroRect;
        NSRect rect = NSMakeRect(screen->x, screen->y, screen->width, screen->height);
        result = first ? rect : NSUnionRect(result, rect); first = false;
    }
    return result;
}
- (NSRange)accessibilityRangeForPosition:(NSPoint)point {
    auto node = context->resolve(identifier);
    if (!node || !node->text) return NSMakeRange(NSNotFound, 0);
    auto field = std::dynamic_pointer_cast<oneui::TextField>(node->widget.lock());
    auto origin = context->bounds({0, 0, 0, 0}, true), x = context->bounds({1, 0, 0, 0}, true), y = context->bounds({0, 1, 0, 0}, true);
    if (!field || !origin || !x || !y || x->x == origin->x || y->y == origin->y) return NSMakeRange(NSNotFound, 0);
    auto offset = field->textOffsetAtPoint({float((point.x - origin->x) / (x->x - origin->x)), float((point.y - origin->y) / (y->y - origin->y))});
    return offset ? [self accessibilityRangeForIndex:oneui::platform::wideToUtf16Offset(node->text->value, *offset)] : NSMakeRange(NSNotFound, 0);
}
- (NSRect)accessibilityFrame {
    auto node = context->resolve(identifier);
    if (!node) return NSZeroRect;
    const auto rect = context->bounds(node->frame, true);
    return rect ? NSMakeRect(rect->x, rect->y, rect->width, rect->height) : NSZeroRect;
}
- (id)accessibilityParent {
    auto node = context->resolve(identifier);
    OneUiAccessibilityOwner* bridge = owner;
    if (!node || !bridge) return nil;
    return node->parent ? [bridge element:node->parent] : bridge->view;
}
- (id)accessibilityWindow {
    OneUiAccessibilityOwner* bridge = owner; return bridge ? bridge->view.window : nil;
}
- (id)accessibilityTopLevelUIElement { return self.accessibilityWindow; }
- (NSArray*)accessibilityChildren {
    OneUiAccessibilityOwner* bridge = owner;
    return bridge && context->resolve(identifier) ? [bridge children:identifier] : @[];
}
- (id)accessibilityHitTest:(NSPoint)point {
    if (!context->resolve(identifier) || !NSPointInRect(point, self.accessibilityFrame)) return nil;
    for (OneUiAccessibleElement* child in [self.accessibilityChildren reverseObjectEnumerator]) {
        id result = [child accessibilityHitTest:point]; if (result) return result;
    }
    return self;
}
- (id)accessibilityFocusedUIElement {
    if (self.isAccessibilityFocused) return self;
    for (OneUiAccessibleElement* child in self.accessibilityChildren) {
        id result = [child accessibilityFocusedUIElement]; if (result) return result;
    }
    return nil;
}
@end
namespace oneui::platform {
namespace {
class CocoaAccessibility final : public NativeAccessibility {
    OneUiAccessibilityOwner* owner_;
    std::vector<AccessibleNode> previous_;
public:
    CocoaAccessibility(std::shared_ptr<AccessibilityContext> context, NSView* view) {
        owner_ = [OneUiAccessibilityOwner new];
        owner_->context = std::move(context); owner_->view = view;
        owner_->elements = [NSMutableDictionary new];
        view.accessibilityElement = YES;
        view.accessibilityRole = NSAccessibilityGroupRole;
    }
    ~CocoaAccessibility() override {
        owner_->view.accessibilityChildren = @[];
        for (OneUiAccessibleElement* element in owner_->elements.allValues)
            NSAccessibilityPostNotification(element, NSAccessibilityUIElementDestroyedNotification);
    }
    void sync() override {
        OneUiAccessibilityOwner* bridge = owner_;
        const auto nodes = bridge->context->snapshot();
        bool structureChanged = nodes.size() != previous_.size();
        std::map<std::uint64_t, const AccessibleNode*> old;
        for (const auto& n : previous_) old[n.id] = &n;
        NSMutableSet* live = [NSMutableSet new];
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            const auto& n = nodes[i]; [live addObject:@(n.id)];
            if (!structureChanged && (n.id != previous_[i].id || n.parent != previous_[i].parent)) structureChanged = true;
            OneUiAccessibleElement* element = [bridge element:n.id];
            if (!element) continue;
            auto found = old.find(n.id);
            const auto* before = found == old.end() ? nullptr : found->second;
            if (n.info.state.focused && (!before || !before->info.state.focused))
                NSAccessibilityPostNotification(element, NSAccessibilityFocusedUIElementChangedNotification);
            if (before && (before->info.value != n.info.value || before->info.state.checked != n.info.state.checked ||
                           before->info.state.selected != n.info.state.selected ||
                           (n.text && before->text && n.text->value != before->text->value) ||
                           (n.number && before->number && n.number->value != before->number->value)))
                NSAccessibilityPostNotification(element, NSAccessibilityValueChangedNotification);
            if (before && n.text && before->text && (n.text->caret != before->text->caret ||
                n.text->start != before->text->start || n.text->end != before->text->end))
                NSAccessibilityPostNotification(element, NSAccessibilitySelectedTextChangedNotification);
            if (before && (before->info.name != n.info.name || before->info.state.disabled != n.info.state.disabled))
                structureChanged = true;
        }
        for (NSNumber* key in bridge->elements.allKeys) if (![live containsObject:key]) {
            NSAccessibilityPostNotification(bridge->elements[key], NSAccessibilityUIElementDestroyedNotification);
            [bridge->elements removeObjectForKey:key];
        }
        bridge->view.accessibilityChildren = [bridge children:0];
        if (structureChanged) NSAccessibilityPostNotification(bridge->view, NSAccessibilityLayoutChangedNotification);
        previous_ = nodes;
    }
};
}
std::unique_ptr<NativeAccessibility> makeNativeAccessibility(std::shared_ptr<AccessibilityContext> context, void* view) {
    return std::make_unique<CocoaAccessibility>(std::move(context), (__bridge NSView*)view);
}
} // namespace oneui::platform
