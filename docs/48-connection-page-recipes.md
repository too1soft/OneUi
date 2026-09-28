# 从字段到完整连接管理页

本例展示如何使用默认组件做出列表、详情和编辑页。它运行在性能实验台中；全部数据为本地模拟，关闭窗口后丢弃，不访问服务器。

## 先运行，再改一个字段

完成仓库 README 的 Skia 准备后，在仓库根目录执行：

```powershell
# C++ 组合入口；先搜索，再双击一条记录查看详情。
.\examples\performance_lab\run.ps1 -Build -Connections -Entry code -Dev
# 关闭前一个窗口，再体验相同 ViewModel 的模板入口。
.\examples\performance_lab\run.ps1 -Connections -Entry template -Dev
# 直接打开第一条连接的详情。
.\examples\performance_lab\run.ps1 -Details
```

连接列表与详情默认浅色，页头可切换深色及紧凑密度。双击／Enter 打开详情，也可以用“编辑选中”直接编辑。详情中的“编辑连接”进入表单，保存或取消后返回详情；从列表直接编辑则返回列表。Esc 遵循同一返回路径；有未保存修改时先确认，Ctrl+S 保存。

试着搜索某条连接，从详情修改它的名字并保存。即使新名字不再匹配搜索条件，详情仍显示同一个业务 ID 的已保存配置；回到列表时保留筛选，无匹配时显示空状态。删除需要确认；取消确认仍回到原页面。保存失败保留输入，可以重试。

## 复制一个小程序，体验默认布局

[details.cpp](../examples/declarative/details.cpp) 是完整的 C++ 程序，只包含一个详情页和主题切换按钮，不依赖性能实验台。构建后即可运行：

```powershell
.\examples\declarative\build.ps1
.\examples\declarative\build\bin\oneui-details.exe
```

先改 `name`、`host`、`note` 的初始内容，再调整窗口宽度。以下是程序中的字段组合方式：

```cpp
oneui::ui::Compose ui(app.mount());
auto grid = ui.formGrid({
    ui.field(L"连接名称", ui.text(name)),
    ui.field(L"主机地址", ui.text(host))
});
```

`name`、`host` 是 `State<std::wstring>`。把 `ui.text(name)` 换成 `ui.input(name)`，同样的字段布局就成为双向绑定的可编辑表单。可运行的输入示例见 [hello.cpp](../examples/declarative/hello.cpp)。

模板中的对应写法如下；完整详情模板见 [Details.one](../examples/performance_lab/views/Details.one)。

```html
<DetailPage>
  <Section title="连接信息">
    <FormGrid>
      <FormRow label="连接名称"><Text :text="list.detailName" /></FormRow>
      <FormRow label="主机地址"><Text :text="list.detailHost" /></FormRow>
    </FormGrid>
  </Section>
  <ActionBar>
    <Button variant="primary" @click="list.edit">编辑连接</Button>
  </ActionBar>
</DetailPage>
```

## 每种布局由谁负责

| 需求 | 组合 | 框架负责 |
|---|---|---|
| 列表占满空间 | `ListPage` + `DataTable` | 表格占剩余高度，原生虚拟化与滚动 |
| 详情可读、正文不过宽 | `DetailPage` + `Section` | 正文最大宽度 1040、正文滚动、分组间距 |
| 编辑表单 | `SettingsPage` + `FormRow` | 正文最大宽度 880、字段标签与说明、校验提示 |
| 自动分栏 | `FormGrid` 包住 `FormRow` | 默认 320 逻辑像素列基准，空间不足时换行 |
| 保存等操作始终可见 | 页面直接子节点 `ActionBar` | 固定底部、按钮和反馈自动换行 |
| 长备注 | 独立 `FormRow` + `Text` | 文本测量与换行，窄窗口标签上移 |
| 搜索、筛选和操作 | `Toolbar` | 横向排列，空间不足时换行 |

本轮没有新增布局算法或页面专用 CSS。详情直接复用现有公共组件和默认主题；页面字段不计算坐标，也没有 resize 回调。性能实验台的外层自绘图表容器仍有自己的布局，不属于这个字段布局示例。

`FormGrid` 只接受直接 `FormRow` 子节点，不支持跨列和完整浏览器 CSS Grid。`basis/min/max` 表示父容器的主轴尺寸，不能一律当成宽度。自定义复杂工作台仍需要设计；这套默认组合主要简化常规表单、列表和详情。

## 修改代码时的入口

| 修改内容 | 文件／入口 |
|---|---|
| 数据、搜索、页面返回、删除确认 | [connection_workspace.hpp](../examples/performance_lab/connection_workspace.hpp) 的 `Connections` |
| C++ 详情布局 | 同文件的 `buildDetails` |
| 模板页面组合 | [Connections.one](../examples/performance_lab/views/Connections.one) 导入 `Details` 和 `Editor` |
| 表单校验、保存快照、失败重试 | [connection_editor.hpp](../examples/performance_lab/connection_editor.hpp) 的 `VM` |
| 详情模板 | [Details.one](../examples/performance_lab/views/Details.one) |
| 默认外观 | [ui_theme.h](../include/oneui/ui_theme.h)；应用覆盖写在 [connections.css](../examples/performance_lab/connections.css) |

例如在 `connections.css` 添加 `Section { gap: 28px; }`，开发模式下保存后会更新间距；删除该规则恢复默认主题值。放在 `Details.one` 的 `<style scoped>` 内只影响详情页。CSS 可热更新，模板结构、import 和 C++ 变更需要关闭应用后重新构建。

页面创建后保留原生控件，通过可见性切换。详情绑定独立的稳定业务 ID，表格选择可以因筛选清空，详情仍能显示原记录；不将表格行号当成业务身份。保存完成更新原数据，详情使用 `Computed` 自动刷新。示例中的返回目的地属于业务流程，无需在布局组件里写特殊判断。

## 验证与截图

运行 `examples/performance_lab/build.ps1 -Test` 可验证两种入口各 300 次列表／详情／编辑往返，及 84 组主题、密度、宽度和页面状态绘制一致性。长中文覆盖 64 字名称与 200 字备注；热更新包括作用域隔离、错误回滚、删除规则及输入保留。截图脚本为 `examples/performance_lab/capture-connections.ps1`。

具体结果与尚未完成的真实 DPI／跨屏／输入法验收见 [本轮验证记录](49-connection-workflow-validation.md)。程序内部缩放与组合输入状态测试不能替代系统输入法候选窗口验收。

2026-09-28：C++ 示例已改用 `Compose`，公共默认间距和只读文字层级已精修。[新写法与当前截图／实测](50-typed-authoring-and-defaults.md)；上面的 49 号报告保留为上一阶段记录。
