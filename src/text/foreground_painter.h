#pragma once

#include "include/core/SkCanvas.h"
#include "include/core/SkBlurTypes.h"
#include "include/core/SkMaskFilter.h"
#include "modules/skparagraph/include/ParagraphPainter.h"

namespace oneui::text {

// SkParagraph caches the paint together with text blobs on its first draw.
// Changing TextStyle::foreground afterwards doesn't invalidate that cache.
// Apply the caller's color at paint time while retaining shaping, positions,
// glyph caches, and color-font rendering (no monochrome layer/filter).
class ForegroundPainter final : public skia::textlayout::ParagraphPainter {
public:
    ForegroundPainter(SkCanvas& canvas, SkColor color) : canvas_(canvas), color_(color) {}
    void drawTextBlob(const sk_sp<SkTextBlob>& blob, SkScalar x, SkScalar y, const SkPaintOrID& source) override {
        SkPaint paint = std::holds_alternative<SkPaint>(source) ? std::get<SkPaint>(source) : SkPaint{};
        paint.setAntiAlias(true);
        paint.setColor(color_);
        canvas_.drawTextBlob(blob, x, y, paint);
    }
    void drawTextShadow(const sk_sp<SkTextBlob>& blob, SkScalar x, SkScalar y, SkColor color, SkScalar blur) override {
        SkPaint paint; paint.setColor(color);
        if (blur > 0) paint.setMaskFilter(SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, blur, false));
        canvas_.drawTextBlob(blob, x, y, paint);
    }
    void drawRect(const SkRect& rect, const SkPaintOrID& source) override {
        if (const auto* paint = std::get_if<SkPaint>(&source)) canvas_.drawRect(rect, *paint);
    }
    void drawFilledRect(const SkRect& rect, const DecorationStyle& style) override {
        SkPaint paint(style.skPaint()); paint.setStyle(SkPaint::kFill_Style); canvas_.drawRect(rect, paint);
    }
    void drawPath(const SkPath& path, const DecorationStyle& style) override { canvas_.drawPath(path, style.skPaint()); }
    void drawLine(SkScalar x0, SkScalar y0, SkScalar x1, SkScalar y1, const DecorationStyle& style) override {
        canvas_.drawLine(x0, y0, x1, y1, style.skPaint());
    }
    void clipRect(const SkRect& rect) override { canvas_.clipRect(rect); }
    void translate(SkScalar x, SkScalar y) override { canvas_.translate(x, y); }
    void save() override { canvas_.save(); }
    void restore() override { canvas_.restore(); }
private:
    SkCanvas& canvas_;
    SkColor color_;
};

} // namespace oneui::text
