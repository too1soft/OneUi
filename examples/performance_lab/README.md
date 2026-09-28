# OneUI 性能实验台

同一个 Windows 原生应用里体验图表、粒子、虚拟列表、连接管理和默认组件。所有连接与保存均为本地模拟，不访问服务器。初次使用先按[仓库 README](../../README.md#运行-demowindows)准备依赖。

在仓库根目录执行：

```powershell
.\examples\performance_lab\run.ps1 -Build -Test
.\examples\performance_lab\run.ps1 -Components -Dev
.\examples\performance_lab\run.ps1 -Connections -Entry template -Dev
.\examples\performance_lab\run.ps1 -Editor -Entry code -Compact
.\examples\performance_lab\run.ps1 -Load heavy
```

每条运行命令打开一个窗口；对比性能前关闭其他实验台窗口。普通启动不监听文件，`-Dev` 才开启样式热更新。`-Entry code|template` 选择连接列表和编辑页的写法，组件展示页统一使用 `Gallery.one`。`-Compact` 使用紧凑密度。

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
| 完整列表与编辑流程 | `-Connections`；搜索、筛选、双击编辑、新建、删除、未保存确认 | [connection_workspace.hpp](connection_workspace.hpp) |
| C++ 表单与异步保存 | `-Editor -Entry code`；校验错误、保存、失败重试 | [connection_editor.hpp](connection_editor.hpp) |
| 模板里的同一业务 | `-Connections -Entry template` | [views/Connections.one](views/Connections.one)、[views/Editor.one](views/Editor.one) |
| 修改外观即时预览 | `-Dev` 后修改外部 CSS 或 `.one` 的 style 块 | [connections.css](connections.css) |

页面使用 SettingsPage/ListPage、FormGrid、FormRow 与 Toolbar。FormGrid 按约 320 逻辑像素列基准自动分栏／换行，字段标签上置；不用逐个 `setFrame`。它不是完整浏览器 Grid，不支持跨列。窄窗口与长中文交由布局和文字测量处理。

模板结构、事件和 C++ 逻辑变化要重新构建；CSS 错误保留上一次有效样式。组件页的加载外观是模拟状态，完整异步保存请到连接编辑页体验。展示页第一次打开时才创建，之后保留。

## 测量与截图

```powershell
# 关闭所有 oneui-performance-lab / gpui-performance-lab 进程后运行
.\examples\performance_lab\compare-entries.ps1 -Rounds 3 -Cycles 200
.\examples\performance_lab\capture-gallery.ps1
```

输出在忽略提交的 `results/` 下。每轮 50 次预热，随后 150 次列表搜索／编辑／取消；代码与模板交替先运行，使用同一 exe/DLL、窗口和数据。测量脚本拒绝与其他实验台同跑。保存的二进制哈希可帮助确认测的是哪个构建。

CPU 百分比是整机逻辑处理器归一化后的进程 CPU；paint 是 CPU 侧控件绘制遍历，不是 GPU 执行时间或显示帧率。综合图表页持续动画，不能用连接页空闲结果代表它。详细[测量结果与边界](../../docs/40-declarative-stage-validation.md)。

截图来自 OneUI 原生离屏客户端捕获，可复现宽／窄、浅／深和紧凑密度。它不证明物理显示器 125%／150% DPI 或系统 IME 候选窗口已通过验收。
