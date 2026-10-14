#include "code_editor_impl.h"

#include "oneui/icon.h"

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <unordered_map>

namespace oneui {
namespace {

constexpr float topPadding = 4.0f;

Color blend(Color a, Color b, float t) {
    auto mix = [t](std::uint8_t x, std::uint8_t y) {
        return static_cast<std::uint8_t>(std::lround(static_cast<float>(x) + (static_cast<float>(y) - static_cast<float>(x)) * t));
    };
    return Color{mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b), mix(a.a, b.a)};
}

Color withAlpha(Color color, std::uint8_t a) {
    color.a = a;
    return color;
}

// Syntax colors derive from semantic theme tokens, so every theme (light or
// dark) gets readable colors without per-theme tables.
Color tokenColor(CodeTokenKind kind, const Theme& t) {
    switch (kind) {
    case CodeTokenKind::Keyword: return t.primary;
    case CodeTokenKind::Type: return blend(t.primary, t.error, 0.45f);
    case CodeTokenKind::Function: return blend(t.warning, t.text, 0.35f);
    case CodeTokenKind::Constant: return blend(t.error, t.primary, 0.3f);
    case CodeTokenKind::Number: return t.warning;
    case CodeTokenKind::String: return t.success;
    case CodeTokenKind::Identifier: return blend(t.text, t.primary, 0.25f);
    case CodeTokenKind::Comment: return t.textSubtle;
    case CodeTokenKind::Operator: return t.textMuted;
    case CodeTokenKind::Parameter: return blend(t.error, t.warning, 0.5f);
    default: return t.text;
    }
}

} // namespace

CodeEditorImpl::FindLayout CodeEditorImpl::findLayout() const {
    FindLayout l;
    const Rect f = owner.frame();
    const float width = std::min(460.0f, std::max(260.0f, f.width - gutterWidth() - 30.0f));
    const float row = 30.0f;
    const float height = replaceVisible ? row * 2.0f + 10.0f : row + 8.0f;
    l.bar = Rect{f.x + f.width - width - 18.0f, f.y + 6.0f, width, height};
    const float y = l.bar.y + 4.0f;
    float right = l.bar.x + l.bar.width - 4.0f;
    auto take = [&](float w, float top) {
        right -= w;
        const Rect r{right, top + 3.0f, w, row - 6.0f};
        right -= 2.0f;
        return r;
    };
    l.close = take(24.0f, y);
    l.next = take(24.0f, y);
    l.previous = take(24.0f, y);
    l.count = take(74.0f, y);
    l.word = take(26.0f, y);
    l.regex = take(26.0f, y);
    l.matchCase = take(26.0f, y);
    l.find = Rect{l.bar.x + 4.0f, y + 2.0f, std::max(40.0f, right - l.bar.x - 6.0f), row - 4.0f};
    if (replaceVisible) {
        const float y2 = y + row + 2.0f;
        right = l.bar.x + l.bar.width - 4.0f;
        l.replaceAll = take(44.0f, y2);
        l.replaceOne = take(66.0f, y2);
        l.replace = Rect{l.bar.x + 4.0f, y2 + 2.0f, std::max(40.0f, right - l.bar.x - 6.0f), row - 4.0f};
    }
    return l;
}

void CodeEditorImpl::paint(Canvas& canvas, bool focused, bool caretVisible) {
    measure(canvas);
    lineHeight = std::round(fontSize * 1.55f);
    const auto& t = theme();
    const Rect f = owner.frame();
    const Rect text = textRect();
    canvas.fillRect(f, t.surface);

    canvas.save();
    canvas.clipRect(f);
    const auto rows = rowCount();
    const auto firstRow = static_cast<std::size_t>(std::max(0.0f, std::floor((scroll - topPadding) / lineHeight)));
    const auto lastRow = std::min(rows, static_cast<std::size_t>(std::ceil((scroll + text.height) / lineHeight)) + 1);
    const auto& primarySel = selections[std::min(primary, selections.size() - 1)];
    const float tab = std::max(1, tabSize) * spaceWidth();
    const auto bracket = focused ? bracketMatch() : std::nullopt;
    refreshMatches();

    // Measure characters the cache has not seen yet, for the visible lines only.
    bool newWidths = false;
    for (auto row = firstRow; row < lastRow; ++row) {
        const auto [line, segment] = lineOfRow(row);
        if (segment != 0) continue;
        const auto& content = doc.line(line);
        for (std::size_t i = 0; i < content.size(); ++i) {
            const wchar_t c = content[i];
            if (c < 128 || (c >= 0xdc00 && c <= 0xdfff) || wide.count(c)) continue;
            const bool pair = c >= 0xd800 && c <= 0xdbff && i + 1 < content.size();
            wide[c] = canvas.measureTextWidthWithFont(content.substr(i, pair ? 2 : 1), fontSize, TextFontFamily::Monospace);
            newWidths = true;
        }
    }
    if (newWidths && wrap) rowsValid = false;

    std::unordered_map<std::size_t, std::vector<CodeToken>> tokenCache;
    canvas.save();
    canvas.clipRect(Rect{text.x - 4.0f, text.y, text.width + 4.0f, text.height});
    for (auto row = firstRow; row < lastRow; ++row) {
        const auto [line, segment] = lineOfRow(row);
        const auto& content = doc.line(line);
        const auto segs = segments(line);
        const std::size_t segStart = segs[segment];
        const std::size_t segEnd = segment + 1 < segs.size() ? segs[segment + 1] : content.size();
        const float y = text.y + topPadding + static_cast<float>(row) * lineHeight - scroll;
        const float originX = text.x - horizontal;
        auto xAt = [&](std::size_t column) { return originX + columnX(line, segStart, column); };

        if (line == primarySel.caret.line && primarySel.empty())
            canvas.fillRect(Rect{text.x - 4.0f, y, text.width + 4.0f, lineHeight}, withAlpha(t.hoverBackground, 200));

        // Decorations under the text.
        for (const auto& dec : decorations) {
            if (line < dec.start.line || line > dec.end.line) continue;
            const std::size_t from = line == dec.start.line ? dec.start.column : 0;
            const std::size_t to = line == dec.end.line ? dec.end.column : content.size();
            const std::size_t a = std::max(from, segStart), b = std::min(to, segEnd);
            if (a > b) continue;
            const float x0 = xAt(a);
            float x1 = xAt(b);
            const bool fullLine = line != dec.end.line && b == content.size();
            switch (dec.kind) {
            case CodeDecorationKind::Statement:
            case CodeDecorationKind::Highlight:
            case CodeDecorationKind::Running: {
                if (fullLine) x1 = std::max(x1, text.x + text.width);
                const Color tint = dec.kind == CodeDecorationKind::Statement ? withAlpha(t.primary, 20)
                                   : dec.kind == CodeDecorationKind::Running ? withAlpha(t.success, 34)
                                                                             : withAlpha(t.warning, 45);
                if (x1 > x0) canvas.fillRect(Rect{x0, y, x1 - x0, lineHeight}, tint);
                break;
            }
            case CodeDecorationKind::Error:
            case CodeDecorationKind::Warning: {
                if (x1 <= x0) x1 = x0 + advance(L'M');
                const Color color = dec.kind == CodeDecorationKind::Error ? t.error : t.warning;
                const float base = y + lineHeight - 3.0f;
                float x = x0;
                bool up = true;
                while (x < x1) {
                    const float next = std::min(x1, x + 3.0f);
                    canvas.drawLine(Point{x, up ? base : base - 2.0f}, Point{next, up ? base - 2.0f : base}, color, 1.0f);
                    x = next;
                    up = !up;
                }
                break;
            }
            }
        }

        // Search matches on this row.
        for (const auto& m : matches) {
            if (m.start.line != line) continue;
            if (m.end.column <= segStart || m.start.column >= segEnd) continue;
            const float x0 = xAt(std::max(m.start.column, segStart)), x1 = xAt(std::min(m.end.column, segEnd));
            const bool active = current >= 0 && &m == &matches[static_cast<std::size_t>(current)];
            canvas.fillRect(Rect{x0, y + 1.0f, std::max(2.0f, x1 - x0), lineHeight - 2.0f},
                            active ? withAlpha(t.warning, 110) : withAlpha(t.warning, 50), 2.0f);
        }

        // Selections.
        for (const auto& s : selections) {
            if (s.empty() || line < s.start().line || line > s.end().line) continue;
            const std::size_t from = line == s.start().line ? s.start().column : 0;
            const std::size_t to = line == s.end().line ? s.end().column : content.size();
            const std::size_t a = std::max(from, segStart), b = std::min(to, segEnd);
            if (a > b || (a == b && !(line != s.end().line && b == segEnd && segment + 1 == segs.size()))) continue;
            float x0 = xAt(a), x1 = xAt(b);
            if (line != s.end().line && b == content.size()) x1 += spaceWidth();
            canvas.fillRect(Rect{x0, y, std::max(1.0f, x1 - x0), lineHeight},
                            focused ? withAlpha(t.primary, 70) : withAlpha(t.textSubtle, 60));
        }

        if (bracket) {
            for (const auto& p : {bracket->first, bracket->second}) {
                if (p.line != line || p.column < segStart || p.column >= segEnd) continue;
                const float x0 = xAt(p.column);
                const float w = std::max(2.0f, xAt(p.column + 1) - x0);
                canvas.fillRect(Rect{x0, y + 1.0f, w, lineHeight - 2.0f}, withAlpha(t.primary, 40), 2.0f);
                canvas.strokeRect(Rect{x0, y + 1.0f, w, lineHeight - 2.0f}, withAlpha(t.primary, 140), 2.0f, 1.0f);
            }
        }

        if (indentGuides && segment == 0 && !content.empty()) {
            std::size_t lead = 0;
            while (lead < content.size() && (content[lead] == L' ' || content[lead] == L'\t')) ++lead;
            const float leadWidth = columnX(line, 0, lead);
            for (float gx = tab; gx < leadWidth - 1.0f; gx += tab)
                canvas.drawLine(Point{originX + gx, y}, Point{originX + gx, y + lineHeight}, withAlpha(t.border, 160), 1.0f);
        }

        // Text, split into token runs and at tabs.
        auto found = tokenCache.find(line);
        if (found == tokenCache.end()) found = tokenCache.emplace(line, lexer.tokens(doc, line)).first;
        const auto& tokens = found->second;
        std::size_t cursor = segStart;
        std::size_t tokenIndex = 0;
        const float right = text.x + text.width;
        auto drawRun = [&](std::size_t from, std::size_t to, CodeTokenKind kind) {
            const Color color = tokenColor(kind, t);
            std::size_t runStart = from;
            for (std::size_t i = from; i <= to; ++i) {
                if (i < to && content[i] != L'\t') continue;
                if (i > runStart) {
                    const float x0 = xAt(runStart);
                    if (x0 > right) return;
                    const float x1 = xAt(i);
                    if (x1 >= text.x - 4.0f)
                        canvas.drawTextStyledWithFont(content.substr(runStart, i - runStart), Rect{x0, y, x1 - x0 + 4.0f, lineHeight},
                                                      color, fontSize, TextAlign::Left, TextFontFamily::Monospace,
                                                      400);
                }
                runStart = i + 1;
            }
        };
        while (cursor < segEnd) {
            while (tokenIndex < tokens.size() && tokens[tokenIndex].start + tokens[tokenIndex].length <= cursor) ++tokenIndex;
            if (tokenIndex < tokens.size() && tokens[tokenIndex].start <= cursor) {
                const auto stop = std::min<std::size_t>(segEnd, tokens[tokenIndex].start + tokens[tokenIndex].length);
                drawRun(cursor, stop, tokens[tokenIndex].kind);
                cursor = stop;
            } else {
                const auto stop = tokenIndex < tokens.size() ? std::min<std::size_t>(segEnd, tokens[tokenIndex].start) : segEnd;
                drawRun(cursor, stop, CodeTokenKind::Plain);
                cursor = stop;
            }
            if (xAt(cursor) > right) break;
        }
    }

    // Carets and the IME preedit.
    if (focused) {
        for (const auto& s : selections) {
            const Rect caret = caretRect(s.caret);
            if (caret.y + caret.height < text.y || caret.y > text.y + text.height) continue;
            if (caretVisible) canvas.fillRect(Rect{caret.x - 1.0f, caret.y + 2.0f, 2.0f, caret.height - 4.0f}, t.text);
        }
        if (!preedit.empty()) {
            const Rect caret = caretRect(primarySel.caret);
            const float width = canvas.measureTextWidthWithFont(preedit, fontSize, TextFontFamily::Monospace);
            canvas.fillRect(Rect{caret.x, caret.y, width + 2.0f, caret.height}, t.surface);
            canvas.drawTextStyledWithFont(preedit, Rect{caret.x, caret.y, width + 4.0f, caret.height}, t.text, fontSize, TextAlign::Left,
                                          TextFontFamily::Monospace);
            canvas.drawLine(Point{caret.x, caret.y + caret.height - 3.0f}, Point{caret.x + width, caret.y + caret.height - 3.0f},
                            t.primary, 1.5f);
        }
    }
    canvas.restore();

    // Gutter.
    const float gutter = gutterWidth();
    canvas.fillRect(Rect{f.x, f.y, gutter, f.height}, t.surfaceMuted);
    canvas.drawLine(Point{f.x + gutter, f.y}, Point{f.x + gutter, f.y + f.height}, withAlpha(t.border, 200), 1.0f);
    if (lineNumbers) {
        for (auto row = firstRow; row < lastRow; ++row) {
            const auto [line, segment] = lineOfRow(row);
            if (segment != 0) continue;
            const float y = text.y + topPadding + static_cast<float>(row) * lineHeight - scroll;
            const bool active = line == primarySel.caret.line;
            canvas.drawTextStyledWithFont(std::to_wstring(line + 1), Rect{f.x, y, gutter - 10.0f, lineHeight},
                                          active ? t.textMuted : t.textSubtle, fontSize * 0.92f, TextAlign::Right,
                                          TextFontFamily::Monospace);
        }
    }

    // Gutter markers for running statements and problems.
    for (const auto& dec : decorations) {
        if (dec.kind != CodeDecorationKind::Running && dec.kind != CodeDecorationKind::Error) continue;
        const float top = text.y + topPadding + static_cast<float>(rowOfLine(dec.start.line)) * lineHeight - scroll;
        const float bottom = text.y + topPadding + static_cast<float>(rowOfLine(dec.end.line) + 1) * lineHeight - scroll;
        if (bottom < f.y || top > f.y + f.height) continue;
        if (dec.kind == CodeDecorationKind::Running)
            canvas.fillRect(Rect{f.x + 2.0f, top + 2.0f, 3.0f, std::max(4.0f, bottom - top - 4.0f)}, t.success, 1.5f);
        else
            canvas.fillEllipse(Rect{f.x + 4.0f, top + lineHeight * 0.5f - 3.0f, 6.0f, 6.0f}, t.error);
    }

    // Scrollbars.
    if (const Rect v = verticalThumb(); v.height > 0.0f) canvas.fillRect(v, withAlpha(t.borderStrong, 150), 3.0f);
    if (const Rect h = horizontalThumb(); h.width > 0.0f) canvas.fillRect(h, withAlpha(t.borderStrong, 150), 3.0f);

    // Find bar chrome; its text fields are child widgets painted afterwards.
    if (findVisible) {
        const auto l = findLayout();
        canvas.drawBoxShadow(l.bar, BoxShadow{t.shadowColor, Point{0.0f, 4.0f}, 12.0f, 0.0f}, 6.0f);
        canvas.fillRect(l.bar, t.surface, 6.0f);
        canvas.strokeRect(l.bar, t.border, 6.0f, 1.0f);
        auto chip = [&](Rect r, const wchar_t* label, bool active) {
            if (active) canvas.fillRect(r, t.primarySoft, 4.0f);
            canvas.drawTextStyled(label, r, active ? t.primary : t.textMuted, t.fontSm, TextAlign::Center, 600);
        };
        chip(l.matchCase, L"Aa", options.matchCase);
        chip(l.regex, L".*", options.regex);
        chip(l.word, L"W", options.wholeWord);
        std::wstring count;
        if (!searchError.empty()) count = L"Invalid";
        else if (query.empty()) count = L"";
        else if (matches.empty()) count = L"No results";
        else {
            const auto& s = selections[std::min(primary, selections.size() - 1)];
            const bool onMatch = current >= 0 && matches[static_cast<std::size_t>(current)].start == s.start() &&
                                 matches[static_cast<std::size_t>(current)].end == s.end();
            count = (onMatch ? std::to_wstring(current + 1) : std::wstring(L"?")) + L" of " + std::to_wstring(matches.size());
        }
        canvas.drawTextStyled(count, l.count, searchError.empty() ? t.textMuted : t.error, t.fontSm, TextAlign::Center);
        auto icon = [&](Rect r, IconSymbol symbol) {
            const float s = 14.0f;
            paintIcon(canvas, symbol, Rect{r.x + (r.width - s) * 0.5f, r.y + (r.height - s) * 0.5f, s, s}, t.textMuted);
        };
        icon(l.previous, IconSymbol::OutlineWorkbenchUp);
        icon(l.next, IconSymbol::OutlineWorkbenchDown);
        icon(l.close, IconSymbol::OutlineWorkbenchClose);
        if (replaceVisible) {
            chip(l.replaceOne, L"Replace", false);
            chip(l.replaceAll, L"All", false);
        }
    }
    canvas.restore();

    // Completion list, drawn above neighbouring widgets while open.
    if (completionVisible) {
        const Rect r = completionRect();
        canvas.drawBoxShadow(r, BoxShadow{t.shadowColor, Point{0.0f, 6.0f}, 16.0f, 0.0f}, 6.0f);
        canvas.fillRect(r, t.surface, 6.0f);
        canvas.strokeRect(r, t.border, 6.0f, 1.0f);
        const int end = std::min(static_cast<int>(completionFiltered.size()), completionTop + completionRows);
        for (int i = completionTop; i < end; ++i) {
            const auto& item = completionItems[completionFiltered[static_cast<std::size_t>(i)]];
            const Rect row{r.x + 4.0f, r.y + 4.0f + static_cast<float>(i - completionTop) * completionRowHeight, r.width - 8.0f,
                           completionRowHeight};
            if (i == completionSelected) canvas.fillRect(row, t.primarySoft, 4.0f);
            const Rect badge{row.x + 4.0f, row.y + 4.0f, 16.0f, row.height - 8.0f};
            const wchar_t letter = item.kind.empty() ? L'?' : static_cast<wchar_t>(std::towupper(item.kind.front()));
            canvas.fillRect(badge, withAlpha(t.primary, 36), 3.0f);
            canvas.drawTextStyled(std::wstring(1, letter), badge, t.primary, t.fontSm * 0.9f, TextAlign::Center, 600);
            const float detailWidth = item.detail.empty() ? 0.0f : std::min(140.0f, canvas.measureTextWidth(item.detail, t.fontSm) + 8.0f);
            canvas.drawTextStyledWithFont(item.label, Rect{badge.x + badge.width + 8.0f, row.y, row.width - 40.0f - detailWidth, row.height},
                                          i == completionSelected ? t.primary : t.text, fontSize, TextAlign::Left,
                                          TextFontFamily::Monospace);
            if (!item.detail.empty())
                canvas.drawTextStyledEllipsized(item.detail, Rect{row.x + row.width - detailWidth - 4.0f, row.y, detailWidth, row.height},
                                                t.textMuted, t.fontSm, TextAlign::Right);
        }
        if (static_cast<int>(completionFiltered.size()) > completionRows) {
            const float track = r.height - 8.0f;
            const float thumb = std::max(16.0f, track * completionRows / static_cast<float>(completionFiltered.size()));
            const float y = r.y + 4.0f + (track - thumb) * completionTop /
                                         static_cast<float>(completionFiltered.size() - completionRows);
            canvas.fillRect(Rect{r.x + r.width - 5.0f, y, 3.0f, thumb}, withAlpha(t.borderStrong, 150), 1.5f);
        }
    }
}

} // namespace oneui
