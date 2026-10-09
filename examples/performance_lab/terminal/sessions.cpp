#include "sessions.hpp"
#include "bridge.hpp"
#include "internal/unicode.h"
#include <cwctype>
#include <oneui/native_interop.h>

namespace lab::terminal {
namespace {
const std::array<std::wstring, 4> keys{L"edge", L"build", L"database", L"cache"};
const std::array<std::wstring, 4> names{L"edge-gateway", L"build-node", L"postgres-lab",
                                        L"cache-lab"};
const std::array<std::wstring, 4> addresses{L"192.0.2.10", L"192.0.2.24", L"192.0.2.36",
                                            L"192.0.2.48"};
std::wstring lower(std::wstring text) {
    for (auto &c : text)
        c = std::towlower(c);
    return text;
}
} // namespace
void Sessions::selection(const std::wstring &key) {
    selected_ = -1;
    for (int i = 0; i < 4; ++i)
        if (keys[i] == key)
            selected_ = i;
    {
        oneui::ui::Batch batch;
        model_.first.set(selected_ == 0);
        model_.second.set(selected_ == 1);
        model_.third.set(selected_ == 2);
        model_.fourth.set(selected_ == 3);
        model_.empty.set(selected_ < 0);
        model_.clear.canExecute.set(selected_ >= 0);
        model_.send.canExecute.set(selected_ >= 0);
        model_.sessionInfo.set(selected_ >= 0 ? L"deploy@" + names[selected_] + L" · 模拟会话"
                                              : L"本地沙箱 · 无活动会话");
        if (selected_ >= 0) {
            model_.quickConnection.set(selected_);
            filter();
        }
    }
    focus();
}
void Sessions::filter() {
    updating_ = true;
    rows_.clear();
    std::vector<oneui::VirtualListItem> rows;
    const auto query = lower(model_.connectionQuery.get());
    int selectedRow = -1;
    for (int i = 0; i < 4; ++i)
        if (query.empty() ||
            lower(names[i] + L" " + addresses[i]).find(query) != std::wstring::npos) {
            if (i == selected_)
                selectedRow = int(rows.size());
            rows_.push_back(i);
            oneui::VirtualListItem item;
            item.title = names[i];
            item.detail = addresses[i] + L" · 模拟";
            item.indicatorVisible = true;
            item.indicatorColor = {73, 146, 98};
            rows.push_back(item);
        }
    connections_->setRichItems(std::move(rows));
    connections_->setSelectedIndex(selectedRow);
    updating_ = false;
}
Sessions::Sessions(SessionsModel &model, oneui::ui::Mount &mount, oneui::Window &window)
    : model_(model), mount_(mount), window_(window) {
    if (terminal_demo_abi_version() != 1)
        throw std::runtime_error("Terminal example owner ABI mismatch");
    owner_ = std::shared_ptr<void>(terminal_demo_create_v1(), terminal_demo_destroy_v1);
    if (!owner_)
        throw std::runtime_error("Cannot create Rust terminal owner");
    auto clipboard = std::make_shared<oneui::SystemClipboard>();
    for (int i = 0; i < 4; ++i) {
        terminals_[i] = std::dynamic_pointer_cast<oneui::TerminalView>(
            oneui::retainNativeWidget(terminal_demo_widget_v1(owner_.get(), i < 2 ? i : i + 1)));
        secondary_[i] = std::dynamic_pointer_cast<oneui::TerminalView>(
            oneui::retainNativeWidget(terminal_demo_widget_v1(owner_.get(), i + 5)));
        if (!terminals_[i] || !secondary_[i])
            throw std::runtime_error("Missing Rust-owned terminal");
        terminals_[i]->setClipboard(clipboard);
        secondary_[i]->setClipboard(clipboard);
        mount_.registerNative("terminal" + std::to_string(i), terminals_[i], owner_);
        mount_.registerNative("secondary" + std::to_string(i), secondary_[i], owner_);
    }
    input_ = std::dynamic_pointer_cast<oneui::TextField>(
        oneui::retainNativeWidget(terminal_demo_widget_v1(owner_.get(), 2)));
    if (!input_)
        throw std::runtime_error("Missing Rust-owned command input");
    input_->setPreferredSize({0, 34});
    input_->setClipboard(clipboard);
    input_->setPlaceholder(L"输入 help、ls、status 或 clear；Enter 运行本地模拟命令");
    input_->setAccessibleName(L"本地演示命令");
    input_->setOnSubmitted([this](const auto &) { send(); });
    mount_.registerNative("input", input_, owner_);
    mount_.registerNative("connections", connections_);
    connections_->setRowHeight(58);
    connections_->setAccessibleName(L"演示连接");
    connections_->setOnChanged([this](int row) {
        if (!updating_ && row >= 0 && row < int(rows_.size()))
            open(rows_[row]);
    });
    model_.closeSession = [this](const auto &key) {
        auto found = std::find(keys.begin(), keys.end(), key);
        if (found == keys.end())
            return;
        auto id = std::find(ids_.begin(), ids_.end(), int(found - keys.begin()));
        if (id != ids_.end())
            close(int(id - ids_.begin()));
    };
    model_.clear.setAction([this] {
        if (selected_ >= 0)
            terminal_demo_clear_v1(owner_.get(), selected_);
    });
    model_.send.setAction([this] { send(); });
    model_.newSession.setAction([this] {
        for (int i = 0; i < 4; ++i)
            if (std::find(ids_.begin(), ids_.end(), i) == ids_.end()) {
                open(i);
                return;
            }
        open((selected_ + 1) % 4);
    });
    model_.searchNext.setAction([this] { search(true); });
    model_.toggleSplit.setAction([this] {
        if (selected_ < 0)
            return;
        oneui::State<bool> *states[]{&model_.pane0, &model_.pane1, &model_.pane2, &model_.pane3};
        states[selected_]->set(!states[selected_]->get());
    });
    subscriptions_.push_back(
        model_.selected.subscribeScoped([this](const auto &key) { selection(key); }));
    subscriptions_.push_back(
        model_.connectionQuery.subscribeScoped([this](const auto &) { filter(); }));
    subscriptions_.push_back(model_.quickConnection.subscribeScoped([this](int i) {
        if (i != selected_)
            open(i);
    }));
    subscriptions_.push_back(
        model_.terminalQuery.subscribeScoped([this](const auto &) { search(false); }));
    filter();
    selection(model_.selected.get());
}
Sessions::~Sessions() {
    if (input_)
        input_->setOnSubmitted({});
    connections_->setOnChanged({});
    model_.closeSession = {};
}
void Sessions::focus(bool command) {
    mount_.flush();
    if (command)
        window_.requestFocus(input_.get(), true);
    else if (selected_ >= 0)
        window_.requestFocus(terminals_[selected_].get(), false);
}
void Sessions::rebuild() {
    std::vector<oneui::ui::TabItem> items;
    for (auto id : ids_)
        items.push_back({keys[id], names[id]});
    model_.tabs.set(std::move(items));
    mount_.flush();
}
void Sessions::open(int id) {
    if (id < 0 || id >= 4)
        return;
    if (std::find(ids_.begin(), ids_.end(), id) == ids_.end())
        ids_.push_back(id);
    rebuild();
    model_.selected.set(keys[id]);
    focus();
}
void Sessions::close(int index) {
    if (index < 0 || index >= int(ids_.size()))
        return;
    ids_.erase(ids_.begin() + index);
    rebuild();
    focus();
}
void Sessions::restore() {
    ids_ = {0, 1};
    rebuild();
    model_.selected.set(keys[0]);
}
void Sessions::send() {
    if (selected_ < 0 || input_->text().empty())
        return;
    auto value = oneui::unicode::toUtf8(input_->text());
    terminal_demo_command_v1(owner_.get(), selected_, {value.data(), value.size()});
    input_->setText(L"");
    focus();
}
void Sessions::search(bool next) {
    if (selected_ < 0)
        return;
    auto terminal = terminals_[selected_];
    const auto query = lower(model_.terminalQuery.get());
    std::vector<std::array<int, 3>> matches;
    const auto grid = terminal->gridSize();
    if (!query.empty())
        for (int row = 0; row < int(grid.height); ++row) {
            std::wstring text;
            std::vector<int> columns;
            for (int col = 0; col < int(grid.width); ++col) {
                auto cell = terminal->cellAt(row, col);
                if (!cell)
                    continue;
                for (auto c : cell->text) {
                    text.push_back(c);
                    columns.push_back(col);
                }
            }
            const auto lowered = lower(text);
            for (auto at = lowered.find(query); at != std::wstring::npos;
                 at = lowered.find(query, at + 1))
                matches.push_back({row, columns[at], columns[at + query.size() - 1]});
        }
    if (matches.empty()) {
        searchIndex_ = -1;
        terminal->clearSelection();
        model_.searchResult.set(query.empty() ? L"当前可见内容" : L"无匹配");
        return;
    }
    searchIndex_ = next ? (searchIndex_ + 1) % int(matches.size()) : 0;
    const auto found = matches[searchIndex_];
    terminal->setSelection(found[0], found[1], found[0], found[2]);
    model_.searchResult.set(std::to_wstring(searchIndex_ + 1) + L" / " +
                            std::to_wstring(matches.size()));
}
} // namespace lab::terminal
