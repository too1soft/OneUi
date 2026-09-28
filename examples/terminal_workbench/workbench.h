#pragma once
#include "bridge.h"
#include "TerminalWorkbench.g.h"
#include <oneui/native_interop.h>
#include <oneui/ui_declarative_app.h>
#include <oneui/ui_theme.h>
#include <oneui/controls/terminal_view.h>
#include <condition_variable>
#include <cwctype>
#include <array>
#include <cmath>
#include <thread>

// Demo business policy only. All layout, native styling, tabs and charts are SDK components.
class Workbench {
public:
    oneui::ui::DeclarativeApp& app;
    WorkbenchVm vm;
    std::shared_ptr<void> foreign;
    std::array<std::shared_ptr<oneui::TerminalView>,4> terminals;
    std::array<std::shared_ptr<oneui::TerminalView>,4> secondary;
    std::shared_ptr<oneui::TextField> input;
    std::shared_ptr<oneui::VirtualList> connections=std::make_shared<oneui::VirtualList>();
    std::shared_ptr<oneui::Table> files=std::make_shared<oneui::Table>();
    std::shared_ptr<oneui::Table> processes=std::make_shared<oneui::Table>();
    std::shared_ptr<oneui::Tabs> tabs;
    std::vector<int> ids{0,1};int selected=0;
    const std::array<std::wstring,4> keys{L"edge",L"build",L"database",L"cache"};
    const std::array<std::wstring,4> names{L"edge-gateway",L"build-node",L"postgres-lab",L"cache-lab"};
    const std::array<std::wstring,4> addresses{L"192.0.2.10",L"192.0.2.24",L"192.0.2.36",L"192.0.2.48"};
private:
    struct File {std::wstring name;bool directory;std::wstring size;};
    std::map<std::wstring,std::vector<File>> folders_;
    std::vector<int> connectionRows_;
    std::vector<std::wstring> fileRows_;
    std::wstring selectedFile_;
    std::vector<std::wstring> selectedFiles_;
    std::vector<oneui::Subscription> subscriptions_;
    bool updatingConnections_=false,updatingFiles_=false,dark_=false,wantSidebar_=false,wantMonitor_=true;
    bool sampling_=false,wantFiles_=true,filesFocused_=false;float lastWidth_=1360;
    int searchIndex_=-1;
    std::vector<std::array<int,3>> matches_;
    std::mutex mutex_;std::condition_variable wake_;bool stopping_=false;
    std::thread worker_;int sample_=0,folderSerial_=0;
    std::shared_ptr<int> life_=std::make_shared<int>(0);
    static std::wstring lower(std::wstring text){for(auto& c:text)c=std::towlower(c);return text;}
    void applySelection(const std::wstring& key) {
        selected=-1;for(int i=0;i<4;++i)if(keys[i]==key)selected=i;
        {oneui::ui::Batch batch;
        vm.first.set(selected==0);vm.second.set(selected==1);vm.third.set(selected==2);vm.fourth.set(selected==3);vm.empty.set(selected<0);
        vm.clear.canExecute.set(selected>=0);vm.send.canExecute.set(selected>=0);
        vm.sessionInfo.set(selected>=0?L"deploy@"+names[selected]+L" · SSH":L"本地沙箱 · 无活动会话");
        if(selected>=0){vm.quickConnection.set(selected);filterConnections();}}
        app.mount().flush();
        if(selected>=0)app.window().requestFocus(terminals[selected].get(),false);
    }
    void filterConnections() {
        updatingConnections_=true;connectionRows_.clear();std::vector<oneui::VirtualListItem> rows;
        auto query=lower(vm.connectionQuery.get());int selectedRow=-1;
        for(int i=0;i<4;++i)if(query.empty() || lower(names[i]+L" "+addresses[i]).find(query)!=std::wstring::npos) {
            if(i==selected)selectedRow=int(rows.size());connectionRows_.push_back(i);
            oneui::VirtualListItem item;item.title=names[i];item.detail=addresses[i]+L"  ·  演示";item.indicatorVisible=true;item.indicatorColor={73,146,98};rows.push_back(item);
        }
        connections->setRichItems(std::move(rows));connections->setSelectedIndex(selectedRow);updatingConnections_=false;
    }
    std::vector<File>& folder() {
        auto [it,inserted]=folders_.try_emplace(vm.path.get());
        if(inserted) {
            it->second={{L"config",true,L"—"},{L"logs",true,L"—"},{L"services",true,L"—"},{L"scripts",true,L"—"},
                {L"README.md",false,L"3.2 KB"},{L"compose.yaml",false,L"1.8 KB"},{L"gateway.toml",false,L"856 B"},{L"deploy.sh",false,L"2.4 KB"}};
            for(int i=1;i<=10;++i)it->second.push_back({L"access-2026-09-"+std::to_wstring(i)+L".log",false,L"128 KB"});
        }
        return it->second;
    }
    void filterFiles() {
        auto& data=folder();std::vector<std::vector<oneui::TableCell>> rows;fileRows_.clear();auto query=lower(vm.fileQuery.get());
        for(const auto& file:data)if(query.empty() || lower(file.name).find(query)!=std::wstring::npos) {
            oneui::TableCell name;name.text=file.name;name.leadingIcon=file.directory?oneui::IconSymbol::OutlineWorkbenchFolder:oneui::IconSymbol::OutlineWorkbenchFile;name.iconSize=16;
            oneui::TableCell size;size.text=file.size;size.alignment=oneui::TextAlign::Right;
            oneui::TableCell modified;modified.text=L"09-28  10:24";
            oneui::TableCell mode;mode.text=file.directory?L"drwxr-xr-x":L"-rw-r--r--";rows.push_back({name,size,modified,mode});fileRows_.push_back(file.name);
        }
        const auto scroll=files->scrollOffset();updatingFiles_=true;files->setRichRows(std::move(rows));
        std::vector<int> selection;std::vector<std::wstring> retained;
        for(int i=0;i<int(fileRows_.size());++i)if(std::find(selectedFiles_.begin(),selectedFiles_.end(),fileRows_[i])!=selectedFiles_.end()){selection.push_back(i);retained.push_back(fileRows_[i]);}
        selectedFiles_=std::move(retained);selectedFile_=selectedFiles_.empty()?L"":selectedFiles_.front();
        files->setSelectedIndices(std::move(selection));files->setScrollOffset(scroll);updatingFiles_=false;vm.removeFile.canExecute.set(!selectedFiles_.empty());
        vm.fileSummary.set(std::to_wstring(fileRows_.size())+L" / "+std::to_wstring(data.size())+L" 个项目 · 模拟文件");
    }
    void openFile(int row) {
        if(row<0 || row>=int(fileRows_.size()))return;
        const auto name=fileRows_[row];const auto& data=folder();auto found=std::find_if(data.begin(),data.end(),[&](const auto& f){return f.name==name;});
        if(found==data.end())return;
        if(found->directory){vm.path.set(vm.path.get()+L"/"+name);vm.fileQuery.set(L"");selectedFile_.clear();selectedFiles_.clear();filterFiles();}
        else vm.feedback.set(L"预览 "+name+L" · 本地模拟文件，无磁盘读取");
    }
public:
    explicit Workbench(oneui::ui::DeclarativeApp& application,bool dark):app(application),dark_(dark) {
        if(terminal_demo_abi_version()!=1)throw std::runtime_error("Terminal demo ABI mismatch");
        foreign=std::shared_ptr<void>(terminal_demo_create_v1(),terminal_demo_destroy_v1);if(!foreign)throw std::runtime_error("Cannot create Rust terminal owner");
        auto& mount=app.mount();
        for(int i=0;i<4;++i){terminals[i]=std::dynamic_pointer_cast<oneui::TerminalView>(oneui::retainNativeWidget(terminal_demo_widget_v1(foreign.get(),i<2?i:i+1)));if(!terminals[i])throw std::runtime_error("Missing terminal");mount.registerNative("terminal"+std::to_string(i),terminals[i],foreign);}
        input=std::dynamic_pointer_cast<oneui::TextField>(oneui::retainNativeWidget(terminal_demo_widget_v1(foreign.get(),2)));
        input->setPreferredSize({0,34});input->setPlaceholder(L"输入 help、ls、status 或 clear；Enter 运行本地模拟命令");input->setAccessibleName(L"本地演示命令");
        input->setOnSubmitted([this](const auto&){send();});
        for(int i=0;i<4;++i){secondary[i]=std::dynamic_pointer_cast<oneui::TerminalView>(oneui::retainNativeWidget(terminal_demo_widget_v1(foreign.get(),i+5)));mount.registerNative("secondary"+std::to_string(i),secondary[i],foreign);}
        processes->setColumns({{L"进程 / PID",0},{L"CPU",84,oneui::TextAlign::Right},{L"RSS",70,oneui::TextAlign::Right}});processes->setRowHeight(42);processes->setHeaderHeight(30);processes->setColumnDividersVisible(false);
        processes->setRows({{L"node · 3124",L"19.0%",L"128M"},{L"postgres · 920",L"8.4%",L"256M"},{L"redis · 1102",L"2.1%",L"84M"},{L"nginx · 1884",L"0.8%",L"32M"},{L"containerd · 743",L"0.5%",L"46M"},{L"sshd · 4310",L"0.2%",L"12M"},{L"systemd · 1",L"0.1%",L"18M"},{L"journald · 344",L"0.1%",L"22M"}});
        processes->setOnChanged([this](int row){if(row>=0)vm.processDetail.set(processes->rows()[row][0]+L" · 用户 deploy · 线程 9 · 本地模拟快照");});
        mount.registerNative("processes",processes);
        mount.registerNative("input",input,foreign);mount.registerNative("connections",connections);mount.registerNative("files",files);
        connections->setRowHeight(58);connections->setAccessibleName(L"演示连接");
        files->setColumns({{L"名称",0},{L"大小",86,oneui::TextAlign::Right},{L"修改时间",148},{L"权限",104}});files->setSelectionMode(oneui::SelectionMode::Multiple);files->setSelectionColumnVisible(true);files->setSelectionColumnWidth(30);files->setRowHeight(31);files->setHeaderHeight(30);files->setColumnDividersVisible(false);files->setAccessibleName(L"模拟文件列表");
        connections->setOnChanged([this](int i){if(!updatingConnections_ && i>=0 && i<int(connectionRows_.size()))openSession(connectionRows_[i]);});
        files->setOnSelectionChanged([this](const auto& indices){if(updatingFiles_)return;selectedFiles_.clear();for(int i:indices)if(i>=0 && i<int(fileRows_.size()))selectedFiles_.push_back(fileRows_[i]);selectedFile_=selectedFiles_.empty()?L"":selectedFiles_.front();vm.removeFile.canExecute.set(!selectedFiles_.empty());});
        files->setOnActivated([this](int row){openFile(row);});
        vm.closeSession=[this](const auto& key){auto found=std::find(keys.begin(),keys.end(),key);if(found!=keys.end()){auto i=std::find(ids.begin(),ids.end(),int(found-keys.begin()));if(i!=ids.end())closeTab(int(i-ids.begin()));}};
        vm.restore.setAction([this]{ids={0,1};rebuildTabs();vm.fileRatio.set(0.54f);vm.monitorRatio.set(0.70f);wantSidebar_=false;wantMonitor_=wantFiles_=true;filesFocused_=false;vm.filesCollapsed.set(false);vm.compactTab.set(L"terminal");resize(lastWidth_);vm.feedback.set(L"布局已恢复 · 会话内容保留");});
        vm.clear.setAction([this]{if(selected>=0)terminal_demo_clear_v1(foreign.get(),selected);});
        vm.theme.setAction([this]{dark_=!dark_;applyTheme();});
        vm.toggleSidebar.setAction([this]{wantSidebar_=!wantSidebar_;resize(lastWidth_);});
        vm.toggleMonitor.setAction([this]{if(vm.narrow.get())vm.compactTab.set(vm.compactTab.get()==L"monitor"?L"terminal":L"monitor");else wantMonitor_=!wantMonitor_;filesFocused_=false;resize(lastWidth_);});
        vm.toggleFiles.setAction([this]{wantFiles_=!wantFiles_;resize(lastWidth_);});
        vm.toggleSampling.setAction([this]{{std::lock_guard<std::mutex> lock(mutex_);sampling_=!sampling_;}vm.samplingLabel.set(sampling_?L"暂停采样":L"开始采样");vm.samplingStatus.set(sampling_?L"每秒更新":L"已暂停");});
        vm.send.setAction([this]{send();});
        vm.up.setAction([this]{auto path=vm.path.get();if(path!=L"/srv/workspace"){path.resize(path.find_last_of(L'/'));vm.path.set(path);selectedFile_.clear();selectedFiles_.clear();filterFiles();}});
        vm.newFolder.setAction([this]{auto name=L"new-folder-"+std::to_wstring(++folderSerial_);folder().insert(folder().begin(),{name,true,L"—"});selectedFile_=name;selectedFiles_={name};vm.fileQuery.set(L"");filterFiles();vm.feedback.set(L"已创建 "+name+L" · 仅保存在本次演示中");});
        vm.removeFile.setAction([this]{auto& data=folder();data.erase(std::remove_if(data.begin(),data.end(),[this](const auto& f){return std::find(selectedFiles_.begin(),selectedFiles_.end(),f.name)!=selectedFiles_.end();}),data.end());selectedFile_.clear();selectedFiles_.clear();filterFiles();vm.feedback.set(L"模拟文件已删除 · 未修改磁盘");});
        vm.showTerminal.setAction([this]{filesFocused_=false;vm.compactTab.set(L"terminal");resize(lastWidth_);});
        vm.showFiles.setAction([this]{if(vm.narrow.get())vm.compactTab.set(L"files");else filesFocused_=!filesFocused_;wantFiles_=true;vm.filesCollapsed.set(false);resize(lastWidth_);});
        vm.showMonitor.setAction([this]{filesFocused_=false;wantMonitor_=true;vm.compactTab.set(L"monitor");resize(lastWidth_);});
        vm.showProcesses.setAction([this]{vm.monitorTab.set(L"processes");vm.showMonitor.execute();});
        vm.toggleCommand.setAction([this]{vm.showTerminal.execute();vm.commandVisible.set(!vm.commandVisible.get());app.mount().flush();if(vm.commandVisible.get())app.window().requestFocus(input.get(),true);});
        vm.toggleSearch.setAction([this]{vm.searchVisible.set(!vm.searchVisible.get());app.mount().flush();if(vm.searchVisible.get())app.window().requestFocus(app.mount().find("terminalSearch").get(),true);else if(selected>=0){terminals[selected]->clearSelection();app.window().requestFocus(terminals[selected].get(),false);}});
        vm.searchNext.setAction([this]{search(true);});
        vm.toggleSplit.setAction([this]{if(selected<0)return;oneui::State<bool>* states[]={&vm.pane0,&vm.pane1,&vm.pane2,&vm.pane3};states[selected]->set(!states[selected]->get());});
        vm.collapseFiles.setAction([this]{const bool collapsed=!vm.filesCollapsed.get();if(collapsed){filesFocused_=false;if(vm.narrow.get())vm.compactTab.set(L"terminal");resize(lastWidth_);}vm.filesCollapsed.set(collapsed);});
        vm.newSession.setAction([this]{for(int i=0;i<4;++i)if(std::find(ids.begin(),ids.end(),i)==ids.end()){openSession(i);return;}openSession((selected+1)%4);});
        vm.download.setAction([this]{if(selectedFile_.empty()){vm.transferLabel.set(L"请先选择要下载的模拟文件");return;}vm.transferValue.set(0);vm.transferring.set(true);vm.transferLabel.set(L"模拟下载 "+std::to_wstring(selectedFiles_.size())+L" 个项目");});
        vm.upload.setAction([this]{auto name=L"upload-sample-"+std::to_wstring(++folderSerial_)+L".txt";folder().insert(folder().begin(),{name,false,L"2.0 KB"});selectedFile_=name;selectedFiles_={name};vm.fileQuery.set(L"");filterFiles();vm.transferLabel.set(L"模拟上传完成 · "+name);});
        vm.showStatus.setAction([this]{vm.showTerminal.execute();input->setText(L"status");send();});
        subscriptions_.push_back(vm.compactTab.subscribeScoped([this](const auto&){resize(lastWidth_);}));
        subscriptions_.push_back(vm.monitorTab.subscribeScoped([this](const auto& id){oneui::ui::Batch batch;vm.resourceVisible.set(id==L"resources");vm.processVisible.set(id==L"processes");vm.connectionVisible.set(id==L"connection");}));
        subscriptions_.push_back(vm.filesCollapsed.subscribeScoped([this](bool value){vm.filesExpanded.set(!value);vm.fileCollapseIcon.set(value?L"right":L"down");}));
        subscriptions_.push_back(vm.terminalQuery.subscribeScoped([this](const auto&){search(false);}));
        subscriptions_.push_back(vm.selected.subscribeScoped([this](const auto& id){applySelection(id);}));
        subscriptions_.push_back(vm.connectionQuery.subscribeScoped([this](const auto&){filterConnections();}));
        subscriptions_.push_back(vm.fileQuery.subscribeScoped([this](const auto&){filterFiles();}));
        subscriptions_.push_back(vm.quickConnection.subscribeScoped([this](int i){if(i!=selected)openSession(i);}));
        filterConnections();filterFiles();sample();applyTheme();applySelection(vm.selected.get());
    }
    ~Workbench(){stop();input->setOnSubmitted({});connections->setOnChanged({});files->setOnChanged({});files->setOnSelectionChanged({});files->setOnActivated({});processes->setOnChanged({});app.window().setClientSizeChangedHandler({});}
    void applyTheme(){app.mount().styles()->replace(oneui::ui::workspaceTheme(dark_)+styles_TerminalWorkbench(),oneui::ui::Density::Compact);vm.themeLabel.set(dark_?L"浅色":L"深色");}
    void resize(float width){lastWidth_=width;const bool narrow=width<1050;oneui::ui::Batch batch;
        vm.narrow.set(narrow);vm.wide.set(!narrow);vm.sidebarVisible.set(wantSidebar_ && !narrow);
        const auto pane=vm.compactTab.get();vm.mainVisible.set(!narrow || pane!=L"monitor");
        vm.terminalVisible.set(narrow?pane==L"terminal":!filesFocused_);
        vm.filesVisible.set(narrow?pane==L"files":wantFiles_);
        vm.monitorVisible.set(narrow?pane==L"monitor":wantMonitor_ && !filesFocused_);
    }
    void search(bool next){
        if(selected<0)return;auto terminal=terminals[selected];const auto query=lower(vm.terminalQuery.get());matches_.clear();
        const auto grid=terminal->gridSize();
        if(!query.empty())for(int row=0;row<int(grid.height);++row){std::wstring text;std::vector<int> columns;for(int col=0;col<int(grid.width);++col){auto cell=terminal->cellAt(row,col);if(!cell)continue;for(auto c:cell->text){text.push_back(c);columns.push_back(col);}}
            auto lowered=lower(text);for(std::size_t pos=lowered.find(query);pos!=std::wstring::npos;pos=lowered.find(query,pos+1))matches_.push_back({row,columns[pos],columns[pos+query.size()-1]});}
        if(matches_.empty()){searchIndex_=-1;terminal->clearSelection();vm.searchResult.set(query.empty()?L"当前可见内容":L"无匹配");return;}
        searchIndex_=next?(searchIndex_+1)%int(matches_.size()):0;auto found=matches_[searchIndex_];terminal->setSelection(found[0],found[1],found[0],found[2]);
        vm.searchResult.set(std::to_wstring(searchIndex_+1)+L" / "+std::to_wstring(matches_.size()));
    }
    void mounted(){tabs=std::dynamic_pointer_cast<oneui::Tabs>(app.mount().find("tabs"));app.window().setClientSizeChangedHandler([this](oneui::Size size){resize(size.width);});}
    void openSession(int i){if(i<0 || i>=4)return;if(std::find(ids.begin(),ids.end(),i)==ids.end())ids.push_back(i);rebuildTabs();vm.selected.set(keys[i]);app.mount().flush();app.window().requestFocus(terminals[i].get(),false);vm.feedback.set(L"已打开 "+names[i]+L" · 本地演示会话");}
    void select(int index,bool focus){vm.selected.set(index>=0 && index<int(ids.size())?keys[ids[index]]:L"");app.mount().flush();if(focus && selected>=0)app.window().requestFocus(terminals[selected].get(),false);}
    void rebuildTabs(){std::vector<oneui::ui::TabItem> items;for(auto i:ids)items.push_back({keys[i],names[i]});vm.tabs.set(std::move(items));app.mount().flush();}
    void closeTab(int index){if(index<0 || index>=int(ids.size()))return;ids.erase(ids.begin()+index);rebuildTabs();if(selected>=0)app.window().requestFocus(terminals[selected].get(),false);}
    void send(){if(selected<0 || input->text().empty())return;auto value=std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>{}.to_bytes(input->text());terminal_demo_command_v1(foreign.get(),selected,{value.data(),value.size()});input->setText(L"");app.window().requestFocus(terminals[selected].get(),false);}
    void sample(){
        ++sample_;auto make=[&](double base,double amplitude,double phase,oneui::Color color){oneui::TimeSeriesChartSeries s;s.name=L"模拟";s.color=color;for(int i=0;i<90;++i)s.values.push_back(base+amplitude*std::sin((i+sample_)*0.13+phase)+amplitude*0.25*std::sin(i*0.7));return s;};
        auto cpu=make(25,10,0,{51,153,102}),memory=make(52,3,2,{111,143,216}),network=make(38,19,1,{66,162,164}),upload=make(18,6,2,{174,148,93});
        vm.cpu.set(std::to_wstring(int(cpu.values.back()))+L".8%");vm.memory.set(std::to_wstring(int(memory.values.back()))+L".1%");
        vm.memoryValue.set(float(memory.values.back()/100));vm.cpuSeries.set({cpu});vm.memorySeries.set({memory});vm.networkSeries.set({network,upload});
    }
    void start(){auto sender=app.dispatcher();auto weak=std::weak_ptr<int>(life_);worker_=std::thread([this,sender,weak]{std::unique_lock<std::mutex> lock(mutex_);while(!wake_.wait_for(lock,std::chrono::seconds(1),[this]{return stopping_;})){const bool sampling=sampling_;lock.unlock();sender.post([this,weak,sampling]{if(weak.expired())return;if(sampling)sample();if(vm.transferring.get()){float progress=std::min(1.0f,vm.transferValue.get()+0.25f);vm.transferValue.set(progress);if(progress>=1){vm.transferring.set(false);vm.transferLabel.set(L"模拟传输完成 · 未写入磁盘");}}});lock.lock();}});}
    void stop(){life_.reset();{std::lock_guard<std::mutex> lock(mutex_);stopping_=true;}wake_.notify_all();if(worker_.joinable())worker_.join();}
};
