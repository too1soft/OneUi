#pragma once
#include <oneui/mobile/presentation.h>
#include <oneui/controls/progress_bar.h>

namespace oneui::mobile {
struct TransferRow {
    std::shared_ptr<Stack> view;
    std::shared_ptr<Label> detail;
    std::shared_ptr<ProgressBar> progress;
};
// A compact progress group with stable status height. Product queue and retry
// policies stay with the consumer; updates retain the current native widgets.
inline TransferRow transferRow(std::wstring title, std::wstring direction, std::wstring status,
    std::optional<double> fraction, ControlPalette palette, float scale, float width) {
    TransferRow result;
    result.view = std::make_shared<Stack>(); result.view->setGap(8);
    result.view->add(pageText(std::move(title), 17 * scale, palette.ink, 600, width));
    result.view->add(pageText(std::move(direction), 12 * scale, palette.secondary, 400, width));
    result.detail = pageText(std::move(status), 13 * scale, palette.secondary, 400, width);
    result.detail->setPreferredSize({0, std::max(result.detail->preferredSize().height, 39 * scale)});
    result.view->add(result.detail);
    if (fraction) {
        result.progress = std::make_shared<ProgressBar>();
        result.progress->setAccessibleName(L"文件传输进度");
        ProgressBarStyleOverride style; style.fill = palette.tint; style.trackBackground = palette.separator;
        result.progress->setStyleOverride(style); result.progress->setPreferredSize({0, 4});
        result.progress->setIndeterminate(*fraction < 0); result.progress->setValue(std::max(0.0, *fraction));
        result.view->add(result.progress);
    }
    result.view->setPreferredSize({0, result.view->contentHeight()});
    return result;
}
}
