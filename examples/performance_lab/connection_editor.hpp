#pragma once
#include <oneui/platform/window.h>
#include <oneui/ui_theme.h>
#include <oneui/ui_focus.h>
#include <oneui/ui_layout_diagnostics.h>
#include <chrono>
#include <regex>

namespace connection_demo {
using namespace oneui;
using namespace oneui::ui;
struct Config {
    std::wstring name=L"上海研发节点",host=L"127.0.0.1",port=L"443",timeout=L"30",note=L"本地模拟连接，不会访问网络。";
    int protocol=0;bool reconnect=true;
    bool operator==(const Config& b)const{return name==b.name && host==b.host && port==b.port && timeout==b.timeout && note==b.note && protocol==b.protocol && reconnect==b.reconnect;}
};
inline std::wstring numberError(const std::wstring& value,int max,const wchar_t* message) {
    try{if(value.empty() || value.find_first_not_of(L"0123456789")!=value.npos || value.size()>6)throw std::invalid_argument("digits");int n=std::stoi(value);if(n<1 || n>max)throw std::invalid_argument("range");return {};}catch(...){return message;}
}
struct VM {
    State<std::wstring> name{Config{}.name},host{Config{}.host},port{Config{}.port},timeout{Config{}.timeout},note{Config{}.note};
    State<int> protocol{0},theme{1},density{0};State<bool> reconnect{true},attempted{false};State<Config> saved{Config{}};
    State<std::vector<std::wstring>> densities{{L"舒适密度",L"紧凑密度"}};
    State<std::vector<std::wstring>> themes{{L"浅色",L"深色"}}, protocols{{L"HTTPS · 模拟",L"HTTP · 模拟",L"TCP · 模拟"}};
    State<std::wstring> message{L"本地模拟配置 · 尚无修改"};
    VmCommand save,reset,failNext,back;
    State<std::wstring> heading{L"编辑连接"}, backText{L"返回性能实验台"}, saveLabel{L"保存连接"};
    State<std::wstring> description{L"本地模拟连接 · 返回实验台会保留草稿，关闭应用后丢弃。"};
    std::function<void(const std::string&)> focusError;
    std::function<void(const Config&)> onSaved;
    std::function<void()> onSaveFailed;
    bool fail=false;
    Config value()const{return {name.get(),host.get(),port.get(),timeout.get(),note.get(),protocol.get(),reconnect.get()};}
    std::wstring error(const std::string& field) const {
        if(field=="name" && (name.get().find_first_not_of(L" \t\r\n")==name.get().npos || name.get().size()>64))return L"请输入 1–64 字的连接名称。";
        if(field=="host") {
            const auto& h=host.get();
            if(h.size()>253 || !std::regex_match(h,std::wregex(L"[A-Za-z0-9]([A-Za-z0-9.-]*[A-Za-z0-9])?")) || h.find(L"..")!=h.npos)return L"请输入主机名或 IPv4 地址，不含协议、路径和空格。";
            std::wistringstream parts(h);std::wstring part;bool numeric=h.find_first_not_of(L"0123456789.")==h.npos;int count=0;
            while(std::getline(parts,part,L'.')){++count;if(part.size()>63 || part.front()==L'-' || part.back()==L'-')return L"主机名的每段最多 63 字，且不能以连字符开头或结尾。";if(numeric && (part.size()>3 || std::stoi(part)>255))return L"IPv4 地址每一段应在 0–255 之间。";}
            if(numeric && count!=4)return L"IPv4 地址需要四段数字。";
        }
        if(field=="port")return numberError(port.get(),65535,L"端口应为 1–65535 的整数。");
        if(field=="timeout")return numberError(timeout.get(),300,L"超时应为 1–300 秒的整数。");
        if(field=="note" && note.get().size()>200)return L"备注最多 200 字。";
        return {};
    }
    Computed<std::wstring> nameError{[this]{return attempted.get()?error("name"):L"";},name,attempted};
    Computed<std::wstring> hostError{[this]{return attempted.get()?error("host"):L"";},host,attempted};
    Computed<std::wstring> portError{[this]{return attempted.get()?error("port"):L"";},port,attempted};
    Computed<std::wstring> timeoutError{[this]{return attempted.get()?error("timeout"):L"";},timeout,attempted};
    Computed<std::wstring> noteError{[this]{return attempted.get()?error("note"):L"";},note,attempted};
    Computed<bool> dirty{[this]{return !(value()==saved.get());},name,host,port,timeout,note,protocol,reconnect,saved};
    Computed<std::wstring> saveText{[this]{return save.running.get()?std::wstring(L"正在保存…"):saveLabel.get();},save.running,saveLabel};
    Computed<std::wstring> status{[this]{if(save.running.get())return std::wstring(L"正在保存本地配置…");if(!save.error.get().empty())return save.error.get();return message.get();},save.running,save.error,message};
    Computed<std::wstring> tone{[this]{return std::wstring(save.running.get()?L"pending":!save.error.get().empty()?L"error":dirty.get()?L"warning":L"success");},save.running,save.error,dirty};
    std::vector<Subscription> subscriptions;
    explicit VM(UiMailbox::Sender sender) {
        subscriptions.push_back(dirty.subscribeScoped([this](bool changed){message.set(changed?L"有未保存的更改":L"当前修改已保存");}));
        save.setAction([this,sender]{
            attempted.set(true);
            for(auto field:{"name","host","port","timeout","note"})if(!error(field).empty()) {message.set(L"请修正标出的字段后保存。");if(focusError)focusError(field);return;}
            const auto snapshot=value();const bool failure=fail;fail=false;
            save.runAsync(sender,[failure](auto cancellation){for(int i=0;i<12 && !cancellation.cancelled();++i)std::this_thread::sleep_for(std::chrono::milliseconds(50));return failure?std::wstring(L"模拟保存失败，请重试；输入内容已保留。"):std::wstring{};},[this,snapshot](bool ok){if(ok){saved.set(snapshot);message.set(!(value()==snapshot)?L"已保存提交时的配置；后续编辑尚未保存。":L"连接已保存 · 仅当前进程有效");if(onSaved)onSaved(snapshot);}else if(onSaveFailed)onSaveFailed();});
        });
        reset.setAction([this]{if(save.running.get())return;auto c=saved.get();Batch batch;name.set(c.name);host.set(c.host);port.set(c.port);timeout.set(c.timeout);note.set(c.note);protocol.set(c.protocol);reconnect.set(c.reconnect);attempted.set(false);save.error.set({});fail=false;message.set(L"已恢复上次保存的配置");});
        failNext.setAction([this]{fail=true;message.set(L"下次保存将模拟失败，可再次保存重试。");});
        subscriptions.push_back(save.running.subscribeScoped([this](bool busy){reset.canExecute.set(!busy);failNext.canExecute.set(!busy);}));
    }
    void load(const Config& config) {
        if(save.running.get())throw std::logic_error("Cannot replace a saving draft");
        Batch batch;
        name.set(config.name);host.set(config.host);port.set(config.port);timeout.set(config.timeout);
        note.set(config.note);protocol.set(config.protocol);reconnect.set(config.reconnect);saved.set(config);
        attempted.set(false);save.error.set({});fail=false;message.set(L"本地模拟配置 · 尚无修改");
    }
};

struct Page { Element root;std::map<std::string,std::shared_ptr<Widget>> fields; };
inline Page buildPage(Mount& ui,VM& vm) {
    std::map<std::string,std::shared_ptr<Widget>> fields;
    auto text=[&](const wchar_t* value){auto e=ui.make("Text");ui.set(e,"text",value);return e;};
    auto action=[&](const wchar_t* text,VmCommand& cmd,const char* style=""){auto e=ui.make("Button",{},style);ui.set(e,"text",text);ui.click(e,cmd);return e;};
    auto input=[&](const char* id,const wchar_t* label,const wchar_t* hint,auto& state,auto& error){auto control=ui.make("Input");ui.model(control,state);fields[id]=control.widget;auto row=ui.make("FormRow",{control});ui.set(row,"label",label);ui.set(row,"hint",hint);ui.bind(row,"error",error);ui.locate(row,__FILE__,__LINE__);return row;};
    auto protocol=ui.make("Select");ui.bind(protocol,"items",vm.protocols);ui.model(protocol,vm.protocol);
    auto protocolRow=ui.make("FormRow",{protocol});ui.set(protocolRow,"label",L"连接协议");ui.set(protocolRow,"hint",L"仅保存配置，不发起实际连接");
    auto generalGrid=ui.make("FormGrid",{input("name",L"连接名称",L"用于识别此连接，支持中文",vm.name,vm.nameError),input("host",L"主机地址",L"主机名或 IPv4，不含 https://",vm.host,vm.hostError),input("port",L"端口",L"1–65535",vm.port,vm.portError),protocolRow});
    auto general=ui.make("Section",{generalGrid});ui.set(general,"title",L"连接信息");
    auto reconnect=ui.make("Switch");ui.set(reconnect,"text",L"允许自动重连");ui.model(reconnect,vm.reconnect);
    auto reconnectRow=ui.make("FormRow",{reconnect});ui.set(reconnectRow,"label",L"断线处理");ui.set(reconnectRow,"hint",L"本地配置选项，不启动后台网络任务");
    auto advanced=ui.make("Section",{input("timeout",L"连接超时",L"1–300 秒",vm.timeout,vm.timeoutError),reconnectRow,input("note",L"备注",L"最多 200 字",vm.note,vm.noteError)});ui.set(advanced,"title",L"连接行为");
    auto status=ui.make("Status");ui.bind(status,"text",vm.status);ui.bind(status,"tone",vm.tone);
    auto save=action(L"",vm.save,"primary");ui.bind(save,"text",vm.saveText);
    auto footer=ui.make("ActionBar",{save,action(L"恢复已保存",vm.reset),action(L"模拟失败",vm.failNext),status});
    auto body=ui.make("SettingsPage",{general,advanced,footer});
    auto theme=ui.make("Select");ui.bind(theme,"items",vm.themes);ui.set(theme,"name",L"编辑页主题");ui.model(theme,vm.theme);
    auto density=ui.make("Select");ui.bind(density,"items",vm.densities);ui.set(density,"name",L"界面密度");ui.model(density,vm.density);
    auto description=text(L"");ui.bind(description,"text",vm.description);
    auto back=action(L"",vm.back);ui.bind(back,"text",vm.backText);
    auto header=ui.make("Header",{description,ui.make("Toolbar",{back,theme,density})});ui.bind(header,"title",vm.heading);
    auto root=ui.make("Page",{header,body});ui.locate(root,__FILE__,__LINE__);return {root,std::move(fields)};
}

class Editor {
    Window& window_;
    std::shared_ptr<int> alive_=std::make_shared<int>(0);
    UiMailbox mailbox_;
    bool scheduled_=false;
    void schedule(){if(scheduled_)return;scheduled_=true;auto weak=std::weak_ptr<int>(alive_);window_.requestAnimationFrame([this,weak](double){if(weak.expired())return;scheduled_=false;mount.flush();});}
public:
    VM vm;
    Mount mount;
    Page page;
    Editor(Window& window,std::function<void()> back):window_(window),vm(mailbox_.sender()),mount([this]{schedule();}),page(buildPage(mount,vm)) {
        auto weak=std::weak_ptr<int>(alive_);
        mailbox_.setWake([this,weak]{window_.post([this,weak]{if(!weak.expired())mailbox_.drain();});});
        vm.back.setAction(std::move(back));
        auto style=[this](int){auto density=vm.density.get()==1?Density::Compact:Density::Comfortable;mount.styles()->replace(declarativeTheme(vm.theme.get()==1,density),density);};
        mount.watch(vm.theme,style);mount.watch(vm.density,style);
        vm.focusError=[this](const std::string& field){mount.flush();window_.prepareLayoutSnapshot();window_.requestFocus(page.fields.at(field).get());revealField(page.root.widget,page.fields.at(field).get());};
    }
    ~Editor(){close();}
    void close(){mailbox_.close();alive_.reset();}
    void afterTab(){auto weak=std::weak_ptr<int>(alive_);window_.post([this,weak]{if(weak.expired())return;window_.prepareLayoutSnapshot();revealField(page.root.widget,focusedField(page.root.widget));});}
};
} // namespace connection_demo
