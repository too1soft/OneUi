#pragma once
#include "oneui/view.h"
#include <string>
namespace oneui {
struct FontPickerItem {std::string id;std::wstring family,label;bool available=true;};
struct FontPickerConfig {
    std::vector<FontPickerItem> items;
    std::string selected;
    std::wstring error,previewText=L"Aa 0123";
    bool loading=false;
    float size=14;
};
class ONEUI_API FontPicker final : public View {
public:
    FontPicker();
    ~FontPicker() override;
    void setConfig(FontPickerConfig);
    void setTriggerLabel(std::wstring);
    void setConfirmation(bool, std::wstring applyLabel, std::wstring cancelLabel);
    void setMonospaceFilter(std::vector<std::string> keys, bool initiallyChecked, std::wstring label);
    void setSearchLabel(std::wstring);
    void setRetryLabel(std::wstring);
    void setPopupViewportProvider(std::function<Rect()>);
    const FontPickerConfig& config() const;
    void setOnSelectionRequested(std::function<void(std::string)>);
    void setOnRetryRequested(std::function<void()>);
    void setOpen(bool);
    bool open() const;
    void setQuery(std::wstring);
    const std::wstring& query() const;
    size_t filteredCount() const;
    size_t paintedRows() const;
    Size naturalSize() const override {return {260,36};}
    Rect paintBounds() const override;
    bool paintsAboveSiblings() const override;
    bool hitTest(Point) const override;
    void paint(Canvas&) override;
    bool onKeyDown(const KeyEvent&) override;
    bool onFocusChanged(bool) override;
    AccessibilityInfo accessibilityInfo() const override;
protected:
    void layoutChildren() override;
    void styleScopeChanged() override;
private:
    struct State;
    std::shared_ptr<State> state_;
    void filter();
    void select(int);
    Rect popupRect() const;
};
} // namespace oneui
