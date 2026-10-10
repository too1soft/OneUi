#pragma once
#include "oneui/controls/font_picker.h"
#include "oneui/controls/text_field.h"
#include "oneui/controls/button.h"
#include "oneui/controls/label.h"
namespace oneui {
// A specialized viewport renderer: visible rows use their actual font family.
// Item count changes never allocate a native Widget per font.
class FontPickerRows final : public Widget {
public:
    std::vector<FontPickerItem> items;
    std::vector<size_t> filtered;
    std::wstring preview=L"Aa 0123";
    float size=14,offset=0;
    int highlight=-1,pressed=-1;
    size_t painted=0;
    std::function<void(int)> activate;
    void paint(Canvas&) override;
    bool onMouseDown(const MouseEvent&) override;
    bool onMouseUp(const MouseEvent&) override;
    bool onMouseWheel(const MouseWheelEvent&) override;
    bool onKeyDown(const KeyEvent&) override;
    bool isFocusable() const override {return interactive();}
    AccessibilityInfo accessibilityInfo() const override;
    void move(int);
    void refresh();
protected:
    bool hasInteractionState() const override{return pressed>=0;}
    void resetInteractionState() override{pressed=-1;}
private:
    int hit(Point) const;
};
struct FontPicker::State {
    FontPickerConfig config;
    std::shared_ptr<Button> selected=std::make_shared<Button>(L"Select font");
    std::shared_ptr<TextField> search=std::make_shared<TextField>(L"Search fonts");
    std::shared_ptr<Label> status=std::make_shared<Label>();
    std::shared_ptr<Button> retry=std::make_shared<Button>(L"Retry");
    std::shared_ptr<FontPickerRows> rows=std::make_shared<FontPickerRows>();
    std::wstring rememberedLabel;
    bool open=false;
    std::function<void(std::string)> selectionRequested;
    std::function<void()> retryRequested;
};
} // namespace oneui
