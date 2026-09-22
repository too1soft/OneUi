#include "oneui/controls/terminal_view.h"
#include "platform/shared/skia_canvas.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkSurface.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

// Real pixels, not a recording canvas: repainting an unchanged suffix must
// reproduce the same glyph positions as the original complete ASCII run.
// Otherwise cursor blinking and small updates leave shifted/repeated text.
static bool repaintMatches(const std::wstring& family, float scale, bool mixed) {
    constexpr int width = 1000, height = 80;
    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(
        static_cast<int>(width * scale), static_cast<int>(height * scale)));
    if (!surface) throw std::runtime_error("raster surface unavailable");
    surface->getCanvas()->scale(scale, scale);
    auto canvas = oneui::rendering::makeSkiaCanvas(*surface->getCanvas());
    oneui::TerminalView terminal;
    terminal.setFrame({0, 0, width, height});
    terminal.setFontFamily(family);
    terminal.setFontSize(13);
    terminal.setCursor({0, 0, false});
    terminal.setCursorBlinking(false);
    std::vector<oneui::TerminalCell> cells(100);
    const std::wstring sample = L"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ:/[]";
    for (std::size_t i = 0; i < cells.size(); ++i) {
        cells[i].text.assign(1, sample[i % sample.size()]);
        if (mixed) {
            if (i % 11 < 4) cells[i].style |= oneui::TerminalCellBold;
            if (i % 17 == 0) cells[i].foreground = {30, 180, 220, 255};
        }
    }
    if (mixed) {
        for (const auto column : {10, 35, 67}) {
            cells[column].text = L"\u4e2d";
            cells[column].style |= oneui::TerminalCellWide;
            cells[column + 1].text.clear();
            cells[column + 1].style = oneui::TerminalCellWideContinuation;
        }
        cells[55].text = L"e\u0301";
    }
    terminal.setGrid(1, 100, std::move(cells));
    terminal.paint(*canvas);
    SkPixmap pixels;
    if (!surface->peekPixels(&pixels)) throw std::runtime_error("pixels unavailable");
    std::vector<SkColor> before;
    for (int y = 0; y < pixels.height(); ++y)
        for (int x = 0; x < pixels.width(); ++x) before.push_back(pixels.getColor(x, y));

    // Clip deliberately splits a run at a non-cell boundary as a real damage
    // region does when widgets or the blinking cursor invalidate part of it.
    canvas->save();
    canvas->clipRect({227, 0, 463, height});
    terminal.paint(*canvas);
    canvas->restore();
    int changed = 0;
    std::size_t index = 0;
    for (int y = 0; y < pixels.height(); ++y) for (int x = 0; x < pixels.width(); ++x, ++index) {
        const auto a = before[index], b = pixels.getColor(x, y);
        if (std::abs(static_cast<int>(SkColorGetR(a)) - static_cast<int>(SkColorGetR(b))) > 8 ||
            std::abs(static_cast<int>(SkColorGetG(a)) - static_cast<int>(SkColorGetG(b))) > 8 ||
            std::abs(static_cast<int>(SkColorGetB(a)) - static_cast<int>(SkColorGetB(b))) > 8) ++changed;
    }
    std::cout << "family-length=" << family.size() << " scale=" << scale << " mixed=" << mixed
              << " changed-pixels=" << changed << '\n';
    return changed <= 8;
}

static void zeroBorderDoesNotPaint() {
    for (const float scale : {1.0f, 1.5f, 2.0f}) {
        auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(100, 100));
        surface->getCanvas()->clear(SK_ColorWHITE);
        surface->getCanvas()->scale(scale, scale);
        auto canvas = oneui::rendering::makeSkiaCanvas(*surface->getCanvas());
        canvas->strokeRect({5, 5, 30, 25}, {0, 0, 0, 255}, 4, 0);
        canvas->strokeRect({5, 5, 30, 25}, {0, 0, 0, 255}, 4, -1);
        SkPixmap pixels;
        if (!surface->peekPixels(&pixels)) throw std::runtime_error("pixels unavailable");
        for (int y = 0; y < 100; ++y) for (int x = 0; x < 100; ++x)
            if (pixels.getColor(x, y) != SK_ColorWHITE) throw std::runtime_error("zero border painted a hairline");
        canvas->strokeRect({5, 5, 30, 25}, {0, 0, 0, 255}, 4, 1);
        bool painted = false;
        for (int y = 0; y < 100; ++y) for (int x = 0; x < 100; ++x) painted |= pixels.getColor(x, y) != SK_ColorWHITE;
        if (!painted) throw std::runtime_error("positive border disappeared");
    }
}

int main() {
    try {
        zeroBorderDoesNotPaint();
        bool passed = true;
        for (const auto* family : {L"", L"Consolas", L"OneUI Missing Terminal Font", L"Segoe UI"})
            for (const float scale : {1.0f, 1.25f, 1.5f})
                for (const bool mixed : {false, true})
                    passed = repaintMatches(family, scale, mixed) && passed;
        if (!passed) throw std::runtime_error("partial terminal repaint changed unchanged text");
        std::cout << "Terminal raster tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
