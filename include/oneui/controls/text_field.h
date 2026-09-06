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
    ~TextField() override;
    void setTextOptions(TextOptions options);
    const TextOptions& textOptions() const { return textOptions_; }
    TextPosition textPosition() const;
    bool setTextPosition(TextPosition position);

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
    void setFontSize(float fontSize);
    float fontSize() const;
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
    bool hasTextComposition() const override { return !composition_.empty(); }
    CommandResult queryBuiltinCommand(const std::string& id) const override;
    CommandResult executeBuiltinCommand(const std::string& id) override;
    CommandResult dispatchBuiltinCommandKey(const KeyEvent& event, const std::string& logicalKey) override;
    bool onTextInput(wchar_t character) override;
    bool onTextCommitted(const std::wstring& text) override;
    TextInputState textInputState() const override;
    void setTextComposition(std::wstring text, std::size_t caret) override;
    bool replaceTextRange(std::size_t start, std::size_t end, const std::wstring& text) override;
    Rect textInputCaretRect() const override;
    bool onFocusChanged(bool focused) override;
    CursorKind cursor(Point point) const override;
    bool isFocusable() const override;
    void setFocusVisible(bool visible) override;
    bool tickAnimations(double nowMs) override;
    AccessibilityInfo accessibilityInfo() const override;

private:
    struct TextLayoutState;
    void ensureTextLayout() const;
    std::size_t toDisplayOffset(std::size_t offset) const;
    std::size_t fromDisplayOffset(std::size_t offset) const;
    std::size_t displayCaretOffset() const;
    Point textOrigin(Rect content) const;
    void paintTextContent(Canvas& canvas, Rect content, const TextFieldStyle& style, bool placeholder);
    mutable std::unique_ptr<TextLayoutState> textLayout_;
    TextOptions textOptions_;
    TextAffinity caretAffinity_ = TextAffinity::Downstream;
    mutable TextAffinity hitAffinity_ = TextAffinity::Downstream;
    std::optional<float> verticalCaretX_;
    float verticalScrollOffset_ = 0;
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
    bool moveCaretVertically(int direction, bool extendSelection);
    void ensureCaretVisible();
    float contentWidthForText() const;
    void invalidateTextMetrics();
    bool hasInteractionState() const override;
    void resetInteractionState() override;

    std::wstring placeholder_;
    std::wstring text_;
    std::wstring composition_;
    std::size_t compositionCaret_ = 0;
    std::size_t caretIndex_ = 0;
    std::size_t selectionAnchor_ = 0;
    bool hasSelection_ = false;
    bool passwordMode_ = false;
    bool readOnly_ = false;
    bool multiline_ = false;
    float lineHeight_ = 20.0f;
    float fontSize_ = 14.0f;
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
};

/// A native multiline text editor using the same style contract as TextField.
class ONEUI_API TextArea final : public TextField {
public:
    explicit TextArea(std::wstring placeholder = {});
};

} // namespace oneui
