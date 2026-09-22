# 渲染路径与验证边界

本文依据当前 Win32 后端实现，区分渲染能力、运行时选择和验收证据。
早期文档中的“仅 Skia raster”不再代表当前 Win32 实现。

## 当前路径

| 后端 | 渲染与呈现 | 验收边界 |
| --- | --- | --- |
| Win32 GPU | WGL/OpenGL + Skia Ganesh；保留绘制 surface，提交窗口 surface 后 SwapBuffers | 依赖宿主驱动；代码已实现不等于全部硬件验收 |
| Win32 软件 | Skia raster；GDI 提交完整或局部像素 | GPU 不可用时的回退，也可显式选择 |
| X11 | 共享 Skia Canvas / raster，XImage 呈现 | 已有 WSLg 记录；真机与发行版验收仍待完成 |
| Wayland | 共享 Skia Canvas / raster，shm 缓冲呈现 | 协议能力依 compositor；WSLg 不替代原生验收 |
| Cocoa | 共享 Skia Canvas / raster，Cocoa 呈现实现 | 源码已接入，尚待 Mac 构建与运行 |

Win32 `gpuRenderingEnabled()` 默认返回启用；`initGPU()` 尝试创建 OpenGL 上下文和
`GrDirectContext`。可用时启用交换间隔；初始化失败保留软件路径。
`ensurePaintSurfaceCapacity()` 创建 GPU render target 或 raster surface；窗口交换失败后
丢弃 GPU surface、标记重绘并回退。环境变量控制的是尝试策略，不能证明实际用了 GPU。

核心控件通过 `Canvas` 绘制，不接触 OpenGL/Skia 私有对象。
GPU 和软件路径共用逻辑像素、DPI、布局、文字及输入契约。

## 运行时排查

以下 PowerShell 命令在仓库根目录执行，只影响当前 shell 及后续启动的进程：

```powershell
# 强制软件路径。
$env:ONEUI_ENABLE_GPU = "0"
$env:ONEUI_RENDER_TRACE = "1"
$env:ONEUI_RENDER_TRACE_FILE = Join-Path $PWD "out/render-software.log"
New-Item -ItemType Directory -Force out | Out-Null
& .\build\msvc-bundled-static\examples\gallery\oneui_gallery.exe

# 恢复默认 GPU 尝试；日志分开保存。
Remove-Item Env:ONEUI_ENABLE_GPU -ErrorAction SilentlyContinue
$env:ONEUI_RENDER_TRACE_FILE = Join-Path $PWD "out/render-default.log"
& .\build\msvc-bundled-static\examples\gallery\oneui_gallery.exe
```

成功初始化时 stderr 会输出 `OneUI GPU rendering enabled (OpenGL+Skia Ganesh, ...)`。
该日志只证明初始化成功；后续仍可能回退，不能把启动日志作为全程 GPU 性能证明。
结束排查后可移除 `ONEUI_RENDER_TRACE` 和 `ONEUI_RENDER_TRACE_FILE` 环境变量。

## 已有优化与边界

- 控件通过矩形失效传播局部重绘；窗口按物理像素 dirty rect 裁剪。
- Win32 交互绘制由定时器调度合并；不同事件、resize 和刷新入口仍需分别测量。
- 软件 backing surface 预留增长容量，GPU surface 按当前窗口尺寸管理。
- 文字共享布局与缓存见[文字引擎状态](38-text-and-interaction-engine.md)。
- `RealtimeFrameView` 接收 BGRA/RGBA 完整帧和 damage 批次；存在 GPU 后端不代表
  已实现硬件解码纹理直通、零复制或 NV12 转换。

早期[远程渲染性能案例](27-performance-case-remote-rendering.md)保存了特定构建和页面的
排查过程，其中数字不是当前版本、GPU 路径或其他产品的性能承诺。

## 验证要求

比较渲染性能时固定构建配置、机器、驱动、DPI、窗口尺寸、数据量和交互脚本，记录实际
路径以及 CPU/GPU 占用、内存、帧耗时分布和输入响应。现有 trace 不直接提供全部这些指标；
缺少端到端测量时不得推算输入到显示延迟，也不作 GPUI 等框架的倍数比较。

CTest、Rust 行为测试、布局 JSON 与 raster 像素测试分别验证不同契约；它们不能替代
真实驱动、跨屏 DPI、系统 IME、窗口缩放及长时间运行的原生验收。
当前平台状态以[支持矩阵](37-native-desktop-backends.md)和[Win7 记录](24-windows7-compatibility.md)为准。
