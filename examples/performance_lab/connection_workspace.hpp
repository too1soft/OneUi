#pragma once
#include "connection_editor.hpp"
#include "component_gallery.hpp"
#include <cwctype>
#include <oneui/ui_dev.h>

namespace connection_demo {
struct Connection {
    std::wstring id;
    Config config;
    bool online = true;
    bool operator==(const Connection& other) const {
        return id == other.id && config == other.config && online == other.online;
    }
};

// All data is synthetic and lives only for this application session.
class Connections {
    VM& editor_;
    int nextId_ = 1001;
    std::wstring editingId_, deletingId_;
    bool creating_ = false;
public:
    enum class Destination { None, List, Lab, Close, Delete };
private:
    Destination pending_ = Destination::None, afterSave_ = Destination::List;
    static std::wstring lower(std::wstring text) {
        for(auto& c:text)c=static_cast<wchar_t>(std::towlower(c));
        return text;
    }
    void leave(Destination destination) {
        pending_=Destination::None;prompt.set(false);
        if(destination==Destination::Close) { if(closeWindow)closeWindow(); return; }
        form.set(false);
        if(destination==Destination::Lab && backToLab)backToLab();
    }
    void saved(const Config& snapshot) {
        auto data=records.get();
        auto found=std::find_if(data.begin(),data.end(),[&](const auto& row){return row.id==editingId_;});
        if(found!=data.end())found->config=snapshot;
        else if(creating_)data.insert(data.begin(),{editingId_,snapshot,true});
        else throw std::logic_error("Edited connection no longer exists");
        creating_=false;
        records.set(std::move(data));
        // VmCommand completion is batched; derived dirty/filtered may be stale here.
        const bool visible=matches({editingId_,snapshot,find(editingId_)->online});
        selectedKey.set(visible?editingId_:L"");
        message.set(visible?L"连接已保存 · 仅当前进程有效":L"连接已保存，当前筛选未显示它；筛选条件已保留。");
        if(prompt.get()) {
            promptTitle.set(L"保存已完成");
            promptText.set(editor_.value()==snapshot?L"提交时的配置已保存。可以继续操作，或离开当前页面。":L"提交时的配置已保存；后续输入仍未保存。继续编辑或放弃后续修改。");
        }
        else if(editor_.value()==snapshot)leave(afterSave_);
        else { afterSave_=Destination::List;editor_.message.set(L"已保存提交时的配置；后续修改尚未保存，请再次保存。"); }
    }
public:
    State<std::vector<Connection>> records;
    State<std::wstring> query, selectedKey, message{L"固定种子模拟数据 · 双击或按 Enter 编辑"};
    State<std::wstring> styleError;
    State<std::vector<std::wstring>> filters{{L"全部状态",L"在线",L"离线"}};
    State<std::vector<TableColumn>> columns{{{L"连接名称",0},{L"地址",0},{L"状态",72}}};
    State<int> filter{0};
    State<bool> form{false}, prompt{false},gallery{false};
    State<std::wstring> promptTitle, promptText;
    State<bool> deleting{false};
    VmCommand add, edit, remove, clear, back, keep, discard, saveLeave, confirmDelete;
    VmCommand showGallery;
    std::function<void()> backToLab, closeWindow;
    std::vector<Subscription> subscriptions;

    bool matches(const Connection& row) const {
        if(filter.get()==1 && !row.online)return false;
        if(filter.get()==2 && row.online)return false;
        auto needle=lower(query.get());
        return needle.empty() || lower(row.config.name+L" "+row.config.host+L" "+row.id).find(needle)!=std::wstring::npos;
    }
    const Connection* find(const std::wstring& id) const {
        for(auto& row:records.get())if(row.id==id)return &row;
        return nullptr;
    }
    Computed<std::vector<TableRow>> filtered{[this]{
        std::vector<TableRow> result;
        for(auto& row:records.get())if(matches(row))result.push_back({row.id,{row.config.name,row.config.host+L":"+row.config.port,row.online?L"在线":L"离线"}});
        return result;
    },records,query,filter};
    Computed<bool> showList{[this]{return !form.get() && !prompt.get() && !gallery.get();},form,prompt,gallery};
    Computed<bool> showForm{[this]{return form.get() && !prompt.get();},form,prompt};
    Computed<bool> empty{[this]{return filtered.get().empty();},filtered};
    Computed<bool> hasRows{[this]{return !filtered.get().empty();},filtered};
    Computed<bool> notDeleting{[this]{return !deleting.get();},deleting};
    Computed<bool> canSavePrompt{[this]{return !deleting.get() && !editor_.save.running.get();},deleting,editor_.save.running};
    Computed<std::wstring> count{[this]{return L"显示 "+std::to_wstring(filtered.get().size())+L" / "+std::to_wstring(records.get().size())+L" 条连接";},filtered,records};
    Computed<std::wstring> selection{[this]{auto row=find(selectedKey.get());return row?L"已选择："+row->config.name:std::wstring(L"选择一条连接进行编辑");},selectedKey,records};
    Computed<std::wstring> emptyTitle{[this]{return records.get().empty()?std::wstring(L"还没有连接"):std::wstring(L"没有匹配的连接");},records};
    Computed<std::wstring> emptyText{[this]{return records.get().empty()?std::wstring(L"新建一条本地模拟连接，开始体验编辑流程。"):std::wstring(L"换个名称或地址，或清除搜索与状态筛选。");},records};

    explicit Connections(VM& editor):editor_(editor) {
        showGallery.setAction([this]{gallery.set(true);});
        std::vector<Connection> seed;seed.reserve(1000);
        uint32_t random=0xC0FFEEu;
        const wchar_t* cities[]={L"上海",L"北京",L"杭州",L"深圳",L"成都"};
        for(int i=1;i<=1000;++i) {
            random=random*1664525u+1013904223u;
            Config config;config.name=std::wstring(cities[(random>>16)%5])+L"研发节点 "+std::to_wstring(i);
            config.host=L"10.24."+std::to_wstring(i/250)+L"."+std::to_wstring(i%250+1);
            config.protocol=i%3;config.port=config.protocol==1?L"80":L"443";
            seed.push_back({std::to_wstring(i),config,(random&3)!=0});
        }
        records.set(std::move(seed));
        editor_.backText.set(L"返回连接列表");editor_.saveLabel.set(L"保存并返回");
        editor_.description.set(L"仅修改本地模拟数据 · 保存后返回列表，保留筛选和滚动位置。");
        editor_.back.setAction([this]{request(Destination::List);});
        editor_.onSaved=[this](const Config& config){saved(config);};
        editor_.onSaveFailed=[this]{afterSave_=Destination::List;};
        add.setAction([this]{openNew();});edit.setAction([this]{open(selectedKey.get());});
        back.setAction([this]{request(Destination::Lab);});
        clear.setAction([this]{Batch batch;query.set({});filter.set(0);});
        remove.setAction([this]{
            auto row=find(selectedKey.get());if(!row || !matches(*row))return;
            deletingId_=row->id;pending_=Destination::Delete;deleting.set(true);
            promptTitle.set(L"删除这条连接？");promptText.set(L"将从本地模拟列表中删除「"+row->config.name+L"」。此操作无法撤销。");prompt.set(true);
        });
        keep.setAction([this]{pending_=Destination::None;afterSave_=Destination::List;prompt.set(false);});
        discard.setAction([this]{
            if(editor_.save.running.get() && pending_!=Destination::Close)return;
            auto destination=pending_;if(destination==Destination::None || destination==Destination::Delete)return;
            if(!editor_.save.running.get())editor_.reset.execute();
            leave(destination);
        });
        saveLeave.setAction([this]{
            if(pending_==Destination::None || pending_==Destination::Delete || editor_.save.running.get())return;
            afterSave_=pending_;pending_=Destination::None;prompt.set(false);editor_.save.execute();
            if(!editor_.save.running.get())afterSave_=Destination::List;
        });
        confirmDelete.setAction([this]{
            if(pending_!=Destination::Delete)return;
            const auto& visible=filtered.get();auto old=std::find_if(visible.begin(),visible.end(),[&](const auto& row){return row.id==deletingId_;});
            const auto index=static_cast<std::size_t>(old-visible.begin());
            auto data=records.get();data.erase(std::remove_if(data.begin(),data.end(),[&](const auto& row){return row.id==deletingId_;}),data.end());
            records.set(std::move(data));
            const auto& remaining=filtered.get();selectedKey.set(remaining.empty()?L"":remaining[std::min(index,remaining.size()-1)].id);
            message.set(L"连接已删除 · 其他连接保持不变");leave(Destination::List);
        });
        auto enable=[this](const auto&){const auto* row=find(selectedKey.get());const bool valid=row && matches(*row) && !form.get() && !prompt.get();edit.canExecute.set(valid);remove.canExecute.set(valid);};
        subscriptions.push_back(selectedKey.subscribeScoped(enable));subscriptions.push_back(filtered.subscribeScoped(enable));
        subscriptions.push_back(form.subscribeScoped(enable));subscriptions.push_back(prompt.subscribeScoped(enable));enable(0);
        subscriptions.push_back(editor_.save.running.subscribeScoped([this](bool busy){discard.canExecute.set(!busy || pending_==Destination::Close);}));
    }
    bool needsGuard() const {return form.get() && (creating_ || !(editor_.value()==editor_.saved.get()) || editor_.save.running.get());}
    void open(const std::wstring& id) {
        if(editor_.save.running.get() || prompt.get())return;
        auto row=find(id);if(!row)return;
        editingId_=id;creating_=false;afterSave_=Destination::List;
        editor_.load(row->config);editor_.heading.set(L"编辑连接");form.set(true);
    }
    void openNew() {
        if(editor_.save.running.get() || prompt.get())return;
        editingId_=std::to_wstring(nextId_++);creating_=true;afterSave_=Destination::List;
        Config config;config.name=L"新连接 "+editingId_;editor_.load(config);editor_.heading.set(L"新建连接");form.set(true);
    }
    void request(Destination destination) {
        if(!needsGuard()){leave(destination);return;}
        pending_=destination;deleting.set(false);
        promptTitle.set(editor_.save.running.get()?L"保存仍在进行":L"还有未保存的修改");
        promptText.set(editor_.save.running.get()?L"可以继续等待；关闭应用会取消任务并丢弃未提交结果。":L"保存后再离开，或放弃修改。继续编辑会保留当前输入和光标。");
        discard.canExecute.set(!editor_.save.running.get() || destination==Destination::Close);prompt.set(true);
    }
};

inline Page buildWorkspace(Mount& ui,VM& vm,Connections& flow) {
    const auto oldScope=ui.scope();ui.setScope("scope_Connections_Editor");
    auto button=[&](const wchar_t* title,VmCommand& command,const char* variant="") {
        auto e=ui.make("Button",{},variant);ui.set(e,"text",title);ui.click(e,command);return e;
    };
    auto form=buildPage(ui,vm);ui.condition(form.root,flow.showForm);
    ui.setScope("scope_Connections");
    auto search=ui.make("SearchInput");ui.model(search,flow.query);ui.set(search,"name",L"搜索连接");ui.set(search,"placeholder",L"搜索名称、地址或 ID");
    auto filter=ui.make("Select");ui.bind(filter,"items",flow.filters);ui.model(filter,flow.filter);ui.set(filter,"name",L"连接状态");
    auto theme=ui.make("Select");ui.bind(theme,"items",vm.themes);ui.model(theme,vm.theme);ui.set(theme,"name",L"连接页主题");
    auto table=ui.make("DataTable");ui.bind(table,"columns",flow.columns);
    ui.bind(table,"items",flow.filtered);ui.model(table,flow.selectedKey);ui.set(table,"name",L"连接列表");ui.condition(table,flow.hasRows);
    ui.tableEvent(table,"activate",flow.edit);ui.tableEvent(table,"delete",flow.remove);
    auto count=ui.make("Text",{},"muted");ui.bind(count,"text",flow.count);
    auto selected=ui.make("Text",{},"muted");ui.bind(selected,"text",flow.selection);
    auto status=ui.make("Status");ui.bind(status,"text",flow.message);
    auto empty=ui.make("EmptyState",{button(L"清除筛选",flow.clear)});ui.bind(empty,"title",flow.emptyTitle);ui.bind(empty,"subtitle",flow.emptyText);ui.condition(empty,flow.empty);empty.grow();
    auto density=ui.make("Select");ui.bind(density,"items",vm.densities);ui.model(density,vm.density);ui.set(density,"name",L"界面密度");
    auto header=ui.make("Header",{ui.make("Toolbar",{button(L"返回性能实验台",flow.back),theme,density,button(L"组件与布局",flow.showGallery)})});ui.set(header,"title",L"连接管理");
    auto list=ui.make("ListPage",{
        ui.make("Toolbar",{search,filter,button(L"新建连接",flow.add,"primary"),button(L"编辑选中",flow.edit),button(L"删除选中",flow.remove,"danger")}),
        ui.make("Toolbar",{count,selected}),table,empty,ui.make("ActionBar",{status})});
    auto listRoot=ui.make("Page",{header,list});ui.condition(listRoot,flow.showList);
    auto keep=button(L"继续 / 取消",flow.keep);
    auto discard=button(L"放弃修改并离开",flow.discard,"danger");ui.condition(discard,flow.notDeleting);
    auto save=button(L"保存后离开",flow.saveLeave,"primary");ui.condition(save,flow.canSavePrompt);
    auto remove=button(L"确认删除",flow.confirmDelete,"danger");ui.condition(remove,flow.deleting);
    auto warning=ui.make("DetailPage",{ui.make("ActionBar",{keep,save,discard,remove})});
    ui.bind(warning,"title",flow.promptTitle);ui.bind(warning,"subtitle",flow.promptText);
    auto prompt=ui.make("Page",{warning});ui.condition(prompt,flow.prompt);
    auto styleError=ui.make("ValidationMessage");ui.bind(styleError,"text",flow.styleError);
    auto root=ui.make("Column",{styleError,listRoot,form.root,prompt},"workspace");root.grow();ui.locate(root,__FILE__,__LINE__);ui.locate(list,__FILE__,__LINE__);
    form.fields["$table"]=table.widget;form.fields["$search"]=search.widget;form.fields["$keep"]=keep.widget;
    for(auto& field:form.fields)ui.remember(field.first[0]=='$'?field.first.substr(1):field.first,field.second);
    ui.setScope(oldScope);
    return {root,std::move(form.fields)};
}

} // namespace connection_demo
#include "Connections.g.h"
namespace connection_demo {
struct TemplateModel { VM& editor; Connections& list; };
inline Page buildTemplate(Mount& ui,VM& vm,Connections& flow) {
    TemplateModel model{vm,flow};auto root=build_Connections(model,ui);
    std::map<std::string,std::shared_ptr<Widget>> fields;
    for(auto name:{"name","host","port","timeout","note"})fields[name]=ui.find(name);
    for(auto name:{"table","search","keep"})fields[std::string("$")+name]=ui.find(name);
    return {root,std::move(fields)};
}

struct StyleSession {
    Mount& mount;VM& vm;Connections& flow;
    std::filesystem::path external;
    bool dev=false;
    std::vector<std::pair<std::string,std::string>> sourceFiles=[] {
        auto files=sources_Connections(),gallery=sources_Gallery();files.insert(files.end(),gallery.begin(),gallery.end());return files;
    }();
    std::string validCss;
    std::size_t applied=0;
    static void appendCss(std::string& output,std::string text,const std::string& scope,const std::string& file,int firstLine=1) {
        try{text=syntax::stripComments(std::move(text));}catch(const std::exception& e){throw std::runtime_error(file+":"+std::to_string(firstLine)+":1: "+e.what());}
        std::size_t cursor=0;
        while(cursor<text.size()) {
            const auto begin=text.find_first_not_of(" \t\r\n",cursor);if(begin==text.npos)break;
            const int line=firstLine+int(std::count(text.begin(),text.begin()+begin,'\n'));
            const auto close=text.find('}',begin);
            const auto rule=syntax::css(text.substr(begin,close==text.npos?text.npos:close-begin+1),scope,file,line);
            StyleSheet check;std::string error;
            if(!check.addRulesFromCss(output+rule,&error))throw std::runtime_error(file+":"+std::to_string(line)+":1: "+error);
            output+=rule;if(close==text.npos)break;cursor=close+1;
        }
    }
    static void appendInline(std::string& output,const std::string& text,const std::string& scope,const std::string& file) {
        std::size_t cursor=0;
        while((cursor=text.find("<style",cursor))!=text.npos) {
            const auto begin=text.find('>',cursor),end=text.find("</style>",begin);
            const int line=1+int(std::count(text.begin(),text.begin()+cursor,'\n'));
            if(begin==text.npos || end==text.npos)throw std::runtime_error(file+":"+std::to_string(line)+":1: unclosed style block");
            if(text.substr(cursor,begin-cursor).find("scoped")==text.npos)throw std::runtime_error(file+":"+std::to_string(line)+":1: inline styles require scoped");
            appendCss(output,text.substr(begin+1,end-begin-1),scope,file,1+int(std::count(text.begin(),text.begin()+begin+1,'\n')));cursor=end+8;
        }
    }
    bool apply() {
        try {
            const auto density=vm.density.get()==1?Density::Compact:Density::Comfortable;
            auto css=declarativeTheme(vm.theme.get()==1,density);
            if(dev)for(auto& source:sourceFiles)appendInline(css,readStyleFile(source.first),source.second,source.first);
            else css+=styles_Connections()+styles_Gallery();
            appendCss(css,readStyleFile(external),{},external.string());
            mount.styles()->replace(css,density);validCss=std::move(css);++applied;flow.styleError.set({});return true;
        }catch(const std::exception& e){flow.styleError.set(wide(e.what()));return false;}
    }
    std::vector<std::string> files() const {
        std::vector<std::string> paths{external.string()};for(auto& source:sourceFiles)paths.push_back(source.first);return paths;
    }
};

class Workspace {
    Window& window_;
    std::shared_ptr<int> alive_=std::make_shared<int>(0);
    UiMailbox mailbox_;
    bool scheduled_=false;
    Widget* beforePrompt_=nullptr;
    bool wasForm_=false, wasPrompt_=false;
    bool wasGallery_=false;
    std::unique_ptr<StyleWatcher> watcher_;
    void schedule() {
        if(scheduled_)return;scheduled_=true;auto weak=std::weak_ptr<int>(alive_);
        window_.requestAnimationFrame([this,weak](double){if(weak.expired())return;scheduled_=false;flush();});
    }
public:
    VM vm;
    Connections flow;
    Mount mount;
    Page page;
    StyleSession styles;
    std::unique_ptr<Samples> samples;
    std::unique_ptr<Element> galleryPage;
    Workspace(Window& window,std::function<void()> back,bool useTemplate=false,bool dev=false,std::filesystem::path css=std::filesystem::path(__FILE__).parent_path()/"connections.css")
      :window_(window),vm(mailbox_.sender()),flow(vm),mount([this]{schedule();}),page(useTemplate?buildTemplate(mount,vm,flow):buildWorkspace(mount,vm,flow)),styles{mount,vm,flow,std::move(css),dev} {
        auto weak=std::weak_ptr<int>(alive_);
        mailbox_.setWake([this,weak]{window_.post([this,weak]{if(!weak.expired())mailbox_.drain();});});
        flow.backToLab=std::move(back);flow.closeWindow=[this]{window_.close();};
        mount.styles()->replace(declarativeTheme(vm.theme.get()==1));
        mount.watch(vm.theme,[this](int){styles.apply();});
        mount.watch(vm.density,[this](int){styles.apply();});
        // Build this optional showcase once, only when first requested.
        mount.watch(flow.gallery,[this](bool shown){
            if(shown && !galleryPage) {
                samples=std::make_unique<Samples>();
                galleryPage=std::make_unique<Element>(buildGallery(mount,vm,*samples));
                page.root.as<Stack>()->add(galleryPage->widget);
                page.root.as<Stack>()->setFlex(galleryPage->widget,galleryPage->flex);
                samples->back.setAction([this]{flow.gallery.set(false);});
            }
            if(galleryPage)galleryPage->widget->setVisible(shown);
        });
        if(dev)watcher_=std::make_unique<StyleWatcher>(mailbox_.sender(),styles.files(),[this]{styles.apply();});
        vm.focusError=[this](const std::string& field){flush();window_.prepareLayoutSnapshot();window_.requestFocus(page.fields.at(field).get());revealField(page.root.widget,page.fields.at(field).get());};
    }
    ~Workspace(){close();}
    void close(){watcher_.reset();mailbox_.close();alive_.reset();}
    void flush() {
        const bool prompt=flow.prompt.get(), form=flow.form.get();
        if(prompt && !wasPrompt_)beforePrompt_=focusedField(page.root.widget);
        mount.flush();window_.prepareLayoutSnapshot();
        if(prompt && !wasPrompt_)window_.requestFocus(page.fields.at("$keep").get());
        else if(!prompt && wasPrompt_ && form==wasForm_ && beforePrompt_)window_.requestFocus(beforePrompt_);
        else if(form!=wasForm_)window_.requestFocus(page.fields.at(form?"name":"$table").get());
        wasForm_=form;wasPrompt_=prompt;
        if(flow.gallery.get()!=wasGallery_) {
            window_.requestFocus(flow.gallery.get()?mount.find("galleryBack").get():page.fields.at("$search").get());
            wasGallery_=flow.gallery.get();
        }
    }
    void afterTab(){auto weak=std::weak_ptr<int>(alive_);window_.post([this,weak]{if(weak.expired())return;window_.prepareLayoutSnapshot();revealField(page.root.widget,focusedField(page.root.widget));});}
};
} // namespace connection_demo
