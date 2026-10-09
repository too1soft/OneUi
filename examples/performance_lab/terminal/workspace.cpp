#include "workspace.hpp"
#include "TerminalWorkbench.g.h"
#include <oneui/ui_theme.h>

namespace lab::terminal {
namespace {
class WorkspaceFrame final : public oneui::View {
  public:
    std::function<void(float)> resized;
    void layoutChildren() override {
        if (resized)
            resized(frame().width);
        for (const auto &child : children())
            child->setFrame(frame());
    }
};
} // namespace
Workspace::Workspace(oneui::Window &window, oneui::Application::Dispatcher dispatch)
    : window_(window), dispatch_(dispatch), mount_([this] { schedule(); }),
      sessions_(model_, mount_, window), files_(model_, mount_),
      activity_(model_, model_, mount_, std::move(dispatch)),
      root_(std::make_shared<WorkspaceFrame>()) {
    log_->setClipboard(std::make_shared<oneui::SystemClipboard>());
    log_->appendLine(L"[INFO] 本地模拟会话已就绪；日志仅保存在本次进程中。", {103, 181, 132});
    mount_.registerNative("eventLog", log_);
    model_.appendLog.setAction([this] {
        if (log_->lineCount() >= 100)
            log_->clearLines();
        log_->appendLine(L"[INFO] 本地事件 " + std::to_wstring(log_->lineCount() + 1),
                         {103, 181, 132});
    });
    model_.clearLog.setAction([this] { log_->clearLines(); });
    bindLayout();
    theme(true, false);
    auto view = build_TerminalWorkbench(model_, mount_);
    root_->add(view.widget);
    std::static_pointer_cast<WorkspaceFrame>(root_)->resized = [this](float width) {
        if (lastWidth_ != width) {
            resize(width);
            mount_.flush();
        }
    };
    mount_.flush();
}
Workspace::~Workspace() {
    deactivate();
    std::static_pointer_cast<WorkspaceFrame>(root_)->resized = {};
}
void Workspace::schedule() {
    if (!active_ || scheduled_)
        return;
    scheduled_ = true;
    auto weak = std::weak_ptr<int>(generation_);
    dispatch_([this, weak] {
        if (!weak.expired()) {
            scheduled_ = false;
            mount_.flush();
        }
    });
}
void Workspace::activate() {
    if (active_)
        return;
    active_ = true;
    generation_ = std::make_shared<int>(0);
    mount_.flush();
    activity_.activate();
}
void Workspace::deactivate() {
    active_ = false;
    generation_.reset();
    scheduled_ = false;
    activity_.deactivate();
}
void Workspace::theme(bool dark, bool compact) {
    const auto palette =
        dark
            ? R"(:root { --accent:#c4ed87; --accent-ink:#16230f; --selection:#36503e; --success:#b2dca4; })"
            : R"(:root { --accent:#28613d; --accent-ink:#ffffff; --selection:#d5e9d7; --success:#28613d; })";
    mount_.styles()->replace(oneui::ui::workspaceTheme(dark) + palette + styles_TerminalWorkbench(),
                             compact ? oneui::ui::Density::Compact
                                     : oneui::ui::Density::Comfortable);
    mount_.flush();
}
void Workspace::resize(float width) {
    lastWidth_ = width;
    const bool narrow = width < 900;
    const bool logs = model_.logVisible.get();
    oneui::ui::Batch batch;
    model_.workspaceVisible.set(!logs);
    model_.narrow.set(narrow);
    model_.wide.set(!narrow);
    model_.sidebarVisible.set(wantSidebar_ && !narrow && !logs);
    const auto pane = model_.compactTab.get();
    model_.mainVisible.set(!logs && (!narrow || pane != L"monitor"));
    model_.terminalVisible.set(!logs && (narrow ? pane == L"terminal" : !filesFocused_));
    model_.filesVisible.set(!logs && (narrow ? pane == L"files" : wantFiles_));
    model_.monitorVisible.set(!logs &&
                              (narrow ? pane == L"monitor" : wantMonitor_ && !filesFocused_));
}
void Workspace::bindLayout() {
    model_.restore.setAction([this] {
        sessions_.restore();
        model_.fileRatio.set(0.54f);
        model_.monitorRatio.set(0.70f);
        wantSidebar_ = false;
        wantMonitor_ = wantFiles_ = true;
        filesFocused_ = false;
        model_.filesCollapsed.set(false);
        model_.logVisible.set(false);
        model_.compactTab.set(L"terminal");
        resize(lastWidth_);
    });
    model_.toggleSidebar.setAction([this] {
        wantSidebar_ = !wantSidebar_;
        resize(lastWidth_);
    });
    model_.toggleMonitor.setAction([this] {
        model_.logVisible.set(false);
        if (model_.narrow.get())
            model_.compactTab.set(model_.compactTab.get() == L"monitor" ? L"terminal" : L"monitor");
        else
            wantMonitor_ = !wantMonitor_;
        filesFocused_ = false;
        resize(lastWidth_);
    });
    model_.toggleFiles.setAction([this] {
        wantFiles_ = !wantFiles_;
        resize(lastWidth_);
    });
    model_.showTerminal.setAction([this] {
        model_.logVisible.set(false);
        filesFocused_ = false;
        model_.compactTab.set(L"terminal");
        resize(lastWidth_);
    });
    model_.showFiles.setAction([this] {
        model_.logVisible.set(false);
        if (model_.narrow.get())
            model_.compactTab.set(L"files");
        else
            filesFocused_ = !filesFocused_;
        wantFiles_ = true;
        model_.filesCollapsed.set(false);
        resize(lastWidth_);
    });
    model_.showMonitor.setAction([this] {
        model_.logVisible.set(false);
        filesFocused_ = false;
        wantMonitor_ = true;
        model_.compactTab.set(L"monitor");
        resize(lastWidth_);
    });
    model_.showLogs.setAction([this] {
        model_.compactTab.set(L"logs");
        model_.logVisible.set(true);
        resize(lastWidth_);
    });
    model_.showProcesses.setAction([this] {
        model_.monitorTab.set(L"processes");
        model_.showMonitor.execute();
    });
    model_.toggleCommand.setAction([this] {
        model_.showTerminal.execute();
        model_.commandVisible.set(!model_.commandVisible.get());
        mount_.flush();
        sessions_.focus(model_.commandVisible.get());
    });
    model_.toggleSearch.setAction([this] {
        model_.searchVisible.set(!model_.searchVisible.get());
        mount_.flush();
        if (model_.searchVisible.get())
            window_.requestFocus(mount_.find("terminalSearch").get(), true);
        else if (sessions_.selected() >= 0) {
            sessions_.terminals()[sessions_.selected()]->clearSelection();
            sessions_.focus();
        }
    });
    model_.collapseFiles.setAction([this] {
        bool collapsed = !model_.filesCollapsed.get();
        if (collapsed) {
            filesFocused_ = false;
            if (model_.narrow.get())
                model_.compactTab.set(L"terminal");
            resize(lastWidth_);
        }
        model_.filesCollapsed.set(collapsed);
    });
    model_.showStatus.setAction([this] {
        model_.showTerminal.execute();
        sessions_.input()->setText(L"status");
        sessions_.send();
    });
    subscriptions_.push_back(model_.compactTab.subscribeScoped([this](const auto &key) {
        model_.logVisible.set(key == L"logs");
        resize(lastWidth_);
    }));
    subscriptions_.push_back(model_.filesCollapsed.subscribeScoped([this](bool value) {
        model_.filesExpanded.set(!value);
        model_.fileCollapseIcon.set(value ? L"right" : L"down");
    }));
}
} // namespace lab::terminal
