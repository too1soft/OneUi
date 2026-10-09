#include "popup_showcase.hpp"
#include <oneui/style_adapter.h>
#include <oneui/ui_native_theme.h>

namespace lab {
using namespace oneui;
namespace {
bool same(Rect a, Rect b) {
    return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
}
void append(Stack &stack, const std::shared_ptr<Widget> &child, float height) {
    stack.add(child);
    stack.setFlex(child, {0, 0, height, height});
}
std::shared_ptr<Stack> column() {
    auto stack = std::make_shared<Stack>();
    stack->setEngine(StackEngine::Yoga);
    stack->setGap(10);
    stack->setPadding(Insets{16});
    return stack;
}
void apply(Widget &widget, const StyleSheet &sheet) {
    ui::applyNativeTheme(widget, sheet, false);
    if (auto button = dynamic_cast<Button *>(&widget))
        applyDemoButton(*button, sheet);
    if (auto view = dynamic_cast<View *>(&widget))
        for (const auto &child : view->children())
            apply(*child, sheet);
}
} // namespace
PopupShowcase::PopupShowcase()
    : body_(column()), panel_(column()), popup_(std::make_shared<Popup>()),
      editor_(std::make_shared<TextField>()), trigger_(std::make_shared<Button>(L"打开弹层")),
      feedback_(std::make_shared<Label>(L"选择模式后打开；下方按钮用于检查外部点击。")) {
    setAccessibleName(L"弹层交互试验区");
    setContent(body_);
    const auto life = lifetimeToken();
    auto mode = std::make_shared<Select>();
    mode->setAccessibleName(L"弹层交互模式");
    mode->setItems({L"外部点击：继续操作", L"外部点击：关闭弹层", L"外部点击：阻止操作"});
    mode->setSelectedIndex(1);
    mode->setOnChanged([this, life](int index) {
        if (life.expired() || !active_)
            return;
        open_.set(false);
        mode_ = PopupInteractionMode(index);
    });
    append(*body_, mode, 38);
    auto placement = std::make_shared<Select>();
    placement->setAccessibleName(L"弹层首选位置");
    placement->setItems({L"位置：下方左对齐", L"位置：下方右对齐", L"位置：上方左对齐",
                         L"位置：上方右对齐", L"位置：左侧", L"位置：右侧"});
    placement->setSelectedIndex(0);
    placement->setOnChanged([this, life](int index) {
        if (life.expired() || !active_)
            return;
        placement_ = PopupPreferredPlacement(index);
        popup_->setPreferredPlacement(placement_);
    });
    append(*body_, placement, 38);
    auto escape = std::make_shared<Checkbox>(L"允许 Esc 关闭");
    escape->setChecked(true);
    escape->setAccessibleName(L"允许 Esc 关闭");
    escape->setOnChanged([weak = std::weak_ptr<Popup>(popup_)](bool value) {
        if (auto popup = weak.lock())
            popup->setCloseOnEscape(value);
    });
    append(*body_, escape, 30);
    auto controlled = std::make_shared<Checkbox>(L"绑定打开状态（与关闭按钮同步）");
    controlled->setAccessibleName(L"绑定弹层打开状态");
    controlled->bindChecked(open_);
    append(*body_, controlled, 30);
    auto background = std::make_shared<Button>(L"外部按钮：测试是否收到点击");
    background->setOnClick([this, life] {
        if (!life.expired() && active_)
            feedback_->setText(L"外部按钮已收到 " + std::to_wstring(++backgroundClicks_) +
                               L" 次点击");
    });
    append(*body_, background, 38);
    feedback_->setAccessibleName(L"弹层反馈");
    feedback_->setTextWrapping(true);
    feedback_->setMaxLines(3);
    append(*body_, feedback_, 62);
    auto spacer = std::make_shared<Label>();
    body_->add(spacer);
    body_->setFlex(spacer, {1, 1, 0});
    trigger_->setOnClick([this, life] {
        if (!life.expired() && active_)
            open_.set(!open_.get());
    });
    append(*body_, trigger_, 38);

    editor_->setAccessibleName(L"弹层草稿");
    editor_->setPlaceholder(L"输入内容；关闭再打开仍保留");
    append(*panel_, editor_, 38);
    auto nested = std::make_shared<Button>(L"打开第二层确认");
    nested->setOnClick([this, life] {
        if (!life.expired() && active_ && open_.get())
            showConfirmation();
    });
    append(*panel_, nested, 38);
    auto close = std::make_shared<Button>(L"关闭弹层并保留草稿");
    close->setOnClick([this, life] {
        if (!life.expired() && active_)
            open_.set(false);
    });
    append(*panel_, close, 38);
    panel_->setPreferredSize({320, 166});
    popup_->setContent(panel_);
    popup_->bindOpen(open_);
    openSubscription_ = open_.subscribeScoped([this](bool value) { showChanged(value); });
}
PopupShowcase::~PopupShowcase() { setActive(false); }
void PopupShowcase::setActive(bool active) {
    active_ = active;
    if (!active) {
        open_.set(false);
        closeConfirmation();
        clearOverlays();
        onFocusChanged(false);
    }
}
void PopupShowcase::showChanged(bool open) {
    if (!open) {
        closeConfirmation();
        removeOverlay(popup_.get());
        feedback_->setText(L"弹层已关闭；绑定状态为 false，草稿保留。");
        if (active_)
            requestFocus(trigger_.get(), true);
        return;
    }
    if (!active_) {
        open_.set(false);
        return;
    }
    popup_->setInteractionMode(mode_);
    popup_->setPreferredPlacement(placement_);
    updateGeometry();
    addOverlay(popup_, popup_->overlayOptions(10));
    requestFocus(editor_.get(), true);
    feedback_->setText(L"弹层已打开；边界不足时自动翻转位置。关闭可保留输入内容。");
}
void PopupShowcase::showConfirmation() {
    if (confirmation_)
        return;
    confirmation_ = std::make_shared<Dialog>(L"第二层确认", L"关闭后，焦点回到上一层草稿。");
    confirmation_->setAccessibleName(L"第二层确认");
    const auto life = lifetimeToken();
    auto close = [this, life] {
        if (life.expired() || !active_)
            return;
        closeConfirmation();
        if (open_.get())
            requestFocus(editor_.get(), true);
    };
    auto accept = std::make_shared<Button>(L"确认并返回草稿");
    applyDemoButton(*accept, *styles_);
    accept->setOnClick(close);
    confirmation_->setActions(accept);
    confirmation_->setOnClose(close);
    confirmation_->setStyleSheet(styles_, {"dialog", {"one-Dialog"}, 0});
    addAnchoredOverlay(confirmation_, OverlayOptions::modal(20), {360, 210}, Insets{8}, 1, 1);
    requestFocus(accept.get(), true);
}
void PopupShowcase::closeConfirmation() {
    if (confirmation_)
        removeOverlay(confirmation_.get());
    confirmation_.reset();
}
void PopupShowcase::updateGeometry() {
    const auto viewport = frame().inset(Insets{8});
    if (!same(popup_->frame(), viewport)) {
        popup_->setFrame(viewport);
        popup_->setViewport(viewport);
    }
    // Trigger layout is owned by Stack; PopupPlacement resolves/clamps the panel.
    if (!same(anchor_, trigger_->frame())) {
        anchor_ = trigger_->frame();
        popup_->setAnchorRect(anchor_);
    }
}
void PopupShowcase::paint(Canvas &canvas) {
    OverlayHost::paint(canvas);
    // Stack arranged the trigger during painting. A changed anchor schedules
    // one further frame; unchanged geometry never requests another paint.
    if (open_.get())
        updateGeometry();
}
void PopupShowcase::setTheme(std::shared_ptr<StyleSheet> sheet, bool dark) {
    styles_ = std::move(sheet);
    setStyleScope(styles_);
    apply(*body_, *styles_);
    apply(*panel_, *styles_);
    feedback_->setColor(dark ? Color{229, 236, 232} : Color{28, 43, 35});
    StyleBox box;
    box.background.color = dark ? Color{29, 40, 35} : Color{241, 247, 239};
    body_->setStyleBox(box);
    PopupStyleOverride popupStyle;
    popupStyle.background = dark ? Color{29, 40, 35} : Color{255, 255, 255};
    popupStyle.border = dark ? Color{103, 145, 116} : Color{112, 145, 123};
    popupStyle.padding = Insets{0};
    popup_->setStyleOverride(popupStyle);
    if (confirmation_)
        confirmation_->setStyleSheet(styles_, {"dialog", {"one-Dialog"}, 0});
}
} // namespace lab
