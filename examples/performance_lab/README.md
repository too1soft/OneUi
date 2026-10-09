# OneUI 试验台

终端工作区现位于 `features/terminal`，不再需要独立终端 Demo。
它复用 Rust 所有者、四组模拟会话、文件/监控区域，并增加日志；
页面离开时停止采样和模拟传输。C++、C、Rust 与 `.one` 接入片段均参与编译检查。
跨窗终端所有权可在 `layout/docking` 的“Rust 终端会话”体验。

针对该迁移的测试目标为 `oneui-lab-terminal-tests`；Windows `build.ps1 -Test` 会运行。
Mac/Linux 复用已构建 SDK，以相同 CMake 目标构建 Rust 动态库与 Demo。

统一入口包含“布局 / 性能 / 功能”两级导航，默认进入基础布局。Windows、Mac、Linux 复用同一套页面；所有终端、远程画面与连接流程使用本地模拟数据。迁移和原生验收仍在进行，当前不能视为全部上线就绪。详见[能力目录](../../docs/unified-lab-capabilities.md)和[本轮验收记录](../../docs/unified-lab-implementation.md)。

Android、iOS、原生 HarmonyOS 的最终验收也以这套试验台为准，按全部能力组和关键交互对齐 PC 完整度。
手机/平板调整导航和布局，但保留同一页面 ID、控件与业务状态；当前 27 页及后续新增能力均不能静默省略。
系统限制须有证据并由用户验收，基础 GPU/生命周期测试程序不算移动试验台完成。
完整条件见[移动端统一试验台验收基线](../../docs/mobile-support.md#最终验收基线统一试验台与-pc-同等完整度)。

Android 实验宿主已复用共享 `layout/basic` 操作区与 Skia GPU 控件，入口在 `mobile/android`。
目前尚无完整导航/代码面板，其余页面与 IME/无障碍仍待接入；不能视为移动端验收完成。
构建、debug APK 和模拟器回归见[Android 接入记录](../../docs/mobile-support.md#android-统一试验台基础布局接入)。

```powershell
.\examples\performance_lab\run.ps1 -Build
.\examples\performance_lab\run.ps1 -Page layout/docking
.\examples\performance_lab\run.ps1 -Page features/multiwindow
.\examples\performance_lab\run.ps1 -Page performance/renderers
```

可执行文件也接受 `--page <稳定页面 ID>`。代码面板直接嵌入 `snippets/` 中参与构建的源码；C++、UTF-8 C、Rust 以及适用的 `.one` 均有编译检查。完整 Demo 构建需要 Rust 工具链，单独构建 SDK 不依赖它。使用 `generate-lab-catalog.py --strict` 检查尚未完成的发布覆盖。

原性能场景参数保持兼容，并使用独立场景排除导航与代码面板开销。以下命令明确进入兼容模式；不要与 `-Page` 混用。

在仓库根目录执行：

```powershell
.\examples\performance_lab\run.ps1 -Build -Test
.\examples\performance_lab\run.ps1 -Components -Dev
.\examples\performance_lab\run.ps1 -Connections -Entry template -Dev
.\examples\performance_lab\run.ps1 -Details -Entry code
.\examples\performance_lab\run.ps1 -Editor -Entry code -Compact
.\examples\performance_lab\run.ps1 -Load heavy
```

每条运行命令打开一个窗口；对比性能前关闭其他实验台窗口。普通启动不监听文件，`-Dev` 才开启样式热更新。`-Entry code|template` 选择连接列表、详情和编辑页的写法，组件展示页统一使用 `Gallery.one`。连接列表与详情默认浅色，页头可切换深色。`-Compact` 使用紧凑密度。

双击或 Enter 查看详情，“编辑选中”直接编辑；从详情编辑后保存或取消返回详情，从列表直接编辑则返回列表。筛选与有效滚动位置保留；Esc 按原路径返回，有未保存修改时先确认。[从字段到页面的复制教程](../../docs/48-connection-page-recipes.md)。

## 选择 CPU / GPU

```powershell
.\examples\performance_lab\run.ps1 -Renderer auto
.\examples\performance_lab\run.ps1 -Renderer gpu
.\examples\performance_lab\run.ps1 -Renderer cpu
.\examples\performance_lab\compare-renderers.ps1 -Rounds 3 -Seconds 5
.\examples\performance_lab\compare-renderers.ps1 -Load heavy -Rounds 3 -Seconds 5
```

每次关闭上一窗口再切换。auto 继承环境，gpu 优先尝试 GPU，cpu 使用软件；都只改变当前应用的启动策略。标题下方显示实际后端、设备或回退原因。对比脚本测空闲、表格滚动、图表和粒子，并拒绝 GPU 回退样本。需先 `-Build`；详见[实测与诊断 API](../../docs/41-renderer-selection-and-product-boundaries.md)。

`-Load heavy` 将粒子提高到 10,000、曲线提高到每条 4,000 点。`summary.csv` 同时记录采样末尾的 Skia 字体／资源／GPU 缓存；这些不是全部进程内存或显存，`skia_gpu_cache_limit_mib` 只是预算。字段说明、实测和限制见[高压缓存诊断](../../docs/43-heavy-renderer-diagnostics.md)。

粒子默认选择 **mesh 索引网格**：GPU 走网格，软件或不支持的输入自动回退到保序批量绘制。`-ParticleMode combined` 选择旧组合方案作对照；其余选项为 `reference`、`precomputed`、`batch`、`mesh`。普通 SDK 控件不受此 Demo 默认值影响。

本机修正版已通过 18 + 36 个严格 GPU RGB 场景；上一轮五次 A/B 的 CPU 侧绘制耗时为 **5.23 → 4.17ms，降低约 20.4%**。对比两个方案：`.\examples\performance_lab\compare-particles.ps1 -Modes combined,mesh -Rounds 5 -Seconds 5`。[画质与性能原始数据](../../docs/46-particle-mesh-parity.md)。

新增长期验证脚本：`.\examples\performance_lab\test-stability.ps1 -Seconds 1800`，流式记录内存、资源计数和绘制耗时；加 `-Seconds 20 -Exercise` 可单独检查页面、暂停与窗口缩放。另有 `-Renderer cpu` 和仅实验台有效的 `-FailGpuInit`。脚本验证省略模式参数后的实际默认值，提前关闭算未完成。[默认行为、稳定性与回退报告](../../docs/47-particle-mesh-default.md)。

## 看什么、改哪里

| 体验 | 操作 | 源码 |
|---|---|---|
| 综合性能 | 切换场景、负载、暂停，观察 CPU／内存／回调间隔 | [main.cpp](main.cpp)、[plots.hpp](plots.hpp) |
| 默认外观与自适应 | `-Components`；切主题、密度；缩到 640px；切表单／表格／反馈 | [views/Gallery.one](views/Gallery.one) |
| 列表、详情与编辑流程 | `-Connections`；搜索、筛选、双击查看详情、新建、删除、未保存确认 | [connection_workspace.hpp](connection_workspace.hpp) |
| C++ 表单与异步保存 | `-Editor -Entry code`；校验错误、保存、失败重试 | [connection_editor.hpp](connection_editor.hpp) |
| 模板里的同一业务 | `-Connections -Entry template` | [views/Connections.one](views/Connections.one)、[views/Details.one](views/Details.one)、[views/Editor.one](views/Editor.one) |
| 修改外观即时预览 | `-Dev` 后修改外部 CSS 或 `.one` 的 style 块 | [connections.css](connections.css) |

页面使用 SettingsPage/ListPage/DetailPage、FormGrid、FormRow 与 Toolbar。FormGrid 按约 320 逻辑像素列基准自动分栏／换行，字段标签上置；不用逐个 `setFrame`。只读 Text 和编辑 Input 共用 FormRow。它不是完整浏览器 Grid，不支持跨列。窄窗口与长中文交由布局和文字测量处理。

模板结构、事件和 C++ 逻辑变化要重新构建；CSS 错误保留上一次有效样式。组件页的加载外观是模拟状态，完整异步保存请到连接编辑页体验。展示页第一次打开时才创建，之后保留。

## 测量与截图

```powershell
# 关闭所有 oneui-performance-lab / gpui-performance-lab 进程后运行
.\examples\performance_lab\compare-entries.ps1 -Rounds 3 -Cycles 200
.\examples\performance_lab\capture-gallery.ps1
.\examples\performance_lab\capture-connections.ps1
```

输出在忽略提交的 `results/` 下。每轮 50 次预热，随后 150 次列表搜索／编辑／取消；代码与模板交替先运行，使用同一 exe/DLL、窗口和数据。测量脚本拒绝与其他实验台同跑。保存的二进制哈希可帮助确认测的是哪个构建。

CPU 百分比是整机逻辑处理器归一化后的进程 CPU；paint 是 CPU 侧控件绘制遍历，不是 GPU 执行时间或显示帧率。综合图表页持续动画，不能用连接页空闲结果代表它。详细[测量结果与边界](../../docs/40-declarative-stage-validation.md)。

截图来自 OneUI 原生离屏客户端捕获，可复现宽／窄、浅／深和紧凑密度。它不证明物理显示器 125%／150% DPI 或系统 IME 候选窗口已通过验收。

## 类型化 C++ 入口

连接列表、详情及编辑页已使用公共 `Compose` API。`ui.field(L"名称", ui.input(vm.name))` 自动创建标签与绑定，`ui.formGrid({...})` 自动分栏。两种入口共用组件元数据、原生控件和布局；[用法、截图与实测](../../docs/50-typed-authoring-and-defaults.md)。当前默认主题进一步区分分组／字段／说明间距，以及只读标签与值的文字层级。

单独测当前代码版／模板版的页面构建成本：

```powershell
.\examples\performance_lab\build-current\bin\oneui-authoring-tests.exe --construction-benchmark
```

该 CSV 包含 2 轮预热后的 10 轮交替样本，计时包含 ViewModel、Mount、控件创建、应用主题和提交属性，不包含窗口启动／绘制／析构。

## 材质与动效场景

```powershell
.\examples\performance_lab\run.ps1 -Build -Effects
.\examples\performance_lab\run.ps1 -Effects -Dev -Renderer gpu
.\examples\performance_lab\compare-effects.ps1 -Rounds 3 -Seconds 5
```

`-Effects` 直达组件展示的第四页：调节三色线性／径向渐变、阴影模糊、展开／淡入和时长，输入框可体验真实内阴影。悬停／按住两个按钮观察反馈，点击切换说明；“直接切换”关闭内容过渡。切换主题、密度和参数时保留同一份输入状态。

布局及 scoped 样式在 [Gallery.one](views/Gallery.one)，参数与命令在 [component_gallery.hpp](component_gallery.hpp)。`-Dev` 支持 CSS 热更新，模板结构仍需重建。应用默认主题没有普遍添加阴影；这是按需使用公共能力的展示。

对比脚本分别测活动和空闲的 CPU／GPU 路径，保存环境、哈希和逐轮数据，拒绝 GPU 回退及空闲额外绘制；计时是 CPU 侧耗时，不代表 GPU 执行时间或显示 FPS。效果页统一使用模板，`-Entry` 不会生成另一套 C++ 效果页，因此不能据此对比两种写法的运行开销。详见[用法、支持边界和验收](../../docs/51-effects-and-motion.md)。

## 完整页面组合

`./run.ps1 -Layouts -Entry code` 或 `-Entry template` 打开场景4。支持导航、列表与详情、限宽设置页、固定底部操作和标准／柔和预设。可用 `-Dev` 保持原有 CSS 热更新。命令行截图可加 `--layout-settings`、`--layout-detail`、`--soft-preset`。见[教程与验证](../../docs/52-layout-recipes-and-presets.md)。

`./compare-shadows.ps1` 用同一实验台二进制对比原尺寸阴影缓存与九宫格缓存；它是阴影路径对照，不是 C++／模板性能对照。

## 展开／收起自动回归

构建后运行 `./check-motion.ps1`，自动打开原生窗口完成十组 CPU／GPU、宽／窄、时长与内容缩放测试。检查位置跳变、进度倒退、慢帧和节点／订阅增长；失败退出非零并保留逐帧 CSV。可加 `-Renderer cpu` 或 `-TargetHz 120`。运行时关闭其他实验台，避免并行编译干扰计时。详见[检测接入、阈值与边界](../../docs/53-motion-continuity-and-diagnostics.md)。

## 内置 DevTools

`./run.ps1 -Effects -DevTools` 开启 SDK 自带诊断面板。F12 显隐、Ctrl+F12 导出工作目录的 `oneui-devtools.json`；正常使用即可发现慢绘制、动画延迟及 Reveal 倒退。它独立于 `-Dev` 样式热更新，也不依赖回归脚本。[完整接入与限制](../../docs/54-built-in-developer-tools.md)。

字体：正式实验台和动效回归窗口统一指定 `Microsoft YaHei UI`。该 Demo 目前使用系统字体，没有随包默认中文字体；SDK 支持注册应用字体数据。


## macOS 实机运行

2026-10-08 已在 Apple M1 / macOS 15.5 上编译运行。复用与源码头文件匹配的 Release OneUI
原生构建目录，不重新编译 Skia。先使 CMake、Ninja 可用，再执行：

```sh
cmake -S examples/performance_lab -B "$HOME/.cache/oneui-release-review/performance-lab" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DONEUI_EXISTING_BUILD_DIR="$HOME/.cache/oneui-release-review/build"
cmake --build "$HOME/.cache/oneui-release-review/performance-lab" --target oneui-performance-lab
open "$HOME/.cache/oneui-release-review/performance-lab/oneui-performance-lab.app" --args \
  --renderer gpu --output "$HOME/.cache/oneui-release-review/performance-lab/results"
```

此 `.app` 是依赖上述现有 dylib 的开发程序，不是可独立分发的安装包。Mac 的 CPU 采样使用
`getrusage`，内存显示 Mach resident size（RSS）；Windows private commit、GDI、USER 和句柄数
在 Mac 报表中标为 `nan`，不能跨平台直接比较。实际渲染后端可显示 Metal / Apple M1。
实验性 circle mesh 已接入 Mac，并通过 Metal 像素对比；但当前短测没有证明稳定的 CPU 收益，默认仍使用保序批量绘制回退。
本次通过遥测自检并启动交互窗口，不代表完成长时间性能对比或全部页面人工验收。


## Mac 性能回归（2026-10-08）

已修复指标字体在每次绘制时切换继承环境、导致隐藏指标也不断触发重绘的问题；使用控件自身的
字体覆盖属性保留等宽字体。Mac 事件循环按剩余动画截止时间等待，并读取所在显示器的刷新周期，
避免每轮工作后重新等待完整的 16 ms。高刷新率及跨屏切换仍需相应设备验收。

统一采样入口为 `benchmark-native.py`，使用固定 `current-case` 目录，成功后只保留 baseline/candidate
汇总和二进制哈希，不为每轮复制应用。关闭其他 Demo 后，给出匹配的 `--exe`、`--library`、
`--output` 与 `--phase baseline|candidate`；默认每个场景预热 3 秒、采样 5 秒、正反顺序各一轮，
包含标准/高压的静止页、表格滚动、图表与粒子。`--min-frame-hz 55` 可用于当前 60 Hz 环境的回归门槛；
候选的静止页如果持续重绘，采样会直接失败。报告包含物理尺寸、每帧 CPU 耗时、P95/P99 回调间隔、
后端、内存和缓存，回调频率不等于显示呈现帧数。跨平台比较必须另行对齐实际物理尺寸和负载。

私有 mesh 实验需要 SDK 配置 `ONEUI_ENABLE_CIRCLE_MESH_EXPERIMENT=ON`，Demo 同时配置
`ONEUI_LAB_MESH_AVAILABLE=ON`；两个选项默认均关闭。新增 Metal 测试验证 GPU/raster 图形与窗口尺寸变化，
以及 18 组粒子规模、透明叠加、缩放和裁剪组合。Metal 小数像素裁剪明确回退，避免边缘缺失；
当前 18 组 mesh/回退结果与参考绘制逐像素相同。默认 SDK 不自动启用这项私有实验。
全局 MSAA 候选未采用：虽然曲线 CPU 绘制耗时降低，但粒子成本及画质一致性不满足本轮门槛。
实测数据和边界见 `docs/37-native-desktop-backends.md` 文末。


### Mac 路径采样与缓存验证（2026-10-08）

对当前进程使用 macOS `sample` 验证：动态图表主要耗时在复杂描边及
`SoftwarePathRenderer`；粒子主要耗时在逐个圆形的 Ganesh 指令构建/合批。
这不代表整个窗口使用软件渲染，窗口后端仍是 Apple M1 Metal。

共享路径适配器现在保留最多 16 个、各不超过 512 点/512 命令的小型路径，
按完整几何相等复用原生路径身份，帮助静态图形复用 Skia 缓存。公开命令数组
直接修改后不会复用旧几何；大路径绕过缓存。该机制不保证所有动画的总 CPU 降低。

Mac 私有粒子 mesh 对小数裁剪增加安全内区检查：仅在整批粒子连同抗锯齿边缘
完全位于裁剪框内时继续批量绘制；触及边界仍整体回退。27 组裁剪/缩放/数量
对比通过，不构成所有变换、裁剪形状或所有 GPU 的保证。默认 mesh 仍关闭。

复杂描边的局部 MSAA 和 mesh 顶点写入试验没有证明稳定的总 CPU 收益，均未保留。
固定的 baseline/candidate CSV 与环境哈希记录本轮对比；上一轮汇总表保留在
`docs/37-native-desktop-backends.md`。不能将 CPU 墙钟绘制时间当成 GPU 执行时间或功耗。


`--desktop-menus` 打开统一桌面菜单示例：场景和负载使用单选菜单，实时运行使用勾选项，
支持快捷键和托盘菜单。Mac 显示系统顶部菜单，Windows/Linux 显示窗口内菜单。
该参数默认关闭，以保持性能基准的原始布局和测试条件。
