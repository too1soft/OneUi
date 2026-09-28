# 连接页面设计一致性记录

2026-09-28。本次为既有原生 OneUI 的列表／详情／编辑流程扩展。审查结论：**ship**，限定于下列四张截图与源码抽查范围；未发现需要阻止本轮交付的设计一致性问题，没有提出实质性视觉修复。

## 依据与范围

产品依据为 [仓库说明](../README.md)、[性能实验台说明](../examples/performance_lab/README.md)及[上一阶段验证记录](40-declarative-stage-validation.md)。视觉依据为现有 [ui_theme.h](../include/oneui/ui_theme.h) 和[原编辑页窄窗口截图](images/performance-lab/editor-narrow.png)。沿用默认绿色强调色、浅／深主题、舒适／紧凑密度及既有语义组件，不建立另一套视觉规范或复制主题 token。

逐张复核的本轮原生客户端截图：

| 截图 | 审查重点 |
|---|---|
| [detail-light-wide.png](images/connections/detail-light-wide.png)（1320×900） | 详情分组、双列字段、正文限宽与底部操作 |
| [detail-dark-narrow.png](images/connections/detail-dark-narrow.png)（640×800） | 深色紧凑、单列重排、正文滚动与可见操作 |
| [list-light-narrow.png](images/connections/list-light-narrow.png)（640×800） | 筛选及操作换行、表格阅读、未选择时的操作状态 |
| [editor-error-narrow.png](images/connections/editor-error-narrow.png)（640×800） | 错误字段、文字提示、正文滚动与底部保存操作 |

源码抽查包含 `examples/performance_lab/connection_workspace.hpp` 的详情组合、稳定 ID 与返回流程，`views/Connections.one`、`views/Details.one` 的对应组合，以及 `main.cpp` 的启动主题、详情入口和键盘分支。另核对 [页面教程](48-connection-page-recipes.md)、[独立详情示例](../examples/declarative/details.cpp)及其构建入口。此记录不是全仓库审计，也不是对所有交互状态的实机验收。

## 一致性结论

- 详情直接复用 `DetailPage`、`Section`、`FormGrid`、`FormRow`、`Text`、`Status` 和 `ActionBar`。主操作、危险操作、错误与次要文字继续使用同一套主题 token；本轮未新增页面专用 CSS 或布局算法。
- 宽窗口分栏、窄窗口纵向重排及操作换行交给现有组件。字段组合不计算坐标，也没有字段专用 resize 回调；实验台外层图表容器的自绘布局不属于这一结论的范围。
- 详情由稳定业务 ID 绑定，编辑保存或取消回到同一条详情；从列表直接编辑则返回列表。筛选条件、有效选择与有效滚动位置遵循已有保留逻辑，筛选移除记录时不承诺保留失效选择。
- 页面明确说明数据和连接状态均为本地模拟，关闭后丢弃。在线状态附有文字，删除使用危险样式和确认流程，错误使用描边与文字共同表达。

## 验收边界

上述截图支持静态视觉判断，源码支持组件与流程一致性判断；不据此宣称动态操作、系统无障碍或所有尺寸均已通过。自动回归、绘制一致性和长文本覆盖结果由本轮验证记录另行列证，不在此重复作测试完成声明。

本轮没有运行网页视觉检测器。真实系统 125%／150% DPI、跨显示器拖动与中文输入法候选窗口仍未验收：桌面自动化内核启动失败，无法完成该轮实机操作。内部缩放和组合输入状态测试不能替代这些验收。

原仓库未建立 `PRODUCT.md`／`DESIGN.md`；本次仅记录既有系统内的扩展一致性，不新建这些文件，也不处理范围外的文档或视觉漂移。
