# 页面骨架与布局诊断

C++ 与模板共用 SettingsPage、ListPage、DetailPage，分别使用 `ui.make("SettingsPage", children)` 或同名标签。三个组件都是窗口内的内容区，不创建窗口，也不内置业务导航。可选 title/subtitle 为空时不占高度。

| 组件 | 默认布局 |
|---|---|
| SettingsPage | 标题固定，正文限宽 880 并滚动，ActionBar 固定在页面底部 |
| DetailPage | 标题固定，正文限宽 1040 并滚动，ActionBar 固定在页面底部 |
| ListPage | 标题和工具栏按内容高度排列，DataTable 占剩余空间并使用原生滚动 |
| ActionBar | 操作和反馈自动换行；每个页面至多一个直接子 ActionBar |

直接子 ActionBar 会移至底部，无论在作者代码中的位置；嵌套在 Section 中的 ActionBar 仍是普通可换行布局。隐藏页面继续拥有控件和订阅，沿用 v-if 的保留语义。

```html
<SettingsPage title="偏好设置">
  <Section title="常规设置">
    <FormRow label="名称" hint="用于区分工作环境" :error="nameError">
      <Input v-model="name" />
    </FormRow>
  </Section>
  <ActionBar>
    <Button variant="primary" @click="save" :text="saveText" />
    <Status :text="status" :tone="statusTone" />
  </ActionBar>
</SettingsPage>
```

## 默认状态组件

- FormRow.error 为空时折叠，否则显示在控件下方，同时合并进控件的可访问说明。更新 hint/error 不重建输入控件。内部子树属于实现细节，可通过 Element.fieldControl 引用实际控件；Text/Status 也可作为只读字段。
- Status 使用原生文字与小圆点，tone 支持 neutral/success/warning/error/pending，可绑定 `State<std::wstring>` 或 Computed；主题切换保留 tone。文字应表达完整含义，不能只靠颜色。
- LoadingState 默认标题为“正在加载…”，支持 subtitle，采用静态反馈而非永久动画。EmptyState 增加 subtitle，仍可包含操作按钮。
- Button 的 primary 用于主要操作，默认样式用于次要操作，danger 用于删除等操作。
- SearchInput 默认 basis=280/grow=1，Select 默认 basis=140，适用于横向工具栏。FormRow 会恢复控件的内容高度。直接放在普通 Column 时，basis 仍约束高度，需要显式覆盖或使用 FormRow。
- Header 与 Toolbar/ActionBar 默认 shrink=0，避免正文挤压标题区域，使 Header 内的换行工具栏越界。

## 开发时的布局诊断

包含 `oneui/ui_layout_diagnostics.h`，在 UI 线程、原生布局完成后调用 `inspectLayout(root, *mount.styles())`，再用 formatLayoutIssues 输出。检查实际矩形越界、文字高度不足和无效尺寸；报告含组件路径、frame、源码位置及父容器的轴方向、min/max/basis/shrink。

隐藏子树不检查；ScrollView 内容超出视口属于正常滚动，不报错。最多返回 100 条。横向文本省略、表格单元格内部布局、绘制阴影/描边溢出、遮挡和完整的约束因果分析不在检查范围，不能替代截图验收。

模板自动记录 .one 标签行号。C++ 可用 `ui.locate(element, __FILE__, __LINE__)` 标记；内部子节点回落到最近标记的祖先，仍给出实际组件路径。

Demo 的 --dev 在已经发生的绘制后检查；布局 revision/frame 未变化时跳过，相同报告不重复输出，也不请求下一帧。PowerShell 入口将报告写到 artifacts/layout-current.txt，也可用 `--layout-report E:/path/report.txt` 单独启用。普通运行不执行这项检查。这是文本诊断，未增加可视化 Inspector。

完整代码和模板示例位于仓库内 examples/declarative，提供设置、列表、详情三页。选择连接后可进入详情并返回；刷新使用约 800ms 的本地模拟等待，不访问网络或持久化数据。

## 表单输入与焦点定位

声明式 `Input` 和 `SearchInput` 默认配置 `SystemClipboard`，支持原生复制、剪切和粘贴；C++ 和模板入口共用该行为。直接构造旧版 `TextField` 的行为未改变，调用者仍可用 `setClipboard` 注入自定义实现。

`oneui/ui_focus.h` 提供 `focusedField(root)` 和 `revealField(root, field)`。后者按已有布局调整祖先 ScrollView 的纵向滚动，使字段保留约 8px 可见边距，不重建控件、不设置字段矩形。它按 ScrollView.frame 作为可见区域，适用于默认页面骨架；不处理横向滚动或自定义内边距视口。

校验失败时，先提交绑定并完成布局，再调用 `window.requestFocus(field)` 和 `revealField`。Tab 跳转应在原生焦点处理结束后投递同样的显示操作。异步投递由页面生命周期句柄保护，关闭页面后丢弃；不能捕获已销毁页面的裸引用。`examples/performance_lab/connection_editor.hpp` 展示完整接入，页面只组合公共组件与主题，表单内部无手工坐标。

`Mount::diagnostics()` 只读返回本挂载持有的 subscriptions、ownedObjects 和待提交 pendingUpdates 数量，用于生命周期回归；不包含 State/Computed 自身及子挂载内部的订阅，不能当作全进程订阅总数。完整列表/编辑/未保存确认示例见 `examples/performance_lab/connection_workspace.hpp`：保留原生页面树，按稳定业务 ID 提交记录快照，用 KeyedTable 更新单行并保留有效列表状态。

性能实验台现已加入只读详情：`buildDetails` 与 `views/Details.one` 复用 `DetailPage + FormGrid + FormRow + Text`，没有另写布局规则。从详情进入编辑后返回原详情，列表状态继续保留。可复制的最小程序和逐步组合说明见[页面教程](48-connection-page-recipes.md)。
