#pragma once
#include "models.hpp"
#include <array>

namespace lab::terminal {
class Sessions {
    SessionsModel &model_;
    oneui::ui::Mount &mount_;
    oneui::Window &window_;
    std::shared_ptr<void> owner_;
    std::array<std::shared_ptr<oneui::TerminalView>, 4> terminals_, secondary_;
    std::shared_ptr<oneui::TextField> input_;
    std::shared_ptr<oneui::VirtualList> connections_ = std::make_shared<oneui::VirtualList>();
    std::vector<int> ids_{0, 1}, rows_;
    std::vector<oneui::Subscription> subscriptions_;
    bool updating_ = false;
    int selected_ = 0, searchIndex_ = -1;
    void selection(const std::wstring &key);
    void filter();
    void rebuild();

  public:
    Sessions(SessionsModel &, oneui::ui::Mount &, oneui::Window &);
    ~Sessions();
    void open(int id);
    void close(int index);
    void restore();
    void send();
    void search(bool next);
    void focus(bool command = false);
    int selected() const { return selected_; }
    const auto &terminals() const { return terminals_; }
    const auto &secondary() const { return secondary_; }
    const auto &input() const { return input_; }
    const auto &connections() const { return connections_; }
};
} // namespace lab::terminal
