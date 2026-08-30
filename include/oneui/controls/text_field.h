#pragma once

#include "oneui/animation.h"
#include "oneui/clipboard.h"
#include "oneui/export.h"
#include "oneui/icon.h"
#include "oneui/reactive.h"
#include "oneui/style.h"
#include "oneui/widget.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace oneui {

class ONEUI_API TextField : public Widget {
public:
    explicit TextField(std::wstring placeholder = {});

    void setPlaceholder(std::wstring placeholder);
    void setText(std::wstring text);
    const std::wstring& text() const;
    void setCaretIndex(std::size_t index);
    std::size_t caretIndex() const;
    void setSelectionRange(std::size_t start, std::size_t end);
    std::size_t selectionStart() const;
    std::size_t selectionEnd() const;
    bool hasSelection() const;
    std::wstring selectedText() const;
    void selectAll();
    void clearSelection();
    bool copySelectionToClipboard(Clipboard& clipboard) const;
    bool cutSelectionToClipboard(Clipboard& clipboard);
    bool pasteFromClipboard(const Clipboard& clipboard);
    bool undo();
    bool redo();
    void setReadOnly(bool readOnly);
    bool readOnly() const;
    void setMultiline(bool multiline);
    bool multiline() const;
    void setLineHeight(float lineHeight);
    float lineHeight() const;
    void setClipboard(std::shared_ptr<Clipboard> clipboard);
    std::shared_ptr<Clipboard> clipboard() const;
    void setPasswordMode(bool enabled);
    bool passwordMode() const;
    void setPasswordMask(wchar_t mask);
    wchar_t passwordMask() const;
    void setPrefixIcon(std::optional<IconSymbol> symbol);
    void setSuffixIcon(std::optional<IconSymbol> symbol);
    void setStyleOverride(TextFieldStyleOverride style);
    void clearStyleOverride();
    void bindText(State<std::wstring>& state);
    void setOnChanged(std::function<void(const std::wstring&)> callback);
    void setOnSubmitted(std::function<void(const std::wstring&)> callback);
    void setDisabled(bool disabled) override;
    void setAnimationScheduler(std::function<void()> scheduler) override;

    void paint(Canvas& canvas) override;
    bool onMouseMove(const MouseEvent& event) override;
    bool onMouseDown(const MouseEvent& event) override;
    bool onMouseUp(const MouseEvent& event) override;
    bool onKeyDown(const KeyEvent& event) override;
    bool onTextInput(wchar_t character) override;
    bool onFocusChanged(bool focused) override;
    CursorKind cursor(Point point) const override;
    bool isFocusable() const override;
    void setFocusVisible(bool visible) override;
    bool tickAnimations(double nowMs) override;
    AccessibilityInfo accessibilityInfo() const override;

private:
    struct TextEditSnapshot {
        std::wstring text;
        std::size_t caretIndex = 0;
        std::size_t selectionAnchor = 0;
        bool hasSelection = false;
    };

    struct TextEditEntry {
        TextEditSnapshot before;
        TextEditSnapshot after;
    };

    struct TextLine {
        std::size_t start = 0;
        std::size_t end = 0;
    };

    bool assignText(std::wstring text, std::size_t nextCaretIndex, bool recordUndo = false);
    bool editable() const;
    TextEditSnapshot makeEditSnapshot() const;
    bool restoreEditSnapshot(const TextEditSnapshot& snapshot);
    void clearEditHistory();
    void clampCaret();
    void clampSelection();
    void setCaretIndexInternal(std::size_t index, bool extendSelection = false);
    bool deleteSelection();
    const std::wstring& value() const;
    std::wstring displayText() const;
    TextFieldStyle resolvedStyle() const;
    TextFieldStyle visualStyle(TextFieldStyle target) const;
    void beginVisualTransition(TextFieldStyle from, TextFieldStyle target);
    void restartCaretBlink();
    std::size_t caretIndexFromPoint(Point point) const;
    std::size_t multilineCaretIndexFromPoint(Point point) const;
    bool moveCaretVertically(int direction, bool extendSelection);
    void ensureCaretVisible();
    void ensureMultilineCaretVisible();
    float contentWidthForText() const;
    void invalidateTextMetrics();
    void updateTextMetrics(const Canvas* canvas = nullptr) const;
    void updateMultilineTextMetrics(const Canvas* canvas = nullptr) const;
    float textWidthAt(std::size_t index) const;
    float multilineTextWidthAt(std::size_t lineIndex, std::size_t index) const;
    std::size_t multilineLineIndexForCaret(std::size_t index) const;
    void paintMultilineContent(
        Canvas& canvas,
        Rect contentRect,
        const TextFieldStyle& style,
        bool hasText,
        bool shouldPaintPlaceholder);
    bool hasInteractionState() const override;
    void resetInteractionState() override;

    std::wstring placeholder_;
    std::wstring text_;
    std::size_t caretIndex_ = 0;
    std::size_t selectionAnchor_ = 0;
    std::size_t textScrollOffset_ = 0;
    bool hasSelection_ = false;
    bool passwordMode_ = false;
    bool readOnly_ = false;
    bool multiline_ = false;
    float lineHeight_ = 20.0f;
    std::size_t verticalScrollLine_ = 0;
    float horizontalScrollOffset_ = 0.0f;
    wchar_t passwordMask_ = L'*';
    bool hovered_ = false;
    bool selecting_ = false;
    bool caretBlinkVisible_ = true;
    double caretBlinkStartMs_ = 0.0;
    bool visualInitialized_ = false;
    std::optional<IconSymbol> prefixIcon_;
    std::optional<IconSymbol> suffixIcon_;
    bool applyingInternalTextChange_ = false;
    std::optional<TextFieldStyleOverride> styleOverride_;
    std::shared_ptr<Clipboard> clipboard_;
    Binding<std::wstring> textBinding_;
    ColorTransition backgroundTransition_;
    ColorTransition foregroundTransition_;
    ColorTransition placeholderTransition_;
    ColorTransition borderTransition_;
    std::function<void(const std::wstring&)> onChanged_;
    std::function<void(const std::wstring&)> onSubmitted_;
    std::vector<TextEditEntry> undoStack_;
    std::vector<TextEditEntry> redoStack_;
    mutable std::wstring measuredDisplayText_;
    mutable std::vector<float> measuredPrefixWidths_;
    mutable bool measuredTextMetricsExact_ = false;
    mutable std::vector<TextLine> measuredLines_;
    mutable std::vector<std::vector<float>> measuredLinePrefixWidths_;
    mutable bool measuredMultilineMetricsExact_ = false;
};

/// A native multiline text editor using the same style contract as TextField.
class ONEUI_API TextArea final : public TextField {
public:
    explicit TextArea(std::wstring placeholder = {});
};

} // namespace oneui
