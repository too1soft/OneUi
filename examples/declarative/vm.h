#pragma once
#include "oneui/ui_declarative.h"
#include <chrono>

using namespace oneui;
using namespace oneui::ui;
template<class T> struct Constant {
    T value; const T& get() const { return value; }
    template<class F> Subscription subscribeScoped(F) { return {}; }
};
struct Tip { std::wstring id; State<std::wstring> text; Tip(std::wstring id,std::wstring text):id(std::move(id)),text(std::move(text)){} };
struct DemoVM {
    State<bool> settings{true}, details{false}, autoRefresh{true};
    State<std::wstring> name{L"我的工作空间"}, interval{L"30"}, query, selectedKey, message{L"所有更改已保存"}, styleError;
    State<int> theme{0}, filter{0};
    State<std::wstring> savedName{name.get()}, savedInterval{interval.get()};
    State<bool> savedRefresh{true}; State<int> savedTheme{0};
    State<std::vector<TableRow>> rows;
    Constant<std::vector<std::wstring>> themes{{L"浅色 · 日间工作",L"深色 · 低光环境"}}, filters{{L"全部连接",L"在线",L"离线"}};
    Constant<std::vector<TableColumn>> columns{{{L"连接名称",0},{L"地址",190},{L"状态",100},{L"延迟",90}}};
    State<std::vector<std::shared_ptr<Tip>>> tips;
    VmCommand save, reload, showSettings, showConnections, showDetails, add, remove, failNext;
    bool shouldFail=false; int nextId=1001;
    Computed<std::wstring> nameError{[this] {return name.get().empty()?std::wstring(L"请输入工作空间名称。"):std::wstring{};},name};
    Computed<std::wstring> intervalError{[this] {
        try { std::size_t end=0; int n=std::stoi(interval.get(),&end); if(end!=interval.get().size() || n<1 || n>3600) throw std::invalid_argument("range"); }
        catch(...) {return std::wstring(L"刷新间隔应为 1–3600 秒的整数。");}
        return std::wstring{};
    },interval};
    Computed<std::wstring> validation{[this] {return nameError.get().empty()?intervalError.get():nameError.get();},nameError,intervalError};
    Computed<bool> dirty{[this] {return name.get()!=savedName.get() || interval.get()!=savedInterval.get() || autoRefresh.get()!=savedRefresh.get() || theme.get()!=savedTheme.get();},name,interval,autoRefresh,theme,savedName,savedInterval,savedRefresh,savedTheme};
    Computed<bool> valid{[this] {return validation.get().empty() && dirty.get();},validation,dirty};
    Computed<std::wstring> saveText{[this] {return save.running.get()?L"正在保存…":L"保存更改";},save.running};
    Computed<std::wstring> status{[this] {
        if(save.running.get()) return std::wstring(L"正在保存本地模拟配置…");
        if(!save.error.get().empty()) return save.error.get();
        return dirty.get()?std::wstring(L"有未保存的更改"):message.get();
    },save.running,save.error,dirty,message};
    Computed<std::wstring> statusTone{[this] {
        return std::wstring(save.running.get()?L"pending":!save.error.get().empty()?L"error":dirty.get()?L"warning":L"success");
    },save.running,save.error,dirty};
    Computed<bool> connections{[this]{return !settings.get() && !details.get();},settings,details};
    Computed<std::vector<TableRow>> filtered{[this] {
        std::vector<TableRow> result;
        for(auto& row:rows.get()) {
            if(filter.get()==1 && row.cells[2]!=L"在线") continue;
            if(filter.get()==2 && row.cells[2]!=L"离线") continue;
            if(!query.get().empty() && row.cells[0].find(query.get())==std::wstring::npos && row.cells[1].find(query.get())==std::wstring::npos) continue;
            result.push_back(row);
        }
        return result;
    },rows,query,filter};
    Computed<bool> empty{[this] {return filtered.get().empty();},filtered};
    Computed<bool> showEmpty{[this]{return empty.get() && !reload.running.get();},empty,reload.running};
    Computed<bool> showTable{[this]{return !empty.get() && !reload.running.get();},empty,reload.running};
    Computed<std::wstring> count{[this] {return std::to_wstring(filtered.get().size())+L" 条连接 · 本地模拟数据";},filtered};
    std::wstring selectedCell(int column) const {
        for(auto& row:rows.get())if(row.id==selectedKey.get())return row.cells[column];
        return L"未选择连接";
    }
    Computed<std::wstring> selectedName{[this]{return selectedCell(0);},selectedKey,rows};
    Computed<std::wstring> selectedAddress{[this]{return selectedCell(1);},selectedKey,rows};
    Computed<std::wstring> selectedStatus{[this]{return selectedCell(2);},selectedKey,rows};
    Computed<std::wstring> selectedLatency{[this]{return selectedCell(3);},selectedKey,rows};
    Computed<std::wstring> selectedTone{[this]{return std::wstring(selectedStatus.get()==L"在线"?L"success":L"neutral");},selectedStatus};
    std::vector<Subscription> subscriptions;
    explicit DemoVM(UiMailbox::Sender sender) {
        std::vector<TableRow> data;
        for(int i=1;i<=1000;++i) data.push_back({std::to_wstring(i),{L"工作节点 "+std::to_wstring(i),L"10.24."+std::to_wstring(i/250)+L"."+std::to_wstring(i%250+1),i%7?L"在线":L"离线",i%7?std::to_wstring(8+(i*17)%80)+L" ms":L"—"}});
        rows.set(std::move(data));
        tips.set({std::make_shared<Tip>(L"1",L"修改名称，体验双向绑定"),std::make_shared<Tip>(L"2",L"切换主题，输入状态保持不变")});
        subscriptions.push_back(valid.subscribeScoped([this](bool value){save.canExecute.set(value);})); save.canExecute.set(valid.get());
        subscriptions.push_back(selectedKey.subscribeScoped([this](const auto& key){remove.canExecute.set(!key.empty());showDetails.canExecute.set(!key.empty());})); remove.canExecute.set(false);showDetails.canExecute.set(false);
        showSettings.setAction([this]{Batch batch;details.set(false);settings.set(true);}); showConnections.setAction([this]{Batch batch;details.set(false);settings.set(false);});
        showDetails.setAction([this]{Batch batch;settings.set(false);details.set(true);});
        reload.setAction([this,sender]{reload.runAsync(sender,[](auto cancellation){
            for(int step=0;step<16 && !cancellation.cancelled();++step)std::this_thread::sleep_for(std::chrono::milliseconds(50));return std::wstring{};
        });});
        failNext.setAction([this]{shouldFail=true; message.set(L"下次保存将模拟失败，可再次保存重试"); name.set(name.get()+L" · 修改");});
        save.setAction([this,sender] {
            auto n=name.get(), i=interval.get(); bool a=autoRefresh.get(), fail=shouldFail; int t=theme.get(); shouldFail=false;
            save.runAsync(sender,[fail](auto cancellation) {
                for(int step=0;step<16 && !cancellation.cancelled();++step) std::this_thread::sleep_for(std::chrono::milliseconds(50));
                return fail?std::wstring(L"模拟保存失败。请点击“保存更改”重试。"):std::wstring{};
            },[this,n,i,a,t](bool success) { if(success) {Batch batch; savedName.set(n);savedInterval.set(i);savedRefresh.set(a);savedTheme.set(t);message.set(L"已保存 · 当前进程内的模拟配置");} });
        });
        add.setAction([this] {auto data=rows.get();int id=nextId++;data.insert(data.begin(),{std::to_wstring(id),{L"新连接 "+std::to_wstring(id),L"127.0.0.1",L"在线",L"1 ms"}});Batch batch;query.set({});filter.set(0);rows.set(std::move(data));selectedKey.set(std::to_wstring(id));});
        remove.setAction([this] {auto data=rows.get();const auto key=selectedKey.get();data.erase(std::remove_if(data.begin(),data.end(),[&](auto& r){return r.id==key;}),data.end());rows.set(std::move(data));});
    }
};
