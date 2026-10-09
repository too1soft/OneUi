#include "../drawing_preview.hpp"
#include "page_factories.hpp"
#include <oneui/controls/icon_view.h>
#include <oneui/controls/image_view.h>

namespace lab {
using namespace oneui;
namespace {
std::vector<std::uint8_t> pixels(int width, int height, int phase = 0) {
    std::vector<std::uint8_t> result(std::size_t(width) * height * 4);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            const auto at = (y * width + x) * 4;
            const bool check = ((x + phase) / 32 + y / 32) % 2;
            result[at] = check ? 196 : 48;
            result[at + 1] = check ? 237 : 98;
            result[at + 2] = check ? 135 : 78;
            result[at + 3] = 255;
        }
    return result;
}
class ImagePage final : public ContentPage {
    std::shared_ptr<ImageView> image_ = std::make_shared<ImageView>();
    std::shared_ptr<IconView> icon_ = std::make_shared<IconView>();
    std::shared_ptr<DrawingPreview> drawing_ = std::make_shared<DrawingPreview>();
    std::shared_ptr<Label> result_;

    void pair(const std::shared_ptr<Widget> &left, const std::shared_ptr<Widget> &right) {
        auto row = std::make_shared<Stack>(StackDirection::Row);
        row->setEngine(StackEngine::Yoga);
        row->setGap(12);
        for (const auto &item : {left, right}) {
            row->add(item);
            row->setFlex(item, {1, 1, 0});
        }
        add(row);
    }

  public:
    ImagePage() {
        text(L"ImageView · RGBA 像素、缩放与对齐", 18);
        auto image = image_;
        image->setAccessibleName(L"图片预览");
        const auto data = pixels(320, 180);
        image->setRgbaPixels(data.data(), data.size(), 320, 180, 1280);
        image->setCornerRadius(12);
        add(image, 200);
        result_ = text(L"已复制 320 × 180 RGBA 像素；原始缓冲区可立即释放。");
        result_->setTextWrapping(true);
        body_->setFlex(result_, {0, 0, 48, 48});
        auto mode = std::make_shared<Select>();
        mode->setAccessibleName(L"图片缩放模式");
        mode->setItems({L"缩放：完整显示", L"缩放：裁剪填满", L"缩放：拉伸", L"缩放：实际尺寸"});
        mode->setSelectedIndex(0);
        mode->setOnChanged([image](int index) { image->setContentMode(ImageContentMode(index)); });
        add(mode);
        auto horizontal = std::make_shared<Select>(), vertical = std::make_shared<Select>();
        horizontal->setAccessibleName(L"图片水平对齐");
        vertical->setAccessibleName(L"图片垂直对齐");
        horizontal->setItems({L"水平：起点", L"水平：居中", L"水平：终点"});
        vertical->setItems({L"垂直：起点", L"垂直：居中", L"垂直：终点"});
        horizontal->setSelectedIndex(1);
        vertical->setSelectedIndex(1);
        horizontal->setOnChanged([image](int index) {
            image->setContentAlignment(ImageContentAlignment(index),
                                       image->verticalContentAlignment());
        });
        vertical->setOnChanged([image](int index) {
            image->setContentAlignment(image->horizontalContentAlignment(),
                                       ImageContentAlignment(index));
        });
        pair(horizontal, vertical);
        text(L"圆角半径", 14);
        auto radius = std::make_shared<Slider>();
        radius->setAccessibleName(L"图片圆角半径");
        radius->setRange(0, 60);
        radius->setValue(12);
        radius->setOnChanged([image](double value) { image->setCornerRadius(float(value)); });
        add(radius);
        auto replace = std::make_shared<Button>(L"更新本地图片");
        replace->setOnClick([image, result = result_, phase = 0]() mutable {
            const auto next = pixels(320, 180, ++phase * 9);
            const bool accepted = image->setRgbaPixels(next.data(), next.size(), 320, 180, 1280);
            result->setText(accepted ? L"本地像素已更新；缩放、对齐与圆角保留。" : L"像素更新失败");
        });
        auto clear = std::make_shared<Button>(L"清空图片");
        clear->setOnClick([image, result = result_] {
            image->clearImage();
            result->setText(L"图片已清空。点击“更新本地图片”恢复预览。");
        });
        pair(replace, clear);
        text(L"IconView · 共享图标与描边", 18);
        icon_->setAccessibleName(L"共享图标预览");
        add(icon_, 72);
        auto symbol = std::make_shared<Select>();
        symbol->setAccessibleName(L"共享图标选择");
        symbol->setItems({L"远程协助", L"设备", L"工具箱", L"设置"});
        symbol->setSelectedIndex(0);
        symbol->setOnChanged([icon = icon_](int index) {
            const IconSymbol values[] = {IconSymbol::RemoteAssist, IconSymbol::Device,
                                         IconSymbol::Toolbox, IconSymbol::Settings};
            if (index >= 0 && index < 4)
                icon->setSymbol(values[index]);
        });
        auto stroke = std::make_shared<Slider>();
        stroke->setAccessibleName(L"图标描边宽度");
        stroke->setRange(1, 4);
        stroke->setValue(1.5);
        stroke->setOnChanged([icon = icon_](double value) { icon->setStrokeWidth(float(value)); });
        pair(symbol, stroke);
        text(L"Canvas · 后端共享的静态绘制", 18);
        add(drawing_, 230);
        auto curve = std::make_shared<Slider>(), opacity = std::make_shared<Slider>();
        curve->setAccessibleName(L"曲线弯曲程度");
        opacity->setAccessibleName(L"绘制透明度");
        curve->setRange(0, 1);
        curve->setValue(.5);
        opacity->setRange(0, 1);
        opacity->setValue(1);
        curve->setOnChanged(
            [drawing = drawing_](double value) { drawing->setCurve(float(value)); });
        opacity->setOnChanged(
            [drawing = drawing_](double value) { drawing->setOpacity(float(value)); });
        text(L"曲线弯曲程度 / 透明度", 14);
        pair(curve, opacity);
        auto radial = std::make_shared<Checkbox>(L"径向渐变");
        auto clip = std::make_shared<Checkbox>(L"裁剪到内框");
        clip->setChecked(true);
        radial->setOnChanged([drawing = drawing_](bool value) { drawing->setRadial(value); });
        clip->setOnChanged([drawing = drawing_](bool value) { drawing->setClipped(value); });
        pair(radial, clip);
        auto note =
            text(L"所有图像均由本地生成。参数改变时绘制；空闲和离页时无动画、采样或网络任务。");
        note->setTextWrapping(true);
        body_->setFlex(note, {0, 0, 48, 48});
    }
    void setTheme(bool dark, bool compact) override {
        ContentPage::setTheme(dark, compact);
        image_->setBackground(dark ? Color{16, 21, 23} : Color{231, 238, 232});
        icon_->setColor(dark ? Color{229, 236, 232} : Color{28, 43, 35});
        icon_->setAccent(dark ? Color{196, 237, 135} : Color{40, 97, 61});
        drawing_->setDark(dark);
    }
};
} // namespace
std::unique_ptr<DemoPage> createMediaPage(const PageDescriptor &, Window &) {
    return std::make_unique<ImagePage>();
}
} // namespace lab
