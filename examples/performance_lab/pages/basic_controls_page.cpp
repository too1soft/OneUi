#include "page_factories.hpp"

namespace lab {
using namespace oneui;
class ControlsPage final : public ContentPage {
    int clicks_ = 0;

  public:
    ControlsPage() {
        auto status = text(L"点击、切换或使用键盘，结果显示在这里。");
        auto primary = action(L"执行操作", [this, status] {
            status->setText(L"按钮已触发 " + std::to_wstring(++clicks_) + L" 次");
        });
        primary->setVariant(ButtonVariant::Primary);
        auto disabled = action(L"禁用按钮", [] {});
        disabled->setDisabled(true);
        auto checkbox = std::make_shared<Checkbox>(L"允许执行操作");
        checkbox->setChecked(true);
        checkbox->setOnChanged([primary, status](bool checked) {
            primary->setDisabled(!checked);
            status->setText(checked ? L"操作已启用" : L"操作已禁用");
        });
        add(checkbox);
        auto toggle = std::make_shared<Switch>(L"模拟选项");
        toggle->setOnChanged(
            [status](bool enabled) { status->setText(enabled ? L"开关：开" : L"开关：关"); });
        add(toggle);
        auto radio = std::make_shared<RadioGroup>();
        radio->setItems({L"自动", L"手动", L"只读"});
        radio->setOrientation(RadioGroup::Orientation::Horizontal);
        radio->setOnChanged(
            [status](int index) { status->setText(L"单选索引：" + std::to_wstring(index)); });
        add(radio);
        auto select = std::make_shared<Select>();
        select->setItems({L"选项一", L"选项二", L"选项三"});
        select->setOnChanged(
            [status](int index) { status->setText(L"选择索引：" + std::to_wstring(index)); });
        add(select);
        auto progress = std::make_shared<ProgressBar>();
        progress->setValue(.35f);
        add(progress, 16);
        auto slider = std::make_shared<Slider>();
        slider->setRange(0, 100);
        slider->setValue(35);
        slider->setOnChanged([progress, status](double value) {
            progress->setValue(float(value / 100));
            status->setText(L"进度：" + std::to_wstring(int(value)) + L"%");
        });
        add(slider);
    }
};
std::unique_ptr<DemoPage> createBasicControlsPage() { return std::make_unique<ControlsPage>(); }
} // namespace lab
