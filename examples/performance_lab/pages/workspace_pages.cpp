#include "page_factories.hpp"

namespace lab {
using namespace oneui;
namespace {
class WorkspacePage final : public ContentPage {
    std::shared_ptr<SplitView> split_ = std::make_shared<SplitView>();
    std::shared_ptr<Panel> editor_ = std::make_shared<Panel>();
    std::vector<std::shared_ptr<TextField>> documents_;

  public:
    WorkspacePage() {
        text(L"工作区 · 保留文档、选择和分栏位置", 18);
        auto tabs = std::make_shared<Tabs>();
        tabs->setItems({L"说明", L"草稿", L"配置"});
        tabs->setDocumentMode(true);
        tabs->setReorderEnabled(false);
        add(tabs);
        for (auto title :
             {L"在这里输入说明。", L"草稿不会因切换标签丢失。", L"theme = green\nlayout = split"}) {
            auto text = std::make_shared<TextField>();
            text->setMultiline(true);
            text->setText(title);
            text->setClipboard(std::make_shared<SystemClipboard>());
            documents_.push_back(text);
        }
        editor_->setContent(documents_.front());
        tabs->setOnChanged([this](int i) {
            if (i >= 0 && i < int(documents_.size()))
                editor_->setContent(documents_[i]);
        });
        auto outline = std::make_shared<TreeView>();
        outline->setItems({{L"workspace", L"", L"本地工作区", L"", true},
                           {L"docs", L"workspace", L"文档", L"", true},
                           {L"settings", L"workspace", L"配置", L"", true}});
        split_->setFirst(outline);
        split_->setSecond(editor_);
        split_->setSplitRatio(.25f);
        split_->setResizable(true);
        split_->setGap(8);
        split_->setMinimumPaneExtent(120, 240);
        add(split_, 400);
        auto result = text(L"拖动分割线，或用键盘调整。");
        split_->setOnSplitRatioCommitted(
            [result](float value) { result->setText(L"分栏比例：" + std::to_wstring(value)); });
        action(L"切换分栏方向", [this] {
            split_->setOrientation(split_->orientation() == SplitOrientation::Horizontal
                                       ? SplitOrientation::Vertical
                                       : SplitOrientation::Horizontal);
        });
    }
};
} // namespace
std::unique_ptr<DemoPage> createWorkspacePage(const PageDescriptor &, Window &) {
    return std::make_unique<WorkspacePage>();
}
} // namespace lab
