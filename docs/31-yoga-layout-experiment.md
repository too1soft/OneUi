# Yoga 原生布局实验

日期：2026-09-25。已接入可运行的 Yoga 布局后端；它仍是显式启用的实验能力，旧 Stack 和 Ui::new 默认不迁移。本阶段选择 Yoga，是因为 OneUI 核心为 C++，它提供直接可用的 C API；Taffy 的 Grid 路线仍保留，未声称完成两个引擎的性能对比。

后续组件、布局缓存与综合 Demo 对照见 [页面组件与动画布局缓存](32-components-and-performance.md)。下文保留本阶段的验证范围与当时结果。

## 作者如何使用

```rust
use oneui::{ui::*, StackEngine};

let ui = Ui::with_engine(scroll(page([
    flow([
        heading("项目").basis(400.0).grow(1.0),
        primary_button("创建", || {}),
    ]),
    responsive_columns(400.0, [
        surface([paragraph("内容根据实际宽度换行，并撑开容器高度。")]),
        surface([paragraph("窗口缩窄后，这一列排到下一行。")]),
    ]),
])), StackEngine::Yoga)?;
```

`flow` 是换行行，`responsive_columns` 给每个项目相同的首选宽度和增长权重，空间不足时换行；首选宽度不是不可缩小的 min-width。`paragraph` 是多行 Label。`scroll` 持有原生 ScrollView 与子树，内容为 Yoga Stack 时自动按视口宽度测高。这里没有 CSS 媒体查询或 Grid；这是内容驱动的换行。

设置 CMake `ONEUI_ENABLE_YOGA=ON` 后，C++ `Stack::setEngine(StackEngine::Yoga)` / Rust `set_engine` 启用；`setWrap` / `set_wrap` 开启换行。未编译 Yoga 或仍处于旧引擎时，启用 Yoga/换行会返回失败，不会静默采用错误布局。新增三个 C ABI 函数为 `oneui_stack_set_engine`、`oneui_stack_set_wrap`、`oneui_widget_measure`，旧函数保持。

## 测量、绘制与失效

`Widget::measure(Size available)` 接收逻辑尺寸约束，正无穷表示无约束。默认回退 naturalSize；Label 使用同一文字引擎、字体环境、富文本范围、最大行数和行高进行宽度约束测量。返回高度至少容纳每一行的行框，防止 SkParagraph 的像素取整与 paint 中的行数计算不一致。

最外层 Yoga Stack 持有一棵与连续 Yoga Stack 子树对应的布局树。普通原生控件是测量叶子，旧布局容器作为不透明叶子继续自行排版。布局树复用节点，只有子项列表改变才重建对应子树；原生控件与 Rust 回调不重建。根内容未失效时跳过同步，叶子 revision 改变时通知 Yoga 重新测量，窗口宽高变化交给 Yoga 缓存处理。绘制期间统一安排嵌套 Stack，避免子容器再次独立求解覆盖父布局。

当前使用保守的 Widget invalidation revision：颜色/焦点变化也可能触发同步。这保证正确性，但不等于已经完成精细的 layout/paint 脏标记分离。自定义控件修改测量结果后必须 invalidate，不能只改变内部数据。

ScrollView 保留显式 contentHeight、preferredSize 的优先级。仅对无显式高度的 Yoga Stack 自动测量；需要滚动条时，预留 gutter 后重新测量。缩宽、缩窄和内容更新后，滚动范围及偏移重新约束。

## 可复现依赖与局部补丁

采用 [Yoga 3.2.1](https://github.com/react/yoga/releases/tag/v3.2.1)，固定提交 `042f5013152eb81c1552dec945b88f7b95ca350f`，归档 SHA-256 为 `4742f41722a16f181e3da37abf943390db1e928f00f26402cb154662ae7f110f`。`cmake/OneUIYoga.cmake` 只在启用时下载并编译核心；可用 FetchContent 的源目录覆盖支持离线构建。

`scripts/patch-yoga-wrap.cmake` 对固定源码做一个精确、幂等补丁：单 flexible child 的零 basis 快捷路径只用于 nowrap。原实现会在 wrap 行中忽略该元素的首选 basis，使标题和操作被挤在同一行。补丁内容不匹配时配置失败，禁止升级依赖时无声跳过。回归覆盖“一项 grow+shrink，另一项固定”的换行场景。第三方原始许可为 MIT，演示目录带有 Yoga 许可副本。

## 验证与微基准

Release 下四组 C++ 测试（control、stack、authoring、yoga）、四项相关 Rust 主线程测试、C/Rust ABI 清单检查通过；另一个 `ONEUI_ENABLE_YOGA=OFF` 构建的 authoring 测试通过。覆盖换行、嵌套段落、字体变化、隐藏和替换子项、min/max、方向变化、静态重复绘制复用测量、滚动底部可达、扩大窗口后滚动偏移归零，以及大标题测量和 paint 的行数一致性。

布局微基准：同机 Release，200 个无绘制工作的叶子，预热一次，连续改变宽度 1200 次，计时范围为容器 paint（布局和空叶子的调用），不含初始化、Skia 渲染、窗口提交或系统交互。最后一次记录：

| 后端 | 中位数 | P95 |
| --- | ---: | ---: |
| 旧 Stack | 6.8 μs | 7.3 μs |
| Yoga | 105.8 μs | 115.3 μs |

这个简单单行用例中 Yoga 更慢，绝对中位数约 0.106 ms；不能用它证明全应用流畅度、内存优势或 GPUI 对比结果。该成本换来了这里验证的换行与复杂测量能力，也支持继续保持按需启用。重新运行 `oneui_yoga_behavior_tests.exe --bench` 可获取当前机器结果。

## 边界

- 没有 Grid、CSS flex 属性解析、媒体查询、完整 Flexbox CSS 映射或整套响应式组件库。
- Yoga 子树的可测量性与缓存正确性已做相关回归，但没有全面覆盖混合旧容器、任意深度、RTL、所有 DPI 和无障碍行为。
- 真实示例支持最小客户区 560×560；图片验证覆盖 1180/640/560×760。页面超高时通过滚动访问，不承诺在一屏内展示所有内容。
- 未重新测试 GPUI、整机内存或动画吞吐，不把微基准当成产品性能结论。
- 新增 C++ 虚方法和内部状态改变 C++ ABI，DLL 与 C++ 使用方仍需一起重编译。Rust 演示链接匹配的新 DLL。
