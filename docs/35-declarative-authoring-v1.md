# 声明式开发 v1

实现日期：2026-09-27。Windows / C++17 / Yoga 验证入口位于仓库内 `examples/declarative`。

## 最小接入

业务逻辑使用 `State<T>`、`ui::Computed<T>`、`ui::VmCommand`。原生代码组合和 `.one` 模板都使用 `ui::Mount`，构建后保留同一棵控件树。

```cmake
set(ONEUI_ENABLE_YOGA ON CACHE BOOL "" FORCE)
add_subdirectory(path/to/oneui sdk)
add_executable(myapp main.cpp)
target_link_libraries(myapp PRIVATE oneui)
include(path/to/oneui/cmake/OneUIView.cmake)
oneui_target_view(myapp NAME Settings SOURCE settings.one VM_HEADER vm.h)
```

`ONEUI_BUILD_VIEWC` 默认启用；可关闭来避免在仅使用原生代码组合的项目中构建编译器。显式包含 `OneUIView.cmake` 仍会提供编译器目标。交叉编译时应自行提供可在构建主机上执行的工具；本版只验证 Windows 本机构建。模板依赖通过 depfile 追踪，修改导入组件也会重新生成。

```cpp
#include "oneui/ui_declarative_app.h"
#include "oneui/ui_theme.h"
#include "Settings.g.h"

int main() {
    oneui::ui::DeclarativeApp app(L"我的应用");
    SettingsVM vm(app.dispatcher());
    auto& ui = app.mount();
    ui.styles()->replace(oneui::ui::declarativeTheme() + styles_Settings());
    return app.run(build_Settings(vm, ui));
}
```

`DeclarativeApp` 持有窗口、挂载对象、调度与后台结果邮箱。`run()` 返回时撤销后台投递及排队回调；ViewModel 需活到 `run()` 返回。不要让后台函数捕获裸窗口或 ViewModel；后台只计算，完成回调在 UI 线程接收结果。`VmCommand::runAsync` 的工作函数收到可查询取消状态的 sender，返回空字符串表示成功，非空表示错误；同一命令运行中拒绝重复启动。

## 模板语法

```html
<template view-model="SettingsVM">
  <Page title="设置">
    <FormRow label="名称" hint="用于识别当前工作空间">
      <Input v-model="name" placeholder="工作空间名称" />
    </FormRow>
    <ValidationMessage :text="validation" />
    <Button variant="primary" :text="saveText" @click="save" />
  </Page>
</template>
<style scoped>
Page { gap: 24px; }
</style>
```

- `:property="member"` 绑定 `State`、`Computed`，或提供 `get()/subscribeScoped()` 的只读值。类型错误由 C++ 编译器报告，`#line` 映射回 `.one`；不存在的组件、属性、事件和表达式由 `oneui-viewc` 拒绝。
- `v-model` 的类型：Input/SearchInput 为 `State<std::wstring>`，Switch 为 `State<bool>`，Select 为 `State<int>`，DataTable 为选中 ID `State<std::wstring>`。`@click` 接受 `VmCommand`。
- DataTable 支持 `@activate="edit"`（双击/Enter）和 `@delete="remove"`（原生删除请求），同样接受 VmCommand，由 ViewModel 读取当前稳定选择 ID。命令负责业务校验及删除确认；事件不会自动删除数据。C++ 对应 `ui.tableEvent(table,"activate",command)`，挂载释放后事件不再执行命令。
- `ref="nameField"` 声明控件引用；构建后用 `ui.find("nameField")` 取得原生 Widget，可交给窗口焦点与滚动定位 API。名字为静态标识符，不允许表达式或 v-for 内引用。同一模板中的重名在编译时报错；跨导入组件的名字须在同一 Mount 内唯一，重复实例在挂载时报错。引用使用 weak_ptr，不延长控件生命周期；C++ 对应 `ui.remember("nameField", input.widget)`。不存在或已释放的引用明确报错。
- `v-if/v-else` 保留已创建的分支，通过可见性切换。隐藏分支不参与绘制和 Yoga 布局，仍保留状态及订阅；不是 Vue 的卸载语义。移除的重复项会释放对应挂载作用域。
- `v-for="item in items" :key="item.id"` 接受可观察的 `vector<shared_ptr<Item>>`。ID 为唯一且非空的 `wstring`，稳定 ID 必须保持同一个 shared item 对象。可变字段用 item 内的 State；不要用同 ID 的新对象替换旧对象。这保证绑定不会捕获失效的 vector 引用。
- `DataTable` 绑定 `vector<ui::TableRow>` 和列定义；`TableRow.id` 是固定的稳定 key（`item-key="id"`）。原生表格虚拟化绘制，数据仍是 owned vector；本版不提供惰性数据源。单选绑定按 ID 恢复，过滤掉的选中项清空。
- `<import name="SettingsHeader" src="Header.one" />` 导入模板。`<slot/>` 是默认插槽，`<slot name="actions"/>` 是具名插槽；调用方用 `slot="actions"` 提供内容。导入模板共用 ViewModel，不提供任意动态组件、运行时反射或完整 Vue props 系统。
- 布局属性 `grow/basis/min/max/shrink` 是静态主轴约束；Row/Toolbar/FormRow 自动换行，Scroll 只接受一个根子节点。类名和 variant 目前是静态属性。复杂表达式放入 Computed，不在模板执行 JS。

## 更容易写对的正文布局

设置、资料编辑、详情等页面可以用 `Scroll + Content`。`Content` 默认把子节点纵向排列，正文最大宽度为 880 个逻辑像素，左对齐；窄窗口自动缩到可用宽度。省去手写 `Row + Column + basis/max/grow`，无需计算坐标。列表页继续用 `Column grow="1"`，让表格占用剩余空间。

```html
<Scroll>
  <Content>
    <Section title="常规设置">
      <FormRow label="工作空间名称" hint="用于区分工作环境">
        <Input v-model="name" />
      </FormRow>
    </Section>
    <Toolbar>
      <Button variant="primary" @click="save">保存更改</Button>
    </Toolbar>
  </Content>
</Scroll>
```

需要更窄、居中的页面时使用 `<Content max-width="720" align="center">`。`align` 支持 `start/center/end`，按物理水平轴对齐；`max-width` 必须大于零。两个属性在模板 v1 中为静态值，修改后重新编译。

代码版使用同一个组件：

```cpp
auto body = ui.make("Content", {section, actions, status});
ui.set(body, "max-width", 720);
ui.set(body, "align", L"center");
auto pageBody = ui.make("Scroll", {body});
```

`Content` 内部保留两个 Yoga Stack，不增加一套布局算法，也不因窗口缩放重建控件。`Content { gap:24px; padding:12px; }` 中 gap 控制正文子项间距，padding 位于限宽正文之外；背景和边框属于外层容器。默认主题给正文 24px 间距，紧凑密度为 16px。只替换为一张没有 gap 声明的样式表时，间距回到零；热更新保留默认主题时则回落到所选密度的主题间距。

`basis/min/max` 仍表示**父容器主轴**尺寸：父节点为 Column 时约束高度，为 Row 时约束宽度。正文宽度优先使用 `Content max-width`，避免把 Column 子项的 `max` 误当宽度。

模板编译器会在属性所在行拒绝负值、NaN/Infinity、超出 float 范围的尺寸、`min > max`、无效对齐值，以及带 `px` 后缀的布局属性。布局属性使用逻辑像素数值，CSS 中才使用 `px`。`selectedIndex` 要求整数，`-1` 表示未选择，不再静默截断小数。这些是静态诊断，不包含运行时溢出检查器。

## 页面骨架、状态和诊断

`SettingsPage`、`ListPage`、`DetailPage` 为设置、列表和详情提供默认布局；`ActionBar` 管理底部操作；`FormRow.error`、`Status`、`LoadingState` 提供语义状态。两种作者入口均支持。使用方式、约束与诊断范围见 [页面骨架与布局诊断](36-declarative-page-patterns.md)。

## 主题和样式反馈

`declarativeTheme(false/true)` 返回浅色/深色默认主题；不修改已有 `ui::Builder` 和 Rust 的默认主题。包含文字层级、绿色强调色、输入/选择/按钮/表格的状态及焦点样式。FormRow 自动设置控件的可访问名称与说明；640px 窄窗口会把标签与输入排成上下两行。

新入口通过 `ui::syntax::css(source, scope, file)` 校验样式，再交给现有 StyleSheet 解析。支持带组件类型的简单选择器，例如 `Button.primary:hover`、`Input:focus`，以及 `:root` 自定义属性和 `var()`。不支持后代选择器、选择器列表、媒体查询、完整 CSS 布局或 JS。不被该组件/状态实际消费的属性报错，不静默忽略；旧 StyleSheet API 行为不变。

`<style scoped>` 编译为组件专属类；跨组件主题变量放在外部 CSS 的 `:root`，避免假装支持浏览器的变量继承。代码入口也可以设置相同 scope，以便与模板版逐像素比较。

`StyleWatcher` 在后台观察文件修改时间，变化稳定 200ms 后向 UI 线程投递一次回调。示例开发模式同时观察 `theme.css` 与所有导入模板内的样式块。构造新样式表成功后才替换旧表，删除声明恢复默认值；错误或文件暂时不存在时保留上次有效样式并显示诊断。整个过程不重建控件，不改变输入内容、焦点、光标、选择和滚动位置。模板结构、ViewModel 或新增 import 仍需重编译。

## 调度与兼容边界

### FormGrid 与密度

`FormGrid` 为原生 Yoga 自动换行容器，直接子节点仅允许 `FormRow`。默认列基准为 320 个逻辑像素，可通过静态 `min-column-width="360"` 修改，必须为有限正数。列分配剩余宽度并在不足时换行，最后一个单独字段占满该行；Grid 内字段标签始终上置。默认限宽正文通常为一／两列，宽容器可产生更多列。独立 FormRow 保持原来的宽屏横排／窄屏上置行为。不支持跨列或完整 CSS Grid。

```html
<FormGrid>
  <FormRow label="名称"><Input v-model="name" /></FormRow>
  <FormRow label="主机"><Input v-model="host" /></FormRow>
</FormGrid>
```

C++ 对应 `ui.make("FormGrid", {nameRow, hostRow})`，布局无需 setFrame 或应用级 resize 回调。

第一版 FormGrid 直接子节点不支持 v-for：当前 repeat 会生成容器，不符合直接 FormRow 的结构约束，编译时明确诊断。单独使用的重复列表能力保持不变。

`#include <oneui/ui_theme.h>` 后可调用 `ui::applyTheme(mount, dark, ui::Density::Compact)`，默认密度是 Comfortable。主题切换保持控件身份、输入与订阅；仅更新声明式入口的尺寸和样式。Comfortable／Compact 输入与选择控件高度为 40／32，表格行高为 44／36，表头高度为 40／32。表格按顶部行比例调整滚动并夹取有效范围，保留业务 ID 选择。

自定义 CSS 使用 `mount.styles()->replace(ui::declarativeTheme(dark, density) + validatedCss, density)`，同时传密度以同步 CSS 和原生几何。FormRow 的非空 error 同时更新原生字段的无障碍说明和错误边框；清除 error 恢复普通边框，主题替换不丢失错误状态。旧原生控件默认值和现有 declarativeTheme(bool) 调用保持可用。

同进程示例：`examples/performance_lab/run.ps1 -Components -Dev`，或在连接管理页点击「组件与布局」。模板 `views/Gallery.one` 展示状态、密度与自适应布局，操作和数据都是本地模拟。展示页首次打开时创建，之后保留；编辑真实示例连接可体验完整异步保存。

### 生命周期

- 旧 State 的通知仍然同步。`ui::Batch` 只合并 Computed 重算和 Mount 调度；作用域结束后刷新派生值，作用域内部可能读到旧派生值。
- Mount 保存最新属性值，在下一次绘制前集中应用。用户输入的双向绑定沿用原生输入控件，避免重绑引发光标和 IME 状态丢失。
- `View::reconcileChildren` 是唯一新增的底层树操作：保留存活节点的焦点链，移除节点时解除 owner 回调。原有 `add/clearChildren` 行为不变。
- 只交付 C++ 作者层；未扩展 Rust 对等 API、C ABI、运行时 Inspector、脚手架或粒子批量渲染。

运行入口见 [示例 README](../examples/declarative/README.md)，回归、截图和测量见 [阶段验收](40-declarative-stage-validation.md)。系统输入法候选窗口、跨真实 DPI 显示器拖动与物理鼠标自动化需要单独验收，不能以内部事件测试替代。

完整业务示例也已接入 `examples/performance_lab`：`run.ps1 -Connections -Entry template -Dev` 使用 `.one` 列表、编辑和确认页；`-Entry code` 使用同一 VM 的手写 C++ 页面。两者共用主题和 `connections.css`，开发模式监听外部 CSS 与所有导入模板的 style 块。模板结构、ref、事件及业务逻辑修改需要重新构建；只改样式会替换规则并保留原生控件。详见 [阶段验收](40-declarative-stage-validation.md)。

新增的类型化 C++ 快捷入口见 [Compose 用法与公共默认布局](50-typed-authoring-and-defaults.md)，与本页的 Mount／模板共用组件元数据及绑定机制；本页旧 API 继续可用。
