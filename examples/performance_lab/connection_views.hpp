#pragma once
#include "connection_models.hpp"

namespace connection_demo {
// The same public layout components handle read-only fields and editable fields.
inline Page buildDetails(Mount &mount, VM &vm, Connections &flow) {
    Compose ui(mount);
    auto info =
        ui.section({ui.formGrid(
                       {ui.field(L"连接名称", ui.text(flow.detailName)),
                        ui.field(L"连接状态", ui.status(flow.detailStatus).tone(flow.detailTone)),
                        ui.field(L"主机地址", ui.text(flow.detailHost)),
                        ui.field(L"端口", ui.text(flow.detailPort)),
                        ui.field(L"连接协议", ui.text(flow.detailProtocol)),
                        ui.field(L"连接 ID", ui.text(flow.detailKey))})})
            .title(L"连接信息");
    auto behavior = ui.section({ui.formGrid({ui.field(L"连接超时", ui.text(flow.detailTimeout)),
                                             ui.field(L"断线处理", ui.text(flow.detailReconnect))}),
                                ui.field(L"备注", ui.text(flow.detailNote))})
                        .title(L"连接行为");
    auto edit = ui.button(L"编辑连接", flow.edit).primary(),
         back = ui.button(L"返回连接列表", flow.detailBack);
    auto body = ui.detailPage({info, behavior,
                               ui.actions({edit, ui.button(L"删除连接", flow.remove).danger(),
                                           ui.status(flow.message)})});
    auto header = ui.header({ui.text(L"已保存的本地配置 · 状态为固定模拟数据，不发起网络连接。"),
                             ui.toolbar({back, ui.select(vm.themes, vm.theme).name(L"详情页主题"),
                                         ui.select(vm.densities, vm.density).name(L"界面密度")})})
                      .title(L"连接详情");
    return {ui.page({header, body}).located(__FILE__, __LINE__),
            {{"$detailEdit", edit.widget()}, {"$detailBack", back.widget()}}};
}

inline Page buildWorkspace(Mount &mount, VM &vm, Connections &flow) {
    Compose ui(mount);
    const auto oldScope = mount.scope();
    mount.setScope("scope_Connections_Editor");
    auto form = buildPage(mount, vm);
    mount.condition(form.root, flow.showForm);
    mount.setScope("scope_Connections_Details");
    auto detail = buildDetails(mount, vm, flow);
    mount.condition(detail.root, flow.showDetail);
    mount.setScope("scope_Connections");
    auto search = ui.search(flow.query).name(L"搜索连接").placeholder(L"搜索名称、地址或 ID");
    auto table = ui.table(flow.columns, flow.filtered, flow.selectedKey)
                     .name(L"连接列表")
                     .visible(flow.hasRows)
                     .onActivate(flow.view)
                     .onDelete(flow.remove);
    auto empty = ui.emptyState({ui.button(L"清除筛选", flow.clear).visible(flow.hasRecords),
                                ui.button(L"新建连接", flow.add).primary().visible(flow.noRecords)})
                     .title(flow.emptyTitle)
                     .subtitle(flow.emptyText)
                     .visible(flow.empty)
                     .grow();
    auto header = ui.header({ui.toolbar({ui.button(L"返回性能实验台", flow.back),
                                         ui.select(vm.themes, vm.theme).name(L"连接页主题"),
                                         ui.select(vm.densities, vm.density).name(L"界面密度"),
                                         ui.button(L"组件与布局", flow.showGallery)})})
                      .title(L"连接管理");
    auto list =
        ui.listPage(
              {ui.toolbar({search, ui.select(flow.filters, flow.filter).name(L"连接状态"),
                           ui.button(L"新建连接", flow.add).primary(),
                           ui.button(L"查看详情", flow.view), ui.button(L"编辑选中", flow.edit),
                           ui.button(L"删除选中", flow.remove).danger()}),
               ui.toolbar({ui.text(flow.count).classes("muted"),
                           ui.text(flow.selection).classes("muted")}),
               table, empty, ui.actions({ui.status(flow.message)})})
            .located(__FILE__, __LINE__);
    auto keep = ui.button(L"继续 / 取消", flow.keep);
    auto warning =
        ui
            .detailPage({ui.actions(
                {keep,
                 ui.button(L"保存后离开", flow.saveLeave).primary().visible(flow.canSavePrompt),
                 ui.button(L"放弃修改并离开", flow.discard).danger().visible(flow.notDeleting),
                 ui.button(L"确认删除", flow.confirmDelete).danger().visible(flow.deleting)})})
            .title(flow.promptTitle)
            .subtitle(flow.promptText);
    auto root =
        ui.column({ui.validation(flow.styleError), ui.page({header, list}).visible(flow.showList),
                   detail.root, form.root, ui.page({warning}).visible(flow.prompt)})
            .classes("workspace")
            .grow()
            .located(__FILE__, __LINE__);
    form.fields.insert(detail.fields.begin(), detail.fields.end());
    form.fields["$table"] = table.widget();
    form.fields["$search"] = search.widget();
    form.fields["$keep"] = keep.widget();
    for (auto &field : form.fields)
        mount.remember(field.first[0] == '$' ? field.first.substr(1) : field.first, field.second);
    mount.setScope(oldScope);
    return {root, std::move(form.fields)};
}

} // namespace connection_demo
#include "Connections.g.h"
namespace connection_demo {
struct TemplateModel {
    VM &editor;
    Connections &list;
};
inline Page buildTemplate(Mount &ui, VM &vm, Connections &flow) {
    TemplateModel model{vm, flow};
    auto root = build_Connections(model, ui);
    std::map<std::string, std::shared_ptr<Widget>> fields;
    for (auto name : {"name", "host", "port", "timeout", "note"})
        fields[name] = ui.find(name);
    for (auto name : {"table", "search", "keep", "detailEdit", "detailBack"})
        fields[std::string("$") + name] = ui.find(name);
    return {root, std::move(fields)};
}
} // namespace connection_demo
