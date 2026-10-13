// Table rasterization and visual style resolution. Included within namespace oneui.
namespace {
std::optional<float> percentValue(const std::wstring& text) {
    std::string ascii;
    ascii.reserve(text.size());
    for (const auto character : text) {
        if (character > 127 || character < 0) return {};
        ascii.push_back(static_cast<char>(character));
    }
    const auto first = ascii.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    auto last = ascii.find_last_not_of(" \t\r\n") + 1;
    if (ascii[last - 1] == '%') --last;
    while (last > first && (ascii[last - 1] == ' ' || ascii[last - 1] == '\t')) --last;
    auto begin = ascii.data() + first;
    const auto end = ascii.data() + last;
    if (begin < end && *begin == '+') ++begin;
    float value = 0;
#if defined(__APPLE__) && defined(_LIBCPP_VERSION)
    // Apple SDK libc++ does not always provide floating-point from_chars.
    // Keep decimal parsing independent of the application's global locale.
    if (begin == end || *begin == '+') return {};
    const std::string number(begin, end);
    if (number.find_first_not_of("0123456789.eE+-") != std::string::npos) return {};
    std::istringstream input{number};
    input.imbue(std::locale::classic());
    if (!(input >> std::noskipws >> value) || input.peek() != std::char_traits<char>::eof() ||
        !std::isfinite(value)) return {};
#else
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc{} || result.ptr != end || !std::isfinite(value)) return {};
#endif
    return std::clamp(value, 0.0f, 100.0f);
}

void applyTableStyleOverride(TableStyle& style, const TableStyleOverride& override) {
    if (override.checkBackground) style.checkBackground = override.checkBackground;
    if (override.checkBorder) style.checkBorder = override.checkBorder;
    if (override.checkRadius) style.checkRadius = *override.checkRadius;
    if (override.background) style.background = *override.background;
    if (override.border) style.border = *override.border;
    if (override.headerBackground) style.headerBackground = *override.headerBackground;
    if (override.headerForeground) style.headerForeground = *override.headerForeground;
    if (override.cellForeground) style.cellForeground = *override.cellForeground;
    if (override.gridLine) style.gridLine = *override.gridLine;
    if (override.rowHovered) style.rowHovered = *override.rowHovered;
    if (override.rowPressed) style.rowPressed = *override.rowPressed;
    if (override.rowSelected) style.rowSelected = *override.rowSelected;
    if (override.scrollbarColor) style.scrollbarColor = *override.scrollbarColor;
    if (override.borderWidth) style.borderWidth = *override.borderWidth;
    if (override.radius) style.radius = *override.radius;
    if (override.fontSize) style.fontSize = *override.fontSize;
    if (override.headerFontSize) style.headerFontSize = *override.headerFontSize;
    if (override.detailFontSize) style.detailFontSize = *override.detailFontSize;
    if (override.detailForeground) style.detailForeground = *override.detailForeground;
    if (override.headerHeight) style.headerHeight = *override.headerHeight;
    if (override.scrollbarWidth) style.scrollbarWidth = *override.scrollbarWidth;
    if (override.cellPadding) style.cellPadding = *override.cellPadding;
    if (override.headerTextInset) style.headerTextInset = override.headerTextInset;
    if (override.cellIconGap) style.cellIconGap = *override.cellIconGap;
}

} // namespace

void Table::paint(Canvas& canvas) {
    const TableStyle style = resolvedStyle();
    const Rect rect = frame();
    canvas.fillRect(rect, style.background, style.radius);
    canvas.strokeRect(rect, style.border, style.radius, style.borderWidth);
    if (focusVisible() && !disabled()) {
        canvas.strokeRect(
            Rect{rect.x - 2.0f, rect.y - 2.0f, rect.width + 4.0f, rect.height + 4.0f},
            theme().focusOutline,
            style.radius + 2.0f,
            1.0f);
    }
    if (columns_.empty()) return;

    const float headerHeight = std::max(1.0f, style.headerHeight);
    const Rect headerRect{rect.x, rect.y, rect.width, std::min(rect.height, headerHeight)};
    const Rect contentRect{rect.x, rect.y + headerHeight, rect.width, std::max(0.0f, rect.height - headerHeight)};
    const float height = effectiveRowHeight(contentRect.height);
    canvas.fillRect(headerRect, style.headerBackground, style.radius);
    canvas.drawLine(Point{rect.x, rect.y + headerHeight}, Point{rect.x + rect.width, rect.y + headerHeight}, style.gridLine, 1.0f);

    const auto paintCheck = [&](float y, float h, bool checked, bool partial) {
        const Rect box{rect.x + (selectionGutter() - 16.0f) * .5f, y + (h - 16.0f) * .5f, 16.0f, 16.0f};
        const Color accent = disabled() ? style.headerForeground : theme().focusOutline;
        canvas.fillRect(box, checked || partial ? accent : style.checkBackground.value_or(style.background), style.checkRadius);
        canvas.strokeRect(box, checked || partial ? accent : style.checkBorder.value_or(style.headerForeground), style.checkRadius, 1.0f);
        if (partial) canvas.drawLine({box.x+4.0f,box.y+8.0f},{box.x+12.0f,box.y+8.0f},Color{255,255,255},1.5f);
        else if (checked) paintIcon(canvas,IconSymbol::Check,box.inset(Insets{3.0f}),Color{255,255,255});
    };
    if (selectionColumnVisible_) paintCheck(headerRect.y,headerRect.height,
        !rows_.empty() && selection_.selectedIndices().size()==rows_.size(),
        !selection_.selectedIndices().empty() && selection_.selectedIndices().size()!=rows_.size());
    float fixedWidth = 0.0f;
    int flexibleCount = 0;
    for (const auto& column : columns_) {
        if (column.width > 0.0f) fixedWidth += column.width;
        else ++flexibleCount;
    }
    const float remainingWidth = std::max(0.0f, rect.width - fixedWidth - selectionGutter());
    float x = rect.x + selectionGutter();
    for (int columnIndex = 0; columnIndex < static_cast<int>(columns_.size()); ++columnIndex) {
        const float width = columnWidth(columnIndex, remainingWidth, flexibleCount);
        if (columnDividersVisible_ && columnIndex > 0) {
            canvas.drawLine(Point{x, rect.y}, Point{x, rect.y + rect.height}, style.gridLine, 1.0f);
        }
        const Insets headerPadding = style.headerTextInset
            ? Insets{0.0f, *style.headerTextInset} : style.cellPadding;
        auto label = Rect{x, rect.y, width, headerHeight}.inset(headerPadding);
        if (columnIndex == sortColumn_) {
            const float titleWidth = std::min(canvas.measureTextWidth(columns_[columnIndex].header, style.headerFontSize, 400), std::max(0.0f, label.width - 22.0f));
            paintIcon(canvas, sortDescending_ ? IconSymbol::ChevronUp : IconSymbol::ChevronDown,
                {label.x + titleWidth + 8.0f, rect.y + (headerHeight - 12.0f) * .5f, 12.0f, 12.0f}, style.headerForeground);
            label.width = titleWidth;
        }
        canvas.drawTextEllipsized(columns_[columnIndex].header, label,
            style.headerForeground, style.headerFontSize, columns_[columnIndex].alignment);
        if (focused() && focusedHeader_ == columnIndex)
            canvas.strokeRect(Rect{x + 1, rect.y + 1, std::max(0.0f,width - 2), headerHeight - 2}, theme().focusOutline, 3, 1);
        x += width;
    }

    if (rows_.empty() || contentRect.height <= 0.0f || height <= 0.0f) return;
    const int first = std::max(0, static_cast<int>(std::floor(scrollOffset_ / height)) - 1);
    const int last = std::min(
        static_cast<int>(rows_.size()),
        static_cast<int>(std::ceil((scrollOffset_ + contentRect.height) / height)) + 1);
    canvas.save();
    canvas.clipRect(contentRect);
    for (int rowIndex = first; rowIndex < last; ++rowIndex) {
        const Rect row = rowRect(rowIndex, height);
        if (selection_.contains(rowIndex)) canvas.fillRect(row, style.rowSelected, 0.0f);
        else if (rowIndex == pressedIndex_) canvas.fillRect(row, style.rowPressed, 0.0f);
        else if (rowIndex == hoveredIndex_) canvas.fillRect(row, style.rowHovered, 0.0f);
        if (rowIndex > 0) {
            canvas.drawLine(Point{rect.x, row.y}, Point{rect.x + rect.width, row.y}, style.gridLine, 1.0f);
        }

        x = rect.x + selectionGutter();
        if (selectionColumnVisible_) paintCheck(row.y,row.height,selection_.contains(rowIndex),false);
        const auto& values = richRows_[static_cast<std::size_t>(rowIndex)];
        for (int columnIndex = 0; columnIndex < static_cast<int>(columns_.size()); ++columnIndex) {
            const float width = columnWidth(columnIndex, remainingWidth, flexibleCount);
            const TableCell empty;
            const auto& cell = columnIndex < static_cast<int>(values.size())
                ? values[static_cast<std::size_t>(columnIndex)] : empty;
            const auto& text = cell.text;
            const auto percentage = columns_[static_cast<std::size_t>(columnIndex)].kind == TableCellKind::Percent
                ? percentValue(text) : std::optional<float>{};
            if (percentage) {
                const auto content = Rect{x, row.y, width, row.height}.inset(style.cellPadding);
                const float textWidth = std::min(std::max(0.0f, content.width),
                    canvas.measureTextWidth(text, style.fontSize, cell.fontWeight));
                canvas.drawTextEllipsized(
                    text,
                    Rect{content.x, content.y, textWidth, content.height},
                    cell.foreground.value_or(style.cellForeground),
                    style.fontSize,
                    TextAlign::Left);
                const float gap = style.cellPadding.right;
                const float barX = content.x + std::min(std::max(0.0f, content.width), textWidth + gap);
                const float barWidth = std::max(0.0f, content.width - textWidth - gap);
                const float barHeight = std::min(style.scrollbarWidth, std::max(0.0f, content.height));
                const Rect track{barX, content.y + (content.height - barHeight) * 0.5f, barWidth, barHeight};
                canvas.fillRect(track, style.gridLine, barHeight * 0.5f);
                canvas.fillRect(
                    Rect{track.x, track.y, track.width * *percentage / 100.0f, track.height},
                    cell.indicator.value_or(disabled() ? style.scrollbarColor : theme().primary),
                    barHeight * 0.5f);
                x += width;
                continue;
            }
            Rect cellRect = Rect{x, row.y, width, row.height}.inset(style.cellPadding);
            const Color foreground = cell.foreground.value_or(style.cellForeground);
            float leading = cellRect.x;
            const bool workbenchFile = cell.leadingIcon == IconSymbol::OutlineWorkbenchFolder
                || cell.leadingIcon == IconSymbol::OutlineWorkbenchFile;
            const float iconSize = std::clamp(workbenchFile ? 18.0f : cell.iconSize, 12.0f, std::max(12.0f, row.height - 12.0f));
            canvas.save();
            canvas.clipRect(Rect{x, row.y, std::max(0.0f, width), row.height});
            if (columns_[columnIndex].action && cell.leadingIcon && text.empty()) {
                paintIcon(canvas, *cell.leadingIcon,
                    Rect{x + (width - iconSize) * 0.5f, row.y + (row.height - iconSize) * 0.5f, iconSize, iconSize}, foreground);
                canvas.restore();
                x += width;
                continue;
            }
            if (cell.leadingIcon) {
                paintIcon(
                    canvas,
                    *cell.leadingIcon,
                    Rect{leading, row.y + (row.height - iconSize) * 0.5f, iconSize, iconSize},
                    foreground);
                leading += iconSize + style.cellIconGap;
            }
            if (cell.indicator) {
                constexpr float diameter = 7.0f;
                canvas.fillEllipse(
                    Rect{leading, row.y + (row.height - diameter) * 0.5f, diameter, diameter},
                    *cell.indicator);
                leading += diameter + 8.0f;
            }
            float trailingReserve = 0.0f;
            if (cell.trailingIcon) trailingReserve = iconSize + 6.0f;
            Rect textRect{
                leading,
                cellRect.y,
                std::max(0.0f, cellRect.x + cellRect.width - leading - trailingReserve),
                cellRect.height};
            const float fontSize = cell.fontSize > 0.0f ? cell.fontSize : style.fontSize;
            const bool twoLines = !cell.detail.empty();
            if (twoLines) { textRect.y = row.y + (row.height - 34.0f) * 0.5f; textRect.height = 18.0f; }
            if (!cell.badge.empty()) {
                const float badgeWidth = std::min(textRect.width * 0.35f, std::max(18.0f, canvas.measureTextWidth(cell.badge, 11.0f, 400) + 8.0f));
                const float titleWidth = std::min(canvas.measureTextWidth(text, fontSize, cell.fontWeight) + 2.0f, std::max(0.0f, textRect.width - badgeWidth - 6.0f));
                const Rect badge{textRect.x + titleWidth + 6.0f, textRect.y + (textRect.height - 16.0f) / 2.0f, badgeWidth, 16.0f};
                canvas.fillRect(badge, cell.badgeBackground.value_or(Color{foreground.r, foreground.g, foreground.b, 18}), 3.0f);
                canvas.drawTextEllipsized(cell.badge, badge, cell.badgeForeground.value_or(style.detailForeground), 11.0f, TextAlign::Center);
                textRect.width = titleWidth;
            }
            canvas.drawTextStyledEllipsized(text, textRect, foreground, fontSize, cell.alignment, cell.fontWeight);
            if (twoLines) {
                canvas.drawTextStyledEllipsized(cell.detail,
                    Rect{leading, textRect.y + 18.0f, std::max(0.0f, cellRect.x + cellRect.width - leading), 16.0f},
                    style.detailForeground, style.detailFontSize, TextAlign::Left, 400);
            }
            if (cell.trailingIcon) {
                paintIcon(
                    canvas,
                    *cell.trailingIcon,
                    Rect{cellRect.x + cellRect.width - iconSize,
                         row.y + (row.height - iconSize) * 0.5f,
                         iconSize,
                         iconSize},
                    foreground);
            }
            canvas.restore();
            x += width;
        }
    }
    if (reordering_ && reorderInsertionIndex_ >= 0) {
        const float rawY = contentRect.y
            + static_cast<float>(reorderInsertionIndex_) * height
            - scrollOffset_;
        const float indicatorY = std::clamp(
            rawY,
            contentRect.y + 1.0f,
            contentRect.y + contentRect.height - 1.0f);
        const float indicatorInset = std::max(4.0f, style.cellPadding.left);
        canvas.drawLine(
            Point{contentRect.x + indicatorInset, indicatorY},
            Point{contentRect.x + contentRect.width - indicatorInset, indicatorY},
            theme().focusOutline,
            2.0f);
    }
    canvas.restore();

    if (maxScrollOffset() > 0.001f) {
        const Rect thumb = verticalThumbRect(style.scrollbarWidth, contentRect.height);
        canvas.fillRect(thumb, style.scrollbarColor, thumb.width / 2.0f);
    }
}

TableStyle Table::resolvedStyle() const {
    const auto& t = theme();
    TableStyle style;
    style.background = disabled() ? t.surfaceMuted : t.surface;
    style.border = t.border;
    style.headerBackground = t.surfaceMuted;
    style.headerForeground = disabled() ? t.textSubtle : t.textMuted;
    style.cellForeground = disabled() ? t.textSubtle : t.text;
    style.detailForeground = t.textMuted;
    style.gridLine = t.border;
    style.rowHovered = t.surfaceMuted;
    style.rowPressed = t.pressedBackground;
    style.rowSelected = t.selectedBackground;
    style.scrollbarColor = t.textSubtle;
    style.borderWidth = 1.0f;
    style.radius = t.radiusMd;
    style.headerHeight = 32.0f;
    style.scrollbarWidth = 4.0f;
    style.cellPadding = Insets{0.0f, 10.0f};
    if (styleOverride_) applyTableStyleOverride(style, *styleOverride_);
    if (headerHeight_) style.headerHeight = *headerHeight_;
    return style;
}

