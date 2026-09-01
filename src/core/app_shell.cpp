#include "oneui/layout/app_shell.h"

#include <algorithm>
#include <utility>

namespace oneui {

namespace {

float fixedExtent(float configured, float preferred) {
    if (configured > 0.0f) {
        return configured;
    }
    return std::max(0.0f, preferred);
}

} // namespace

void AppShell::setSidebar(std::shared_ptr<Widget> child) {
    sidebar_ = std::move(child);
    rebuildChildren();
}

void AppShell::setHeader(std::shared_ptr<Widget> child) {
    header_ = std::move(child);
    rebuildChildren();
}

void AppShell::setContent(std::shared_ptr<Widget> child) {
    content_ = std::move(child);
    rebuildChildren();
}

void AppShell::setFooter(std::shared_ptr<Widget> child) {
    footer_ = std::move(child);
    rebuildChildren();
}

void AppShell::setPadding(Insets padding) {
    padding_ = padding;
    invalidate();
}

void AppShell::setGap(float gap) {
    gap_ = std::max(0.0f, gap);
    invalidate();
}

void AppShell::setSidebarWidth(float width) {
    sidebarWidth_ = std::max(0.0f, width);
    invalidate();
}

void AppShell::setHeaderHeight(float height) {
    headerHeight_ = std::max(0.0f, height);
    invalidate();
}

void AppShell::setFooterHeight(float height) {
    footerHeight_ = std::max(0.0f, height);
    invalidate();
}

void AppShell::setFooterSpanSidebar(bool span) {
    if (footerSpansSidebar_ == span) {
        return;
    }
    footerSpansSidebar_ = span;
    invalidate();
}

void AppShell::setSidebarVisible(bool visible) {
    if (sidebarVisible_ == visible) {
        return;
    }
    sidebarVisible_ = visible;
    invalidate();
}

void AppShell::setStyleBox(StyleBox style) {
    styleBox_ = std::move(style);
    invalidate();
}

void AppShell::clearStyleBox() {
    styleBox_.reset();
    invalidate();
}

bool AppShell::sidebarVisible() const {
    return sidebarVisible_;
}

float AppShell::sidebarWidth() const {
    return sidebarWidth_;
}

float AppShell::headerHeight() const {
    return headerHeight_;
}

float AppShell::footerHeight() const {
    return footerHeight_;
}

bool AppShell::footerSpansSidebar() const {
    return footerSpansSidebar_;
}

void AppShell::paint(Canvas& canvas) {
    if (styleBox_) {
        paintStyleBox(canvas, frame(), *styleBox_);
    }
    View::paint(canvas);
}

void AppShell::layoutChildren() {
    Rect available = frame().inset(padding_);
    available.width = std::max(0.0f, available.width);
    available.height = std::max(0.0f, available.height);

    Rect main = available;
    if (footerSpansSidebar_ && footer_ && footer_->visible()) {
        const float footerHeight = std::min(fixedExtent(footerHeight_, footer_->preferredSize().height), main.height);
        footer_->setFrame(Rect{available.x, available.y + available.height - footerHeight, available.width, footerHeight});
        const float used = std::min(main.height, footerHeight + gap_);
        main.height -= used;
    }

    if (sidebar_ && sidebar_->visible() && sidebarVisible_) {
        const float sidebarWidth = std::min(fixedExtent(sidebarWidth_, sidebar_->preferredSize().width), main.width);
        sidebar_->setFrame(Rect{main.x, main.y, sidebarWidth, main.height});
        const float used = std::min(main.width, sidebarWidth + gap_);
        main.x += used;
        main.width -= used;
    }

    if (header_ && header_->visible()) {
        const float headerHeight = std::min(fixedExtent(headerHeight_, header_->preferredSize().height), main.height);
        header_->setFrame(Rect{main.x, main.y, main.width, headerHeight});
        const float used = std::min(main.height, headerHeight + gap_);
        main.y += used;
        main.height -= used;
    }

    if (!footerSpansSidebar_ && footer_ && footer_->visible()) {
        const float footerHeight = std::min(fixedExtent(footerHeight_, footer_->preferredSize().height), main.height);
        footer_->setFrame(Rect{main.x, main.y + main.height - footerHeight, main.width, footerHeight});
        const float used = std::min(main.height, footerHeight + gap_);
        main.height -= used;
    }

    if (content_ && content_->visible()) {
        content_->setFrame(main);
    }
}

void AppShell::rebuildChildren() {
    clearChildren();
    if (sidebar_) {
        add(sidebar_);
    }
    if (header_) {
        add(header_);
    }
    if (content_) {
        add(content_);
    }
    if (footer_) {
        add(footer_);
    }
    invalidate();
}

} // namespace oneui
