# WP4 S3–S8：最终检查点

实施基线：`b48df20`，分支 `codex/workspace-components`。这是提交评审的执行记录，
不是 Claude 的验收结论。S1/S2 已验收，S3–S8 本轮连续实施；不进入 WP5/WP6。

## 交付范围

| 步骤 | 交付 | 分步证据 |
|---|---|---|
| S3 | PaneGrid、扁平树投影、200ms cubic、resize hold/最终真实 viewport、两阶段关闭、故障注入；拆 TerminalView | [S3 报告](60-workspace-interaction-components-s3-report.md) |
| S4 | PaneHeader、拖放域/落点/预览、pane↔tab、方向 focus/swap、逐次最多 8 条导航；拆 Tabs | [S4 报告](60-workspace-interaction-components-s4-report.md) |
| S5 | C++ Stack insert/remove、命名 size 容器查询、ContainerPage、最多一次附加布局；拆 CSS 解析 | [S5 报告](60-workspace-interaction-components-s5-report.md) |
| S6 | DockWorkspace/DockItem、停靠与浮动、八向 resize、compact/最大化、语义事件、恢复锚点 | [S6 报告](60-workspace-interaction-components-s6-report.md) |
| S7 | FontPicker、MediaPicker/异步 MediaPreview、Slider/Card 声明式注册、逐项 ThemeScope | [S7 报告](60-workspace-interaction-components-s7-report.md) |
| S8 | 独立 Rust 示例 exe、`.one/.oneschema`、本地模拟内容、原生根事件驱动、500 循环身份测试、性能原始记录与截图 | [示例说明](../examples/workspace_interactions/README.md) |

普通 UI 没有新应用层 C++ 桥。`native_acceptance.cpp` 仅是可选测试驱动，向原生根
控件注入事件、读取几何和计时；不创建界面、不编辑业务树、不注入 OS 输入。
`content.rs` 只构建 TerminalView 与模拟会话，业务状态在 Rust VM。

## 最终集成修正

- 旧 S2 的 identity-only DockWorkspace 未提供几何投影时保留结构子项；有任意几何
  投影时仍进行严格事务校验。旧 scope/租约测试没有因 S6 被破坏。
- PaneHeader outline 图标改走已有统一 `paintIcon`；颜色与按钮同步来自标题样式。
  VM 决定单叶/广播标题显隐、编号和关闭许可。
- 导航后恢复从原生根到内容的焦点链；内容租约与已有 IME/历史对象保持。
- Dock 恢复锚点和当前 hidden 状态分开：锚点可在恢复后继续存在，不能据此误报隐藏。
- 字体/媒体控件接受缺少可选 token 的主题，单个缺省颜色不再导致整个调色板回退；
  深色媒体说明和真实 PNG 预览均已截图。
- 修正窗口先 paint 后 tick 时多出一个 Preparing 绘制的问题；第一张实际初始几何
  绘制就是 WarmFrame。连续两次 paint 之间没有 tick 时，第二次开始 Running 的
  t=0，不能再排除第二个 WarmFrame。确定性测试覆盖两种调度顺序，完整 200ms 曲线
  和最后一次尺寸结算不变。包含 WarmFrame 的首次延迟仍单独列出。

## ABI 冻结与构建边界

**C ABI 44 没有变化**。相对 b48df20，`oneui_c_api.h`、`oneui_view_c_api.h`、Rust sys
和 ABI 声明均无 diff；不新增 C 导出，不改 VTable，不增 Stack C/Rust 增删 API。
内部 C++ 帧探针 `oneui::internal::setWorkspaceFrameProbe` 仅供测试，位于 src/internal，
并非公开 C ABI。示例内 `workspace_acceptance_*` 只链接进自己的 exe。

未配置 build/capture；按其 Ninja compdb 重放直接 MSVC `/O2 /DNDEBUG` 编译，Rust
使用 `--release`。S3/S5/S7 的公开类布局变更分别先清理已知 native 对象再重编；
最后的私有状态变更也重编了所有依赖它的 PaneGrid 翻译单元。新 CMake target 已接入，
**全量 Ninja/CTest 仍由 Claude 在正常终端复核**，本报告不把直接 MSVC 冒充 Ninja 成功。

## 测试结果

最终原始输出见 [validation.log](benchmarks/workspace-interactions/final/validation.log)
及 [native-tests.json](benchmarks/workspace-interactions/final/native-tests.json)：

- 原生测试程序 **23/23** 通过，包含 motion 8 组、interaction 5 组、dock 4 组、
  picker 4 组、container/Stack 5 组，以及 WP3/S2 scope、租约、列表、reload 回归。
- viewc workspace **14/14**、binary **11/11** 通过。
- 普通 `cargo test -p oneui -p oneui-sys -p oneui-abi-tests -p oneui-view-tests`
  能完成编译；运行时托盘断言及连带锁中毒仍在本沙箱出现，不能声称该命令退出成功。
  排除两个已知托盘用例后，mainthread **106/106**、workspace_layout **14/14**、
  视图集成 **10/10** 通过，sys/abi 等其余测试完成。未删除或放宽原断言。
- 独立示例普通 `cargo test` **6/6** 通过；身份压力测试 **500/500** 循环通过。
- 正式性能批次和 12 张截图均退出成功，汇总脚本核验 5×100×18 场景完整性、
  每次运动最多一个 WarmFrame，以及 20 个首次分屏各恰好一个 WarmFrame。

## A1–A10 证据索引

| ID | 实现与直接验证 | 边界 |
|---|---|---|
| A1 | tree_tests 深度 1/64/65/1000、非法图/比例、任意列表顺序；pane runtime 投影/事务；示例 StableList 增量与 500 循环 | 未把 1000 终端作为性能承诺 |
| A2 | motion_tests：5/10px 命中、鼠标末态、120ms 多 epoch、取消/capture loss、Home/End/.025/修饰键、双击 .5；最后真实 viewport 一次 | 减少动画也结算两阶段关闭 |
| A3 | interaction_tests：.28/.72、中央交换、单叶 tab 最近边、多叶拒绝、pane→tab、过期/capture、8/16/50ms 预览轨迹；示例真实根拖动改变 VM；预览截图 | 单一 overlay，无文字 |
| A4 | 319/320 显隐、按钮不启动拖动、4/8px 阈值、无邻居、导航队列 8 条与溢出；VM 单叶/广播标题与编号测试；原生内容身份保持 | 几何导航与平台 IME 的产品级全链路仍由 WP6 复核 |
| A5 | Rust Broadcast 测试：建立/结束、接收者移除、断线/自行输入暂停、源不随 focus 变、粘贴确认与取消前无副作用 | 只用本地模拟输入，不启动 SSH/PTY |
| A6 | dock_workspace_tests 四组：阈值/Alt 末态/过期/取消、八向逆向 resize、compact 保留 float、pin/order、键盘 idle；旧 Dock 回归；Rust 锚点 roundtrip/非法 snapshot | 置顶限定工作台内 |
| A7 | S2 scope/host、WP3 50+50 reload、C ABI 11 寿命场景；独立示例 16 内容×500 循环保址保租约，关闭窗口全部释放 | 模拟池预建 16；显式关闭后重新创建 key 的次数另计 |
| A8 | 精确 860/1100/1.6/H0、窄模式 selected details、CSS 切换/移除、嵌套依赖拒绝、至多两次布局、Stack move/跨父/IME/focus；Yoga/Legacy 两套 | 仅命名 size 查询，不含滞回与任意 CSS container 特性 |
| A9 | 1000 字体有界可见行、IME/空/错/缺失选择；真实 PNG/不支持/晚到/取消/销毁；Slider 四阶段与 set 无回发；12 主题单项 delta/全局不变；Rust 同 IR 实例化 | 本地静态图片、白名单颜色 token |
| A10 | 18 场景×100×5、20 新进程（10 提前、10 第16帧后）、软件参考；逐帧 CSV 含 phase/update/census/VM/present/interval；500ms idle 确定性检查 | 最终性能数字及门槛判定见下节 |

**A1–A10 在本次组件/本地模拟示例范围内均通过自验，等待 Claude 最终验收。**
表中列的是已建立的直接证据；不将 OneUI 组件测试推断为已完成 iShell/WP6 现场、
像素或远程输入安全回归。A4 的产品级导航/IME 场景仍应在 WP6 以真实业务 VM 重跑。

## 性能与首次分屏

正式批次已完成，NVIDIA OpenGL 门槛通过。本次锁定 NVIDIA GeForce RTX 5080、驱动 581.80，Windows x64，
200Hz、96 DPI、系统客户端动画开启；示例使用 Consolas，1600×1000，VSync on。

`work_submit_ms = 原生 update + (整个 paint/submit − SwapBuffers) + 首帧 VM mutation/flush`。
`census_ms` 是其中既有驱动普查吸收函数的子集，不能再次相加。`interval_ms` 是实际
paint 回调间隔，不称为显示器端到端延迟。只排除 phase=2，每个运动至多一个；
Preparing、Running、最终 Idle 帧均纳入。UI 线程外的异步媒体解码不计作 paint。
CSV visible 表示 PaneGrid 内部布局条数；场景 15–17 的祖先 DockItem 已隐藏，
所以单独计算 16 个可见窗格门槛时不计入它们，全场景结果仍保留这些工具布局耗时。

完整逐场景表见 [performance.md](benchmarks/workspace-interactions/final/performance.md)，
原始逐帧 CSV、分位数和二进制哈希分别在同目录及 `metrics.json`、`manifest.json`。

| 批次 | 非预热帧 | 最大 work+submit ms | P95 ms | >16ms |
|---|---:|---:|---:|---:|
| OpenGL 18 场景×100×5 | 236557 | 15.2243 | 4.1712 | 0 |
| 其中实际可见 16 窗格 | 174638 | 15.1631 | 见逐场景表 | 0 |
| 首次分屏：第 1 帧后，10 新进程 | 381 | 14.0416 | 5.1414 | 0 |
| 首次分屏：第 16 帧后，10 新进程 | 388 | 14.3520 | 1.0429 | 0 |

五轮总 P50=1.4070ms、P99=5.0934ms；SwapBuffers P95=0.0346ms；
实际 paint 间隔 P95=5.8952ms、最大 19.0252ms，后者不是 work+submit 门槛。
五轮 WarmFrame 共 5500 个、最大 30.7659ms；首次分屏提前/第16帧后两组
各 10 个 WarmFrame、最大分别为 **39.0417/33.5442ms**。
软件参考中 pane 拆为 tab 有一个 16.7277ms 帧，其他场景无 >16ms 帧；不计作主后端失败。

没有加入 startup-ready 输入门控，也没有改变既有第 2/3/4/6/8/12/16 帧驱动吸收策略。
提前分屏组在启动预热完成前发起。第一张实际绘制是唯一排除的 WarmFrame，
其延迟仍保留于原始 trace，交由 Claude 判断是否需要另立启动延迟目标。

负载边界：示例启动预建 16 个本地模拟会话/NativeHost，各自独立 tab；首次分屏
测的是这些现存内容的首次显示/分屏，不含真实 PTY 或业务线程的创建。
性能驱动使用初始 80×24 本地 echo 网格，无持续 PTY/网络输入；普通交互示例支持
每秒模拟输出。内容新建另由每轮 100 次显式关闭/补回与 S2 动态 INSERT 测试覆盖。
这些数据不外推为 WP6 真实终端业务负载的性能保证。

## 内容身份与租约

身份 CSV 和结果日志在 `docs/benchmarks/workspace-interactions/final/`。500 次身份循环使用
reduced-motion，每次操作后 flush；真实运动另由五轮性能批次覆盖。每次循环检查
opaque widget handle 和 Rust owner 地址相等；宿主持有的 native Widget 地址另列。
16 个会话均通过 `resolve_scope([{instance_id:"pool",key:"session/n"}])` 找到同名 terminal。
循环期间 `created=16 / attached=16 / dropped=0`，每个 owner `strong_count=2`
（模拟池一份 + native 内容租约一份）；窗口关闭后 `dropped=16`。

本次 CSV 的样例：session/0 handle `0x1b991b0c190`、native Widget
`0x1b98f258130`、Rust owner `0x1b991cd4700`；session/1 分别为
`0x1b991b0c270`、`0x1b99194f530`、`0x1b991cd3380`。其余 14 项见
`final/identity-frames.identity.csv`。地址仅用于本进程内相等性证明，ASLR 下重跑会不同。

性能循环另外包含关闭并重新创建一个会话，故每轮 100 次应为 created/attached/drop
各 116；这些显式重建不属于移动造成的重建。resize 正确性由真实 TerminalViewport
观察器及确定性测试断言，性能日志的累计 viewport 数不能代替逐 epoch 的一次性断言。

## 文件规模、截图、产物与待复核

| 原文件 | b48df20 行数 | 当前行数 | 迁出的职责文件（行数） |
|---|---:|---:|---|
| style_sheet.cpp | 1246 | 115 | rule_parser 87、css_values 366、declarations 298、selectors 121、color 97、box 259 |
| tabs.cpp | 1098 | 214 | edit 176、pointer 224、geometry 227、style 173、paint 159 |
| terminal_view.cpp | 1901 | 364 | selection 372、paint 341、pointer 339、input 177、animation 219、viewport 76；helpers.h 70 |

新建手写源文件均小于 500 行，模板小于 400 行。三处指定大文件已实际迁出职责，
不是用 include 拼回同一个翻译单元。唯一仍超过 500 行的修改源文件是既有
`window_win32.cpp`（4138→4147）：仅增加 9 行内部计时接线，逻辑在独立
workspace_frame_probe 文件；没有把新 workspace 控件职责塞进窗口实现。此处明确
保留既有平台模块例外，避免为测试挂钩进行无关窗口重构。

交付自查见 `final/delivery-audit.json`：247 个交付文件，文本均为 UTF-8 无 BOM，
无问号乱码、替换字符或零宽空格；`git diff --check` 通过。最大新增手写源文件为
`acceptance.rs` 424 行。exe/DLL/viewc/模板哈希复核通过，60176 字节 IR 在实测 exe
偏移 623984 处逐字节匹配；release 示例 DLL 与 capture DLL 相同。

截图目录：[final](benchmarks/workspace-interactions/final/)。12 张真实运行截图：
light/dark × wide/narrow、floating、drag-preview、sftp-wide/medium/narrow、
monitor-wide、themes（12 项同屏）、media（同一本地 PNG 的 contain/cover）。
另有 64×64 的可再生 preview-fixture.png，属于测试输入而非运行截图。
PNG 同名 JSON 保留实际布局；没有用截图替代控件事件或几何断言。

产物审计见 [artifacts.md](benchmarks/workspace-interactions/final/artifacts.md)。仅统计本工作树
`build/`，可读取部分约 8.48 GiB，低于 30 GiB；8 个旧 viewc 临时目录大小未知，
不能声称统计完整。旧 ILK/重复对象删除被自动审批审查拒绝，释放量为 0，未换工具绕过。
保留固定 capture/cargo-target/codex-views 输出供正常终端复核，未复制第二套示例交付。

待复核/裁决：

1. Claude 在正常终端 clean 后全量 Ninja/CTest，并重跑普通 Cargo 托盘测试；本轮未运行全量 CTest。
2. 首次分屏 WarmFrame 最大 39.0417ms 已完整呈报。推荐维持 Q10、不新增启动门控；
   本次非预热帧均达标，真实 PTY/IME/远端输入在 WP6 重跑。
3. `window_win32.cpp` 仅 9 行内部计时接线的既有大文件例外，推荐接受；不影响三处指定拆分。
4. 构建清理受阻项由正常终端处理，先确认无构建/测试进程及目录访问权限，按清单删缓存而非整个目录。
5. 没有需要改动已冻结 C ABI 的问题。C ABI 相对 S2 diff 为空，记录在 `final/abi44-header-diff.txt`。

提交信息保留每步清单，同时提供合并信息：注册表、工厂、编译器、运行时与 CMake
在 S3–S8 交叉修改，不能把最终共享文件任意切给某一步而留下契约不一致的中间提交。
MyStudio **未同步**：写工具受禁止审批策略限制，未绕过；不声称已有 execution/handoff 回执。
