# 页面组件与动画布局缓存

日期：2026-09-25。此阶段把已有组合布局扩展为可复用的页面模式，并把综合性能 Demo 迁移到组件树。Yoga 仍为可选后端；旧 `Stack` 与 Rust `Ui::new` 的默认行为保持不变。

后续分阶段追踪与指标重绘修复见 [绘制归因与更新阶段](33-frame-profile-and-update-phase.md)。本文保留本阶段的测试结论。

## 更少的页面代码

Rust 入口为 `oneui::ui::*`。`page_header`、`toolbar`、`section`、`metric`、`form_row`、`empty_state` 组合现有原生控件，统一字体层级、间距、表面、按钮和输入框的状态颜色。`flow` 按内容宽度换行；`scroll` 为超高页面提供原生滚动。

```rust
use oneui::ui::*;

fn main() -> Result<(), UiError> {
    App::new("实验台", scroll(page([
        page_header("实验设置", "调整参数并开始实验。", []),
        section("运行状态", "实时数值可以传入 Rc<Label>。", [
            metric("已完成", text("42"), "项"),
            toolbar([primary_button("运行", || {})]),
        ]),
    ]))).size(1100, 760).run()?;
    Ok(())
}
```

完整可运行示例：`bindings/rust/oneui/examples/components.rs`，包含原生 TextField、计数回调、指标与空状态。`App::build()` 返回持有 `window` 与 `ui` 的 `MountedApp`，方便接入 dispatcher 或更新主题；`App::run()` 管理窗口和回调生命周期。默认使用 Yoga，未启用 Yoga 的 SDK 会明确报错。

C++ 提供 `<oneui/ui.h>` 的 `ui::Builder` 与 `<oneui/ui_app.h>` 的 `ui::App`：

```cpp
#include <oneui/ui_app.h>

int runDemo() {
    oneui::ui::Builder ui;
    oneui::ui::App app(L"实验台");
    return app.run(ui.scroll(ui.page({
        ui.pageHeader(ui.paragraph(L"实验设置", "heading"),
                      ui.text(L"调整参数并开始实验。", "muted"), {}),
        ui.metric(L"已完成", ui.text(L"42"), L"项")
    })));
}
```

`App::window()` 提供高级窗口配置；`run(root, ready)` 的可选 ready 回调在窗口初始化并显示后调用，适合启动动画。core-only 使用方只包含 `ui.h`，避免引入平台窗口依赖。

C++ `native(widget)` 只将已有控件接入树，保留它的显式样式；如需给原生输入框套用主题，显式调用 `ui.style(field)`。Rust 的 `widget(control)` 则遵循已有 Ui 规则，挂载时应用该 Ui 的主题。C++ Builder 在创建/调用 style 时应用样式快照；Rust `Ui::replace_css()` 可以重新应用整棵树的主题。两者不能被描述为相同的热更新能力。

## CSS 与布局各负责什么

主题的唯一源文件是 `bindings/rust/oneui/src/ui/default.css`。Rust 使用 `include_str!`，CMake 将相同文件嵌入 C++ 私有生成头；没有第二份手工同步的默认主题。

CSS 负责颜色、字体、padding、gap、圆角及控件状态。组件负责结构与语义层级，Yoga 负责约束测量和换行。当前没有媒体查询、CSS Grid 或完整浏览器 Flexbox 属性映射。漂亮默认值来自组件与主题的配合，CSS 解析器本身不生成页面结构。

`form_row` 是视觉标签布局，没有建立程序化 label/control 关系；应用仍需通过原生控件可用的无障碍 API 设置名称。它不是完整表单验证或无障碍表单框架。

## 为什么增加缓存

滚动容器可能先按完整宽度测高，再为滚动条预留空间测高，最后按实际尺寸排版。即使内容没变，这些不同约束也会反复切换 Yoga 根样式，让动画绘制触发额外布局调用。

YogaState 新增四项测量结果缓存和最后一次精确布局缓存。键包含可用尺寸与 `measureRevision()`。测量命中只返回尺寸，不修改已安排的 Yoga 树；精确布局命中复用布局，但仍将坐标分配给控件，支持滚动和原点移动。文字、子树或约束失效后重新计算。

`Stack::setLayoutCacheEnabled(false)` 用于诊断；`layoutStats()` 返回 calculations、measureCacheHits、arrangeCacheHits。calculations 统计进入 `YGNodeCalculateLayout` 的次数，不是 Yoga 内部执行全部计算的次数，也不是 CPU 时间。仍采用保守的 invalidation revision，尚未分离 layout 与 paint 脏标记。

综合 Demo 的指标文本改为约 10 Hz 刷新，动画、遥测采样和绘制仍逐帧执行。`--metrics-every-frame` 恢复逐帧文本更新，`--uncached-layout` 关闭缓存，用于同程序对照。

## 演示与验证

工作区 `GPUI_demo/component-demo` 的 C++ 综合示例直接复用原版 `oneui-demo/plots.hpp` 和 `telemetry.hpp`。粒子公式、曲线点数、负载档位以及 VirtualList 全量行数据保持原逻辑；页面区域尺寸有变化，因此与原版或 GPUI 的整应用结果不能当成纯布局引擎差异。

相关验证包括：重复 ScrollView paint 不再重复调用布局计算，文字更新与宽度变化正确失效，关闭缓存恢复计算，非法约束被拒绝，原生按钮回调被保留，Rust App 生命周期释放回调。实际窗口检查覆盖宽/窄综合页、图表/粒子/列表分页面与 Rust 示例。

最终检查：5 组 C++ 测试（control、stack、authoring、yoga、component）、4 项相关 Rust 主线程测试、C/Rust ABI 清单检查均通过；另外 `ONEUI_ENABLE_YOGA=OFF` 的 authoring 构建与测试通过。真实窗口验证了综合页空格暂停及负载快捷键，Rust 示例 Tab 显示按钮焦点环、Enter 将计数由 0 更新为 1。当前自动化无法执行可靠的鼠标坐标操作，未声称完成鼠标端到端验收；可访问树中的 focused_element 仍可能报告根窗口，原生焦点与 UIA 映射值得后续单独完善。

性能协议、原始导出及结果见 `GPUI_demo/component-demo/PERFORMANCE.md`。窗口演示入口为 `GPUI_demo/run-components.ps1`，测量入口为 `GPUI_demo/compare-components.ps1`。

## 边界与下一阶段

- C++ DLL 和使用方须匹配重编译；新增 C++ 类型/内部成员不保证旧 C++ 二进制兼容。本阶段没有新增 C ABI 函数。
- Rust 与 C++ 的新入口是可用的薄封装，尚未提供完整项目生成器、预编译 SDK 分发或跨平台打包向导。
- 这轮没有改写 VirtualList 的数据所有权；十万行字符串仍占内存。后续应独立实现按可见范围提供数据的接口。
- 帧间隔统计来自 UI 回调，不是显示器呈现帧数、GPU 时间或输入到画面的延迟。
- 若继续优化动画，应先细分文字测量、绘制、提交、布局的耗时，再决定脏标记分离与渲染批处理的优先级。
