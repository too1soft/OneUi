# WP4 S6：DockWorkspace

状态：实现及本地组件验证完成；继续 S7/S8。C ABI 44 保持冻结，没有新增 C 导出。

## 实现与边界

新增 DockWorkspaceView / DockItemView；旧纯模型 DockWorkspace 不变。布局使用 DockTreeIndex 验证任意深度扁平树，再以预分配工作数组计算几何。复用 splitMinimum/splitAmount/splitRects、dockHitTest、clampDockFloating 和 resizeDockFloating，窄于 minimum 时保持工作区尺寸及比例，不扩张外框。

placements 与节点叶必须精确对应。隐藏、浮动和紧凑模式均保留原生内容；浮动绘制顺序为 pinned/order/key。浮动位置以工作区局部坐标发布，工作区变窄时仅约束显示矩形，不覆盖保存值。新状态先完整校验、准备子项顺序，再发布；非法树和 placement 保留旧场景。

拖动阈值 7px 由 DragHandle 处理。docked 使用最近边、floating 使用 strict 42px/四导引命中；源与浮动目标排除。释放重新命中最终坐标与 Alt；浮动内容直接跟随，落点预览按 45ms 指数曲线、单帧 dt 上限 50ms 平滑。compact 禁止 docked 拖出、保留 floating；maximized 优先。Esc、capture loss、revision 变化取消手势；请求等待 ack/revision，拒绝恢复权威几何。

八向 resize 使用原始矩形加总位移，越界后反向立即响应。divider 释放产生 ratioRequested；键盘 0.025 步长、Home/End 和 120ms 静止结算；双击均分。标题 DragHandle 双击发最大化请求，工具按钮不触发标题动作。raise/pin/visibility/redock/float 均为意图；hideable=false 抑制隐藏，菜单浮动限制 560×620，缺省 400×360。

`.one` 属性、结构化 app-named schema projection 和全部语义事件已接入，DockWorkspace/DockItem COMP 版本更新至 2。恢复锚点及持久化仍属于 VM；S8 示例使用既有纯模型/快照协议，不在 View 中复制业务 reducer。没有改动 performance_lab。

## 验证

| 程序 | 结果 |
|---|---|
| dock_workspace_tests | 4 组通过：约束布局、compact/max 优先、非法快照原子性；阈值/释放点/最终 Alt/拒绝/过期/取消；八向 resize 及逆向；divider/键盘/菜单意图 |
| dock_runtime_tests | 通过：编译 .one/schema、动态工具池、类型化 raise/bounds 事件；隐藏/恢复保留相同 Widget 与 NativeHost 内容；内容租约仅关闭时释放一次，VM 租约平衡 |
| dock_gesture_tests | 通过：既有严格导引、tab 区域、无效几何和 C/C++ 一致性 |
| dock_session_tests | 通过：既有内容身份、事务回滚、分离窗口及主题寿命 |

DockSession 的旧 exe 首次异常退出；S5 已 clean 重编对象，但该 exe 尚未重新链接。用本轮对象重新链接后通过，包括真实 OpenGL 窗口。此问题不算产品回归，也不把旧 exe 的结果当作本轮结果。

使用固定 build/capture 的既有命令直接 MSVC 编译链接，没有重新配置。完整交互矩阵、restore-anchor 快照 roundtrip、主题截图及性能证据在 S8 汇总；本报告不将它们提前标记通过。MyStudio 未同步。
