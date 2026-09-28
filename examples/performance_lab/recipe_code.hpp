#pragma once
namespace connection_demo {
inline Element buildRecipeCode(Mount& mount,VM&,Samples& s) {
    Compose ui(mount);
    auto list=ui.listPage({
        ui.toolbar({ui.search(s.query).ref("recipeSearch").placeholder(L"搜索节点").name(L"搜索连接"),ui.button(L"查看详情",s.openDetail).disabled(s.noSelection)}),
        ui.table(s.columns,s.rows,s.selected).ref("recipeTable").visible(s.hasRows).onActivate(s.openDetail).name(L"连接列表"),
        ui.emptyState({ui.button(L"清除搜索",s.clear)}).title(L"没有匹配的节点").subtitle(L"调整搜索条件，或清除搜索。").visible(s.empty).grow(),
        ui.actions({ui.status(s.count)})
    }).title(L"连接管理").subtitle(L"双击或按 Enter 查看详情；窄窗口在列表与详情之间切换。");
    auto detail=ui.detailPage({
        ui.surface({
            ui.field(L"名称",ui.text(s.nodeName)),ui.field(L"业务 ID",ui.text(s.selected)),
            ui.field(L"表面层次",ui.select(s.appearances,s.appearanceIndex).ref("recipeAppearance")),
            ui.text(L"示例数据仅用于展示。切换外观和窗口尺寸会保留选择与滚动位置。").classes("muted")
        }).appearance(s.surfaceAppearance).visible(s.hasSelection),
        ui.emptyState().title(L"先选择一个节点").subtitle(L"在左侧列表中选择，或双击查看详情。").visible(s.noSelection),
        ui.actions({ui.button(L"返回列表",s.closeDetail).ref("recipeBack")})
    }).title(L"连接详情");
    auto settings=ui.settingsPage({
        ui.surface({ui.formGrid({
            ui.field(L"连接名称",ui.input(s.name).ref("recipeName")).hint(L"清空后应用可查看验证提示").error(s.error),
            ui.field(L"自动重连",ui.toggle(s.enabled).text(L"允许重连")).hint(L"本地演示开关")
        })}).appearance(L"outlined"),
        ui.actions({ui.button(L"应用设置",s.applySettings).primary(),ui.status(s.saveStatus)})
    }).ref("recipeSettings").title(L"工作区设置").subtitle(L"字段自动分栏；底部操作保持可见。仅修改本次演示。").visible(s.workspaceSettings);
    return ui.sidebarLayout(ui.sidebar({
        ui.text(L"演示工作区").classes("section-title"),
        ui.button(L"连接管理",s.showWorkspaceList).variant(s.listVariant),
        ui.button(L"工作区设置",s.showWorkspaceSettings).variant(s.settingsVariant)
    }),ui.column({ui.masterDetail(list,detail).ref("recipePanes").detailOpen(s.detailOpen).visible(s.workspaceList),settings}).grow().basis(0)).ref("recipeShell");
}
}
