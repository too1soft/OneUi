# 终端工作区验收示例

独立 Windows 示例；不依赖 `examples/performance_lab/`。普通界面全部来自
`Workspace.one` / `Workspace.oneschema`，Rust VM 由 viewc 生成 trait。
`content.rs` 只创建原生 TerminalView 和模拟会话，不启动 shell、SSH 或网络连接。

## 构建与运行

在已经配置好 OneUI、viewc 和 Rust 的正常开发终端中：

```powershell
cmake --build build/capture --target workspace_interactions
build/cargo-target/release/workspace-interactions.exe
```

新增 CMake target 需要正常终端更新生成文件。不要在受限沙箱重新配置现有
`build/capture`。若修改了公开 C++ 类布局，先清理对象再完整重编。

普通 Cargo 测试不要求编译验收桥：

```powershell
$env:ONEUI_LIB_DIR="$PWD/build/capture"
$env:CARGO_TARGET_DIR="$PWD/build/cargo-target"
cargo test --manifest-path examples/workspace_interactions/Cargo.toml
```

直接 Cargo 构建验收版还需要 `ONEUI_WORKSPACE_BRIDGE_DIR` 指向包含
`workspace_acceptance.lib` 的目录。该桥只链接到示例 exe；没有新增 SDK 的 C 导出。
可用 `ONEUI_VIEWC` 指定编译器，缺省使用本工作树的 `build/capture/oneui-viewc.exe`。

## 操作

- 顶栏切换 1/2/4/16 窗格、逐分支均分、还原、方向导航与交换。
- 窗格标题按钮请求分屏、最大化、广播和关闭；双击标题最大化，右键打开会话操作。
  单窗格且未广播时隐藏标题。拖动标题到中心交换，到边缘分屏，到标签栏拆为标签；
  只有单窗格标签可拖回窗格，按最近边合入。
- 分隔条拖动、双击及键盘微调走组件的命中和尺寸结算逻辑。
- 文件、监控、指南与终端组成 DockWorkspace。拖标题停靠，末态 Alt 请求浮动；
  浮动可八向调整尺寸。顶栏提供置顶、返回原位、收起、显示和专注模式。
- 广播逻辑在 `broadcast.rs`：接收者自行输入或断线会暂停；切换焦点不改变源；
  源多行粘贴须确认。终端输入只在内存回显。
- 外观抽屉展示字体选择、本地静态图片路径、媒体意图、Slider 和 12 项 ThemeScope。
  清除只清配置，不删除文件。示例不代替应用的文件选择/导入服务。

模拟池预创建 16 个会话，隐藏的会话保留原生内容。关闭采用 `closing → settled →
list REMOVE → host detach`；再次使用已关闭 key 创建新会话是显式重建，移动不重建。

## 自动化

```powershell
python examples/workspace_interactions/run_acceptance.py `
  --exe build/cargo-target/release/workspace-interactions.exe `
  --output docs/benchmarks/workspace-interactions/final
```

脚本顺序执行身份/根事件测试、12 张截图、20 个首次分屏进程、5×100 次十八场景循环及
软件后端参考。五轮 GPU 测试约需 25 分钟；同时编译或运行其他 GPU 程序会污染数据。
固定输出目录覆盖更新，不保留多轮副本。脚本的 `PASS` 表示进程/功能断言成功，
**性能是否通过以 summary.json 的 max_ms 和 slow_frames 为准**。

完整批次结束后运行 `python examples/workspace_interactions/summarize.py
docs/benchmarks/workspace-interactions/final`，生成 performance.md / metrics.json，
检查 5×100 的完整性与单个 WarmFrame，另列 P50/P95/P99、实际帧间隔和首次预热耗时。

`--acceptance` 从原生根控件注入 Pointer/Text/Composition，校验拖动确实修改 VM，
再做 500 次拆并、交换、跨标签、最大化和收起循环。没有 SendInput、系统键盘鼠标或
WM 消息注入。`--perf --repeats N` 输出逐帧 CSV；`--first-split --early` 在第一个
已呈现帧后请求分屏，普通首次分屏在第 16 个启动帧后执行。后续场景按 motion Idle
谓词推进，并在下一次命令前排空关闭通知，不以固定等待帧数代替运动结算。

CSV phase：0 Idle，1 Preparing，2 WarmFrame，3 Running，4 FinalLayout。
work_submit_ms 包含原生 update、整个 paint/submit（扣除 SwapBuffers）以及首帧
Rust VM mutation/flush 的实测耗时；present_ms 和 interval_ms 分列。
只排除 phase=2；Preparing 的慢帧照实计入。native_update_ms 与 census_ms 是 work 的
子集，frame 为进程内的实际采样帧序号。普通截图在约 900ms 后采集用于视觉检查，
不作为动画完成、性能或异步媒体加载完成的判据。
visible 是 PaneGrid 内部布局条数；场景 15–17 的 DockItem 祖先隐藏，所以汇总
“16 个可见窗格”时排除这三个专注工具场景，全场景表仍保留它们。

其他参数：`--dark`、`--narrow`（880px）、`--width`、`--height`、`--panes`、
`--settings`、`--gallery`（只展开 12 项主题）、`--media path.png`、`--drag-preview`、
`--floating`、`--terminal-only`、`--tool sftp|monitor`、
`--capture path.png`（同时输出布局 JSON）、`--smoke`。

## 文件边界

`forest.rs` 管分屏业务树；`tools.rs` 使用 SDK DockWorkspace 业务模型；
`commands.rs` 校验意图；`publish.rs` / `delta.rs` 发布稳定 key 增量；
`rows.rs` / `vm.rs` 实现生成契约；`content.rs` 管内容租约；
`acceptance*.rs` / `native_acceptance.cpp` 是可选测试驱动。
引擎组件不直接编辑会话、分屏持久状态或业务配置。

完整结果及限制见 `docs/60-workspace-interaction-components-s8-report.md`。
