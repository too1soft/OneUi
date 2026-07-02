#include "oneui/layout/product_shell.h"

#include <algorithm>
#include <cstddef>

namespace oneui {

ProductShellLayout computeProductShellLayout(Size viewport, const ProductShellMetrics& metrics) {
    const float width = std::max(0.0f, viewport.width);
    const float height = std::max(0.0f, viewport.height);
    const bool sidebarVisible = width >= metrics.compactBreakpoint && metrics.sidebarWidth > 0.0f;
    const float sidebarWidth = sidebarVisible ? std::min(metrics.sidebarWidth, width) : 0.0f;
    const float mainX = sidebarWidth;
    const float mainWidth = std::max(0.0f, width - sidebarWidth);
    const float headerHeight = std::min(std::max(0.0f, metrics.headerHeight), height);
    const float footerHeight = std::min(std::max(0.0f, metrics.footerHeight), std::max(0.0f, height - headerHeight));

    ProductShellLayout layout;
    layout.sidebarVisible = sidebarVisible;
    layout.sidebar = Rect{0.0f, 0.0f, sidebarWidth, height};
    layout.header = Rect{mainX, 0.0f, mainWidth, headerHeight};
    layout.footer = Rect{mainX, std::max(0.0f, height - footerHeight), mainWidth, footerHeight};
    layout.content = Rect{
        mainX,
        headerHeight,
        mainWidth,
        std::max(0.0f, height - headerHeight - footerHeight)
    };
    return layout;
}

ProductSidebarLayout computeProductSidebarLayout(Rect sidebar, const ProductShellMetrics& metrics) {
    const float x = sidebar.x;
    const float y = sidebar.y;
    const float width = std::max(0.0f, sidebar.width);
    const float height = std::max(0.0f, sidebar.height);
    const float navLeft = x + 8.0f;
    const float navWidth = std::max(0.0f, width - 16.0f);
    const float navHeight = std::max(26.0f, metrics.sidebarNavHeight);
    const float navGap = std::max(0.0f, metrics.sidebarNavGap);

    ProductSidebarLayout layout;
    layout.avatar = Rect{x + 12.0f, y + metrics.sidebarProfileTop + 3.0f, 32.0f, 32.0f};
    layout.accountName = Rect{x + 54.0f, y + metrics.sidebarProfileTop - 1.0f, std::max(0.0f, width - 84.0f), 20.0f};
    layout.accountMeta = Rect{x + 54.0f, y + metrics.sidebarProfileTop + 22.0f, 56.0f, 18.0f};
    layout.renewPill = Rect{x + 111.0f, y + metrics.sidebarProfileTop + 22.0f, 36.0f, 18.0f};
    layout.expandChevron = Rect{x + width - 28.0f, y + metrics.sidebarProfileTop + 12.0f, 14.0f, 14.0f};
    layout.navItemCount = 4;
    for (int index = 0; index < layout.navItemCount; ++index) {
        const float itemTop = y + metrics.sidebarNavTop + static_cast<float>(index) * (navHeight + navGap);
        layout.navItems[static_cast<std::size_t>(index)] = Rect{navLeft, itemTop, navWidth, navHeight};
        layout.navIcons[static_cast<std::size_t>(index)] = Rect{navLeft + 12.0f, itemTop + (navHeight - 16.0f) * 0.5f, 16.0f, 16.0f};
    }
    layout.separator = Rect{x + 20.0f, y + metrics.sidebarNavTop + (navHeight + navGap) * 3.0f + navHeight + 8.0f, std::max(0.0f, width - 40.0f), 1.0f};
    layout.bottomSettings = Rect{navLeft, y + std::max(0.0f, height - 62.0f), navWidth, navHeight};
    layout.bottomSettingsIcon = Rect{layout.bottomSettings.x + 12.0f, layout.bottomSettings.y + (navHeight - 16.0f) * 0.5f, 16.0f, 16.0f};
    return layout;
}

ProductTopBarLayout computeProductTopBarLayout(Rect header, const ProductShellMetrics& metrics) {
    const float top = header.y + 12.0f;
    const float searchWidth = std::min(std::max(220.0f, metrics.topBarSearchWidth), std::max(0.0f, header.width - 260.0f));
    const float searchX = header.x + std::max(22.0f, std::min(108.0f, header.width * 0.12f));
    const float right = header.x + header.width - 20.0f;
    const float action = std::max(28.0f, metrics.topBarActionSize);
    const float promoWidth = std::min(std::max(120.0f, metrics.topBarPromoWidth), std::max(0.0f, right - searchX - searchWidth - 28.0f));

    ProductTopBarLayout layout;
    layout.search = Rect{searchX, top, searchWidth, std::max(28.0f, metrics.topBarSearchHeight)};
    layout.notification = Rect{right - action, top, action, action};
    layout.newSegment = Rect{layout.notification.x - 74.0f, top, 56.0f, action};
    layout.classicSegment = Rect{layout.newSegment.x - 58.0f, top, 58.0f, action};
    layout.promo = Rect{std::max(layout.search.x + layout.search.width + 18.0f, layout.classicSegment.x - promoWidth - 12.0f), top, promoWidth, action};
    return layout;
}

ProductDashboardLayout computeProductDashboardLayout(Rect content, const ProductShellMetrics& metrics) {
    const float padding = std::max(0.0f, metrics.contentPadding);
    const float gap = std::max(0.0f, metrics.cardGap);
    const float x = content.x + padding;
    const float y = content.y + padding;
    const float width = std::max(0.0f, content.width - padding * 2.0f);
    const float height = std::max(0.0f, content.height - padding * 2.0f);
    const float serviceHeight = std::min(std::max(0.0f, metrics.serviceCardHeight), height);
    const float cardHeight = std::max(0.0f, metrics.actionCardHeight);
    const bool twoColumns = width >= 660.0f;
    const float columnWidth = twoColumns ? std::max(0.0f, (width - gap) / 2.0f) : width;

    ProductDashboardLayout layout;
    layout.twoColumns = twoColumns;
    layout.serviceCard = Rect{x, y, width, serviceHeight};

    const float cardsY = y + serviceHeight + gap;
    layout.primaryCard = Rect{x, cardsY, columnWidth, cardHeight};
    layout.secondaryCard = Rect{
        twoColumns ? (x + columnWidth + gap) : x,
        twoColumns ? cardsY : (cardsY + cardHeight + gap),
        columnWidth,
        cardHeight
    };

    const float logY = std::max(
        layout.primaryCard.y + layout.primaryCard.height,
        layout.secondaryCard.y + layout.secondaryCard.height) + gap;
    layout.logCard = Rect{x, logY, width, std::max(metrics.minLogHeight, y + height - logY)};
    return layout;
}

ProductFormRowLayout computeProductFormRowLayout(Rect card, float rowTop, float trailingWidth, const ProductShellMetrics& metrics) {
    const float padding = std::max(0.0f, metrics.cardPadding);
    const float labelWidth = std::max(0.0f, metrics.labelWidth);
    const float controlHeight = std::max(1.0f, metrics.controlHeight);
    const float trailing = std::max(0.0f, trailingWidth);
    const float gap = std::max(0.0f, metrics.cardGap);
    const float left = card.x + padding;
    const float right = card.x + std::max(0.0f, card.width - padding);
    const float labelRight = std::min(right, left + labelWidth);
    const float trailingX = std::max(labelRight, right - trailing);
    const float controlRight = trailing > 0.0f ? std::max(labelRight, trailingX - gap) : right;

    ProductFormRowLayout row;
    row.label = Rect{left, rowTop + 2.0f, std::max(0.0f, labelRight - left), controlHeight};
    row.control = Rect{labelRight, rowTop, std::max(0.0f, controlRight - labelRight), controlHeight};
    row.trailing = Rect{trailingX, rowTop, trailing, controlHeight};
    return row;
}

ProductAssistHomeLayout computeProductAssistHomeLayout(Rect content, const ProductShellMetrics& metrics) {
    const float padding = std::max(0.0f, metrics.contentPadding);
    const float gap = std::max(0.0f, metrics.cardGap);
    const float maxWidth = std::max(320.0f, metrics.assistMaxWidth);
    const float availableWidth = std::max(0.0f, content.width - padding * 2.0f);
    const float width = std::min(maxWidth, availableWidth);
    const float x = content.x + padding + std::max(0.0f, (availableWidth - width) / 2.0f);
    const float y = content.y + std::max(0.0f, metrics.assistTopInset);
    const float inputHeight = std::max(24.0f, metrics.assistInputHeight);
    const float buttonWidth = std::min(std::max(100.0f, metrics.assistButtonWidth), width);
    const float buttonHeight = std::max(28.0f, metrics.assistButtonHeight);
    const float codeInputWidth = std::max(0.0f, width - buttonWidth - gap);
    const float modeTop = y + 330.0f;
    const float recentTop = y + 376.0f;
    const float cardWidth = std::max(120.0f, metrics.assistRecentCardWidth);
    const float cardHeight = std::max(64.0f, metrics.assistRecentCardHeight);
    const int columns = width >= cardWidth * 3.0f + gap * 2.0f ? 3 : (width >= cardWidth * 2.0f + gap ? 2 : 1);

    ProductAssistHomeLayout layout;
    layout.localTitle = Rect{x, y, width, 28.0f};
    layout.localSwitch = Rect{x + std::min(std::max(0.0f, width - 48.0f), 162.0f), y + 2.0f, 48.0f, 24.0f};
    layout.localDevicePill = Rect{x + std::min(std::max(0.0f, width - 104.0f), 220.0f), y - 1.0f, 104.0f, 28.0f};
    layout.localCode = Rect{x, y + 54.0f, width, 56.0f};
    layout.permanentCode = Rect{x, y + 114.0f, width, 28.0f};
    layout.localCredentialInput = Rect{x, y + 164.0f, codeInputWidth, inputHeight};
    layout.generateButton = Rect{x + codeInputWidth + gap, y + 164.0f, buttonWidth, inputHeight};
    layout.remoteTitle = Rect{x, y + 234.0f, width, 28.0f};
    layout.remoteDeviceInput = Rect{x, y + 282.0f, std::max(0.0f, (width - buttonWidth - gap) * 0.58f), inputHeight};
    layout.remoteCodeInput = Rect{
        layout.remoteDeviceInput.x + layout.remoteDeviceInput.width + gap,
        y + 282.0f,
        std::max(0.0f, width - layout.remoteDeviceInput.width - buttonWidth - gap * 2.0f),
        inputHeight};
    layout.remoteConnectButton = Rect{x + width - buttonWidth, y + 282.0f, buttonWidth, buttonHeight};
    layout.remoteStopButton = Rect{x + width - buttonWidth, y + 282.0f + buttonHeight + 8.0f, buttonWidth, 30.0f};
    layout.remoteModeDesktop = Rect{x, modeTop, 98.0f, 24.0f};
    layout.remoteModeFile = Rect{x + 118.0f, modeTop, 98.0f, 24.0f};
    layout.recentTitle = Rect{x, recentTop, width, 24.0f};
    layout.recentCardCount = 6;

    const float cardsTop = recentTop + 34.0f;
    const float computedCardWidth = columns > 1 ? std::min(cardWidth, (width - gap * static_cast<float>(columns - 1)) / static_cast<float>(columns)) : width;
    for (int index = 0; index < layout.recentCardCount; ++index) {
        const int row = index / columns;
        const int column = index % columns;
        layout.recentCards[static_cast<std::size_t>(index)] = Rect{
            x + static_cast<float>(column) * (computedCardWidth + gap),
            cardsTop + static_cast<float>(row) * (cardHeight + gap),
            computedCardWidth,
            cardHeight};
    }
    return layout;
}

ProductStatusStripLayout computeProductStatusStripLayout(Rect strip, bool expanded) {
    const float left = strip.x + 16.0f;
    const float top = strip.y + 10.0f;
    const float right = strip.x + std::max(0.0f, strip.width - 14.0f);
    const float actionHeight = 26.0f;
    const float detailsWidth = 48.0f;
    const float copyWidth = 48.0f;

    ProductStatusStripLayout layout;
    layout.icon = Rect{left, top + 6.0f, 16.0f, 16.0f};
    layout.title = Rect{left + 26.0f, top, 96.0f, 20.0f};
    layout.copyButton = Rect{std::max(left, right - copyWidth - detailsWidth - 8.0f), top + 1.0f, copyWidth, actionHeight};
    layout.detailsButton = Rect{std::max(left, right - detailsWidth), top + 1.0f, detailsWidth, actionHeight};
    layout.message = Rect{
        left + 26.0f,
        top + 21.0f,
        std::max(0.0f, layout.copyButton.x - (left + 26.0f) - 12.0f),
        20.0f};
    layout.details = expanded
        ? Rect{left + 26.0f, strip.y + 58.0f, std::max(0.0f, strip.width - 56.0f), std::max(0.0f, strip.height - 68.0f)}
        : Rect{left + 26.0f, strip.y + strip.height, std::max(0.0f, strip.width - 56.0f), 0.0f};
    return layout;
}

ProductWindowChromeLayout computeProductWindowChromeLayout(Size viewport, const ProductShellMetrics& metrics) {
    const float width = std::max(0.0f, viewport.width);
    const float height = std::max(0.0f, viewport.height);
    const float border = std::min(std::max(0.0f, metrics.windowBorderWidth), std::min(width, height) / 2.0f);
    const float titleHeight = std::min(std::max(24.0f, metrics.windowTitleBarHeight), std::max(0.0f, height - border * 2.0f));
    const float buttonWidth = std::min(std::max(32.0f, metrics.windowTitleButtonWidth), std::max(0.0f, width / 3.0f));
    const float titleY = border;
    const float buttonY = titleY;
    const float buttonHeight = titleHeight;

    ProductWindowChromeLayout layout;
    layout.frame = Rect{0.0f, 0.0f, width, height};
    layout.titleBar = Rect{border, titleY, std::max(0.0f, width - border * 2.0f), titleHeight};
    layout.closeButton = Rect{std::max(border, width - border - buttonWidth), buttonY, buttonWidth, buttonHeight};
    layout.maximizeButton = Rect{std::max(border, layout.closeButton.x - buttonWidth), buttonY, buttonWidth, buttonHeight};
    layout.minimizeButton = Rect{std::max(border, layout.maximizeButton.x - buttonWidth), buttonY, buttonWidth, buttonHeight};
    layout.caption = Rect{border + 12.0f, titleY, std::max(0.0f, layout.minimizeButton.x - border - 24.0f), titleHeight};
    layout.content = Rect{
        border,
        titleY + titleHeight,
        std::max(0.0f, width - border * 2.0f),
        std::max(0.0f, height - titleHeight - border * 2.0f)};
    return layout;
}

} // namespace oneui
