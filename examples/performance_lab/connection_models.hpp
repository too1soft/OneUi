#pragma once
#include "connection_editor.hpp"
#include <cwctype>

namespace connection_demo {
struct Connection {
    std::wstring id;
    Config config;
    bool online = true;
    bool operator==(const Connection &other) const {
        return id == other.id && config == other.config && online == other.online;
    }
};

// All data is synthetic and lives only for this application session.
class Connections {
    VM &editor_;
    int nextId_ = 1001;
    std::wstring editingId_, deletingId_;
    bool creating_ = false;

  public:
    enum class Destination { None, List, Detail, Lab, Close, Delete };

  private:
    Destination pending_ = Destination::None, afterSave_ = Destination::List,
                editorReturn_ = Destination::List;
    static std::wstring lower(std::wstring text) {
        for (auto &c : text)
            c = static_cast<wchar_t>(std::towlower(c));
        return text;
    }
    void leave(Destination destination) {
        pending_ = Destination::None;
        prompt.set(false);
        if (destination == Destination::Close) {
            if (closeWindow)
                closeWindow();
            return;
        }
        form.set(false);
        if (destination != Destination::Detail)
            detailKey.set({});
        if (destination == Destination::Lab && backToLab)
            backToLab();
    }
    void saved(const Config &snapshot) {
        auto data = records.get();
        auto found = std::find_if(data.begin(), data.end(),
                                  [&](const auto &row) { return row.id == editingId_; });
        if (found != data.end())
            found->config = snapshot;
        else if (creating_)
            data.insert(data.begin(), {editingId_, snapshot, true});
        else
            throw std::logic_error("Edited connection no longer exists");
        creating_ = false;
        records.set(std::move(data));
        // VmCommand completion is batched; derived dirty/filtered may be stale here.
        const bool visible = matches({editingId_, snapshot, find(editingId_)->online});
        selectedKey.set(visible ? editingId_ : L"");
        message.set(visible ? L"连接已保存 · 仅当前进程有效"
                            : L"连接已保存，当前筛选未显示它；筛选条件已保留。");
        if (prompt.get()) {
            promptTitle.set(L"保存已完成");
            promptText.set(editor_.value() == snapshot
                               ? L"提交时的配置已保存。可以继续操作，或离开当前页面。"
                               : L"提交时的配置已保存；后续输入仍未保存。继续编辑或放弃后续修改。");
        } else if (editor_.value() == snapshot)
            leave(afterSave_);
        else {
            afterSave_ = editorReturn_;
            editor_.message.set(L"已保存提交时的配置；后续修改尚未保存，请再次保存。");
        }
    }

  public:
    State<std::vector<Connection>> records;
    State<std::wstring> query, selectedKey, detailKey,
        message{L"固定种子模拟数据 · 双击或按 Enter 查看详情"};
    State<std::wstring> styleError;
    State<std::vector<std::wstring>> filters{{L"全部状态", L"在线", L"离线"}};
    State<std::vector<TableColumn>> columns{{{L"连接名称", 0}, {L"地址", 0}, {L"状态", 72}}};
    State<int> filter{0};
    State<bool> form{false}, prompt{false}, gallery{false};
    State<std::wstring> promptTitle, promptText;
    State<bool> deleting{false};
    VmCommand add, edit, remove, clear, back, keep, discard, saveLeave, confirmDelete;
    VmCommand showGallery, view, detailBack;
    std::function<void()> backToLab, closeWindow;
    std::vector<Subscription> subscriptions;

    bool matches(const Connection &row) const {
        if (filter.get() == 1 && !row.online)
            return false;
        if (filter.get() == 2 && row.online)
            return false;
        auto needle = lower(query.get());
        return needle.empty() ||
               lower(row.config.name + L" " + row.config.host + L" " + row.id).find(needle) !=
                   std::wstring::npos;
    }
    const Connection *find(const std::wstring &id) const {
        for (auto &row : records.get())
            if (row.id == id)
                return &row;
        return nullptr;
    }
    Computed<std::vector<TableRow>> filtered{
        [this] {
            std::vector<TableRow> result;
            for (auto &row : records.get())
                if (matches(row))
                    result.push_back({row.id,
                                      {row.config.name, row.config.host + L":" + row.config.port,
                                       row.online ? L"在线" : L"离线"}});
            return result;
        },
        records, query, filter};
    Computed<bool> showList{[this] {
                                return !form.get() && !prompt.get() && !gallery.get() &&
                                       detailKey.get().empty();
                            },
                            form, prompt, gallery, detailKey};
    Computed<bool> showDetail{[this] {
                                  return !form.get() && !prompt.get() && !gallery.get() &&
                                         !detailKey.get().empty();
                              },
                              form, prompt, gallery, detailKey};
    Computed<bool> showForm{[this] { return form.get() && !prompt.get(); }, form, prompt};
    Computed<bool> empty{[this] { return filtered.get().empty(); }, filtered};
    Computed<bool> hasRows{[this] { return !filtered.get().empty(); }, filtered};
    Computed<bool> notDeleting{[this] { return !deleting.get(); }, deleting};
    Computed<bool> canSavePrompt{[this] { return !deleting.get() && !editor_.save.running.get(); },
                                 deleting, editor_.save.running};
    Computed<std::wstring> count{[this] {
                                     return L"显示 " + std::to_wstring(filtered.get().size()) +
                                            L" / " + std::to_wstring(records.get().size()) +
                                            L" 条连接";
                                 },
                                 filtered, records};
    Computed<std::wstring> selection{[this] {
                                         auto row = find(selectedKey.get());
                                         return row ? L"已选择：" + row->config.name
                                                    : std::wstring(L"选择一条连接查看详情或编辑");
                                     },
                                     selectedKey, records};
    Computed<std::wstring> emptyTitle{[this] {
                                          return records.get().empty()
                                                     ? std::wstring(L"还没有连接")
                                                     : std::wstring(L"没有匹配的连接");
                                      },
                                      records};
    Computed<std::wstring> emptyText{
        [this] {
            return records.get().empty() ? std::wstring(L"新建一条本地模拟连接，开始体验编辑流程。")
                                         : std::wstring(L"换个名称或地址，或清除搜索与状态筛选。");
        },
        records};
    Computed<bool> hasRecords{[this] { return !records.get().empty(); }, records};
    Computed<bool> noRecords{[this] { return records.get().empty(); }, records};
    Computed<Config> detailConfig{[this] {
                                      auto row = find(detailKey.get());
                                      return row ? row->config : Config{};
                                  },
                                  detailKey, records};
    Computed<std::wstring> detailName{[this] { return detailConfig.get().name; }, detailConfig};
    Computed<std::wstring> detailHost{[this] { return detailConfig.get().host; }, detailConfig};
    Computed<std::wstring> detailPort{[this] { return detailConfig.get().port; }, detailConfig};
    Computed<std::wstring> detailProtocol{
        [this] { return editor_.protocols.get().at(detailConfig.get().protocol); }, detailConfig,
        editor_.protocols};
    Computed<std::wstring> detailTimeout{[this] { return detailConfig.get().timeout + L" 秒"; },
                                         detailConfig};
    Computed<std::wstring> detailReconnect{
        [this] {
            return std::wstring(detailConfig.get().reconnect ? L"允许自动重连" : L"不自动重连");
        },
        detailConfig};
    Computed<std::wstring> detailNote{[this] {
                                          return detailConfig.get().note.empty()
                                                     ? std::wstring(L"未填写备注")
                                                     : detailConfig.get().note;
                                      },
                                      detailConfig};
    Computed<std::wstring> detailStatus{
        [this] {
            auto row = find(detailKey.get());
            return std::wstring(row && row->online ? L"在线 · 模拟状态" : L"离线 · 模拟状态");
        },
        detailKey, records};
    Computed<std::wstring> detailTone{[this] {
                                          auto row = find(detailKey.get());
                                          return std::wstring(row && row->online ? L"success"
                                                                                 : L"neutral");
                                      },
                                      detailKey, records};

    explicit Connections(VM &editor) : editor_(editor) {
        showGallery.setAction([this] { gallery.set(true); });
        std::vector<Connection> seed;
        seed.reserve(1000);
        uint32_t random = 0xC0FFEEu;
        const wchar_t *cities[] = {L"上海", L"北京", L"杭州", L"深圳", L"成都"};
        for (int i = 1; i <= 1000; ++i) {
            random = random * 1664525u + 1013904223u;
            Config config;
            config.name =
                std::wstring(cities[(random >> 16) % 5]) + L"研发节点 " + std::to_wstring(i);
            config.host =
                L"10.24." + std::to_wstring(i / 250) + L"." + std::to_wstring(i % 250 + 1);
            config.protocol = i % 3;
            config.port = config.protocol == 1 ? L"80" : L"443";
            seed.push_back({std::to_wstring(i), config, (random & 3) != 0});
        }
        records.set(std::move(seed));
        editor_.backText.set(L"返回连接列表");
        editor_.saveLabel.set(L"保存并返回");
        editor_.description.set(L"仅修改本地模拟数据 · 保存后返回列表，保留筛选和滚动位置。");
        editor_.back.setAction([this] { request(editorReturn_); });
        editor_.onSaved = [this](const Config &config) { saved(config); };
        editor_.onSaveFailed = [this] { afterSave_ = editorReturn_; };
        add.setAction([this] { openNew(); });
        edit.setAction(
            [this] { open(detailKey.get().empty() ? selectedKey.get() : detailKey.get()); });
        view.setAction([this] { showDetails(selectedKey.get()); });
        detailBack.setAction([this] { request(Destination::List); });
        back.setAction([this] { request(Destination::Lab); });
        clear.setAction([this] {
            Batch batch;
            query.set({});
            filter.set(0);
        });
        remove.setAction([this] {
            auto row = find(detailKey.get().empty() ? selectedKey.get() : detailKey.get());
            if (!row)
                return;
            deletingId_ = row->id;
            pending_ = Destination::Delete;
            deleting.set(true);
            promptTitle.set(L"删除这条连接？");
            promptText.set(L"将从本地模拟列表中删除「" + row->config.name +
                           L"」。此操作无法撤销。");
            prompt.set(true);
        });
        keep.setAction([this] {
            pending_ = Destination::None;
            afterSave_ = editorReturn_;
            prompt.set(false);
        });
        discard.setAction([this] {
            if (editor_.save.running.get() && pending_ != Destination::Close)
                return;
            auto destination = pending_;
            if (destination == Destination::None || destination == Destination::Delete)
                return;
            if (!editor_.save.running.get())
                editor_.reset.execute();
            leave(destination);
        });
        saveLeave.setAction([this] {
            if (pending_ == Destination::None || pending_ == Destination::Delete ||
                editor_.save.running.get())
                return;
            afterSave_ = pending_;
            pending_ = Destination::None;
            prompt.set(false);
            editor_.save.execute();
            if (!editor_.save.running.get())
                afterSave_ = editorReturn_;
        });
        confirmDelete.setAction([this] {
            if (pending_ != Destination::Delete)
                return;
            const auto &visible = filtered.get();
            auto old = std::find_if(visible.begin(), visible.end(),
                                    [&](const auto &row) { return row.id == deletingId_; });
            const auto index = static_cast<std::size_t>(old - visible.begin());
            auto data = records.get();
            data.erase(std::remove_if(data.begin(), data.end(),
                                      [&](const auto &row) { return row.id == deletingId_; }),
                       data.end());
            records.set(std::move(data));
            const auto &remaining = filtered.get();
            selectedKey.set(
                remaining.empty() ? L"" : remaining[std::min(index, remaining.size() - 1)].id);
            message.set(L"连接已删除 · 其他连接保持不变");
            leave(Destination::List);
        });
        auto enable = [this](const auto &) {
            const auto *row = find(detailKey.get().empty() ? selectedKey.get() : detailKey.get());
            const bool valid = row && (!detailKey.get().empty() || matches(*row)) && !form.get() &&
                               !prompt.get() && !gallery.get();
            edit.canExecute.set(valid);
            remove.canExecute.set(valid);
            view.canExecute.set(valid && detailKey.get().empty());
        };
        subscriptions.push_back(selectedKey.subscribeScoped(enable));
        subscriptions.push_back(filtered.subscribeScoped(enable));
        subscriptions.push_back(detailKey.subscribeScoped(enable));
        subscriptions.push_back(gallery.subscribeScoped(enable));
        subscriptions.push_back(form.subscribeScoped(enable));
        subscriptions.push_back(prompt.subscribeScoped(enable));
        enable(0);
        subscriptions.push_back(editor_.save.running.subscribeScoped([this](bool busy) {
            discard.canExecute.set(!busy || pending_ == Destination::Close);
        }));
    }
    bool needsGuard() const {
        return form.get() && (creating_ || !(editor_.value() == editor_.saved.get()) ||
                              editor_.save.running.get());
    }
    void showDetails(const std::wstring &id) {
        if (form.get() || prompt.get() || gallery.get() || !find(id))
            return;
        detailKey.set(id);
        message.set(L"配置仅在当前进程有效，关闭应用后丢弃。");
    }
    void open(const std::wstring &id) {
        if (editor_.save.running.get() || prompt.get())
            return;
        auto row = find(id);
        if (!row)
            return;
        editingId_ = id;
        creating_ = false;
        editorReturn_ = detailKey.get() == id ? Destination::Detail : Destination::List;
        afterSave_ = editorReturn_;
        editor_.backText.set(editorReturn_ == Destination::Detail ? L"返回连接详情"
                                                                  : L"返回连接列表");
        editor_.description.set(
            editorReturn_ == Destination::Detail
                ? L"仅修改本地模拟数据 · 保存后返回详情，列表筛选和滚动位置保留。"
                : L"仅修改本地模拟数据 · 保存后返回列表，保留筛选和滚动位置。");
        editor_.load(row->config);
        editor_.heading.set(L"编辑连接");
        form.set(true);
    }
    void openNew() {
        if (editor_.save.running.get() || prompt.get())
            return;
        editingId_ = std::to_wstring(nextId_++);
        creating_ = true;
        editorReturn_ = afterSave_ = Destination::List;
        detailKey.set({});
        editor_.backText.set(L"返回连接列表");
        editor_.description.set(L"仅创建本地模拟数据 · 保存后返回列表；关闭应用后丢弃。");
        Config config;
        config.name = L"新连接 " + editingId_;
        editor_.load(config);
        editor_.heading.set(L"新建连接");
        form.set(true);
    }
    void request(Destination destination) {
        if (!needsGuard()) {
            leave(destination);
            return;
        }
        pending_ = destination;
        deleting.set(false);
        promptTitle.set(editor_.save.running.get() ? L"保存仍在进行" : L"还有未保存的修改");
        promptText.set(editor_.save.running.get()
                           ? L"可以继续等待；关闭应用会取消任务并丢弃未提交结果。"
                           : L"保存后再离开，或放弃修改。继续编辑会保留当前输入和光标。");
        discard.canExecute.set(!editor_.save.running.get() || destination == Destination::Close);
        prompt.set(true);
    }
};
} // namespace connection_demo
