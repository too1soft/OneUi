# 更容易布局，也更容易做出协调界面

> 后续进展：已完成 Yoga 实验接入、自动换行、约束式段落测量与滚动联动，见[第三阶段记录](31-yoga-layout-experiment.md)。下文保留初始研究与选型依据。

研究日期：2026-09-25。结论：优先交付**内容测量 + 语义组合 + 默认视觉规则**，将完整布局引擎接入放在同一套上层 API 后面。增加 CSS 语法覆盖面、引入算法库，都不能单独保证产品界面好看。

## 从成熟方案借鉴什么

| 方案 | 已核实的能力 | 对 OneUI 的启发 | 集成代价 / 限制 |
| --- | --- | --- | --- |
| GPUI | 官方示例提供链式元素和 flex 风格写法 | 让结构在代码中可读，减少坐标和样板 | 写法本身不提供完整的产品设计规范 |
| Qt Quick Layouts | 布局使用隐式/首选尺寸、最小最大约束和填充策略 | 测量控件内容，然后安排位置；应用不应维护一串相互依赖的宽高计算 | 不能只借用语法而缺少测量协议 |
| Taffy | Rust 实现 CSS Block、Flexbox、Grid，提供树布局 API；项目列出 GPUI 为使用方 | 完整 Grid / flex-wrap 需求适合评估成熟算法，不继续手写所有规范 | OneUI C++ 核心需要 Rust 静态库/C 桥、工具链及缓存/树所有权适配；不是换一个 Cargo 依赖就完成 |
| Yoga | C++ 实现、可嵌入的 Flexbox 布局引擎 | 与 OneUI C++ 核心的技术边界更接近，适合以 Flex 为主的路线 | 官方定位为 Flexbox；本次未把它视为完整 CSS Grid 方案，也未做本机性能比较 |

主要来源（仅官方/项目原始文档）：[GPUI](https://gpui.rs/)、[Qt Quick Layouts](https://doc.qt.io/qt-6/qtquicklayouts-overview.html)、[Taffy 项目](https://github.com/DioxusLabs/taffy)、[Taffy API 层次](https://docs.rs/taffy/latest/taffy/)、[Yoga](https://www.yogalayout.dev/docs/about-yoga)。

上述集成优先级属于对当前 OneUI 架构的工程判断，非这些项目对 OneUI 的背书；没有拿项目自带基准推导本应用的帧率。

## 为什么不是“再写一点 CSS 就够了”

CSS 能表达 padding 和颜色，但需要控件实际消费这些属性，并且布局必须知道变化后的内容尺寸。第一阶段已修复 Button padding 从解析到绘制的断点。本阶段把自然尺寸暴露为统一协议，使 Button/Label/Stack 可在字体、文案、样式改变后参与内容排版。

界面的协调性还需要另一层默认规则：标题与正文比例、8/12/16/24/28 的间距、按钮内边距、颜色层级、圆角、hover/pressed/disabled/focus 状态。把这些集中到主题，页面作者只做少量必要选择，比要求每个页面手动调几十个属性更可靠。主题是起点，不能代替产品的信息架构和交互设计。

## 本阶段已落地

1. **原生测量入口**：`Widget::naturalSize()`，默认 preferredSize；Button、Label、Stack 实现内容测量。Stack 一次 naturalSize 查询只测量每个后代一次，新增 12 层嵌套回归测试；内容尺寸向上取整，避免分数字形宽度导致意外省略。
2. **内容布局**：`StackFlex.contentBasis` / Rust `FlexBasis::Content`。仍保留未配置 flex 的旧布局规则。自定义绘制控件可以提供 naturalSize 或 preferredSize。
3. **跨语言接口**：新增 set_flex / clear_flex / set_justify / natural_size 的 C ABI 声明、实现和 Rust 包装，更新 supported-symbols 清单。C ABI 校验非法数值、错误控件类型和非直接子项。
4. **Rust 组合层**：`ui::{page,row,column,columns,surface,toolbar,text,heading,muted,button,primary_button,widget}`。Node 描述先构建，Ui 统一创建并持有原生树及回调；没有每帧重建或 diff。
5. **可选主题**：`ui/default.css` 统一设计规则；`Ui::replace_css` 更新同一张主题表并重新应用当前组合，失败保持旧样式。显式间距覆盖主题值。
6. **真实窗口**：GPUI_demo/composition-demo 的 Rust 页面没有 setFrame 或应用层文字测量。可切换文案和密度，展示内容变化引起的重排。原性能演示和旧构建保持独立。

## 建议的最终作者体验

页面主要使用语义组件与三类尺寸意图：按内容、填剩余空间、明确约束。默认组件应覆盖页面标题、操作栏、表单行、空状态、表格工具栏和带标题的内容区，再允许应用覆写主题。

CSS 管主题和必要的细节；组合代码管结构和行为；布局内核管测量与位置。可以以后增加热重载和布局检查器，但不要求初学者先掌握完整 CSS，也不在同一页面混用两个独立的坐标系统。

## 下一阶段：引擎如何选

不要马上全量迁移旧 Stack，先做兼容实验：

- 保留现有 C ABI 和 retained Widget 树，分别接入一个 Yoga / Taffy 实验容器；共享文字测量与原生绘制。
- 对同一批用例检查布局正确性：内容尺寸、长中文、多行、DPI、wrap、隐藏项、min/max、嵌套、滚动、稳定焦点。
- 比较布局重算次数、节点分配、CPU/内存、帧时间分位数；区分首次构建、连续 resize、内容更新和纯动画。引擎微基准不等于最终应用速度。
- 如果主要诉求是 C++ 核心中的完整 Flexbox，先评估 Yoga；如果 Grid 与 Rust 技术复用是明确要求，优先评估 Taffy 的 C 桥。以实验结果做最后选择。

本轮没有接入这两个引擎，也没有实现 wrap/Grid。自然尺寸目前不接收可用宽度，因此多行段落和复杂响应式还需约束式 measure/arrange 协议。布局失效缓存也需结合实际动画场景设计，不能以“有缓存”替代失效正确性。

## 兼容性

旧 Stack 不调用新选项时维持旧行为。新增 Widget 虚方法及 Stack 内部数据改变 C++ ABI，C++ 应用和 DLL 必须一起重新编译；不能把新 DLL 塞到旧 C++ Demo 旁。C ABI 原有结构未改变，新增函数需要链接匹配的 OneUI 构建。Rust 组合层不是对外发布的独立 SDK，构建脚本负责本地演示链接。
