#pragma once
#include "activity.hpp"
#include "files.hpp"
#include "sessions.hpp"
#include <oneui/controls/log_view.h>
#include <oneui/layout/panel.h>

namespace lab::terminal {
class Workspace final {
    oneui::Window &window_;
    oneui::Application::Dispatcher dispatch_;
    std::shared_ptr<int> generation_;
    bool active_ = false, scheduled_ = false;
    WorkbenchModel model_;
    oneui::ui::Mount mount_;
    Sessions sessions_;
    Files files_;
    Activity activity_;
    std::shared_ptr<oneui::View> root_;
    std::shared_ptr<oneui::LogView> log_ = std::make_shared<oneui::LogView>();
    std::vector<oneui::Subscription> subscriptions_;
    bool wantSidebar_ = false, wantMonitor_ = true, wantFiles_ = true, filesFocused_ = false;
    float lastWidth_ = 0;
    void bindLayout();
    void schedule();

  public:
    Workspace(oneui::Window &, oneui::Application::Dispatcher);
    ~Workspace();
    void resize(float width);
    void theme(bool dark, bool compact);
    void activate();
    void deactivate();
    const auto &root() const { return root_; }
    auto &model() { return model_; }
    auto &mount() { return mount_; }
    auto &sessions() { return sessions_; }
    auto &files() { return files_; }
    auto &activity() { return activity_; }
};
} // namespace lab::terminal
