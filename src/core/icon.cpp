#include "oneui/icon.h"

#include "oneui/canvas.h"

#include <algorithm>
#include <cmath>

namespace oneui {
namespace {

Point p(Rect rect, float x, float y) {
    return Point{rect.x + rect.width * x, rect.y + rect.height * y};
}

Rect r(Rect rect, float x, float y, float w, float h) {
    return Rect{rect.x + rect.width * x, rect.y + rect.height * y, rect.width * w, rect.height * h};
}

IconPrimitive line(Rect rect, float x1, float y1, float x2, float y2, Color color, float strokeWidth) {
    IconPrimitive primitive;
    primitive.kind = IconPrimitiveKind::Line;
    primitive.from = p(rect, x1, y1);
    primitive.to = p(rect, x2, y2);
    primitive.color = color;
    primitive.strokeWidth = strokeWidth;
    return primitive;
}

IconPrimitive shape(IconPrimitiveKind kind, Rect rect, Color color, float strokeWidth, bool filled = false, float radius = 0.0f) {
    IconPrimitive primitive;
    primitive.kind = kind;
    primitive.rect = rect;
    primitive.color = color;
    primitive.strokeWidth = strokeWidth;
    primitive.filled = filled;
    primitive.radius = radius;
    return primitive;
}

IconPrimitive poly(Rect rect, std::initializer_list<Point> points, Color color, float strokeWidth, bool closed = false, bool filled = false) {
    IconPrimitive primitive;
    primitive.kind = filled ? IconPrimitiveKind::Polygon : IconPrimitiveKind::Polyline;
    primitive.color = color;
    primitive.strokeWidth = strokeWidth;
    primitive.closed = closed || filled;
    primitive.filled = filled;
    int index = 0;
    for (const auto& point : points) {
        if (index >= static_cast<int>(primitive.points.size())) {
            break;
        }
        primitive.points[static_cast<std::size_t>(index++)] = p(rect, point.x, point.y);
    }
    primitive.pointCount = index;
    return primitive;
}

} // namespace

std::vector<IconPrimitive> buildIconPrimitives(
    IconSymbol symbol,
    Rect rect,
    Color color,
    Color accent,
    float strokeWidth) {
    const Color accentColor = accent.a == 0 ? color : accent;
    const float sw = std::max(1.0f, strokeWidth);
    std::vector<IconPrimitive> primitives;

    switch (symbol) {
    case IconSymbol::BrandBloom:
        primitives.push_back(shape(IconPrimitiveKind::Circle, r(rect, 0.08f, 0.08f, 0.84f, 0.84f), accentColor, sw, true));
        primitives.push_back(shape(IconPrimitiveKind::Circle, r(rect, 0.38f, 0.17f, 0.24f, 0.24f), color, sw, true));
        primitives.push_back(shape(IconPrimitiveKind::Circle, r(rect, 0.58f, 0.38f, 0.24f, 0.24f), color, sw, true));
        primitives.push_back(shape(IconPrimitiveKind::Circle, r(rect, 0.38f, 0.58f, 0.24f, 0.24f), color, sw, true));
        primitives.push_back(shape(IconPrimitiveKind::Circle, r(rect, 0.18f, 0.38f, 0.24f, 0.24f), color, sw, true));
        primitives.push_back(shape(IconPrimitiveKind::Circle, r(rect, 0.42f, 0.42f, 0.16f, 0.16f), accentColor, sw, true));
        break;
    case IconSymbol::Search:
        primitives.push_back(shape(IconPrimitiveKind::Circle, r(rect, 0.18f, 0.16f, 0.48f, 0.48f), color, sw));
        primitives.push_back(line(rect, 0.60f, 0.60f, 0.82f, 0.82f, color, sw));
        break;
    case IconSymbol::RemoteAssist:
        primitives.push_back(shape(IconPrimitiveKind::RoundRect, r(rect, 0.12f, 0.20f, 0.66f, 0.50f), color, sw, false, rect.width * 0.08f));
        primitives.push_back(line(rect, 0.32f, 0.80f, 0.58f, 0.80f, color, sw));
        primitives.push_back(line(rect, 0.45f, 0.70f, 0.45f, 0.80f, color, sw));
        primitives.push_back(shape(IconPrimitiveKind::Circle, r(rect, 0.66f, 0.55f, 0.22f, 0.22f), accentColor, sw, true));
        break;
    case IconSymbol::Monitor:
    case IconSymbol::Desktop:
        primitives.push_back(shape(IconPrimitiveKind::RoundRect, r(rect, 0.12f, 0.18f, 0.76f, 0.50f), color, sw, false, rect.width * 0.06f));
        primitives.push_back(line(rect, 0.40f, 0.76f, 0.60f, 0.76f, color, sw));
        primitives.push_back(line(rect, 0.50f, 0.68f, 0.50f, 0.76f, color, sw));
        break;
    case IconSymbol::Device:
        primitives.push_back(shape(IconPrimitiveKind::RoundRect, r(rect, 0.30f, 0.12f, 0.40f, 0.76f), color, sw, false, rect.width * 0.08f));
        primitives.push_back(line(rect, 0.44f, 0.76f, 0.56f, 0.76f, color, sw));
        break;
    case IconSymbol::Toolbox:
        primitives.push_back(shape(IconPrimitiveKind::RoundRect, r(rect, 0.14f, 0.34f, 0.72f, 0.46f), color, sw, false, rect.width * 0.06f));
        primitives.push_back(shape(IconPrimitiveKind::RoundRect, r(rect, 0.36f, 0.20f, 0.28f, 0.18f), color, sw, false, rect.width * 0.04f));
        primitives.push_back(line(rect, 0.14f, 0.50f, 0.86f, 0.50f, color, sw));
        break;
    case IconSymbol::Compass:
        primitives.push_back(shape(IconPrimitiveKind::Circle, r(rect, 0.14f, 0.14f, 0.72f, 0.72f), color, sw));
        primitives.push_back(poly(rect, {Point{0.58f, 0.25f}, Point{0.45f, 0.58f}, Point{0.72f, 0.45f}}, accentColor, sw, true, true));
        break;
    case IconSymbol::Settings:
        primitives.push_back(shape(IconPrimitiveKind::Circle, r(rect, 0.32f, 0.32f, 0.36f, 0.36f), color, sw));
        primitives.push_back(line(rect, 0.50f, 0.08f, 0.50f, 0.22f, color, sw));
        primitives.push_back(line(rect, 0.50f, 0.78f, 0.50f, 0.92f, color, sw));
        primitives.push_back(line(rect, 0.08f, 0.50f, 0.22f, 0.50f, color, sw));
        primitives.push_back(line(rect, 0.78f, 0.50f, 0.92f, 0.50f, color, sw));
        break;
    case IconSymbol::Bell:
        primitives.push_back(poly(rect, {Point{0.25f, 0.64f}, Point{0.34f, 0.34f}, Point{0.50f, 0.24f}, Point{0.66f, 0.34f}, Point{0.75f, 0.64f}}, color, sw));
        primitives.push_back(line(rect, 0.22f, 0.64f, 0.78f, 0.64f, color, sw));
        primitives.push_back(shape(IconPrimitiveKind::Circle, r(rect, 0.45f, 0.72f, 0.10f, 0.10f), color, sw, true));
        break;
    case IconSymbol::Minimize:
        primitives.push_back(line(rect, 0.22f, 0.62f, 0.78f, 0.62f, color, sw));
        break;
    case IconSymbol::Maximize:
        primitives.push_back(shape(IconPrimitiveKind::Rect, r(rect, 0.28f, 0.28f, 0.44f, 0.44f), color, sw));
        break;
    case IconSymbol::Restore:
        primitives.push_back(shape(IconPrimitiveKind::Rect, r(rect, 0.34f, 0.24f, 0.38f, 0.38f), color, sw));
        primitives.push_back(shape(IconPrimitiveKind::Rect, r(rect, 0.24f, 0.38f, 0.38f, 0.38f), color, sw));
        break;
    case IconSymbol::Close:
        primitives.push_back(line(rect, 0.28f, 0.28f, 0.72f, 0.72f, color, sw));
        primitives.push_back(line(rect, 0.72f, 0.28f, 0.28f, 0.72f, color, sw));
        break;
    case IconSymbol::Heart:
        primitives.push_back(poly(rect, {Point{0.50f, 0.78f}, Point{0.22f, 0.48f}, Point{0.30f, 0.28f}, Point{0.50f, 0.38f}, Point{0.70f, 0.28f}, Point{0.78f, 0.48f}}, color, sw, true));
        break;
    case IconSymbol::File:
        primitives.push_back(poly(rect, {Point{0.28f, 0.16f}, Point{0.58f, 0.16f}, Point{0.74f, 0.32f}, Point{0.74f, 0.84f}, Point{0.28f, 0.84f}}, color, sw, true));
        primitives.push_back(line(rect, 0.58f, 0.16f, 0.58f, 0.34f, color, sw));
        primitives.push_back(line(rect, 0.58f, 0.34f, 0.74f, 0.34f, color, sw));
        break;
    case IconSymbol::Sparkle:
        primitives.push_back(line(rect, 0.50f, 0.12f, 0.50f, 0.88f, accentColor, sw));
        primitives.push_back(line(rect, 0.12f, 0.50f, 0.88f, 0.50f, accentColor, sw));
        primitives.push_back(line(rect, 0.28f, 0.28f, 0.72f, 0.72f, color, sw));
        primitives.push_back(line(rect, 0.72f, 0.28f, 0.28f, 0.72f, color, sw));
        break;
    case IconSymbol::RadioOn:
        primitives.push_back(shape(IconPrimitiveKind::Circle, r(rect, 0.12f, 0.12f, 0.76f, 0.76f), accentColor, sw));
        primitives.push_back(shape(IconPrimitiveKind::Circle, r(rect, 0.33f, 0.33f, 0.34f, 0.34f), accentColor, sw, true));
        break;
    case IconSymbol::RadioOff:
        primitives.push_back(shape(IconPrimitiveKind::Circle, r(rect, 0.12f, 0.12f, 0.76f, 0.76f), color, sw));
        break;
    case IconSymbol::ToggleOn:
        primitives.push_back(shape(IconPrimitiveKind::RoundRect, r(rect, 0.04f, 0.16f, 0.92f, 0.68f), accentColor, sw, true, rect.height * 0.34f));
        primitives.push_back(shape(IconPrimitiveKind::Circle, r(rect, 0.58f, 0.22f, 0.32f, 0.56f), color, sw, true));
        break;
    case IconSymbol::KeyDots:
        primitives.push_back(shape(IconPrimitiveKind::Circle, r(rect, 0.05f, 0.32f, 0.18f, 0.18f), color, sw, true));
        primitives.push_back(shape(IconPrimitiveKind::Circle, r(rect, 0.29f, 0.32f, 0.18f, 0.18f), color, sw, true));
        primitives.push_back(shape(IconPrimitiveKind::Circle, r(rect, 0.53f, 0.32f, 0.18f, 0.18f), color, sw, true));
        primitives.push_back(shape(IconPrimitiveKind::Circle, r(rect, 0.77f, 0.32f, 0.18f, 0.18f), color, sw, true));
        break;
    case IconSymbol::Copy:
        primitives.push_back(shape(IconPrimitiveKind::RoundRect, r(rect, 0.30f, 0.18f, 0.48f, 0.56f), color, sw, false, rect.width * 0.06f));
        primitives.push_back(shape(IconPrimitiveKind::RoundRect, r(rect, 0.20f, 0.30f, 0.48f, 0.56f), color, sw, false, rect.width * 0.06f));
        break;
    case IconSymbol::ChevronDown:
        primitives.push_back(poly(rect, {Point{0.25f, 0.38f}, Point{0.50f, 0.62f}, Point{0.75f, 0.38f}}, color, sw));
        break;
    case IconSymbol::ChevronUp:
        primitives.push_back(poly(rect, {Point{0.25f, 0.62f}, Point{0.50f, 0.38f}, Point{0.75f, 0.62f}}, color, sw));
        break;
    case IconSymbol::Plus:
        primitives.push_back(line(rect, 0.50f, 0.20f, 0.50f, 0.80f, color, sw));
        primitives.push_back(line(rect, 0.20f, 0.50f, 0.80f, 0.50f, color, sw));
        break;
    case IconSymbol::User:
        primitives.push_back(shape(IconPrimitiveKind::Circle, r(rect, 0.34f, 0.12f, 0.32f, 0.32f), color, sw));
        primitives.push_back(poly(rect,
            {Point{0.20f, 0.86f}, Point{0.23f, 0.66f}, Point{0.38f, 0.57f}, Point{0.62f, 0.57f}, Point{0.77f, 0.66f}, Point{0.80f, 0.86f}},
            color, sw));
        break;
    case IconSymbol::Globe:
        primitives.push_back(shape(IconPrimitiveKind::Circle, r(rect, 0.14f, 0.14f, 0.72f, 0.72f), color, sw));
        primitives.push_back(line(rect, 0.16f, 0.50f, 0.84f, 0.50f, color, sw));
        primitives.push_back(shape(IconPrimitiveKind::Circle, r(rect, 0.36f, 0.14f, 0.28f, 0.72f), accentColor, sw));
        break;
    case IconSymbol::Play:
        primitives.push_back(poly(rect, {Point{0.36f, 0.24f}, Point{0.78f, 0.50f}, Point{0.36f, 0.76f}}, color, sw, true));
        break;
    }

    return primitives;
}

void paintIcon(Canvas& canvas, IconSymbol symbol, Rect rect, Color color, Color accent, float strokeWidth) {
    for (const auto& primitive : buildIconPrimitives(symbol, rect, color, accent, strokeWidth)) {
        switch (primitive.kind) {
        case IconPrimitiveKind::Line:
            canvas.drawLine(primitive.from, primitive.to, primitive.color, primitive.strokeWidth);
            break;
        case IconPrimitiveKind::Rect:
        case IconPrimitiveKind::RoundRect:
            if (primitive.filled) {
                canvas.fillRect(primitive.rect, primitive.color, primitive.radius);
            } else {
                canvas.strokeRect(primitive.rect, primitive.color, primitive.radius, primitive.strokeWidth);
            }
            break;
        case IconPrimitiveKind::Circle:
            if (primitive.filled) {
                canvas.fillEllipse(primitive.rect, primitive.color);
            } else {
                canvas.strokeEllipse(primitive.rect, primitive.color, primitive.strokeWidth);
            }
            break;
        case IconPrimitiveKind::Polyline:
        case IconPrimitiveKind::Polygon:
            for (int i = 1; i < primitive.pointCount; ++i) {
                canvas.drawLine(primitive.points[static_cast<std::size_t>(i - 1)],
                    primitive.points[static_cast<std::size_t>(i)],
                    primitive.color,
                    primitive.strokeWidth);
            }
            if (primitive.closed && primitive.pointCount > 2) {
                canvas.drawLine(primitive.points[static_cast<std::size_t>(primitive.pointCount - 1)],
                    primitive.points[0],
                    primitive.color,
                    primitive.strokeWidth);
            }
            break;
        }
    }
}

} // namespace oneui
