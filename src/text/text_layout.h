#pragma once

#include "oneui/canvas.h"
#include "oneui/text.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class SkCanvas;
class SkFontMgr;

namespace oneui::text {

// All offsets here are native wide-string offsets. Only this adapter translates
// SkParagraph's mixed UTF-8/UTF-16 APIs. No Skia types enter the public API.
struct Position {
    std::size_t offset = 0;
    TextAffinity affinity = TextAffinity::Downstream;
};

struct LayoutOptions {
    TextOptions text;
    std::wstring family;
    TextFontFamily fallbackFamily = TextFontFamily::Default;
    float size = 14.0f;
    int weight = 400;
    float width = 1000000.0f;
    float lineHeight = 0.0f;
    float scale = 1.0f;
    TextAlign align = TextAlign::Left;
    std::size_t maxLines = 0;
    bool ellipsis = false;
    bool sensitive = false;
};

struct Line {
    std::size_t start = 0;
    std::size_t end = 0;
    float top = 0;
    float height = 0;
    float baseline = 0;
    float width = 0;
};

struct LayoutStats {
    std::uint64_t layouts = 0;
    std::uint64_t cacheHits = 0;
    std::size_t cachedEntries = 0;
    std::size_t cachedTextBytes = 0;
};

class Layout {
public:
    ~Layout();
    Layout(const Layout&) = delete;
    Layout& operator=(const Layout&) = delete;
    static std::shared_ptr<Layout> make(const std::wstring& value, const LayoutOptions& options = {});
    float width() const;
    float height() const;
    float baseline() const;
    int unresolvedGlyphs() const;
    const std::vector<Line>& lines() const;
    const std::vector<std::size_t>& graphemes() const;
    std::size_t previous(std::size_t offset) const;
    std::size_t next(std::size_t offset) const;
    std::size_t floor(std::size_t offset) const;
    std::size_t ceil(std::size_t offset) const;
    std::size_t utf8Offset(std::size_t wideOffset) const;
    std::size_t wideOffset(std::size_t utf8Offset) const;
    Position hitTest(Point point) const;
    Rect caret(Position position) const;
    Position moveVisual(Position position, int direction) const;
    Position moveVertical(Position position, int direction, float targetX) const;
    std::vector<Rect> selection(std::size_t start, std::size_t end) const;
    std::pair<std::size_t, std::size_t> word(std::size_t offset) const;
    void paint(SkCanvas& canvas, Point origin, Color color) const;
    static LayoutStats stats();
    static void clearCache();
    // Private test hook: pinned font files, no installed-font dependency/fallback.
    static void installTestFonts(const std::vector<std::string>& paths);

private:
    void ensureStops() const;
    struct Impl;
    explicit Layout(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

} // namespace oneui::text
