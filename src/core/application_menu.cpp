#include "internal/application_menu.h"
#include "oneui/controls/button.h"
#include "oneui/controls/popup.h"
#include "oneui/layout/scroll_view.h"
#include "oneui/ui_native_theme.h"
#include <algorithm>

namespace oneui::internal {
namespace {
class MenuSeparator : public Widget {
  public:
    void paint(Canvas &canvas) override {
        const auto b = frame();
        canvas.drawLine({b.x + 6, b.y + 4}, {b.x + b.width - 6, b.y + 4}, {128, 128, 128, 80});
    }
};
class MenuRow : public View {
  protected:
    void layoutChildren() override {
        float x = frame().x;
        for (const auto &child : children()) {
            const float width = std::max(48.f, child->naturalSize().width);
            child->setFrame({x, frame().y, width, 28});
            x += width;
        }
    }

  public:
    bool onKeyDown(const KeyEvent &e) override {
        if (e.key == Key::Left || e.key == Key::Right) {
            auto key = e;
            key.key = Key::Tab;
            key.shift = e.key == Key::Left;
            if (!View::onKeyDown(key)) {
                if (key.shift)
                    focusLastLeaf();
                else
                    focusFirstLeaf();
            }
            return true;
        }
        if (e.key == Key::Down) {
            auto key = e;
            key.key = Key::Enter;
            return View::onKeyDown(key);
        }
        return View::onKeyDown(e);
    }
};
class MenuShell : public View {
  public:
    Color background{255,255,255};
    void paint(Canvas& canvas)override {canvas.fillRect(frame(),background);View::paint(canvas);}
    std::shared_ptr<MenuRow> bar = std::make_shared<MenuRow>();
    std::shared_ptr<Widget> body;
    void update() {
        // Menus use F10/arrows; normal Tab navigation stays in the document.
        bar->setTabStop(false);
        std::vector<std::shared_ptr<Widget>> children{bar};
        if (body)
            children.push_back(body);
        reconcileChildren(std::move(children));
    }

  protected:
    void layoutChildren() override {
        const auto b = frame();
        const float h = bar->children().empty() ? 0.f : 28.f;
        bar->setVisible(h > 0);
        bar->setFrame({b.x, b.y, b.width, h});
        if (body)
            body->setFrame({b.x, b.y + h, b.width, std::max(0.f, b.height - h)});
    }
};
class MenuPanel : public View {
  public:
    std::function<void()> back;
    std::vector<Widget *> branches;
    std::weak_ptr<ScrollView> scroll;
    void revealFocus() {
        auto viewport = scroll.lock();
        auto *child = focusedChild();
        if (!viewport || !child)
            return;
        const auto bounds = viewport->frame(), target = child->frame();
        if (target.y < bounds.y)
            viewport->setScrollOffset(viewport->scrollOffset() + target.y - bounds.y);
        else if (target.y + target.height > bounds.y + bounds.height)
            viewport->setScrollOffset(viewport->scrollOffset() + target.y + target.height -
                                      bounds.y - bounds.height);
    }
    bool onKeyDown(const KeyEvent &e) override {
        if (e.key == Key::Left || e.key == Key::Escape) {
            auto action = back;
            if (action)
                action();
            return true;
        }
        if (e.key == Key::Home) {
            focusFirstLeaf();
            revealFocus();
            return true;
        }
        if (e.key == Key::End) {
            focusLastLeaf();
            revealFocus();
            return true;
        }
        if (e.key == Key::Up || e.key == Key::Down) {
            auto key = e;
            key.key = Key::Tab;
            key.shift = e.key == Key::Up;
            if (!View::onKeyDown(key)) {
                if (key.shift)
                    focusLastLeaf();
                else
                    focusFirstLeaf();
            }
            revealFocus();
            return true;
        }
        if (e.key == Key::Right) {
            if (std::find(branches.begin(), branches.end(), focusedChild()) == branches.end())
                return true;
            auto key = e;
            key.key = Key::Enter;
            return View::onKeyDown(key);
        }
        return View::onKeyDown(e);
    }

  protected:
    void layoutChildren() override {
        float y = frame().y;
        for (const auto &child : children()) {
            const float h = child->preferredSize().height;
            child->setFrame({frame().x, y, frame().width, h});
            y += h;
        }
    }
};
} // namespace
struct ApplicationMenuHost::State {
    ApplicationMenuHost &host;
    std::shared_ptr<MenuShell> shell = std::make_shared<MenuShell>();
    std::vector<NativeMenuItem> items;
    std::vector<std::shared_ptr<Popup>> popups;
    std::function<void(int)> action;
    CommandScope commands;
    std::vector<Subscription> shortcuts;
    bool closing = false;
    Rect viewport{};
    bool hasViewport = false;
    std::weak_ptr<Widget> returnFocus;
    bool keyboardMenu = false;
    std::shared_ptr<StyleSheet> palette;
    std::size_t paletteVersion=0;
    void styleButton(Button& button) {
        ButtonStateStyleOverride normal;
        const auto p=palette?ui::nativePalette(*palette):ui::NativePalette{};
        normal.background=p.surface;normal.foreground=p.ink;normal.borderWidth=0;
        normal.radius=0;normal.fontSize=13;normal.padding=Insets{4,12};normal.transition=TransitionSpec{0};
        ButtonStyleOverride style;style.normal=normal;
        normal.background=p.hover;style.hovered=normal;style.pressed=normal;
        normal.foreground=p.muted;normal.background=p.surface;style.disabled=normal;
        button.setStyleOverride(style);
    }
    void syncPalette() {
        const auto next=shell->body?shell->body->styleScope():nullptr;
        if(next==palette && (!next||next->version()==paletteVersion))return;
        palette=next;paletteVersion=next?next->version():0;
        const auto p=palette?ui::nativePalette(*palette):ui::NativePalette{};
        shell->background=p.surface;
        for(auto& child:shell->bar->children())if(auto button=dynamic_cast<Button*>(child.get()))styleButton(*button);
        close();
    }
    void restoreFocus() {
        keyboardMenu = false;
        if (auto target = returnFocus.lock())
            host.requestFocus(target.get(), true);
        returnFocus.reset();
    }
    bool blocked() const {
        for (auto child = shell->body; child; child = child->activeFocusChild())
            if (child->isCommandBoundary())
                return true;
        return false;
    }
    explicit State(ApplicationMenuHost &value) : host(value) {
        shell->update();
        host.setContent(shell);
    }
    void close(std::size_t depth = 0) {
        if (closing)
            return;
        closing = true;
        while (popups.size() > depth) {
            auto popup = popups.back();
            popups.pop_back();
            popup->setOnClosed({});
            popup->setOpen(false);
            host.removeOverlay(popup.get());
        }
        closing = false;
    }
    void invoke(int id) {
        if (blocked())
            return;
        auto callback = action;
        close();
        restoreFocus();
        if (callback)
            callback(id);
    }
    void open(const std::vector<NativeMenuItem> &values, Rect anchor, std::size_t depth) {
        if (blocked())
            return;
        close(depth);
        auto panel = std::make_shared<MenuPanel>();
        const auto weak = host.lifetimeToken();
        panel->back = [this, weak, depth] {
            if (!weak.expired())
                close(depth);
        };
        float height = 0, width = 180;
        for (const auto &item : values) {
            if (item.separator) {
                auto spacer = std::make_shared<MenuSeparator>();
                spacer->setPreferredSize({0, 8});
                panel->add(spacer);
                height += 8;
                continue;
            }
            const std::wstring mark =
                item.check == MenuCheck::Normal ? L""
                : item.checked ? (item.check == MenuCheck::Radio ? L"\u25cf " : L"\u2713 ")
                               : L"   ";
            auto button = std::make_shared<Button>(mark + item.text);
            styleButton(*button);
            button->setAccessibleName(item.text);
            if (item.check != MenuCheck::Normal)
                button->setAccessibleDescription(item.checked ? L"Checked" : L"Unchecked");
            button->setContentAlign(TextAlign::Left);
            button->setDisabled(!item.enabled);
            button->setTrailingText(item.children.empty() ? menuShortcutLabel(item.shortcut)
                                                          : L"\u203a");
            button->setPreferredSize({0, 28});
            if (!item.children.empty())
                panel->branches.push_back(button.get());
            const auto child = std::weak_ptr<Button>(button);
            button->setOnClick([this, weak, item, child, depth] {
                if (weak.expired())
                    return;
                if (item.children.empty()) {
                    invoke(item.id);
                    return;
                }
                if (auto button = child.lock())
                    open(item.children, button->frame(), depth + 1);
            });
            width = std::max(width, button->naturalSize().width + 24);
            height += 28;
            panel->add(button);
        }
        panel->setPreferredSize({width, height});
        auto scroll = std::make_shared<ScrollView>();
        scroll->setContent(panel);
        scroll->setContentHeight(height);
        scroll->setPreferredSize({width, height});
        panel->scroll = scroll;
        auto popup = std::make_shared<Popup>();
        popup->setContent(scroll);
        popup->setAnchorRect(anchor);
        popup->setViewport(host.frame());
        popup->setPreferredPlacement(depth ? PopupPreferredPlacement::RightStart
                                           : PopupPreferredPlacement::BottomStart);
        popup->setInteractionMode(PopupInteractionMode::LightDismiss);
        popup->setOnClosed([this, weak, depth] {
            if (!weak.expired())
                close(depth);
        });
        popups.push_back(popup);
        host.addOverlay(popup, popup->overlayOptions(100 + int(depth)));
        popup->setOpen(true);
        host.requestFocus(panel.get(), true);
        panel->focusFirstLeaf();
    }
};
ApplicationMenuHost::ApplicationMenuHost() : state_(std::make_unique<State>(*this)) {}
ApplicationMenuHost::~ApplicationMenuHost() { state_->close(); }
void ApplicationMenuHost::setApplicationContent(std::shared_ptr<Widget> content) {
    state_->shell->body = std::move(content);
    state_->shell->update();
    invalidate();
}
void ApplicationMenuHost::setAction(std::function<void(int)> action) {
    state_->action = std::move(action);
}
void ApplicationMenuHost::setItems(std::vector<NativeMenuItem> items) {
    auto &s = *state_;
    s.close();
    s.shortcuts.clear();
    s.items = std::move(items);
    s.shell->bar->clearChildren();
    const auto weak = lifetimeToken();
    for (const auto &item : s.items) {
        if (item.separator)
            continue;
        auto button = std::make_shared<Button>(item.text);
        s.styleButton(*button);
        button->setDisabled(!item.enabled);
        const auto child = std::weak_ptr<Button>(button);
        button->setOnClick([this, weak, item, child] {
            if (weak.expired())
                return;
            if (item.children.empty())
                state_->invoke(item.id);
            else if (auto b = child.lock())
                state_->open(item.children, b->frame(), 0);
        });
        s.shell->bar->add(button);
    }
    const auto registerKeys = [&](const auto &self, const std::vector<NativeMenuItem> &nodes,
                                  bool enabled) -> void {
        for (const auto &item : nodes) {
            if (!item.children.empty())
                self(self, item.children, enabled && item.enabled);
            else if (!item.separator && !item.shortcut.key.empty())
                s.shortcuts.push_back(s.commands.registerCommand(
                    std::to_string(item.id),
                    [this, weak, id = item.id] {
                        if (!weak.expired())
                            state_->invoke(id);
                    },
                    [active = enabled && item.enabled] { return active; }, item.shortcut));
        }
    };
    registerKeys(registerKeys, s.items, true);
    invalidate();
}
bool ApplicationMenuHost::onKeyDown(const KeyEvent &event) {
    const bool boundary = state_->blocked();
    if (!boundary && !state_->items.empty() && event.virtualKey == 0x79 && !event.control &&
        !event.alt && !event.win) {
        state_->close();
        if (state_->keyboardMenu) {
            state_->restoreFocus();
            return true;
        }
        auto focused = state_->shell->body;
        while (focused && focused->activeFocusChild())
            focused = focused->activeFocusChild();
        state_->returnFocus = focused;
        state_->keyboardMenu = true;
        requestFocus(state_->shell->bar.get(), true);
        state_->shell->bar->focusFirstLeaf();
        return true;
    }
    if (event.key == Key::Escape && state_->popups.empty() && state_->keyboardMenu) {
        state_->restoreFocus();
        return true;
    }
    // Event callbacks may replace the menu or destroy the owning window.
    const auto weak = lifetimeToken();
    if (OverlayHost::onKeyDown(event))
        return true;
    if (weak.expired())
        return true;
    if (!state_->popups.empty())
        return true;
    // Use the focused subtree so an application modal blocks background shortcuts.
    return oneui::dispatchCommandKey(state_->shell->body, &state_->commands, event);
}
bool ApplicationMenuHost::onFocusChanged(bool focused) {
    if (!focused)
        state_->close();
    return OverlayHost::onFocusChanged(focused);
}
void ApplicationMenuHost::paint(Canvas &canvas) {
    auto &s = *state_;
    s.syncPalette();
    const auto b = frame();
    if (!s.hasViewport || b.x != s.viewport.x || b.y != s.viewport.y ||
        b.width != s.viewport.width || b.height != s.viewport.height) {
        s.viewport = b;
        s.hasViewport = true;
        for (auto &popup : s.popups)
            popup->setViewport(b);
    }
    OverlayHost::paint(canvas);
}
} // namespace oneui::internal
