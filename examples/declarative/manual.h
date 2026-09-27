#pragma once
#include "vm.h"
// Intentionally authored by hand, not generated: same adapters, VM and tree.
inline Element buildManual(DemoVM& vm,Mount& ui) {
    ui.setScope("scope_Demo");
    auto text=[&](std::wstring value,std::string cls="") { auto e=ui.make("Text",{},cls);ui.set(e,"text",value);return e; };
    auto button=[&](std::wstring value,VmCommand& cmd,std::string variant="") {auto e=ui.make("Button",{},variant);ui.set(e,"text",value);ui.click(e,cmd);return e;};
    auto nav=ui.make("Toolbar",{button(L"偏好设置",vm.showSettings),button(L"连接管理",vm.showConnections)});
    auto styleError=ui.make("ValidationMessage"); ui.bind(styleError,"text",vm.styleError);
    auto navSlot=ui.make("Column",{nav}); auto defaultSlot=ui.make("Column",{styleError});
    ui.setScope("scope_Demo_WorkspaceHeader");
    auto header=ui.make("Header",{text(L"配置工作习惯，管理连接。所有数据均为本地模拟。","muted"),navSlot,defaultSlot});ui.set(header,"title",L"工作空间");
    ui.setScope("scope_Demo");
    auto name=ui.make("Input");ui.model(name,vm.name);ui.set(name,"placeholder",L"输入工作空间名称");
    auto toggle=ui.make("Switch");ui.model(toggle,vm.autoRefresh);ui.set(toggle,"text",L"启用自动刷新");
    auto interval=ui.make("Input");ui.model(interval,vm.interval);ui.set(interval,"placeholder",L"30");
    auto theme=ui.make("Select");ui.bind(theme,"items",vm.themes);ui.model(theme,vm.theme);
    auto field=[&](const wchar_t* label,const wchar_t* hint,Element control) {auto e=ui.make("FormRow",{control});ui.set(e,"label",label);ui.set(e,"hint",hint);return e;};
    auto nameRow=field(L"工作空间名称",L"用于区分当前工作环境",name);ui.bind(nameRow,"error",vm.nameError);
    auto intervalRow=field(L"刷新间隔",L"1–3600 秒；关闭自动刷新后仍保留配置",interval);ui.bind(intervalRow,"error",vm.intervalError);
    auto section=ui.make("Section",{
        nameRow,field(L"自动刷新",L"保持连接状态为最新",toggle),
        intervalRow,field(L"外观",L"立即预览，不影响当前输入内容",theme)});
    ui.set(section,"title",L"常规设置");
    auto save=button(L"",vm.save,"primary");ui.bind(save,"text",vm.saveText);
    auto status=ui.make("Status");ui.bind(status,"text",vm.status);ui.bind(status,"tone",vm.statusTone);
    auto actions=ui.make("ActionBar",{save,button(L"模拟保存失败",vm.failNext),status});
    auto tips=ui.repeat(vm.tips,[](auto& item){return item->id;},[](Mount& mount,auto item){auto e=mount.make("Text",{},"muted");mount.bind(e,"text",item->text);return e;});
    auto settings=ui.make("SettingsPage",{section,actions,tips});ui.condition(settings,vm.settings);
    auto search=ui.make("SearchInput");ui.model(search,vm.query);ui.set(search,"placeholder",L"搜索名称或地址");ui.set(search,"name",L"搜索连接");
    auto filter=ui.make("Select");ui.bind(filter,"items",vm.filters);ui.model(filter,vm.filter);ui.set(filter,"name",L"连接状态");
    auto tools=ui.make("Toolbar",{search,filter,button(L"新建连接",vm.add,"primary"),button(L"刷新",vm.reload),button(L"查看详情",vm.showDetails),button(L"删除选中",vm.remove,"danger")});
    auto count=text(L"","muted");ui.bind(count,"text",vm.count);
    auto selected=ui.make("Status");ui.bind(selected,"text",vm.selectedStatus);ui.bind(selected,"tone",vm.selectedTone);
    auto summary=ui.make("Toolbar",{count,selected});
    auto loading=ui.make("LoadingState");ui.set(loading,"subtitle",L"正在刷新本地模拟连接，筛选条件和选择将保留。");ui.condition(loading,vm.reload.running);
    auto empty=ui.make("EmptyState");ui.set(empty,"title",L"没有匹配的连接");ui.set(empty,"subtitle",L"试试其他名称、地址，或切换状态筛选。");ui.condition(empty,vm.showEmpty);
    auto table=ui.make("DataTable");ui.bind(table,"columns",vm.columns);ui.bind(table,"items",vm.filtered);ui.model(table,vm.selectedKey);ui.set(table,"name",L"连接列表");ui.condition(table,vm.showTable);
    auto connections=ui.make("ListPage",{tools,summary,loading,empty,table});ui.condition(connections,vm.connections);
    auto bound=[&](auto& source){auto e=text(L"");ui.bind(e,"text",source);return e;};
    auto detailStatus=ui.make("Status");ui.bind(detailStatus,"text",vm.selectedStatus);ui.bind(detailStatus,"tone",vm.selectedTone);
    auto detailSection=ui.make("Section",{field(L"连接名称",L"",bound(vm.selectedName)),field(L"地址",L"",bound(vm.selectedAddress)),field(L"状态",L"",detailStatus),field(L"延迟",L"",bound(vm.selectedLatency))});ui.set(detailSection,"title",L"基本信息");
    auto detail=ui.make("DetailPage",{detailSection,ui.make("ActionBar",{button(L"返回连接列表",vm.showConnections)})});ui.set(detail,"title",L"连接详情");ui.set(detail,"subtitle",L"查看当前连接的本地模拟信息。");ui.condition(detail,vm.details);
    ui.locate(settings,__FILE__,__LINE__);ui.locate(connections,__FILE__,__LINE__);ui.locate(detail,__FILE__,__LINE__);
    return ui.make("Page",{header,settings,connections,detail});
}
