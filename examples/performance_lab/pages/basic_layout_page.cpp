#include "page_factories.hpp"
#include <oneui/layout/grid.h>
#include <oneui/layout/wrap.h>

namespace lab {
using namespace oneui;
class LayoutPage final : public ContentPage {
    std::shared_ptr<Stack> stack_ = std::make_shared<Stack>(StackDirection::Row);
    std::shared_ptr<Grid> grid_ = std::make_shared<Grid>(3);
    bool column_ = false;
    float gap_ = 12;
    int columns_ = 3;
    std::shared_ptr<Label> result_;
    void resizeExamples() {
        const float stackHeight = column_ ? 250 + 2 * gap_ : 210;
        body_->setFlex(stack_, {0, 0, stackHeight, stackHeight});
        const int rows = (6 + columns_ - 1) / columns_;
        const float gridHeight = rows * 62.f + (rows - 1) * gap_;
        // Yoga's unconstrained content measurement needs the minimum as well as
        // flex-basis. A basis alone lets this non-Yoga grid measure as zero.
        body_->setFlex(grid_, {0, 0, gridHeight, gridHeight});
    }
    void feedback() {
        result_->setText(std::wstring(L"方向：") + (column_ ? L"纵向" : L"横向") +
            L" · 间距：" + std::to_wstring(int(gap_)) + L" · 列数：" + std::to_wstring(columns_));
    }
    std::shared_ptr<Panel> block(const wchar_t *title, Color color) {
        auto panel = std::make_shared<Panel>();
        auto label = std::make_shared<Label>(title);
        label->setColor({20, 35, 27});
        panel->setContent(label);
        panel->setBackground(color);
        panel->setPadding(Insets{16});
        panel->setPreferredSize({130, 62});
        return panel;
    }

  public:
    LayoutPage() {
        result_ = text(L"方向：横向 · 间距：12 · 列数：3");
        text(L"Stack · 主轴与空间分配", 18);
        stack_->setEngine(StackEngine::Yoga);
        stack_->setGap(12);
        for (auto title : {L"固定 130", L"弹性 1", L"弹性 2"})
            stack_->add(block(title, {196, 237, 135}));
        stack_->setFlex(stack_->children()[0], {0, 0, 130, 60});
        stack_->setFlex(stack_->children()[1], {1, 1, 100, 60});
        stack_->setFlex(stack_->children()[2], {2, 1, 100, 60});
        add(stack_, 210);
        action(L"切换横向 / 纵向", [this] {
            column_ = !column_;
            stack_->setDirection(column_ ? StackDirection::Column : StackDirection::Row);
            resizeExamples();
            feedback();
        });
        auto gap = std::make_shared<Slider>();
        gap->setRange(0, 40);
        gap->setValue(12);
        gap->setOnChanged([this](double value) {
            gap_ = float(value);
            stack_->setGap(float(value));
            grid_->setGap(float(value));
            resizeExamples();
            feedback();
        });
        add(gap);
        text(L"Grid · 固定列网格", 18);
        grid_->setGap(12);
        grid_->setAutoRows(62);
        for (auto title : {L"网格 A", L"网格 B", L"网格 C", L"网格 D", L"网格 E", L"网格 F"})
            grid_->add(block(title, {97, 201, 187}));
        add(grid_, 150);
        auto columns = std::make_shared<Slider>();
        columns->setRange(1, 4);
        columns->setStep(1);
        columns->setValue(3);
        columns->setOnChanged([this](double value) {
            columns_ = int(value);
            grid_->setColumns(int(value));
            resizeExamples();
            feedback();
        });
        add(columns);
        text(L"Wrap · 缩窄窗口查看自动换行", 18);
        auto wrap = std::make_shared<Wrap>();
        wrap->setGap(12);
        wrap->setRowGap(12);
        for (auto title : {L"Panel", L"Stack", L"Grid", L"Wrap", L"ScrollView"})
            wrap->add(block(title, {236, 173, 114}));
        add(wrap, 220);
    }
};
std::unique_ptr<DemoPage> createBasicLayoutPage() { return std::make_unique<LayoutPage>(); }
} // namespace lab
