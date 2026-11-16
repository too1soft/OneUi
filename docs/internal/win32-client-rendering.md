# Windows 客户区重绘、打印与捕获契约

本文维护 Windows 原生窗口的实际绘制入口、标准打印消息和验证方法。公开接口仍以 `include/oneui/platform/window.h` 为准；本次没有增加 C/C++/Rust 公共签名、ABI 版本或产品截图权限。

## 1. 三条路径与适用范围

| 路径 | 图像来源 | 用途与边界 |
|---|---|---|
| `WM_PAINT` | 当前 Widget 树，经 Skia OpenGL 或软件表面提交 | 正常桌面显示；保留脏区、GPU 提交、呈现和诊断计时 |
| `WM_PRINTCLIENT` | 同一 Widget 树，经独立软件表面完整重绘到调用方 DC | Windows 标准客户区打印；不要求窗口先显示，不依赖 QA 编译开关 |
| `captureFramePng` | 同一客户区绘制函数，经 WIC 编码到文件 | 仅 `ONEUI_ENABLE_TEST_FRAME_CAPTURE=ON` 构建；普通生产构建仍不可调用 |

重绘客户区是应用级绘制结果，不是 DWM 合成器或物理屏幕截图。它不证明屏幕无遮挡、GPU 呈现成功、桌面远程会话有效，也不等价于覆盖任意原生子窗口、WebView 或其他进程的内容。

## 2. Windows 消息规则

`WM_PRINTCLIENT` 的 `wParam` 是调用方提供的 HDC；窗口应完整绘制客户区，不能只画当前无效区。该入口不调用 `BeginPaint` / `EndPaint`，因为 DC 已由调用方提供。`DefWindowProc` 不实现应用自绘客户区，因此必须自行处理。

`WM_PRINT` 继续交给系统默认窗口过程。它按 `PRF_CLIENT` 转发客户区绘制，按其他标记处理非客户区和原生子窗口。OneUI 不重写这些系统职责。直接 `WM_PRINTCLIENT` 支持以下行为：

- 空 DC 安全返回，不分配画面。
- 设置 `PRF_CHECKVISIBLE` 且窗口隐藏时不绘制。
- 未设置该标记时，尚未呈现的隐藏窗口也可以绘制当前内容。
- 使用当前逻辑客户区、物理客户区和 `normalizedDpiScale()`；内容缩放与窗口像素尺寸分开。
- 输出完整的自上而下 BGRA 图像；调用方的 viewport origin、映射和裁剪继续由 GDI 应用，不修改或释放调用方 DC。
- 内容与 tooltip 从实际窗口状态绘制；仅当编译了开发工具且当前启用 overlay 时绘制相同 overlay。

官方依据：[WM_PRINTCLIENT](https://learn.microsoft.com/en-us/windows/win32/gdi/wm-printclient)、[WM_PRINT](https://learn.microsoft.com/en-us/windows/win32/gdi/wm-print)、[PrintWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-printwindow)。

## 3. 模块与依赖方向

`src/platform/win32/window_win32_client_render.inc` 只负责当前客户区的共享绘制及标准 DC 输出：

1. `paintClientContent` 对实际内容设置当前逻辑 frame，执行 Widget 绘制及 tooltip。
2. `renderClientSurface` 创建当前物理客户区大小的 BGRA 软件表面，清屏、应用 DPI/内容缩放，再调用共享内容绘制；是否包含开发 overlay 由调用方明确选择。
3. `printClient` 检查 DC 和可见性选项，以正常 Surface 背景绘制完整客户区，再交给 `StretchDIBits`。

`window_win32_paint.inc` 继续拥有原生呈现、脏区、缓存表面和计时；它调用共享内容绘制，不把 GPU 提交移入打印路径。`window_win32_frame_api.inc` 继续拥有公开窗口 API 与 WIC 文件编码，移除其中重复的离屏布局/内容绘制。消息模块只负责分派。

`clientContentPainting_` 是窗口 UI 线程上的重入标记；作用域守卫负责恢复。内容绘制期间嵌套请求离屏重绘会被拒绝，避免递归进入同一 Widget 树。它不是跨线程锁，不能据此从任意工作线程访问 Widget。外部线程通过正常窗口消息交由窗口线程处理。

## 4. 背景、布局快照与测试截图

普通打印使用 `colors::Surface` 的不透明背景，符合正常整帧清屏。`prepareLayoutSnapshot` 保持透明离屏布局准备，并不包含开发 overlay。测试 PNG 保持透明背景及原 overlay 行为；原有 `ONEUI_TEST_CAPTURE_CLEAR_TOOLTIP` 仅在测试截图编译分支执行，不改变普通打印中的真实 tooltip。

共享函数不切换正常窗口的渲染后端，不把 GPU 诊断报告成软件呈现，也不把临时软件表面当成已提交的 GPU 帧。打印调用的额外分配不计为正常窗口累计 paints；正常 `WM_PAINT` 的记录点继续由原生绘制函数维护。

## 5. 跨进程与本机实际限制

本机普通 Release 配置保持 `ONEUI_ENABLE_TEST_FRAME_CAPTURE=OFF`。同进程直接发送 `WM_PRINTCLIENT`、通过 `WM_PRINT` 转发、隐藏与可见窗口、原点和裁剪、模型变更、遮挡、resize、内容缩放以及重复绘制均已用真实窗口验证。

Windows 11 本机 OpenGL 路径下，`PrintWindow` 返回非零仍可能得到无效画面。消息跟踪曾观察到它触发 `WM_PAINT`，没有进入新客户区打印分支；因此补齐 `WM_PRINTCLIENT` 不能据此宣布修复该 GPU 捕获路径。软件渲染下 `PrintWindow` 取得正确像素，独立子进程的实际客户区也通过校验。

不要把本进程创建的内存 DC 直接通过普通跨进程 `SendMessage` 当作可靠捕获协议。本机诊断中消息成功返回，但目标颜色未写入调用方位图。跨进程验证使用系统 `PrintWindow` 和明确的软件渲染配置。该限制没有通过注入、全局钩子、修改系统捕获/隐私策略或开启生产测试截图来绕过。

子进程验收必须等待“属于本次 PID 的窗口已可见”，不能只等待标题或 HWND 出现。本次首轮跨进程失败来自窗口刚创建、内容尚未显示的竞态；加入可见性条件后，软件 `PrintWindow` 得到预期 `193b5f` 像素，进程正常关闭且退出码为 0。

## 6. 自动化测试与判定

`tests/win32_print_client_tests.cpp` 使用实际 OneUI 窗口、自绘三色内容和 top-down DIB，读取三个相离位置确认颜色、方向及内容身份。它不使用截图模拟数据去替代窗口绘制。

| 用例组 | 断言 |
|---|---|
| 未显示窗口 | 直接打印当前内容；`PRF_CHECKVISIBLE` 保留位图原值 |
| 可见及变更 | 最新模型内容可在下一次原生 paint 前重绘；空 DC 安全 |
| `WM_PRINT` | 系统默认过程转发完整客户区，考虑客户区原点 |
| 调用方 DC | 7×9 viewport origin 保持，裁剪外像素不变 |
| 遮挡 | 第二个窗口使用不同颜色，目标仍输出自身内容 |
| 尺寸及缩放 | 实际 resize 后内容缩放为 2，完整颜色和方向正确 |
| 生命周期 | 连续 64 次绘制的 GDI 对象数没有持续增长 |
| 独立进程 | 软件渲染下跨进程 `PrintWindow` 像素正确；仅关闭本次子进程，正常退出 0 |

普通 Release 的新测试在自动渲染配置下 32 个检查通过，软件配置下 35 个检查通过，二者有重叠，不能相加成 67 个独立功能。自动配置实际记录 backend=2 / OpenGL，软件配置 backend=1；同进程原始 `PrintWindow` 的颜色诊断分别为 false / true。这些诊断如实保留，测试成功不代表 GPU 跨进程捕获成功。

同时通过的既有套件为 developer tools window、Win32 accessibility、Win32 window loop、frame profile、text layout、realtime frame view behavior、interaction C API、C API behavior。与两个新渲染配置合计 10 个 CTest 项通过；这不是全仓库、所有 OS 或所有插件测试已经重新运行。

另复用已有 `build/editor-capture`，保持该目录原有测试 PNG 开关 ON，成功编译共享函数及 WIC 分支，并运行两个打印配置通过。它们属于 QA 构建兼容检查，与普通 Release 分列；本次没有实际调用该 QA PNG API，也没有把 QA DLL 交给产品候选。

## 7. 复现

在已有 MSVC 环境、仓库根目录执行，复用现有固定构建目录：

```powershell
cmake --build build/release --target oneui oneui_win32_print_client_tests --parallel 2
ctest --test-dir build/release -R '^oneui_win32_print_client' -V -j 1
ctest --test-dir build/release -R '^(oneui_win32_window_loop_tests|oneui_developer_tools_window_tests|oneui_win32_accessibility_tests)$' --output-on-failure -j 1
```

完整常规回归名单和构建环境应与消费工程的验证清单一同保留。`build/release/win32-print-client-before.log` 保存原 DLL 的首轮 29 检查 / 19 失败；最终源码的测试项目数和断言组成随后扩充，不能把前后总数机械逐项对应。

## 8. 模块规模与兼容

按含空行、注释的物理行数：窗口装配 cpp 295→297；消息分派 478→481；原生绘制 318→316；frame API 228→194；新增共享绘制 86 行。迁出的既有职责是离屏客户区布局/绘制以及正常内容绘制的重复调用序列，不是只有转发的新空壳。测试文件 268 行，未因生产文件预算压缩断言。

Win32Window 仍由多个 include 片段组成，仍共享窗口状态；本次没有声称整个窗口实现已经解耦。只迁移与当前捕获/重绘直接相关的职责。消费程序必须记录实际装载 DLL 的摘要，不能仅凭源码修复继承旧候选的验收。

## 9. 后续仍需独立验证

默认 GPU 跨进程捕获、DWM/WGC、复杂原生子窗口、打印机设备及其他驱动组合仍需相应环境证据。消费产品的真实窗口、输入、滚动、审查、持久化、取消和退出必须在自己的最终 EXE/DLL 组合上完成；框架三色窗口不替代产品验收。

## 10. 消费应用的后续独立结果

iShell普通Release随后重建，实际消费本次普通DLL。GUI SHA-256为7eb512da2e0213c2a296b8b86b1b012253678981e20e31675f15b7e66dcef804，DLL为13f5d7625111b0790b93c96d045b4a9921977fecac5ae539fa6d29ac4305f692。产品在auto配置、真实可见窗口下通过dark/light两主题的Calendar创建、预览、完整审查与取消、真实Deno、暂停恢复及正常重开，并取得28张有效PrintWindow画面。

独立finish逐张核对28图身份、摘要及尺寸，ship仅覆盖两尺寸的局部原生Calendar界面。输入是目标窗口原生消息，布局DPI为1；实际renderer/device未查询。该消费结果与前文独立C++窗口的OpenGL无效捕获诊断分别成立，不相互覆盖，也不推广为所有GPU、WGC或其他平台通过。

消费侧完整模块、失败历史、复现及剩余分支见ishell_oneui仓库docs/plugins/native-calendar-window.md，原始报告为其target/check-evidence/host-terminal/native-plugin-calendar下的self-2/report.json及supplement/report.json。当前图不使用QA DLL；本节没有新增框架构建或重复执行前述10/2项测试。
