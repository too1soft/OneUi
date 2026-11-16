#pragma once
#include "oneui/controls/icon_view.h"
#include "oneui/mobile/presentation.h"

namespace oneui::mobile {
inline std::shared_ptr<Stack> profileSummary(std::wstring name, std::wstring detail,
                                             const ControlPalette &palette, float scale, float width) {
    auto row = std::make_shared<Stack>(StackDirection::Row);
    row->setAlign(StackAlign::Center); row->setGap(16); row->setPadding({8, 0, 8, 0});
    auto glyph = std::make_shared<IconView>(IconSymbol::User);
    glyph->setGlyphSize(32); glyph->setColor(palette.tint);
    auto avatar = std::make_shared<Panel>();
    avatar->setContent(glyph); avatar->setBackground(palette.surface);
    avatar->setBorderWidth(0); avatar->setRadius(32); avatar->setPreferredSize({64, 64});
    row->add(avatar);
    auto text = std::make_shared<Stack>(); text->setGap(4);
    const auto available = std::max(1.f, width - 80);
    auto heading = pageText(std::move(name), 24 * scale, palette.ink, 650, available);
    heading->setLineHeight(29.28f * scale);
    heading->setPreferredSize({0, std::ceil(heading->measure({available, INFINITY}).height)});
    text->add(heading);
    text->add(pageText(std::move(detail), 16 * scale, palette.secondary, 400, available));
    text->setPreferredSize({0, text->contentHeight()});
    row->add(text); StackFlex fill; fill.grow = 1; row->setFlex(text, fill);
    row->setPreferredSize({0, std::max(64.f, text->contentHeight()) + 16});
    return row;
}
} // namespace oneui::mobile
