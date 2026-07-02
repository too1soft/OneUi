#pragma once

#include "oneui/export.h"
#include "oneui/geometry.h"
#include "oneui/style_sheet.h"

#include <array>

namespace oneui {

struct ProductShellMetrics {
    float compactBreakpoint = 820.0f;
    float sidebarWidth = 184.0f;
    float headerHeight = 86.0f;
    float footerHeight = 30.0f;
    float contentPadding = 18.0f;
    float cardGap = 14.0f;
    float cardPadding = 16.0f;
    float cardHeaderHeight = 30.0f;
    float controlHeight = 26.0f;
    float labelWidth = 108.0f;
    float buttonWidth = 96.0f;
    float serviceCardHeight = 74.0f;
    float actionCardHeight = 154.0f;
    float minLogHeight = 132.0f;
    float assistMaxWidth = 640.0f;
    float assistTopInset = 18.0f;
    float assistInputHeight = 42.0f;
    float assistButtonWidth = 190.0f;
    float assistButtonHeight = 42.0f;
    float assistRecentCardWidth = 190.0f;
    float assistRecentCardHeight = 88.0f;
    float windowTitleBarHeight = 34.0f;
    float windowBorderWidth = 1.0f;
    float windowTitleButtonWidth = 46.0f;
    float sidebarProfileTop = 18.0f;
    float sidebarNavTop = 82.0f;
    float sidebarNavHeight = 38.0f;
    float sidebarNavGap = 6.0f;
    float topBarSearchWidth = 472.0f;
    float topBarSearchHeight = 32.0f;
    float topBarPromoWidth = 290.0f;
    float topBarSegmentWidth = 112.0f;
    float topBarActionSize = 32.0f;
};

struct ProductShellLayout {
    Rect sidebar;
    Rect header;
    Rect content;
    Rect footer;
    bool sidebarVisible = false;
};

struct ProductSidebarLayout {
    Rect avatar;
    Rect accountName;
    Rect accountMeta;
    Rect renewPill;
    Rect expandChevron;
    std::array<Rect, 6> navItems{};
    std::array<Rect, 6> navIcons{};
    int navItemCount = 0;
    Rect separator;
    Rect bottomSettings;
    Rect bottomSettingsIcon;
};

struct ProductTopBarLayout {
    Rect search;
    Rect promo;
    Rect classicSegment;
    Rect newSegment;
    Rect notification;
};

struct ProductDashboardLayout {
    Rect serviceCard;
    Rect primaryCard;
    Rect secondaryCard;
    Rect logCard;
    bool twoColumns = false;
};

struct ProductFormRowLayout {
    Rect label;
    Rect control;
    Rect trailing;
};

struct ProductAssistHomeLayout {
    Rect localTitle;
    Rect localDevicePill;
    Rect localSwitch;
    Rect localCode;
    Rect permanentCode;
    Rect localCredentialInput;
    Rect generateButton;
    Rect remoteTitle;
    Rect remoteDeviceInput;
    Rect remoteCodeInput;
    Rect remoteConnectButton;
    Rect remoteStopButton;
    Rect remoteModeDesktop;
    Rect remoteModeFile;
    Rect recentTitle;
    std::array<Rect, 6> recentCards{};
    int recentCardCount = 0;
};

struct ProductStatusStripLayout {
    Rect icon;
    Rect title;
    Rect message;
    Rect copyButton;
    Rect detailsButton;
    Rect details;
};

struct ProductWindowChromeLayout {
    Rect frame;
    Rect titleBar;
    Rect caption;
    Rect minimizeButton;
    Rect maximizeButton;
    Rect closeButton;
    Rect content;
};

struct ProductShellStyle {
    StyleBox window;
    StyleBox titleBar;
    StyleBox sidebar;
    StyleBox header;
    StyleBox content;
    StyleBox footer;
    StyleBox card;
    StyleBox navItem;
    StyleBox selectedNavItem;
    StyleBox statusStrip;
};

ONEUI_API ProductShellLayout computeProductShellLayout(Size viewport, const ProductShellMetrics& metrics = {});
ONEUI_API ProductSidebarLayout computeProductSidebarLayout(Rect sidebar, const ProductShellMetrics& metrics = {});
ONEUI_API ProductTopBarLayout computeProductTopBarLayout(Rect header, const ProductShellMetrics& metrics = {});
ONEUI_API ProductDashboardLayout computeProductDashboardLayout(Rect content, const ProductShellMetrics& metrics = {});
ONEUI_API ProductFormRowLayout computeProductFormRowLayout(
    Rect card,
    float rowTop,
    float trailingWidth,
    const ProductShellMetrics& metrics = {});
ONEUI_API ProductAssistHomeLayout computeProductAssistHomeLayout(
    Rect content,
    const ProductShellMetrics& metrics = {});
ONEUI_API ProductStatusStripLayout computeProductStatusStripLayout(
    Rect strip,
    bool expanded = false);
ONEUI_API ProductWindowChromeLayout computeProductWindowChromeLayout(
    Size viewport,
    const ProductShellMetrics& metrics = {});

} // namespace oneui
