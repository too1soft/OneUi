#include "internal/frame_profile.h"
#include "text_layout.h"
#include "text_layout_cache.h"
#include "text_index.h"
#include "font_family_list.h"
#include "foreground_painter.h"
#include "internal/unicode.h"
#include "platform/shared/skia_canvas.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkData.h"
#include "include/core/SkFontMgr.h"
#include "modules/skparagraph/include/FontCollection.h"
#include "modules/skparagraph/include/Paragraph.h"
#include "modules/skparagraph/include/ParagraphBuilder.h"
#include "modules/skparagraph/include/TypefaceFontProvider.h"
#include "modules/skunicode/include/SkUnicode_icu.h"
#include "unicode/ubidi.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#ifdef ONEUI_EMBEDDED_ICU
bool SkLoadICU();
#endif

namespace oneui::text {
namespace p = skia::textlayout;
namespace {
thread_local detail::LayoutCache cache;
thread_local LayoutStats counters;
thread_local sk_sp<p::FontCollection> testFonts;
thread_local std::vector<SkString> testFontFamilies;

sk_sp<SkUnicode> unicodeEngine() {
#ifdef ONEUI_EMBEDDED_ICU
    if (!SkLoadICU()) throw std::runtime_error("OneUI could not initialize embedded Unicode data");
#endif
    thread_local auto engine = SkUnicodes::ICU::Make();
    if (!engine) throw std::runtime_error("OneUI could not initialize its Unicode text engine");
    return engine;
}

const char* defaultFamily(TextFontFamily family) {
#ifdef _WIN32
    return family == TextFontFamily::Monospace ? "Consolas" : "Segoe UI";
#elif defined(__APPLE__)
    return family == TextFontFamily::Monospace ? "Menlo" : "Helvetica Neue";
#else
    return family == TextFontFamily::Monospace ? "monospace" : "sans-serif";
#endif
}

sk_sp<p::FontCollection> fonts() {
    if (testFonts) return testFonts;
    thread_local auto collection = [] {
        auto result = sk_make_sp<p::FontCollection>();
        result->setAssetFontManager(rendering::makeEmbeddedFontManager());
        result->setDefaultFontManager(rendering::makePlatformFontManager(), defaultFamily(TextFontFamily::Default));
        result->enableFontFallback();
        // OneUI owns the bounded cache. In particular SkParagraph must never
        // retain password layouts in its independent process-lifetime cache.
        result->getParagraphCache()->turnOn(false);
        return result;
    }();
    return collection;
}

Rect rect(const SkRect& r) { return {r.x(), r.y(), r.width(), r.height()}; }
}

struct Layout::Impl {
    struct Part { std::size_t start; std::size_t end; float y; std::shared_ptr<Layout> layout; };
    struct Stop { Position position; Rect bounds; };
    LayoutOptions options;
    detail::TextIndex index;
    mutable std::vector<std::size_t> boundaries;
    std::size_t lineCount = 0;
    mutable std::vector<Line> lines;
    std::unique_ptr<p::Paragraph> paragraph;
    std::vector<Part> parts;
    std::vector<SkColor> inlineForegrounds;
    mutable std::vector<Stop> stops;
    mutable bool stopsReady = false;
    float width = 0, height = 0, baseline = 0;

    explicit Impl(const std::wstring& value, LayoutOptions opts)
        : options(std::move(opts)), index(value) {}

    const std::vector<std::size_t>& graphemes() const {
        // Measuring and drawing use the shaped paragraph. Editing and
        // truncation request the native grapheme offsets on first use.
        if (!boundaries.empty()) return boundaries;
        std::vector<std::size_t> resolved;
        auto breaker = unicodeEngine()->makeBreakIterator(options.text.locale.c_str(), SkUnicode::BreakType::kGraphemes);
        if (!breaker || !breaker->setText(index.utf8.data(), static_cast<int>(index.utf8.size())))
            throw std::runtime_error("OneUI could not create a grapheme iterator");
        for (auto offset = breaker->first(); !breaker->isDone(); offset = breaker->next())
            resolved.push_back(index.utf8ToWide.at(static_cast<std::size_t>(offset)));
        if (resolved.empty() || resolved.front() != 0) resolved.insert(resolved.begin(), 0);
        const auto end = index.utf8ToWide.back();
        if (resolved.back() != end) resolved.push_back(end);
        boundaries = std::move(resolved);
        return boundaries;
    }

    void buildParagraph() {
        internal::FrameSpan buildSpan(internal::FrameStage::TextBuild);
        p::TextStyle style;
        const std::string family = options.family.empty() ? defaultFamily(options.fallbackFamily) : unicode::toUtf8(options.family);
        std::vector<SkString> families;
        for (const auto& candidate : fontFamilyList(family)) families.emplace_back(candidate.c_str());
        if (families.empty()) families.emplace_back(defaultFamily(options.fallbackFamily));
        style.setFontFamilies(families);
        if (testFonts && options.family.empty()) style.setFontFamilies(testFontFamilies);
        style.setFontSize(options.size);
        style.setLetterSpacing(options.text.letterSpacing);
        if (options.text.tabularNumbers) style.addFontFeature(SkString("tnum"), 1);
        style.setFontStyle(SkFontStyle(options.weight, SkFontStyle::kNormal_Width, SkFontStyle::kUpright_Slant));
        style.setLocale(SkString(options.text.locale.c_str()));
        if (options.lineHeight > 0) { style.setHeight(options.lineHeight / options.size); style.setHeightOverride(true); }
        p::ParagraphStyle paragraphStyle;
        paragraphStyle.setTextStyle(style);
        p::StrutStyle strut;
        strut.setStrutEnabled(true);
        strut.setFontFamilies(style.getFontFamilies());
        strut.setFontSize(options.size);
        strut.setFontStyle(style.getFontStyle());
        if (options.lineHeight > 0) {
            strut.setHeight(options.lineHeight / options.size);
            strut.setHeightOverride(true);
        }
        paragraphStyle.setStrutStyle(strut);
        bool rtl = options.text.direction == TextDirection::RTL;
        if (options.text.direction == TextDirection::Auto) {
            internal::FrameSpan bidiSpan(internal::FrameStage::TextBidi);
            UErrorCode error = U_ZERO_ERROR;
            auto* bidi = ubidi_open();
            ubidi_setPara(bidi, reinterpret_cast<const UChar*>(index.utf16.data()), static_cast<int32_t>(index.utf16.size()), UBIDI_DEFAULT_LTR, nullptr, &error);
            rtl = (ubidi_getParaLevel(bidi) & 1) != 0;
            ubidi_close(bidi);
            if (U_FAILURE(error)) throw std::runtime_error("OneUI could not resolve paragraph direction");
        }
        paragraphStyle.setTextDirection(rtl ? p::TextDirection::kRtl : p::TextDirection::kLtr);
        paragraphStyle.setTextAlign(options.align == TextAlign::Center ? p::TextAlign::kCenter :
                                    options.align == TextAlign::Right ? p::TextAlign::kRight : p::TextAlign::kLeft);
        if (options.maxLines) paragraphStyle.setMaxLines(options.maxLines);
        if (options.ellipsis) paragraphStyle.setEllipsis(u"\u2026");
        auto builder = p::ParagraphBuilder::make(paragraphStyle, fonts(), unicodeEngine());
        const auto addTextRange = [&](std::size_t start, std::size_t end) {
            if (end <= start) return;
            const auto byteStart = index.wideTo8.at(start);
            const auto byteEnd = index.wideTo8.at(end);
            builder->addText(index.utf8.data() + byteStart, byteEnd - byteStart);
        };
        std::size_t cursor = 0;
        for (const auto& span : options.spans) {
            addTextRange(cursor, span.start);
            auto spanStyle = style;
            if (span.fontSize > 0.0f) spanStyle.setFontSize(span.fontSize);
            const auto spanWeight = span.fontWeight > 0 ? span.fontWeight : options.weight;
            spanStyle.setFontStyle(SkFontStyle(
                spanWeight,
                SkFontStyle::kNormal_Width,
                span.italic ? SkFontStyle::kItalic_Slant : SkFontStyle::kUpright_Slant));
            if (span.monospace) {
                if (testFonts) spanStyle.setFontFamilies({testFontFamilies.front()});
                else spanStyle.setFontFamilies({SkString(defaultFamily(TextFontFamily::Monospace))});
            }
            if (span.foreground.a > 0) {
                const auto foreground = SkColorSetARGB(
                    span.foreground.a, span.foreground.r, span.foreground.g, span.foreground.b);
                inlineForegrounds.push_back(foreground);
                spanStyle.setForegroundPaintID(static_cast<int>(inlineForegrounds.size()));
            }
            if (span.background.a > 0) {
                SkPaint paint;
                paint.setAntiAlias(true);
                paint.setColor(SkColorSetARGB(
                    span.background.a, span.background.r, span.background.g, span.background.b));
                spanStyle.setBackgroundPaint(paint);
            }
            int decorations = p::TextDecoration::kNoDecoration;
            if (span.underline) decorations |= p::TextDecoration::kUnderline;
            if (span.strikethrough) decorations |= p::TextDecoration::kLineThrough;
            if (decorations != p::TextDecoration::kNoDecoration) {
                spanStyle.setDecoration(static_cast<p::TextDecoration>(decorations));
                const auto decoration = span.foreground.a > 0
                    ? SkColorSetARGB(span.foreground.a, span.foreground.r, span.foreground.g, span.foreground.b)
                    : SkColorSetARGB(210, 148, 163, 184);
                spanStyle.setDecorationColor(decoration);
            }
            builder->pushStyle(spanStyle);
            addTextRange(span.start, span.end);
            builder->pop();
            cursor = span.end;
        }
        addTextRange(cursor, index.wideTo8.size() - 1);
        paragraph = builder->Build();
    }

    void shape() {
        buildParagraph();
        internal::FrameSpan shapeSpan(internal::FrameStage::TextShape);
        const float layoutWidth = options.text.wrap == TextWrapMode::NoWrap && !options.ellipsis ? 1000000.0f : options.width;
        paragraph->layout(layoutWidth);
        // No-wrap text aligns within its natural width; callers place the block.
        if (layoutWidth != options.width && options.align != TextAlign::Left) {
            paragraph->layout(std::ceil(paragraph->getMaxIntrinsicWidth()) + 1.0f);
        }
        width = std::max(0.0f, paragraph->getLongestLine());
        height = paragraph->getHeight();
        baseline = paragraph->getAlphabeticBaseline();
        lineCount = std::max(std::size_t{1}, paragraph->lineNumber());
        ++counters.layouts;
    }

    const std::vector<Line>& lineGeometry() const {
        if (!lines.empty()) return lines;
        // SkParagraph's metrics walk glyph styles on every line. Measuring
        // and painting labels only need the shaped dimensions and line count.
        std::vector<Line> resolved;
        resolved.reserve(lineCount);
        if (paragraph) {
            std::vector<p::LineMetrics> metrics;
            paragraph->getLineMetrics(metrics);
            for (const auto& line : metrics) {
                resolved.push_back({index.utf8ToWide.at(std::min(line.fStartIndex, index.utf8.size())),
                                    index.utf8ToWide.at(std::min(line.fEndIndex, index.utf8.size())),
                                    static_cast<float>(line.fBaseline - line.fAscent),
                                    static_cast<float>(line.fAscent + line.fDescent),
                                    static_cast<float>(line.fBaseline), static_cast<float>(line.fWidth)});
            }
            if (resolved.empty()) resolved.push_back({0, 0, 0, height, baseline, 0});
        } else {
            for (const auto& part : parts) {
                for (auto line : part.layout->lines()) {
                    line.start += part.start; line.end += part.start;
                    line.top += part.y; line.baseline += part.y;
                    resolved.push_back(line);
                }
            }
        }
        lines = std::move(resolved);
        return lines;
    }
};

Layout::Layout(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
Layout::~Layout() = default;

std::shared_ptr<Layout> Layout::make(const std::wstring& value, const LayoutOptions& requested) {
    internal::FrameSpan span(internal::FrameStage::TextLayout);
    LayoutOptions options = requested;
    if (!std::isfinite(options.size) || options.size <= 0 || !std::isfinite(options.width) || options.width < 0 ||
        !std::isfinite(options.lineHeight) || options.lineHeight < 0 || !std::isfinite(options.scale) || options.scale <= 0 || !std::isfinite(options.text.letterSpacing))
        throw std::invalid_argument("Invalid OneUI text layout metrics");
    std::size_t previousSpanEnd = 0;
    for (const auto& span : options.spans) {
        if (!std::isfinite(span.fontSize) || span.fontSize < 0.0f ||
            span.fontWeight < 0 || span.fontWeight > 1000 || span.start >= span.end ||
            span.end > value.size() || span.start < previousSpanEnd ||
            unicode::boundary(value, span.start) != span.start ||
            unicode::boundary(value, span.end) != span.end) {
            throw std::invalid_argument("Invalid OneUI text style span");
        }
        previousSpanEnd = span.end;
    }
    if (options.text.locale.empty()) options.text.locale = "und";
    if (options.text.wrap == TextWrapMode::NoWrap && !options.ellipsis) options.width = 1000000.0f;
    if (!options.sensitive) {
        internal::FrameSpan cacheSpan(internal::FrameStage::TextCache);
        if (auto result = cache.find(value, options)) {
            ++counters.cacheHits;
            return result;
        }
    }
    auto impl = [&] {
        internal::FrameSpan indexSpan(internal::FrameStage::TextIndex);
        return std::make_unique<Impl>(value, options);
    }();
    // Each hard paragraph has its own base direction and cache entry. Edits in
    // one paragraph reuse the shaped results for all unchanged paragraphs.
    if (options.spans.empty() && value.find(L'\n') != std::wstring::npos) {
        std::size_t start = 0;
        do {
            auto end = value.find(L'\n', start);
            if (end == std::wstring::npos) end = value.size();
            auto textEnd = end;
            if (textEnd > start && value[textEnd - 1] == L'\r') --textEnd;
            auto paragraphOptions = options;
            paragraphOptions.maxLines = 0;
            // Unlimited SkParagraph lines plus ellipsis collapse a paragraph to
            // one visual line. Shape full paragraphs first; only the final
            // visible paragraph may truncate to the remaining line budget.
            if (options.text.wrap == TextWrapMode::WordWrap) paragraphOptions.ellipsis = false;
            auto part = make(value.substr(start, textEnd - start), paragraphOptions);
            bool lastVisible = false;
            if (options.maxLines) {
                const auto remaining = options.maxLines - impl->lineCount;
                if (part->lineCount() >= remaining) {
                    // Only the final visible paragraph needs the hidden tail to
                    // let SkParagraph place its ellipsis. Earlier hard paragraphs
                    // retain independent Auto directions and cache identities.
                    paragraphOptions.maxLines = remaining;
                    paragraphOptions.ellipsis = options.ellipsis;
                    auto limited = std::make_unique<Impl>(value.substr(start), paragraphOptions);
                    limited->shape();
                    part = std::shared_ptr<Layout>(new Layout(std::move(limited)));
                    lastVisible = true;
                }
            }
            impl->parts.push_back({start, end, impl->height, part});
            impl->lineCount += part->lineCount();
            if (start == 0) impl->baseline = part->baseline();
            impl->width = std::max(impl->width, part->width());
            impl->height += part->height();
            if (lastVisible || end == value.size()) break;
            start = end + 1;
        } while (start <= value.size());
    } else impl->shape();
    auto result = std::shared_ptr<Layout>(new Layout(std::move(impl)));
    // Cache leaf paragraphs only; composite entries would keep evicted children
    // alive outside the cache's accounting and duplicate whole documents.
    if (!options.sensitive && result->impl_->parts.empty())
        cache.insert(value, options, result);
    return result;
}

float Layout::width() const { return impl_->width; }
float Layout::height() const { return impl_->height; }
float Layout::baseline() const { return impl_->baseline; }
int Layout::unresolvedGlyphs() const {
    if (impl_->paragraph) return std::max(0, impl_->paragraph->unresolvedGlyphs());
    int count = 0;
    for (const auto& part : impl_->parts) count += part.layout->unresolvedGlyphs();
    return count;
}
std::size_t Layout::lineCount() const { return impl_->lineCount; }
const std::vector<Line>& Layout::lines() const { return impl_->lineGeometry(); }
const std::vector<std::size_t>& Layout::graphemes() const { return impl_->graphemes(); }
std::size_t Layout::floor(std::size_t offset) const {
    const auto& offsets = graphemes();
    auto it = std::upper_bound(offsets.begin(), offsets.end(), offset);
    return it == offsets.begin() ? 0 : *std::prev(it);
}
std::size_t Layout::ceil(std::size_t offset) const {
    const auto& offsets = graphemes();
    auto it = std::lower_bound(offsets.begin(), offsets.end(), offset);
    return it == offsets.end() ? offsets.back() : *it;
}
std::size_t Layout::previous(std::size_t offset) const { return floor(offset ? offset - 1 : 0); }
std::size_t Layout::next(std::size_t offset) const {
    return offset >= impl_->index.wideTo8.size() - 1 ? impl_->index.wideTo8.size() - 1 : ceil(offset + 1);
}
std::size_t Layout::utf8Offset(std::size_t offset) const { return impl_->index.wideTo8.at(std::min(offset, impl_->index.wideTo8.size() - 1)); }
std::size_t Layout::wideOffset(std::size_t offset) const { return impl_->index.utf8ToWide.at(std::min(offset, impl_->index.utf8.size())); }

Position Layout::hitTest(Point point) const {
    // A document click only needs the paragraph under the pointer. Do not
    // materialize caret geometry for every offscreen paragraph on first use.
    for (const auto& part : impl_->parts) {
        if (point.y < part.y + part.layout->height() || &part == &impl_->parts.back()) {
            auto position = part.layout->hitTest({point.x, point.y - part.y});
            position.offset += part.start;
            return position;
        }
    }
    ensureStops();
    Position result;
    float bestY = std::numeric_limits<float>::infinity();
    float bestX = std::numeric_limits<float>::infinity();
    for (const auto& stop : impl_->stops) {
        const auto& box = stop.bounds;
        const float dy = point.y < box.y ? box.y - point.y : std::max(0.0f, point.y - (box.y + box.height));
        const float dx = std::fabs(point.x - box.x);
        if (dy < bestY - 0.001f || (std::fabs(dy - bestY) < 0.001f && dx < bestX)) {
            result = stop.position; bestY = dy; bestX = dx;
        }
    }
    return result;
}

Rect Layout::caret(Position pos) const {
    pos.offset = floor(pos.offset);
    for (const auto& part : impl_->parts) {
        if (pos.offset <= part.end || &part == &impl_->parts.back()) {
            auto r = part.layout->caret({pos.offset >= part.start ? pos.offset - part.start : 0, pos.affinity});
            r.y += part.y; return r;
        }
    }
    if (impl_->index.utf8.empty()) return {0, 0, 1, std::max(1.0f, height())};
    const bool usePrevious = pos.offset > 0 && (pos.affinity == TextAffinity::Upstream || pos.offset == impl_->index.utf8ToWide.back());
    const auto start = usePrevious ? previous(pos.offset) : pos.offset;
    const auto end = usePrevious ? pos.offset : next(pos.offset);
    auto boxes = impl_->paragraph->getRectsForRange(static_cast<unsigned>(impl_->index.wideTo16[start]),
        static_cast<unsigned>(impl_->index.wideTo16[end]), p::RectHeightStyle::kMax, p::RectWidthStyle::kTight);
    if (boxes.empty()) return {0, 0, 1, std::max(1.0f, height())};
    const auto& box = usePrevious ? boxes.back() : boxes.front();
    const bool rtl = box.direction == p::TextDirection::kRtl;
    return {(usePrevious != rtl) ? box.rect.right() : box.rect.left(), box.rect.top(), 1, box.rect.height()};
}

void Layout::ensureStops() const {
    if (!impl_->stopsReady) {
        if (!impl_->parts.empty()) {
            for (const auto& part : impl_->parts) {
                part.layout->ensureStops();
                for (auto stop : part.layout->impl_->stops) {
                    stop.position.offset += part.start;
                    stop.bounds.y += part.y;
                    impl_->stops.push_back(stop);
                }
            }
        } else {
            for (const auto offset : graphemes()) {
                for (const auto affinity : {TextAffinity::Downstream, TextAffinity::Upstream}) {
                    Position candidate{offset, affinity};
                    impl_->stops.push_back({candidate, caret(candidate)});
                }
            }
        }
        std::stable_sort(impl_->stops.begin(), impl_->stops.end(), [](const auto& a, const auto& b) {
            return std::tie(a.bounds.y, a.bounds.x, a.position.offset) < std::tie(b.bounds.y, b.bounds.x, b.position.offset);
        });
        impl_->stops.erase(std::unique(impl_->stops.begin(), impl_->stops.end(), [](const auto& a, const auto& b) {
            return a.position.offset == b.position.offset && std::fabs(a.bounds.x - b.bounds.x) < 0.01f &&
                std::fabs(a.bounds.y - b.bounds.y) < 0.01f;
        }), impl_->stops.end());
        impl_->stopsReady = true;
    }
}

Position Layout::moveVisual(Position pos, int direction) const {
    if (!direction) return pos;
    ensureStops();
    const auto current = caret(pos);
    auto it = std::find_if(impl_->stops.begin(), impl_->stops.end(), [&](const auto& stop) {
        return stop.position.offset == floor(pos.offset) && std::fabs(stop.bounds.x - current.x) < 0.01f &&
            std::fabs(stop.bounds.y - current.y) < 0.01f;
    });
    if (it == impl_->stops.end()) return pos;
    if (direction < 0 && it != impl_->stops.begin()) return std::prev(it)->position;
    if (direction > 0 && std::next(it) != impl_->stops.end()) return std::next(it)->position;
    return pos;
}

Position Layout::moveVertical(Position pos, int direction, float targetX) const {
    const auto current = caret(pos);
    auto it = std::find_if(lines().begin(), lines().end(), [&](const auto& line) { return current.y < line.top + line.height - 0.1f; });
    if (it == lines().end()) it = std::prev(lines().end());
    if (direction < 0 && it != lines().begin()) --it;
    else if (direction > 0 && std::next(it) != lines().end()) ++it;
    else return pos;
    return hitTest({targetX, it->top + it->height / 2});
}

std::vector<Rect> Layout::selection(std::size_t start, std::size_t end) const {
    if (start > end) std::swap(start, end);
    start = floor(start); end = ceil(end);
    std::vector<Rect> result;
    if (start == end) return result;
    for (const auto& part : impl_->parts) {
        if (end <= part.start || start > part.end) continue;
        for (auto r : part.layout->selection(start > part.start ? start - part.start : 0, std::min(end, part.end) - part.start)) {
            r.y += part.y; result.push_back(r);
        }
    }
    if (!impl_->paragraph) return result;
    for (const auto& box : impl_->paragraph->getRectsForRange(static_cast<unsigned>(impl_->index.wideTo16[start]),
        static_cast<unsigned>(impl_->index.wideTo16[end]), p::RectHeightStyle::kMax, p::RectWidthStyle::kTight)) result.push_back(rect(box.rect));
    return result;
}

std::pair<std::size_t, std::size_t> Layout::word(std::size_t offset) const {
    auto breaker = unicodeEngine()->makeBreakIterator(impl_->options.text.locale.c_str(), SkUnicode::BreakType::kWords);
    if (!breaker || !breaker->setText(impl_->index.utf8.data(), static_cast<int>(impl_->index.utf8.size())))
        throw std::runtime_error("OneUI could not create a word iterator");
    const auto target = utf8Offset(offset);
    std::size_t before = 0;
    for (auto index = breaker->first(); !breaker->isDone(); index = breaker->next()) {
        const auto after = static_cast<std::size_t>(index);
        if (after > target) return {wideOffset(before), wideOffset(after)};
        before = after;
    }
    return {floor(offset), next(offset)};
}

void Layout::paint(SkCanvas& canvas, Point origin, Color color) const {
    for (const auto& part : impl_->parts) part.layout->paint(canvas, {origin.x, origin.y + part.y}, color);
    if (!impl_->paragraph) return;
    ForegroundPainter painter(
        canvas,
        SkColorSetARGB(color.a, color.r, color.g, color.b),
        impl_->inlineForegrounds, impl_->options.sensitive);
    impl_->paragraph->paint(&painter, origin.x, origin.y);
}
LayoutStats Layout::stats() {
    auto result = counters;
    result.cachedEntries = cache.size();
    result.cachedTextBytes = cache.textBytes();
    return result;
}
void Layout::clearCache() { cache.clear(); counters = {}; }
void Layout::installTestFonts(const std::vector<std::string>& paths) {
    clearCache(); testFonts.reset(); testFontFamilies.clear();
    if (paths.empty()) return;
    auto provider = sk_make_sp<p::TypefaceFontProvider>();
    for (const auto& path : paths) {
        auto face = rendering::makeTypefaceFromData(SkData::MakeFromFileName(path.c_str()));
        if (!face) throw std::runtime_error("Could not load required test font: " + path);
        const SkString alias(("OneUI Test " + std::to_string(testFontFamilies.size())).c_str());
        provider->registerTypeface(std::move(face), alias);
        testFontFamilies.push_back(alias);
    }
    testFonts = sk_make_sp<p::FontCollection>();
    testFonts->setTestFontManager(provider);
    testFonts->setDefaultFontManager(provider, testFontFamilies.front().c_str());
    testFonts->disableFontFallback();
    testFonts->getParagraphCache()->turnOn(false);
}
} // namespace oneui::text
