# WP4 S4：标题栏、拖动域与方向导航

状态：核心组件与声明式接入完成，本地测试通过；最终 A1–A10 结论汇入 S8。基线 b48df20，工作分支 codex/workspace-components。

## 交付

- PaneHeader：29px 标题栏，319/320px 紧凑切换，4px 拖柄与 8px 标题区阈值；标题/路径、广播角色、历史返回、关闭/分屏/最大化与菜单意图；identity/actions/menu 静态槽。新增可选 `broadcast-label` 供 VM 本地化角色文字，缺省 Source/Receiver，不影响 ABI。
- 同一 View 内按 domain 隔离的 WorkspaceDragDomain；单一 PaneGrid 持有稳定 pane 池。释放使用最后坐标；中央开区间 (.28,.72)²、边缘平局顺序、单叶 tab 拖入、多叶 pane 拖出、revision 失效、Esc/capture loss 取消。
- 预览由 PaneGrid 单层绘制，不参与命中；指数平滑 τ=45ms、dt 上限 50ms。请求后保持预览，收到 ack 或新 revision 清除。DragHandle/DropSurface 与 Tabs 接入同一域；Tabs MOVE 意图使用稳定 key，before 为空表示末尾。
- 几何邻居查找只发 focus/swap 意图。导航 serial 幂等；同帧最多执行 8 次，回调退出后 flush，因此下一条可观察 VM 接受的上一次导航；溢出有累计诊断。
- 修正 S3 的同步键盘 ratio 接受路径：保留 120ms hold，不因每个接受快照中途结算；Esc 恢复起始比例。

## 验证

直接 MSVC 重放已配置的 /O2 /DNDEBUG 命令，没有重新配置 build/capture；这不是全量 Ninja/CTest 的声明。

| 测试 | 结果 |
|---|---|
| oneui_pane_interaction_tests | 5 组通过：真实根事件路由、阈值、最终落点/过期/取消、导航、同步接受与 8/16/50ms 预览轨迹 |
| oneui_pane_motion_tests | 6 组通过，含分配故障注入及 publish 无分配 |
| oneui_pane_runtime_tests | 通过；重复项 PaneHeader 命令进入正确 item scope，终端地址保持，租约平衡 |
| viewc/test_workspace.py | 11/11，含槽、结构投影及事件签名正反例 |
| oneui_workspace_scope_tests | 通过 |
| oneui_view_runtime_tests | 9 场景通过 |
| oneui_control_behavior_tests / oneui_interaction_tests | 均通过，覆盖原 Tabs 行为 |

A5 的广播组、断线暂停与多行粘贴确认属于应用 VM，不放入 SDK 控件。S4 已提供广播角色展示与语义事件；完整 VM 场景将在 S8 独立示例验证，本报告不将它记作已经通过。

## 拆分与 ABI

原 tabs.cpp 1,098 行，拆分后 tabs.cpp 214、tabs_edit.cpp 176、tabs_pointer.cpp 224、tabs_geometry.cpp 227、tabs_style.cpp 173、tabs_paint.cpp 159。职责分别为状态、编辑、手势、几何、样式和绘制，保留原生手势规则。新交互文件均低于 500 行。

C ABI 仍为 44，无 C 头文件、导出或 VTable 变化。COMP 的 Tabs 扩展使用 version 2；新控件采用独立完整契约。不是新增 C ABI。

构建目录复核：capture 6.842 GiB、cargo-target 1.009 GiB、codex-views 0.428 GiB，共约 8.28 GiB；仅计本工作树 build，低于 30 GiB。固定辅助脚本/日志在 build 下，最终 S8 统一清理。MyStudio 未同步；未调用需要审批的写入工具。
