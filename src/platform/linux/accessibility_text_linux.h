#pragma once
#include "platform/shared/accessibility.h"
#include "oneui/controls/text_field.h"
#include "oneui/platform/window.h"
#include "internal/unicode.h"
#include "platform/shared/text_input.h"
#include <gio/gio.h>
#include <pango/pango.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace oneui::platform {
inline std::optional<Rect> accessibleTextRect(const std::shared_ptr<AccessibilityContext>& context,
                                              const AccessibleNode& node, Rect rect, unsigned coordinates) {
    if (coordinates > 2) return std::nullopt;
    auto result = context->bounds(rect, coordinates == 0);
    if (result && coordinates == 2 && node.parent) {
        auto parent = context->resolve(node.parent);
        auto origin = parent ? context->bounds(parent->frame, false) : std::nullopt;
        if (!origin) return std::nullopt;
        result->x -= origin->x; result->y -= origin->y;
    }
    return result;
}
inline GVariant* accessibleTextCall(const std::string& interface, const std::string& method, GVariant* args,
                                   const std::shared_ptr<AccessibilityContext>& context, const AccessibleNode& node) {
    if (!node.text || node.sensitive) throw std::runtime_error("Text is not exposed");
    auto field = std::dynamic_pointer_cast<TextField>(node.widget.lock());
    if (!field) throw std::runtime_error("Text control is defunct");
    const auto& value = node.text->value;
    const int length = static_cast<int>(value.size());
    const auto rangeValid = [&](int start, int end) { return start >= 0 && start <= end && end <= length; };
    const auto unchanged = [&] {
        auto now = context->resolve(node.id);
        return now && now->text && !now->info.state.disabled && now->text->value == value;
    };
    if (interface == "org.a11y.atspi.EditableText") {
        if (method == "SetTextContents") {
            const char* text; g_variant_get(args, "(&s)", &text);
            return g_variant_new("(b)", context->replaceText(node.id, 0, value.size(), unicode::fromUtf8(text)));
        }
        if (method == "InsertText") {
            int position, count; const char* text; g_variant_get(args, "(i&si)", &position, &text, &count);
            const std::string_view bytes(text);
            const auto size = count < 0 ? 0 : std::min(std::size_t(count), bytes.size());
            const bool valid = position >= 0 && position <= length && count >= 0 && utf8BoundaryBefore(bytes, size) == size;
            return g_variant_new("(b)", valid && context->replaceText(node.id, position, position, unicode::fromUtf8(bytes.substr(0, size))));
        }
        if (method == "PasteText") {
            int position; g_variant_get(args, "(i)", &position);
            if (position < 0 || position > length || node.info.state.readOnly || node.info.state.disabled) return g_variant_new("(b)", FALSE);
            auto clipboard = field->clipboard(); if (!clipboard) clipboard = std::make_shared<SystemClipboard>();
            const auto insertion = clipboard->text();
            return g_variant_new("(b)", unchanged() && context->replaceText(node.id, position, position, insertion));
        }
        int start, end; g_variant_get(args, "(ii)", &start, &end);
        if (method == "DeleteText") return g_variant_new("(b)", rangeValid(start, end) && context->replaceText(node.id, start, end, {}));
        if (method == "CopyText" || method == "CutText") {
            if (!rangeValid(start, end) || node.info.state.disabled || (method == "CutText" && node.info.state.readOnly)) {
                if (method == "CopyText") throw std::runtime_error("Copy unavailable");
                return g_variant_new("(b)", FALSE);
            }
            auto clipboard = field->clipboard(); if (!clipboard) clipboard = std::make_shared<SystemClipboard>();
            clipboard->setText(value.substr(start, end - start));
            return method == "CopyText" ? g_variant_new("()") :
                g_variant_new("(b)", unchanged() && context->replaceText(node.id, start, end, {}));
        }
        return nullptr;
    }
    if (method == "GetText") {
        int start, end; g_variant_get(args, "(ii)", &start, &end); if (end == -1) end = length;
        return g_variant_new("(s)", rangeValid(start, end) ? unicode::toUtf8(value.substr(start, end - start)).c_str() : "");
    }
    if (method == "GetCharacterAtOffset") {
        int offset; g_variant_get(args, "(i)", &offset);
        return g_variant_new("(i)", offset >= 0 && offset < length ?
            int(unicode::isUnicodeScalar(value[offset]) ? value[offset] : unicode::kReplacementCodePoint) : -1);
    }
    if (method == "SetCaretOffset") {
        int offset; g_variant_get(args, "(i)", &offset);
        return g_variant_new("(b)", offset >= 0 && context->selectText(node.id, offset, offset));
    }
    if (method == "GetNSelections") return g_variant_new("(i)", node.text->start != node.text->end ? 1 : 0);
    if (method == "GetSelection") {
        int selection; g_variant_get(args, "(i)", &selection);
        const bool valid = selection == 0 && node.text->start != node.text->end;
        return g_variant_new("(ii)", valid ? int(node.text->start) : -1, valid ? int(node.text->end) : -1);
    }
    if (method == "AddSelection" || method == "SetSelection" || method == "RemoveSelection") {
        int selection = 0, start = 0, end = 0;
        if (method == "RemoveSelection") {
            g_variant_get(args, "(i)", &selection); start = end = node.text->caret;
        } else if (method == "AddSelection") g_variant_get(args, "(ii)", &start, &end);
        else g_variant_get(args, "(iii)", &selection, &start, &end);
        const bool occupied = node.text->start != node.text->end;
        return g_variant_new("(b)", selection == 0 && (method == "AddSelection" ? !occupied : occupied) &&
            rangeValid(start, end) && context->selectText(node.id, start, end));
    }
    if (method == "GetStringAtOffset" || method == "GetTextAtOffset" || method == "GetTextBeforeOffset" || method == "GetTextAfterOffset") {
        int offset; unsigned kind; g_variant_get(args, "(iu)", &offset, &kind);
        const bool modern = method == "GetStringAtOffset";
        if (offset < 0 || offset > length || kind > (modern ? 4u : 6u)) return g_variant_new("(sii)", "", -1, -1);
        const unsigned boundary = modern ? (kind == 0 ? 0 : kind == 1 ? 1 : kind == 2 ? 3 : kind == 3 ? 5 : 7) : kind;
        std::vector<int> edges{0};
        if (boundary == 0) { for (int p = 1; p < length; ++p) edges.push_back(p); }
        else if (boundary == 5 || boundary == 6) {
            for (auto line : field->textLineRanges()) {
                auto p = boundary == 5 ? line.first : line.second;
                if (boundary == 6 && p && p <= value.size() && value[p - 1] == L'\n') --p;
                if (p && p < value.size()) edges.push_back(int(p));
            }
        } else if (boundary == 7) {
            for (int p = 1; p < length; ++p) if (value[p - 1] == L'\n') edges.push_back(p);
        } else {
            const auto utf8 = unicode::toUtf8(value);
            std::vector<PangoLogAttr> attributes(value.size() + 1);
            const auto& locale = field->textOptions().locale;
            pango_get_log_attrs(utf8.c_str(), utf8.size(), -1, pango_language_from_string(locale.c_str()), attributes.data(), attributes.size());
            for (int p = 1; p < length; ++p) {
                const auto a = attributes[p];
                if ((boundary == 1 && a.is_word_start) || (boundary == 2 && a.is_word_end) ||
                    (boundary == 3 && a.is_sentence_start) || (boundary == 4 && a.is_sentence_end)) edges.push_back(p);
            }
        }
        edges.push_back(length); std::sort(edges.begin(), edges.end()); edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
        int index = int(std::upper_bound(edges.begin(), edges.end(), offset) - edges.begin()) - 1;
        if (method == "GetTextBeforeOffset") --index;
        if (method == "GetTextAfterOffset") ++index;
        if (index < 0 || index + 1 >= int(edges.size())) return g_variant_new("(sii)", "", offset, offset);
        const int start = edges[index], end = edges[index + 1];
        return g_variant_new("(sii)", unicode::toUtf8(value.substr(start, end - start)).c_str(), start, end);
    }
    if (method == "GetAttributeValue") return g_variant_new("(s)", "");
    if (method == "GetAttributes" || method == "GetAttributeRun" || method == "GetDefaultAttributes" || method == "GetDefaultAttributeSet") {
        auto* dictionary = g_variant_new_array(G_VARIANT_TYPE("{ss}"), nullptr, 0);
        return method == "GetAttributes" || method == "GetAttributeRun" ? g_variant_new("(@a{ss}ii)", dictionary, 0, length) :
            g_variant_new("(@a{ss})", dictionary);
    }
    if (method == "GetCharacterExtents" || method == "GetRangeExtents") {
        int start, end; unsigned coordinates;
        if (method == "GetCharacterExtents") { g_variant_get(args, "(iu)", &start, &coordinates); end = start < length ? start + 1 : start; }
        else g_variant_get(args, "(iiu)", &start, &end, &coordinates);
        if (!rangeValid(start, end)) throw std::runtime_error("Invalid text range");
        std::optional<Rect> result;
        for (const auto& rect : field->textRangeRects(start, end)) {
            auto r = accessibleTextRect(context, node, rect, coordinates);
            if (!r) throw std::runtime_error("Coordinate space unavailable");
            if (!result) result = r;
            else {
                const float right = std::max(result->x + result->width, r->x + r->width);
                const float bottom = std::max(result->y + result->height, r->y + r->height);
                result->x = std::min(result->x, r->x); result->y = std::min(result->y, r->y);
                result->width = right - result->x; result->height = bottom - result->y;
            }
        }
        if (!result) throw std::runtime_error("Text geometry unavailable");
        return g_variant_new("(iiii)", int(std::floor(result->x)), int(std::floor(result->y)),
            int(std::ceil(result->x + result->width) - std::floor(result->x)), int(std::ceil(result->y + result->height) - std::floor(result->y)));
    }
    if (method == "GetOffsetAtPoint") {
        int x, y; unsigned coordinates; g_variant_get(args, "(iiu)", &x, &y, &coordinates);
        auto origin = accessibleTextRect(context, node, {0, 0, 1, 1}, coordinates);
        if (!origin || origin->width <= 0 || origin->height <= 0) return g_variant_new("(i)", -1);
        auto offset = field->textOffsetAtPoint({(x - origin->x) / origin->width, (y - origin->y) / origin->height});
        return g_variant_new("(i)", offset ? int(*offset) : -1);
    }
    if (method == "ScrollSubstringTo" || method == "ScrollSubstringToPoint") return g_variant_new("(b)", FALSE);
    // Bounded-range queries require clipping/scroll-container semantics that
    // are not implemented yet. Report an error rather than fabricated ranges.
    if (method == "GetBoundedRanges") throw std::runtime_error("Bounded ranges unavailable");
    return nullptr;
}
} // namespace oneui::platform
