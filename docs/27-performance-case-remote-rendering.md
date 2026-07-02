# OneUI 性能排查经典案例：Remote 首页拖拽、全屏与 hover 卡顿

日期：2026-05-28

## 背景

Remote 首页已经切到 Rust 调用 OneUI C ABI，但在高配置机器上仍出现明显问题：

- 最大化、全屏、退出全屏时偶发闪烁。
- 拖动改变窗口大小时卡顿、闪烁、掉帧严重。
- hover 不跟手，快速移动鼠标时控件状态滞后或跳过。
- 用户明确指出：顶级 CPU/GPU 上简单桌面 UI 仍卡顿，不能归咎硬件。

这个案例的价值不在 Remote 页面本身，而在排查方法：必须先度量，再修 OneUI 通用机制，禁止给某个页面、某个坐标、某个组件写特殊分支。

## 红线

本次排查后，以下做法明确禁止：

- 禁止凭感觉改渲染路径。
- 禁止用固定坐标、固定宽度、固定区域修复某个页面。
- 禁止在 Remote 里补通用 UI 行为。
- 禁止把性能问题推给 Debug、CPU、GPU，除非 trace 已经证明瓶颈不在 OneUI。
- 禁止只看“能不能运行”，必须看帧耗时、输入响应和局部重绘。

允许并鼓励：

- 给 OneUI 通用 backend 增加 trace。
- 在 Canvas primitive、调度器、dirty rect、缓存层做通用优化。
- 用 smoke + 压测复现，不靠肉眼猜。
- 把每次优化前后的数字写下来。

## 排查过程

### 1. 先加 OneUI 渲染 trace

在 Win32 backend 中增加 `ONEUI_RENDER_TRACE`：

```powershell
$env:ONEUI_RENDER_TRACE = "1"
$env:ONEUI_RENDER_TRACE_FILE = "E:\project\byname\remote\target\debug\oneui-render.log"
```

trace 记录：

- `wm_size`：窗口尺寸变更消息数量。
- `paints/full/partial`：总绘制次数、满屏绘制次数、局部绘制次数。
- `avg_paint`：平均整体 paint 耗时。
- `avg_content`：OneUI 内容绘制耗时。
- `avg_blit`：Win32 blit 耗时。
- `surface_alloc_ms`：paint surface 分配耗时。
- `text / measure / shadow / gradient`：主要 primitive 的调用次数和总耗时。

第一次 trace 结果：

```text
avg_paint=63.20ms avg_content=61.68ms avg_blit=0.54ms surface_alloc_ms=0.24
```

结论：

- 不是 GDI blit 慢。
- 不是 surface 分配慢。
- 主要瓶颈在 OneUI 内容绘制。

### 2. 拆 primitive 耗时

继续把内容绘制拆成 `text / measure / shadow / gradient`：

```text
avg_paint=63.45ms avg_content=61.80ms avg_blit=0.57ms
text=109/13.13ms measure=24/0.02ms shadow=32/145.41ms gradient=9/15.29ms
```

结论：

- 阴影是最大热点。
- 文本和渐变也有成本。
- text measure 不是瓶颈。

这一步非常关键。如果没有 primitive trace，很容易误判为窗口、CPU、Debug 模式、DWM 或 GPU 问题。

### 3. 修交互调度：不要在鼠标事件里同步重绘

原问题：

`scheduleInteractivePaint()` 名义上是调度，实际会先 `flushPendingPaint()`，相当于鼠标移动、hover、输入时同步 `UpdateWindow()`。如果一帧内容绘制要 20ms，鼠标消息就会被 UI 绘制阻塞，表现为“不跟手”。

修复：

- 交互只 `InvalidateRect`。
- 用统一 timer 合并 paint。
- resize 中不再对每个 `WM_SIZE` 同步 `RDW_UPDATENOW`。

原则：

交互事件负责更新状态和标脏，绘制由调度器合并。不能在输入消息处理中做重活。

### 4. 修阴影：从每帧 blur 改为通用缓存

原问题：

OneUI 的 `drawBoxShadow` 每次绘制都使用 `SkImageFilters::Blur`，在 Debug 下尤其昂贵。阴影在 Remote 首页里非常多，满屏重绘时会反复计算。

修复分两步：

- 先用缓存的 `SkMaskFilter` 替代每次创建 image filter。
- 再把同尺寸、同圆角、同 blur、同颜色的阴影 raster 成 `SkImage` 缓存复用。

优化后：

```text
shadow=70/0.19ms
```

对比之前：

```text
shadow=67/61.26ms
```

原则：

阴影是典型昂贵 primitive，应该由 UI 框架通用缓存，不应该让每个业务页面自己避开阴影。

### 5. 修渐变：缓存渐变位图

原问题：

同样的卡片渐变在每帧重复 raster。shader 构造缓存收益不明显，说明主要成本不是 shader 创建，而是渐变填充本身。

修复：

- 对同尺寸、同圆角、同颜色、同角度的渐变生成 `SkImage`。
- 后续绘制直接 `drawImage`。

优化后：

```text
gradient=5/0.01ms
```

对比之前：

```text
gradient=5/1.96ms
```

### 6. 修文本：缓存 SkTextBlob

原问题：

每次绘制文本都构造 `SkFont`、测量、绘制 simple text。measure 本身不是大头，但文本绘制数量多，仍会累积。

修复：

- 对 text + size + weight 缓存 `SkTextBlob`、bounds、font metrics。
- `drawTextStyled` 和 `measureTextWidth` 共用缓存。

收益不如阴影和渐变明显，但这是通用框架应该具备的基础能力。

### 7. 修 borderless 全屏路径

Remote 使用 borderless 窗口。全屏/恢复不需要频繁修改 window style。

修复：

- borderless fullscreen 只保存窗口矩形。
- 进入全屏：一次 `SetWindowPos` 到 monitor rect。
- 退出全屏：一次 `SetWindowPos` 回 saved rect。

原则：

窗口状态切换要减少中间状态，避免先移动到左上角、再 resize、再触发额外重绘的可见过程。

## 关键数据

### 首屏 / 窗口状态绘制

优化前：

```text
avg_paint=63.20ms avg_content=61.68ms avg_blit=0.54ms surface_alloc_ms=0.24
```

优化后：

```text
avg_paint=19.44ms avg_content=17.81ms avg_blit=0.57ms
text=110/12.71ms shadow=35/24.44ms gradient=9/4.78ms
```

### hover / input 局部绘制

优化后：

```text
paints=24 full=0 partial=24 avg_paint=1.05ms avg_content=0.83ms
shadow=70/0.18ms gradient=5/0.01ms
```

这说明局部交互已经可以做到接近即时响应。

### 连续 resize 压测

优化前：

```text
avg_paint=24.69ms avg_content=24.05ms
shadow=468/477.53ms gradient=108/142.28ms
```

优化后：

```text
avg_paint=10.71ms avg_content=10.02ms
shadow=715/101.60ms gradient=165/29.12ms
```

resize 仍可能继续优化，但已经从明显掉帧区间降到更接近可接受区间。

## 验证命令

构建 OneUI Debug：

```powershell
$cmd = 'call "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" && cmake --build E:\project\byname\oneui\build\msvc-bundled-static --config Debug'
cmd.exe /d /s /c $cmd
```

运行 OneUI 测试：

```powershell
$cmd = 'call "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" && ctest --test-dir E:\project\byname\oneui\build\msvc-bundled-static -C Debug --output-on-failure'
cmd.exe /d /s /c $cmd
```

构建 Remote Rust shell：

```powershell
cargo build -p remote-rust-oneui-shell
```

复制 OneUI DLL 到 Remote Debug 输出：

```powershell
Copy-Item E:\project\byname\oneui\build\msvc-bundled-static\oneui.dll E:\project\byname\remote\target\debug\oneui.dll -Force
```

运行 smoke + trace：

```powershell
$env:ONEUI_RENDER_TRACE = "1"
$env:ONEUI_RENDER_TRACE_FILE = "E:\project\byname\remote\target\debug\oneui-render.log"

powershell -ExecutionPolicy Bypass -File E:\project\byname\remote\scripts\test-oneui-shell-screenshot-smoke.ps1 `
  -Exe E:\project\byname\remote\target\debug\remote-rust-oneui-shell.exe `
  -OneUiDll E:\project\byname\remote\target\debug\oneui.dll `
  -Out E:\project\byname\remote\target\debug\oneui-shell-debug-perf.png `
  -ExerciseHover `
  -ExerciseInput `
  -SettleMilliseconds 1000
```

## 复盘结论

这次问题不是 Remote 页面写错一个 hover，也不是机器配置不够，而是 OneUI 的通用渲染管线存在几个基础缺陷：

- 交互事件同步刷新，导致 UI 线程被绘制阻塞。
- 昂贵 primitive 没有缓存，尤其是阴影和渐变。
- full resize 过程每帧重绘成本过高。
- 全屏路径对 borderless 窗口不够直接。

正确的处理方式是：

1. 先加 trace。
2. 用 trace 判断瓶颈层级。
3. 在 OneUI 通用机制修复。
4. 用 smoke、压测和截图回归验证。
5. 把数据写入文档，形成下一次排查模板。

这套流程以后适用于所有 OneUI 下游产品，包括 Remote、网云穿、iShell 和后续工具。

## 后续优化方向

本次已经解决主要卡顿来源，但还有可继续优化的方向：

- layout dirty 标记，避免每次 paint 都递归 layout。
- 文本 glyph/cache 进一步优化，减少 Debug 下 text 绘制成本。
- resize 时更精细地合并 `WM_SIZE`，避免过密满屏重绘。
- 为 shadow/gradient cache 增加 LRU，而不是简单超过容量清空。
- 增加自动化 resize perf baseline，避免性能回退。

