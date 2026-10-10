# WP4 S3：PaneGrid 与尺寸运动

基线 b48df20；在 codex/workspace-components 上实施。S2 已由 Claude 验收；本阶段之后继续 S4–S8，不请求中途审批。本报告是 S3 局部验收，不代替 S8 的 A1–A10 全套验收。

交付：平铺 PaneGrid/Pane、结构化 nodes/groups 适配、真实 viewport 观察与 hold、200ms ease-out-cubic 和单个 WarmFrame、最大化/还原、两阶段关闭、并发重定向、divider 的 120ms 静止 epoch、末点提交、键盘步长/最小值、Esc/capture loss 取消。动画只操作预分配索引，不读 VM；普通内容字段通知不会重启动画。NativeHost 等待内容时不持续请求动画帧，attach 唤醒后继续。空闲布局不自发 invalidate。

终端观察使用 ContentViewportSource C++ 接口。TerminalView 的 legacy viewport 回调仍保留；工作区观察回调只排队，下一 UI update 才 invoke VM。迁移后的应用须只选择一种路径执行 session.resize，避免自己同时订阅两条通道。未达到一个完整字符格时不报告虚假的 1×1。已有内容在几何调整期间不 detach/recreate。

组件级事务：候选几何、内容观察闭包和 children plan 在发布之前准备；实际发布不分配。逐分配点注入 bad_alloc 验证旧场景和 hold 保留，成功候选在禁止分配时完成发布。这不是整个引擎的事务声明。

ABI 仍为 44，C 头文件、VTable 和导出清单无改动。PaneGrid、Pane、NativeHost 的 COMP 契约升为 2，增加完整属性/事件；这是声明式 IR 的组件契约变更，旧 reader 明确拒绝不匹配的 COMP。基础 WP3 黄金 IR 字节测试通过。WorkspaceNode 使用 leaf/split；应用可以自行命名 record，必须满足结构字段与类型。

## 验证

直接重放 capture 的 MSVC /O2 /DNDEBUG 配置；修改既有公共类布局后清理对象并重编，未运行 CMake configure。新文件补入相同编译/链接闭包。此结果不冒充 Ninja/CTest 的正常终端复核。

| 项目 | 结果 |
|---|---|
| Pane motion | 6 组通过：曲线/hold、重定向、divider epoch/取消、延迟挂载、键盘/关闭、分配失败 |
| Pane runtime | 结构 schema、测量事件参数、权威快照、非法树回滚、原内容地址保持通过 |
| TerminalView 既有行为 | exit 0 |
| workspace tree / scope-host | 通过，沿用 S1/S2 深树与租约覆盖 |
| WP3 runtime / lists | 9 runtime、11 typed record/delta/reload、8 list 场景通过 |
| WP3 reload | 50 保留 + 50 移除/恢复循环，IME/lease/Host 回滚通过 |
| C ABI lifetime | 11 场景通过；ABI sync 通过 |
| viewc workspace / binary | 10 / 11 测试通过，含结构投影正反例与事件参数类型拒绝 |

## 性能冒烟

真实 Win32 窗口，1600×1000，Consolas 13，80×24 模拟 cell grid；NVIDIA GeForce RTX 5080/PCIe/SSE2，OpenGL + Skia Ganesh，vsync 开启。每场景本阶段只执行一次；16 个内容对象在启动时建立，初始只显示一个。S8 仍须补足重复采样、按需新建内容、20 个新进程及启动预热前分屏。

内部 probe 将 SwapBuffers 墙钟时间单列；work+submit = paint 总耗时减 present 等待，加 root 动画 update 耗时。驱动线程普查耗时仍计入 work，没有把它从测量中移除。窗口呈现间隔保留在 CSV；不把该数据说成 GPU 完成/屏幕扫描时间。只排除每场景一个明确的 WarmFrame，Preparing 和最终帧均保留。

| 场景 | GL 非预热帧数 | GL max ms | GL P95 ms | GL >16ms | 软件 max ms | 软件 P95 ms | 软件 >16ms |
|---|---:|---:|---:|---:|---:|---:|---:|
| 1→2 | 38 | 5.0473 | 0.5968 | 0 | 23.7636 | 23.5834 | 9 |
| 2→4 | 38 | 1.4678 | 1.0707 | 0 | 47.5000 | 47.4933 | 8 |
| 4→8 | 38 | 1.8777 | 1.7374 | 0 | 59.9560 | 59.8671 | 6 |
| 8→16 | 38 | 3.4622 | 3.0796 | 0 | 68.3041 | 67.7856 | 5 |
| 16 最大化 | 38 | 4.3162 | 3.4377 | 0 | 67.3678 | 65.9275 | 4 |
| 还原 | 38 | 2.9003 | 2.7022 | 0 | 68.2525 | 68.0019 | 5 |

GPU 和软件两次运行各创建 16 个 TerminalView，实际 viewport 回调均为 33 次：初始 1、分屏 2+4+8+16、最大化 1、还原 1。运动中没有 viewport 回调。原始数据在 `docs/benchmarks/workspace-interactions/s3-smoke-{gl,software}.csv`。软件数据只作参考。

## 大文件拆分

物理行数含空行和注释。TerminalView 原 `src/core/terminal_view.cpp` 为 1,901 行；拆后保留网格和配置 364 行。选择 372、绘制 341、指针 339、输入 177、动画/局部失效 219、viewport 76；纯私有辅助头 70。viewport 文件新增 hold/观察逻辑，其余迁移保持旧逻辑。新 PaneGrid/runtime 文件均低于 500 行。最终准确行数在 S8 统一复核。

S3 尚未验收的事项由后续步骤承担：跨 tab 拖动/预览、PaneHeader、方向导航、广播模拟 VM、容器查询、DockWorkspace、D8 和独立示例。性能冒烟不等同于完整 A10。

MyStudio：本会话已读取工作台与项目上下文；写入未同步，未调用需要审批的受限写工具。
