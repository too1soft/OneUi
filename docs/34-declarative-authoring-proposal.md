# 声明式开发与默认视觉方案（提案）

状态：原始提案，日期：2026-09-26。2026-09-27 已落地 C++ v1（模板编译、响应式作者层、两种主题和 CSS 热更新），具体语义、范围与验收限制见 [声明式开发 v1](35-declarative-authoring-v1.md)。下文保留原始设计目标；Rust 对等接口、Inspector 和脚手架仍属后续工作。

## 性能现状

已修复的是指标在 paint 内更新引发的额外重绘。大量图元绘制与提交仍是高压模式主要开销，尚未通过批量绘制解决。此前 47.3% CPU 降幅只对应标准负载的逐帧指标压力模式，不能解释为整应用普遍提速。完整证据见 `GPUI_demo/component-demo/PROFILING.md`。

## 现有基础与缺口

已核实 C++ `include/oneui/reactive.h` 提供 State、Binding、作用域订阅；Label/TextField 等提供属性绑定。State 当前为 UI 线程模型，工作线程经 post 交付更新。现有 ui::Builder / Rust ui::Node 提供一次构建并保留原生控件树的组合方式；默认 CSS 与 Yoga 支持部分样式和内容布局。

这些可支撑手工 MVVM，但当前组合层没有形成完整统一的响应式页面协议。需要补齐派生状态、批量通知、命令的可执行/执行中状态、列表增量更新与订阅生命周期，再提供统一的作者入口。Rust 不能被默认认为已拥有与 C++ 相同的 State/Binding API。

## 推荐架构

MVVM 管理状态与操作，声明式语法描述结构，CSS/主题描述视觉，三者可以共存。

```text
C++ / Rust ViewModel：状态、派生值、命令
             ↓
统一绑定与更新调度：合并状态变化，在绘制前提交
             ↓
OneUI 原生控件树 → Yoga 测量/布局 → 现有渲染后端
             ↑
代码组合 / 模板编译器：共用控件、属性和事件协议
```

建议最终提供类似 Vue 的 `.one` 模板加 CSS；业务逻辑保留在类型明确的 C++/Rust ViewModel 中。模板编译为控件创建、绑定与事件连接代码，不要求嵌入浏览器或 JavaScript 运行时。它借鉴 Vue 作者体验，不宣称兼容 Vue SFC、任意 JS 表达式或全部浏览器 CSS。

以下是**目标语法示意，目前不可运行**：

```html
<template view-model="ConnectionVM">
  <Page title="连接管理">
    <Toolbar>
      <SearchInput v-model="query" placeholder="搜索连接" />
      <Button variant="primary" @click="create">新建连接</Button>
    </Toolbar>
    <DataTable :items="filteredConnections" item-key="id" />
  </Page>
</template>

<style scoped>
  Page { gap: var(--space-lg); }
</style>
```

属性/事件名和 ViewModel 类型须可检查，错误指向源文件行列。第一版只接受明确的字段访问、绑定和命令引用；复杂业务计算放到 ViewModel。分支与列表使用稳定身份，更新时保持焦点、输入光标、选择及滚动位置。生命周期结束必须自动释放订阅，异步任务取消或失效后的结果不能写回已销毁界面。

## 怎样让默认界面精美

1. **成体系的主题**：统一文字层级、语义颜色、间距、圆角、图标尺寸、密度与焦点状态，提供少量经过实际页面验证的主题。浅色/深色不只是背景换色。
2. **完整组件**：输入、选择、按钮、表格、导航、弹窗等交付默认布局及 hover/focus/disabled/loading/error 状态，表单同时提供程序化标签与验证提示。
3. **页面模式**：设置页、列表筛选页、详情页、主从页、仪表盘模板解决信息层级与排版。页面作者填内容、绑定数据即可获得协调的基础界面。
4. **可反馈的开发工具**：CSS 文件修改后即时预览，选中控件查看最终样式来源、布局尺寸和溢出原因；保存输入与窗口状态，避免每次重启从头操作。

CSS 解析成功必须对应控件实际消费；不支持的属性应明确诊断，不能静默忽略造成“怎么调都不对”。声明式语法改善代码可读性，不能替代这些视觉默认值。

## 实施顺序与验收

1. 先做统一响应式/命令协议及一个原生代码版设置页：输入双向绑定、派生提示、保存中与失败状态、后台结果经 dispatcher 回到 UI。
2. 完善主题和设置/列表两类页面模式，在长中文、窄窗口、125%/150% DPI、键盘导航下验证；让应用层不再手工 setFrame。
3. 在相同页面上增加最小模板编译器，证明模板和代码入口的行为、控件身份与性能一致；包含属性、事件、条件、带 key 的列表和组件插槽。
4. 再补样式热重载、布局检查器及创建/运行项目工具，减少 SDK 引用和构建配置负担。

不要同时维护两套状态运行时。代码 DSL 和模板应只是同一底层协议的不同入口。不要在已有复杂渲染瓶颈尚未解决时承诺换语法就能提速。

## 参考

- [Vue 模板语法](https://vuejs.org/guide/essentials/template-syntax)：声明式绑定结构。
- [Vue 响应式基础](https://vuejs.org/guide/essentials/reactivity-fundamentals)：状态变化驱动视图更新。
- [Flutter 状态管理](https://docs.flutter.dev/data-and-backend/state-mgmt/simple)：声明式 UI 中分离应用状态。
- [Slint 属性与绑定](https://docs.slint.dev/latest/docs/slint/guide/language/coding/properties/)：原生 UI 的属性依赖绑定案例。

以上实施路线是针对 OneUI 当前代码和用户体验目标的工程建议，不是这些项目对 OneUI 的兼容性承诺。
