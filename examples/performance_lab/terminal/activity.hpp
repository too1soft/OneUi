#pragma once
#include "models.hpp"
#include <condition_variable>
#include <oneui/application.h>
#include <thread>

namespace lab::terminal {
// Runs only while the visible page has explicitly enabled sampling or a simulated transfer.
class Activity {
    MonitorModel &monitor_;
    FilesModel &files_;
    std::shared_ptr<oneui::Table> processes_ = std::make_shared<oneui::Table>();
    std::vector<oneui::Subscription> subscriptions_;
    oneui::Application::Dispatcher dispatch_;
    std::thread worker_;
    std::mutex mutex_;
    std::condition_variable wake_;
    std::shared_ptr<int> generation_;
    bool active_ = false, stopping_ = false;
    int sample_ = 0, ticks_ = 0;
    void update();
    void tick();
    void stop();

  public:
    Activity(MonitorModel &, FilesModel &, oneui::ui::Mount &, oneui::Application::Dispatcher);
    ~Activity();
    void activate();
    void deactivate();
    void sample();
};
} // namespace lab::terminal
