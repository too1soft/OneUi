# 展开／收起连续性与自动检测

## 修复了什么

材质与动效页的 Reveal 有两种不同的问题：

1. 通用数值过渡为了让颜色立即反馈，启动时先推进 8ms；用于布局高度时产生位置跳跃，第一次真实计时回调还可能倒退。Reveal 现在从真实起点开始，快速反向切换继续沿当前进度过渡。
2. 内容可见性切换时，父 Stack 的 gap 整段出现／消失。非换行纵向 Stack 现在让 Reveal 相邻间距随展开进度变化；内容控件保持同一实例。Legacy 与 Yoga 均有测试，切换到横向／换行／移除 Reveal 后恢复普通 gap。
3. 软件绘制在 125%／150% 内容缩放下暴露出阴影开销。大尺寸整数外阴影复用小块缓存；CPU 分别画边角并填充中心，避免大图缩放采样。不透明纯色背景覆盖的内部区域通过单次差集裁剪跳过，保留圆角边缘；透明背景仍完整绘制阴影。GPU 保留九宫格路径。

`Canvas::clipOutRect` 是可选 C++ 优化接口，不支持的自定义后端返回 false，保留完整绘制。没有更改 C ABI。更新 C++ SDK 后应一起重编译应用及自定义 Canvas 子类。

本页覆盖[上一阶段报告](52-layout-recipes-and-presets.md)中“软件仍使用原尺寸缓存”的现状说明；旧数据保留为历史基准。

## 自动运行

在仓库根目录执行（Windows，先关闭其他实验台）：

```powershell
.\examples\performance_lab\build.ps1 -Test
.\examples\performance_lab\check-motion.ps1
# 单后端、不同目标刷新率：
.\examples\performance_lab\check-motion.ps1 -Renderer gpu -TargetHz 120
```

脚本会自行打开和关闭测试窗口。每个后端覆盖 120／220／400ms 过渡、1320／640px 窗口和 100%／125%／150% 内容缩放；每组包含常规及快速反向切换，共十组。它复用真实 Gallery 控件、命令与原生动画调度，不通过测试计时器逐帧强推动画。

默认输出 `examples/performance_lab/results/motion-check/`：

- `summary.csv`：每组通过／失败、跳变、倒退、慢帧和最大间隔。
- `motion-frames.csv`：每帧进度、实测／预期高度、位置和内容绘制耗时。
- `motion-report.txt`：源模板位置、后端及参数。
- `environment.json`：机器环境与测试二进制哈希。

不自动重试失败组，失败日志保留。返回非零可作为 Windows 原生测试机上的回归门禁；建议性能门禁独占测试机，避免编译或其他重负载干扰。

默认 60Hz 帧预算为 16.67ms，间隔 >25ms 记慢帧，>50ms 记停顿；当前门禁要求无几何跳变、无进度倒退、无停顿，慢帧占比不超过 20%，并检查控件样式节点和订阅数不增长。几何容差 1.5 逻辑像素用于现有布局取整，不能理解为帧帧亚像素精确。门禁不是承诺锁定 60 FPS。

## 在自己的场景接入

`#include <oneui/ui_motion_diagnostics.h>` 提供无分配的 `oneui::ui::MotionAudit`：

```cpp
oneui::ui::MotionAudit audit(1000.0 / 60.0);
// 用户改变目标时开始一段；保留累计报告，排除交互间的空闲时间。
audit.begin(nowMs, reveal->progress(), opening);
// 在实际 paint 完成后采样；expectedHeight 来自场景的布局契约。
audit.observe(paintEndMs, reveal->progress(), actualHeight,
              expectedHeight, contentPaintMs);
const auto& result = audit.report();
```

这不是默认打开的全局监控：调用方需要指定要测的控件与预期轨迹。SDK 单测会注入 20px 跳变、反向进度和 60ms 停顿，确保检测器确实报警；Demo 再验证真实布局与原生调度。仅看平均 CPU／paint 不足以发现位置跳变。

## 实测与边界

数据见下方本机报告。采样点位于原生控件树 paint 完成，包含内容绘制以及相邻采样之间的调度影响；不是显示器呈现时间或 GPU 时间戳。125%／150% 是内容缩放，不能替代物理 DPI 切换、跨屏和真实显示观感检查。其他布局、复杂内容和其他机器需要各自建立轨迹与帧预算。

### 2026-09-28 本机结果

Windows 10 22H2、Ryzen 9 9950X3D、RTX 5080，Release、60Hz 预算。修复前 GPU 路径即使内容 paint 最长约 0.50ms，仍检测到最大 21px 几何偏差和 12 次进度倒退；说明低绘制耗时不等于动效连续。

| 场景 | 修复前／中间基线最长采样间隔 | 最终最长采样间隔 | 最终结果 |
|---|---:|---:|---|
| GPU，五组参数 | 仅修几何后 6.1–6.8ms | 6.3–6.8ms | 无几何错误、倒退、慢帧或停顿 |
| CPU，125%内容缩放 | 48.34ms（仅修几何） | 25.45ms | 1 次 >25ms 慢帧，无停顿 |
| CPU，150%内容缩放 | 50.40ms（仅修几何） | 20.80ms | 无慢帧、无停顿 |

十组均通过门禁，最大几何偏差不超过 1.47 逻辑像素（布局取整范围）。CPU100% 宽／窄三组最长间隔 12.26–19.59ms。没有删掉或重试慢帧；这些是单轮本机回归结果，不是跨机器性能承诺，也没有把采样间隔倒数当作显示 FPS。

同一构建的原路径／优化路径整图像素对照：CPU 三档缩放最大差均为 0；GPU 最大通道差 0／11／10，整图平均绝对差 0／0.003031／0.001768，满足原有门槛。包含透明背景、非整数坐标、小尺寸和圆角阴影。

SDK 36/36 通过，实验台 `build.ps1 -Test` 全套通过。原始记录：[最终十组数据与环境](benchmarks/motion-20260928/final/summary.csv)、[环境及二进制哈希](benchmarks/motion-20260928/final/environment.json)、[修复前 GPU 报告](benchmarks/motion-20260928/before-gpu/motion-report.txt)、[软件优化前失败组](benchmarks/motion-20260928/after-geometry-before-cpu-optimization.csv)、[像素对照](benchmarks/motion-20260928/pixel-comparison.txt)。基准来自隔离工作树，未将主目录其他未提交功能混入对照。

合回主目录后再次通过实验台全套测试、两后端原生动效与像素检查；GPU100%／CPU150%最长采样间隔分别为 6.41／21.07ms，无几何错误、倒退、慢帧或停顿。[合并后原始记录](benchmarks/motion-20260928/main-verification/main-tests.log)。原有改动通过正反向三方合并及 1002 个无关受跟踪文件哈希校验保留。

后续更新：[内置开发诊断](54-built-in-developer-tools.md)已将通用慢帧与 Reveal 倒退检测接入窗口运行时。普通使用无需手动采样；本页专项轨迹与脚本继续用于自动回归。

字体口径更新：上述归档动效数据的回归窗口使用 SDK 默认 `Segoe UI` 加系统中文回退；发现其与正式 Demo 字体不同后，已统一为 `Microsoft YaHei UI`。旧数据保留为同一字体配置下的修复前后对照，不直接视作新版字体配置的基准。统一后 GPU／1320px／220ms／100% 再次通过连续性检查（无几何错误、倒退、慢帧或停顿）。
