# Linux / macOS 原生后端：实现、构建与验收

这是当前跨平台实现的状态记录，不是全部平台已经支持的发布声明。Windows 保持既有主线；
Linux 与 macOS 属于同一期开发，只有完成各自原生验收后才能提升支持等级。
最新增量见文末：WSL 已恢复读写，Linux GPU 构建和 X11/Wayland 回归通过，
AT-SPI / NSAccessibility 语义桥接及文本、数值接口已接入。2026-10-08 已在 Apple M1 / macOS 15.5
完成构建、50 项 CTest、硬件 Metal 与安装后 SDK 验证；其他架构、系统基线和完整人工桌面验收仍未完成。

下面的构建记录是跨平台阶段基线。后续完整文字依赖与 Unicode 修复的验证记录见
[文字引擎状态](38-text-and-interaction-engine.md)：Unicode Bidi 已通过，MinGW 匹配依赖仍待完成。
旧构建通过不代表当前全部增量已在各平台重新验证。
Win32 GPU/raster 路径见[渲染说明](39-rendering-and-validation.md)，Win7 独立配置见[兼容记录](24-windows7-compatibility.md)。

## 支持矩阵

| 目标 | 已实现 | 已构建 / 自动测试 | 已原生验收 |
| --- | --- | --- | --- |
| Windows x64，MSVC bundled-static | Win32，Skia Ganesh GPU / raster 回退 | 已有本机回归记录；按对应构建核对 | 完整人工输入/多屏/驱动验收仍待完成 |
| Windows，MinGW | 同一 Win32 源码 | 旧基线通过；当前匹配文字依赖阻塞 | 不沿用旧产物宣称当前版本验收 |
| Ubuntu 24.04 x86_64 X11 | Xlib / XIM / RandR / selection / EGL+XImage / GTK / StatusNotifier / XDND | Ubuntu 24.04.4 WSLg 已构建、CTest/Rust 通过 | 待真实 Ubuntu 桌面；WSLg 的 X11 可能由 XWayland 提供 |
| Ubuntu 24.04 x86_64 Wayland | xdg-shell / EGL+shm / xkbcommon / data-device / text-input-v3 / GTK / StatusNotifier | WSLg 原生 Wayland 连接、Gallery、CTest/Rust 已运行 | 待真实桌面与 IME；无交互序号的剪贴板测试明确跳过，不计通过 |
| Ubuntu 24.04 ARM64 | 同一 Linux 源码路径 | 未构建 | 待 ARM64 机器 |
| 银河麒麟桌面 V10 x86_64 / ARM64，默认 X11 | 同一 X11 源码路径 | 未在发行版基线上构建 | 待对应发行版、架构、输入法 |
| 统信桌面 UOS 20 x86_64 / ARM64，默认 X11 | 同一 X11 源码路径 | 未在发行版基线上构建 | 待对应发行版、架构、输入法 |
| macOS Apple Silicon | Cocoa / Metal / CoreText / NSAccessibility | M1 / macOS 15.5 / Xcode 16.4，Release 构建及 50/50 CTest 通过 | 硬件 Metal 自动测试通过；完整人工验收及 macOS 13 基线待完成 |
| macOS 13+ Intel | 同一 Cocoa 源码路径 | 未构建 | 待 Intel Mac |

当前开发环境：Windows x64；WSL Ubuntu 24.04.4 x86_64，WSLg，`DISPLAY=:0`、
`WAYLAND_DISPLAY=wayland-0`，Clang 18。这不是 Linux ARM64、国产发行版的验收证据。
macOS 独立实机记录见文末 2026-10-08 小节。
MSVC bundled-static 的 DLL 通过 product 运行库审计；本轮 MinGW 回归使用已有
`msys2-dynamic` 开发配置，依赖 `libskia.dll`，仅通过 development 审计，不能作为独立发布包。
测试数量以本次构建的 `ctest -N` / Cargo 输出为准，不在概览中维护易漂移的数量。

## 后端选择与能力

### Rust 应用接入增量

`oneui::desktop::open_uri` / `open_path` 提供系统处理器激活：URI 先检查 scheme 语法、长度和控制字符；Unix 使用单个原样参数调用 `/usr/bin/open` 或 `xdg-open`，不拼 shell。应用仍须执行自己的 scheme 和权限策略。

`clipboard_text_with_limit(max_bytes)` 在分配前检查原生所需字节数，并拒绝读取期间增大的结果；`clipboard_text()` 保留接口，默认上限 16 MiB。它们须在平台 UI 线程使用，Wayland 的交互序号约束仍然适用。

Mac 新增可选 C 入口 `oneui_macos_set_open_uri_callback`（`oneui/application_events.h`），不改变现有 ABI 45 结构。AppKit 在首次窗口初始化时安装 URL Apple Event 接收器；未订阅时暂存最多 32 条、每条最多 8192 字节的冷启动事件。Rust `application_events::OpenUriSubscription` 是进程唯一、不可跨线程的主线程订阅；释放时先清原生回调再释放闭包，回调允许释放订阅并隔离 panic。应用同时处理 argv 和订阅事件；打包需在 Info.plist 声明 scheme。旧 SDK 即使 ABI 数值相同也可能缺少这一入口，Mac 消费者必须使用匹配的新构建。

本次增量已做 Linux 原生构建及 Rust 绑定检查；当前变更的 AppKit 编译、冷/热 URL 启动与退订仍需 Mac 实机验证，不沿用旧版 Mac 验收记录。

`ONEUI_LINUX_BACKEND=auto|x11|wayland` 在首次创建连接时读取。`auto` 在有
`WAYLAND_DISPLAY` 时优先连接 Wayland；初始化失败且有 `DISPLAY` 时打印原因并尝试 X11。
显式选择不会静默回退，错误的值直接失败。没有桌面会话不伪造一个空窗口。

```cpp
auto window = oneui::Window::create(L"Native desktop", 640, 480);
window->initialize();
const auto backend = window->backend();
const auto capabilities = window->capabilities();
if (capabilities & oneui::WindowCapabilityPlacement) {
    oneui::WindowPlacement placement;
    window->getWindowPlacement(placement);
}
```

C 对应 `oneui_window_backend`、`oneui_window_capabilities` 与返回成功状态的
`oneui_window_initialize_checked`。Rust 对应 `Window::backend()`、`Window::capabilities()`；
`Window::new` 会检查原生初始化失败。现有 ABI 版本及布局规则见
[C ABI 集成指南](c-abi-integration.md)，未改变已发布 C 结构的字段布局。

| 能力 | Win32 | Cocoa | X11 | Wayland |
| --- | --- | --- | --- | --- |
| 剪贴板 | 系统服务 | NSPasteboard | CLIPBOARD selection / INCR | data-device；写入需有效输入序号 |
| IME | 原有 Windows 路径 | NSTextInputClient | XIM 可用时 | compositor 暴露 text-input-v3 时 |
| 绝对位置恢复 | 是 | 是 | 是 | 不支持，placement 返回失败 |
| 主动激活 / topmost | 是 | 是 | EWMH，受窗口管理器策略约束 | 不声称支持；topmost 初始化失败 |
| 原生文件对话框 | 保留已有实现 | NSOpenPanel/NSSavePanel/NSAlert，未原生验证 | GTK 文件/确认/输入对话框 | GTK；父窗口关联依赖 xdg-foreign-v2 |
| 托盘 / 通知 | 保留已有实现 | NSStatusItem/UserNotifications，未原生验证 | Ayatana StatusNotifier / DBus Notifications | 同 X11；桌面必须提供托盘 host |
| GPU 渲染 | WGL/Skia Ganesh | Metal/CAMetalLayer，M1 硬件自动测试通过 | EGL/OpenGL ES/Skia Ganesh | EGL/wayland-egl/Skia Ganesh |
| 文件拖入 | 原有路径 | NSDraggingDestination，未验证 | XDND v3–v5 / URI-list / INCR | data-device URI-list / 异步有界管道，真实拖入待验证 |
| 内容缩放 / 开发诊断 | 是 | 共享桌面实现 | 共享桌面实现 | 共享桌面实现 |
| 辅助功能桥接（进行中） | 已有 MSAA | NSAccessibility 文本/数值等 AppKit 自动测试通过；VoiceOver 人工验收待完成 | AT-SPI 树/角色/状态/焦点/按钮、文本编辑与数值；D-Bus 集成通过 | 同 X11；不伪造全局屏幕坐标 |

能力位是运行时信息，不是验收标记；Wayland 的剪贴板能力会随输入序号可用性变化。
缺少 IME 协议不等于已经支持中文输入。Wayland `workArea` 暂按输出边界报告，不能推断
面板保留区；旧版协议包缺少 fractional-scale-v1 时只使用输出整数缩放。
Linux 在 GTK 初始化失败、DBus 托盘 host 不存在时返回失败，不把窗口隐藏到不可恢复的托盘。
托盘标题/菜单由宿主桌面呈现；通知成功表示通知服务接受请求，不保证用户实际看到。
X11 当前按会话 Xft.dpi 处理逻辑缩放，混合 DPI 显示器的策略仍需真机验证；
XImage 提交要求 32 位 BGRA-compatible TrueColor 存储，不支持的 visual 明确报错。
Wayland 客户端装饰、Cocoa 全屏/还原与输入法替换范围也必须接受真实窗口系统的验证。

## 实现边界

- `src/platform/shared/` 包含 Skia Canvas、路径、文字测量/字体缓存及桌面调度器；OS GUI 头只在平台实现出现。
- `src/platform/native_services.cpp` 隔离现有 Win32 文件对话框/托盘；`src/capi` 只依赖 OneUI 抽象。
- Cocoa 使用 NSApplication/NSWindow/NSView、CoreText、NSPasteboard；默认尝试 Metal，失败回退 CoreGraphics 栅格显示。
- X11 使用 Xlib、XIM、RandR 和系统光标；默认尝试匹配窗口 visual 的 EGL，失败回退 XImage。
- Wayland 使用 frame callback、客户端标题栏；默认尝试 EGL，失败回退带缓冲释放的共享内存；客户区与标题栏分别处理内容缩放。
- 各窗口拥有独立的 post/动画队列。关闭清空队列，关闭一窗不结束其他窗口；只能通过 post 从工作线程修改 UI。
- Linux 同一连接的窗口和桌面服务使用首次创建连接的 UI 线程；跨线程直接访问会报告错误，不并发调用 Xlib/Wayland。
- Cocoa 窗口必须在进程主线程创建和运行。Rust 安全层测试使用共享的进程主线程 harness，保留全部用例；不以线程不满足 AppKit 要求为由忽略用例。该入口仍须在 Mac 实际运行验证。

文本通过共享 UTF-8 / native-wchar 转换器；Windows `wchar_t` 为 UTF-16，Linux/macOS 为 UTF-32。
Rust `WChar` 随目标平台变化，safe 层不假设两个字节。推荐使用带长度 UTF-8 C 接口。
输入法 preedit 不提交文档，commit 原子更新并保留一次撤销；取消不修改文本。emoji 的光标、
删除已接入 ICU 字素边界，绘制/测量使用共享 SkParagraph；完整一致性与原生 IME 仍待验收。
macOS 编辑快捷键使用 Command，原始事件的 Control/Meta 保留物理含义。

Wayland 状态提交/取消及候选窗定位遵循
[text-input-v3 协议](https://chromium.googlesource.com/external/anongit.freedesktop.org/git/wayland/wayland-protocols/+/refs/heads/master/unstable/text-input/text-input-unstable-v3.xml)。
剪贴板发送是有期限的非阻塞传输；接收端关闭管道不能通过 SIGPIPE 终止宿主，也不改变进程全局信号策略。

## Linux 源码构建

每种发行版/架构都在自己的基线上构建。下面仅适用于 Ubuntu 24.04；麒麟/UOS 的软件包名、
工具链和最低 libc 尚需在原生系统核实，不能把 Ubuntu `.so` 直接当成兼容产物。

```sh
sudo apt-get install cmake ninja-build clang-18 llvm-18 python3 pkg-config \
  libx11-dev libxrandr-dev libwayland-dev wayland-protocols libxkbcommon-dev \
  libfontconfig1-dev libfreetype-dev fonts-noto-cjk \
  libgtk-3-dev libayatana-appindicator3-dev libatspi2.0-dev at-spi2-core \
  libegl1-mesa-dev libgles2-mesa-dev
# 按 third_party/README.md 获取 Skia，并同步其 DEPS。
export PATH=/usr/lib/llvm-18/bin:$PATH
bash scripts/build-skia-posix.sh
cmake -S . -B build/native -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DONEUI_SKIA_MODE=bundled-static -DCMAKE_CXX_COMPILER=clang++
cmake --build build/native --parallel 6
ONEUI_LINUX_BACKEND=x11 ctest --test-dir build/native --output-on-failure
ONEUI_LINUX_BACKEND=wayland ctest --test-dir build/native --output-on-failure
ONEUI_LINUX_BACKEND=wayland build/native/examples/gallery/oneui_gallery
```

固定 Skia revision：`1f26101197bff9fcd939a791beb3094297436d59`。
脚本拒绝不匹配的源码版本，GN 使用该 checkout 的 fetch-gn 固定版本。
`ONEUI_GN` 可指向已下载的同版本 GN；`ONEUI_BUNDLED_SKIA_OUT` 可覆盖输出目录。
Skia 脚本默认 `ONEUI_SKIA_GPU=1`，Linux 启用 Ganesh/EGL，macOS 启用 Ganesh/Metal。
CPU 专用开发构建需同时使用 `ONEUI_SKIA_GPU=0` 与 CMake `-DONEUI_ENABLE_GPU=OFF`；旧 CPU Skia 不能链接新的默认 GPU 构建。
Skia 构建与 SDK 打包脚本会先检查输出目录、临时目录至少有 4 GiB 可用空间。
WSL 还会读取当前发行版的 Windows 注册路径，检查承载 VHD 的实际宿主卷；
不能用 Linux `df` 显示的虚拟剩余容量代替此项检查。宿主查询失败时停止执行。
普通 CMake/Cargo 构建前也应运行 `python3 scripts/check-build-space.py <构建输出目录> /tmp`；
此检查只确认启动时余量，不预留空间，也不能代替长时间构建期间的容量监控。
运行时 `ONEUI_RENDERER=software` 或 `ONEUI_ENABLE_GPU=0` 可强制栅格路径。
`rendererInfo()` 返回实际 API、设备与回退原因；llvmpipe/softpipe/SwiftShader 报告 `software-opengl-driver`，不作为硬件加速验收。
当前 Linux 已验证 Clang/LLVM 18；macOS 匹配的 Apple Clang/Xcode 版本尚待原生构建锁定。

macOS 使用同一 Skia 脚本及 CMake 路径，需要 Xcode Command Line Tools、Ninja/Python3；
分别使用原生 arm64 / x86_64 工具链，部署目标至少 13.0。CMake 启用 Objective-C++/ARC，
链接 Cocoa/CoreText/CoreGraphics，并生成开发用 `oneui_gallery.app`。此流程尚未在 Mac 运行。

## SDK 与静态开发产物

```sh
bash scripts/package-sdk-posix.sh build/native dist/native
# 使用上一步输出的实际 SDK 路径：
cmake -S tests/sdk_consumer -B build/consumer -DOneUI_DIR=/path/to/sdk/cmake
cmake --build build/consumer
ctest --test-dir build/consumer --output-on-failure
```

SDK 含 `.so` / `.dylib`、公开头、`OneUI::oneui`、Gallery、Rust 源码及文档；
排除静态 Skia、node_modules、Nuxt 构建输出和截图/缓存。打包记录真实发行版、架构、会话、
源码 revision/dirty 状态与工具链缓存。`native_acceptance=pending` 不会由构建成功自动改为通过。
脚本拒绝覆盖已有 archive，保留 staging 用于消费者检查。

Linux 使用 `$ORIGIN`，macOS 使用 `@rpath` / `@loader_path`；开发 `.app` 放入自己的
Frameworks 目录，未签名/公证。常规 SDK 只验证动态消费；静态消费使用源码构建的
`OneUI::oneui_static` 及匹配 Skia 依赖，不能把大型静态库混入普通 SDK。

## 必须补齐的正式验收

1. 每个目标的系统版本、架构、桌面会话、窗口管理器、输入法和工具链记录。
2. 中文组合/提交/取消、候选窗位置、emoji、剪贴板跨进程、大文本及快捷键。
3. 焦点切换、窗口拖动/缩放、多屏 DPI、反复创建销毁、关闭后的回调取消。
4. Gallery 全页交互；VirtualList 更新保持滚动/选择，leading、footer、StatusStrip 与字体回退。
5. 空闲 CPU、闪烁、缓冲/资源泄漏；C/C++/Rust 动态及静态消费者；SDK 依赖/导出符号。

自动测试、交叉编译、WSL 只能填写对应列。Mac、国产系统、ARM64 缺失机器是正式验收条件；
未满足前本期不标记为全部完成。不购买机器、不修改下游仓库、不强推。

## 2026-10-05：无 Mac 环境的代码补漏与 Linux 回归

本轮仍没有 Mac SDK/机器，未编译 Objective-C++、未运行 AppKit 测试，也未提升 macOS 支持等级。
已修复的源码与配置问题：

- Cocoa 修饰键事件不再读取仅适用于普通键盘事件的 `isARepeat`/字符属性；Fn 不再误报 Command。
- 区分输入法 `unmarkText`（接受预编辑）与取消/失焦（丢弃预编辑），并防止旧输入会话提交到新控件。
- Cocoa 的 UTF-16 范围统一转换到 native wchar 索引，保护 emoji 代理对、空范围、越界长度与整数溢出；
  查询不到文本时返回 `nil` 与 `NSNotFound`。右键/中键拖动补齐事件转发，标题/全屏/边框/placement 补线程检查。
- macOS 最低部署版本在 `project()` 与目标创建前设为 13.0，显式更高版本和环境变量保留，低版本明确拒绝；
  CMake/Rust 静态消费者补齐固定 Skia 的 AppKit/ApplicationServices 框架依赖。
- Linux/macOS 动态与静态目标通过链接 `oneui_core` 传递 Yoga 等依赖。
  Linux 仅隐藏第三方归档符号，避免 `--exclude-libs,ALL` 误隐藏 `enumerateSystemFonts()` 等公共接口。
- 共享 Skia 矩形裁剪采用非抗锯齿像素裁剪，消除小数缩放下重复局部重绘的边缘混色。
- 修复测试中的固定 Windows 字体坐标与可选窗口居中能力假设；补齐已有 C/Rust 声明遗漏的四个 ABI 清单条目。

行为依据：[AppKit isARepeat](https://developer.apple.com/documentation/appkit/nsevent/isarepeat)、
[unmarkText](https://developer.apple.com/documentation/appkit/nstextinputclient/unmarktext())、
[CMake 对象库依赖传播](https://cmake.org/cmake/help/latest/command/target_link_libraries.html#linking-object-libraries)。

本轮执行记录（工作区版本，Ubuntu 24.04 x86_64 WSLg、Clang 18、Release、`ONEUI_ENABLE_YOGA=ON`）：

| 检查 | 结果 |
| --- | --- |
| 全量 CMake 构建，含动态/静态库、Gallery、测试 | 通过 |
| `ONEUI_LINUX_BACKEND=x11` CTest | 48/48 通过 |
| `ONEUI_LINUX_BACKEND=wayland` CTest | 47 通过，1 剪贴板用例因缺少交互序号跳过；包含显式选择 X11 的独立测试 |
| `oneui_utf16_range_tests` | Windows MinGW 16 位 wchar、Linux 32 位 wchar 均通过 |
| `oneui_macos_configuration_tests` | 部署版本默认值、环境/显式覆盖、低版本/空值拒绝、Linux 隔离均通过；仅测试配置逻辑 |
| 动态导出核对 | 字体枚举 C++ 接口已导出；抽查 ICU/HarfBuzz/Yoga 私有入口未导出 |
| `oneui_abi_sync` | 通过；本轮未重跑 Cargo 套件 |
| 新增 `oneui_macos_backend_tests` | 已注册到 Apple CTest；未编译、未运行 |

AppKit 专用测试覆盖修饰键、预编辑提交/撤销、失焦取消、编辑器切换、UTF-16 替换/查询与错误线程调用。
未来在 Mac 的正常构建后运行 `ctest --test-dir build/native -R 'macos|utf16|desktop_input|backend_contract' --output-on-failure`。
该自动测试也不能代替中文输入法、候选窗、多屏、全屏/最大化恢复与真实鼠标操作验收。
以上为较早的 CPU 基线；后续对话框、托盘和 GPU 增量见下节。

## 2026-10-05：原生服务、GPU 与发布准备增量

本轮实现文件/文件夹选择、多选、保存、确认/密码输入、窗口托盘与旧 C 托盘句柄，
补齐关闭策略、托盘菜单回调、通知、双击/减少动画偏好。Linux 原生服务进入既有事件循环，
模态期间继续处理 post/绘制，屏蔽父窗口输入；关闭 owner 后取消对话框与托盘回调。
保存对话框在追加默认扩展名后检查实际目标，确认覆盖的文件与返回的路径一致。

GPU 直接使用 Skia Ganesh 绘制原生后缓冲，并处理尺寸变化、独立窗口资源与失败回退；
不是把 CPU 位图上传后称为 GPU 绘制。当前每次 GPU 提交完整重绘，局部重绘性能优化尚待测量。
Linux/macOS 还增加内容缩放、渲染/缓存诊断、开发诊断与报告导出，以及平台文件拖入实现。
Rust 托盘结构和 tooltip 参数改为平台 `WChar`，消除 Linux/macOS 的 u16/u32 编译错误。
SDK 安装附带所嵌入的固定 Skia/文字/图像/Yoga 依赖许可证，打包记录 GPU/Yoga 配置。

本次实测环境仍为 Ubuntu 24.04 x86_64 WSLg、Clang 18、Release、Yoga ON：

| 检查 | 结果与边界 |
| --- | --- |
| GPU-enabled Skia 与完整 OneUI 动态/静态构建 | 通过 |
| X11 CTest | 53/53 通过 |
| Wayland CTest | 52 通过，1 项剪贴板因缺少用户输入序号跳过 |
| 新增 Linux 原生服务测试 | 真实 GTK 弹窗；Unicode/密码/取消/单选多选/缓冲长度；隔离 DBus peer 验证注册、通知与关闭策略 |
| 新增渲染测试 | X11/Wayland EGL 与 raster 实际呈现、多窗口资源、缩放/缓存/开发诊断；设备为 llvmpipe，未验证真实 GPU |
| 文件拖入测试 | URI-list 校验与 X11 XDND 普通/INCR 传输通过；不代表 Wayland/macOS 真实拖入已验收 |
| Rust 动态与静态，X11 | 各 101 个主线程用例、1 个字体集成用例、13 个 ABI 用例通过 |
| Rust 动态，Wayland | 同上；仍须按桌面交互限制验收 |
| 安装后的 C/C++ 动态 SDK 消费者与运行库路径检查 | 通过，未发布产物 |
| macOS 新原生服务/Metal/拖入与测试源码 | 未编译、未运行；不能提升支持等级 |

Wayland 并行回归曾出现一次连接中断；单独复现与后续全量重跑均通过，原因尚未确定，
稳定性验收应保留此记录。GTK/StatusNotifier 测试不代替 GNOME/KDE 桌面面板的可见性/点击验收。

**发布状态仍为未就绪。** 尚缺 Mac 两架构构建、Metal/输入法/多屏/签名公证；
Linux 真实 GPU、GNOME/KDE、目标发行版和 ARM64 验收；Wayland 激活与桌面策略适配；
Linux AT-SPI/macOS NSAccessibility 的控件语义桥接也尚未补齐。
Wayland 绝对定位、任意置顶与窗口透明度不能按 Win32 的系统权限假设实现，必须按 compositor 协议报告能力。
不得把这些项目或未运行的测试写成“全部功能已支持”。

最后一轮环境故障：宿主 C 盘耗尽，Ubuntu WSL 报磁盘 I/O 错误，ext4 进入只读状态，
新 Linux 进程已不能正常启动。已停止继续构建，未重启 WSL、未删除其他应用数据。
上表依据故障前已返回的测试输出；WSL 日志目前也因 I/O 错误无法读取或备份。
工作区 `out/native-release-review/STATUS.txt` 仅保存结果摘要，不是原始日志备份。
最后补入的“内容缩放同步最小客户区尺寸”尚未重新编译/回归，恢复环境后必须复验。
E 盘源码仍可访问；WSL 内的临时构建产物不能作为可靠发布输入，环境恢复后应重新生成。

## 2026-10-05：释放宿主空间后的代码复核

按用户要求删除三个麒麟 ISO 后，C 盘释放约 16.57 GiB。Ubuntu 仍处于 I/O/只读故障状态；
单独终止并启动 Ubuntu 未恢复，启动返回 `E_UNEXPECTED` / `E_FAIL`。
整个 WSL 的重启会中断 Docker，尚待用户确认；本节没有新的 Linux 原生运行通过记录。

- 桌面共享输入入口补齐模态期间的 IME 范围替换与预编辑拦截，同时允许取消已有预编辑；
  关闭窗口后也拒绝迟到的键盘、文本和 IME 回调。新增嵌套模态回归用例。
- Cocoa sheet 与应用模态对话框统一清理计时器、sheet 和输入抑制；回调异常时恢复状态，
  回调同时确认对话框并销毁 owner 时返回取消。新增 AppKit 生命周期与缩放约束用例，未在 Mac 编译运行。
- Cocoa 激活路径补最小化还原；模态事件泵补 IME 会话同步，避免保留已切换编辑器的预编辑状态。
- EGL 窗口明确使用 framebuffer 0，避免将滤镜/图层留下的离屏 framebuffer 当成窗口后缓冲。
- 新增构建空间预检与 8 个回归用例，覆盖 WSL 虚拟容量充足但宿主盘耗尽、不同宿主盘、
  查询失败和不存在的输出目录。8/8 通过；Windows 实际卷查询及低空间拒绝行为通过。

本次另通过 macOS CMake 部署配置测试、C/Rust ABI 清单核对，以及新增共享输入测试的
Windows C++17 语法检查。上次最小客户区缩放修复和本节原生代码仍需恢复 WSL 后重新编译、
跑 X11/Wayland 回归；macOS 仍待原生环境。发布状态保持未就绪。

## 2026-10-05：WSL 恢复后的重新构建与回归

用户授权后执行整个 WSL 的重启，Ubuntu 根文件系统恢复 `rw`，读写探针与 WSLg 连接正常。
重启后原 `/tmp` 构建目录已清空；本次从固定源码重新构建 GPU Skia 和 OneUI Release，
产物位于 `/home/hkoos/.cache/oneui-release-review`，Yoga ON、Clang 18、Ubuntu 24.04 x86_64。
结束检查未发现本次启动后的 ext4/I/O 错误，C 盘仍有约 16 GiB 余量。

| 本次实际检查 | 结果 |
| --- | --- |
| GPU Skia、OneUI 动态/静态库、Gallery 和测试全量构建 | 通过 |
| X11 CTest | 55/55 通过 |
| Wayland CTest | 54 通过，1 项剪贴板因缺少输入序号跳过；包含显式 X11 协议用例 |
| EGL 离屏目标回归 | 读取真实窗口后缓冲验证第二帧像素；通过 |
| EGL 修复前负向对照 | 在独立目录还原旧 framebuffer 选择代码，同一测试按预期失败；未改动工作区源码 |
| X11 最小客户区缩放 | 实际读取 WM_NORMAL_HINTS 验证约束随内容缩放更新；GPU/软件渲染均通过 |
| 嵌套原生模态输入 | IME 提交/范围替换/预编辑拦截及取消恢复用例通过 |
| Rust 动态和静态 × X11 和 Wayland | 四种组合均为 101 主线程用例、1 字体用例、13 ABI 用例通过 |
| 安装后 C/C++ SDK 消费者 × X11 和 Wayland | 两种后端各 2/2 通过；运行库依赖/路径审计通过 |
| Linux SDK 打包 | 通过；包内动态库与已验证消费者使用的安装库一致；保留 `native_acceptance=pending` |
| 显式 GPU OFF 构建 | 成功；X11/Wayland 的软件渲染与输入用例各 2/2 通过 |
| 构建空间预检 | Linux 8/8 通过，实际 WSL 宿主卷查询通过 |

新测试 `oneui_egl_framebuffer_tests` 保留上一帧的 Skia 离屏目标，确保下一帧确实绘制到
EGL 窗口 framebuffer 0。此回归在旧实现上可复现，并非仅验证初始化或计数器变化。
渲染设备仍是 **llvmpipe (LLVM 20.1.2)**，没有真实硬件 GPU 验收。回归串行运行，
没有因此消除前文记录的 Wayland 并行连接中断风险。macOS 本轮仍未编译或运行。

本次原始构建/回归日志已直接保存在工作区 `out/native-release-review/recovery-*.log`，
不再只依赖 WSL 临时目录。之前未复验的最小客户区缩放修改已纳入本次 Linux 构建和回归。
**发布状态仍为未就绪**：前文的 Mac、真实 GPU/桌面、目标发行版/ARM64、可访问性桥接等项目仍待完成。

## 2026-10-06：基础辅助功能桥接

新增共享语义树，使用控件所有权身份保持节点 ID，重排/临时模态不改变身份，移除或销毁后拒绝旧引用的操作。
仅向系统公开可见语义控件；继承父控件禁用状态；焦点落到实际叶节点；密码字段即使显式设置了
`accessibleValue` 也不导出明文。OneUI 焦点捕获层和嵌套原生模态都隔离背景树，操作转发前后重新验证成员身份。

Linux 通过已有 GLib 事件泵异步连接 AT-SPI 总线，在真实 registry 注册应用与窗口。
实现 Accessible、Application、Component 和可操作控件的 Action 接口，发送名称/角色/状态/焦点及层级变更通知。
控件重排通知保留身份；按钮回调异常转换成 D-Bus 错误；按钮关闭最后一个窗口时，异步送完回复再释放连接。
尊重 `NO_AT_BRIDGE=1`，无总线时不阻塞窗口启动。X11 返回实际屏幕坐标；Wayland 仅提供可确定的窗口相对坐标，
全局位置不可用时返回无效坐标。构建增加 `atspi-2` 协议常量头依赖；库本身不直接链接 libatspi。
隔离总线测试需要 `dbus-run-session` 与 `at-spi2-core` 提供的真实 registry，缺失服务时测试失败而非伪造通过。

macOS 新增 NSAccessibilityElement 代理、原生视图的焦点/命中查询和变化通知；屏幕边界采用 Cocoa point，
保留内容缩放，避免额外乘 backingScale。支持基础名称/角色/值/状态、聚焦/按钮操作与文本整体替换；
只读、密码和失效代理有防护。新增 AppKit 专用测试，但当前没有 Mac SDK/机器，**本节 Mac 代码未编译、未运行**。

本节完成时的 Linux 自动测试验证基础协议，不等于完整读屏验收。当时尚未提供 AT-SPI Text/EditableText/Value/Selection/Table
专用接口、复杂控件的虚拟子项、跨平台自绘标题栏按钮语义、完整文本范围/光标朗读与滚动可见性语义；
还需要 Orca/VoiceOver、真实桌面及目标发行版验收。文本/数值后续增量见下节，不能把此桥接写成“无障碍功能完整支持”或“可以上线”。

| 本轮验证（Ubuntu 24.04 x86_64 WSLg，Clang 18，Yoga ON） | 结果 |
| --- | --- |
| GPU 配置动态/静态库、示例与测试构建 | 通过；仍为 llvmpipe，非硬件 GPU 验收 |
| X11 全量 CTest | 57/57 通过 |
| Wayland 全量 CTest | 56 通过，1 项剪贴板因无输入序号跳过 |
| 真实 AT-SPI registry / D-Bus 集成 | 两种后端通过；覆盖发现、Unicode、状态、密码、操作、模态、重排通知、异常与自销毁回复 |
| 共享语义树与生命周期 | 两种后端通过；覆盖父级禁用、密码路由、只读、稳定身份、OneUI/原生模态和失效引用 |
| 显式 GPU OFF 构建 | 通过；X11/Wayland 的两项辅助功能测试及软件渲染测试各 3/3 通过 |
| macOS 桥接与新增 AppKit 用例 | 已纳入 Apple 构建；未编译、未运行 |

上一节 SDK 压缩包没有重新生成，不包含本节新增实现，不应作为当前源码的发布产物。

实现依据：[AT-SPI D-Bus 接口](https://github.com/GNOME/at-spi2-core/tree/main/xml)、
[AppKit NSAccessibilityElement](https://developer.apple.com/documentation/appkit/nsaccessibilityelement-swift.class?language=objc)。
本轮构建、X11/Wayland 全量与专项回归日志保存在 `out/native-release-review/accessibility-*.log`。

## 2026-10-06：文本编辑与数值辅助功能

Linux 在实际 TextField 上增加 AT-SPI Text/EditableText 接口：按 Unicode 字符读取文本、查询字符数/光标/
单选区、按词/句/实际排版行获取文本，使用与绘制相同的文字布局返回字符/范围边界与命中偏移。
修改范围、整体替换、复制/剪切/粘贴走真实编辑器和剪贴板抽象，保留撤销；发送文本、光标与选区变化通知。
普通文本读取使用字符偏移，`InsertText` 的长度遵守协议中的 **UTF-8 字节数**；不完整的 UTF-8 截断返回失败。
协议依据：[AT-SPI EditableText](https://github.com/GNOME/at-spi2-core/blob/main/xml/EditableText.xml)。

密码 TextField 不暴露 Text/EditableText 接口，切换密码模式后旧接口引用也不能读取/复制文字；
只读编辑被拒绝，合法选区查询仍可使用。IME 预编辑期间拒绝范围操作，不取消用户正在组合的文本。
聚焦回调更换文档时拒绝基于旧文本的修改，避免将旧偏移应用到新文档。

Slider 的 Value 接口返回实际范围、步长和数值，拒绝禁用、只读、非有限数值与越界写入；
确定进度的 ProgressBar 提供只读数值，不确定进度不伪造百分比。辅助功能数值输入和键盘输入共用
Begin/Update/Commit 交互回调；应用回调抛异常后释放交互状态，避免永久卡在“拖动中”。
D-Bus Properties.Set 回调关闭最后一窗时，保持连接直到回复送出，与普通 Action 的生命周期分别测试。

macOS 对应新增 UTF-16 字符数/范围/选区、文本替换、排版行与屏幕命中查询，以及数值上下限、写入和增减动作。
Cocoa 文本屏幕坐标转换处理翻转视图和内容缩放。只对具有相应能力的控件开放文本/数值 selector。
补入 AppKit 专用 Unicode、密码、只读、几何、步长、进度与自销毁用例；**无 Mac SDK/机器，尚未编译或运行**。

| 本轮实际验证（Ubuntu 24.04 x86_64 WSLg，Clang 18，Yoga ON） | 结果 |
| --- | --- |
| GPU 配置动态/静态库、示例与测试构建 | 通过，渲染设备仍为 llvmpipe |
| X11 全量 CTest | 58/58 通过 |
| Wayland 全量 CTest | 57 通过，1 项剪贴板因缺少输入序号跳过 |
| 新增协议与交互专项 | 两后端均通过；独立 D-Bus 客户端验证文本/选区/几何、剪贴板抽象、撤销、密码/只读、数值和关闭回复 |
| GPU OFF 构建及专项 | 构建通过；X11/Wayland 的辅助功能、滑块交互及软件渲染各 5/5 通过 |
| macOS 对应源码与测试 | 已纳入 Apple 构建，未编译、未运行 |

当前 Text 接口面向 TextField；复杂控件虚拟子项、Selection/Table、细粒度样式属性、按屏幕矩形检索文本、
跨滚动容器定位文字及自绘标题栏语义仍待补齐。`GetBoundedRanges` 明确返回错误，滚动请求返回 false，
不伪造结果；Wayland 的全局文本坐标不可用也不会伪造。还需 Orca/VoiceOver、真实 GPU/桌面、国产发行版/
ARM64 和 Mac 签名公证等验收，**发布状态仍未就绪**。此前 SDK 包未重新生成，不包含本轮实现。
原始构建/回归日志：`out/native-release-review/text-a11y-*.log`。


## 2026-10-08：Apple Silicon 实机构建与自动验收

通过 SSH 在 Apple M1（arm64，16 GiB）、macOS 15.5、Xcode 16.4 / Apple Clang 17 上验证。
CMake 3.31.6 与 Ninja 使用隔离工具环境，复用固定构建目录；没有升级系统 Homebrew 包。
源码为 Windows 工作树快照，远端 `.oneui-source-snapshot.json` 记录实际源文件 SHA-256，
不是仅检出 HEAD，因此测试结论不自动覆盖之后的并行修改。

修复了 CATransaction 缺少头文件、Apple libc++ 缺少浮点 `from_chars`、桌面目标内部头文件路径，
以及 Yoga 禁用 RTTI 导致 macOS 异常类型匹配失败的问题。CoreText 系统字体发现保持原有路径；
对 CoreText 无法加载的应用字体启用 FreeType 回退，SDK 同时携带 FreeType 许可证。
剪贴板测试使用 macOS Command 快捷键；行高测试按实际字体度量验证行数上限；
Release 渲染测试明确验证开发诊断关闭；修正模态输入测试把 placeholder 当作正文的夹具错误。

| 本轮实际验证 | 结果 |
| --- | --- |
| 固定 Skia + Ganesh Metal + paragraph / shaper / Unicode | 构建通过 |
| Release，GPU ON，Yoga ON | 动态库、静态库、Gallery、远程 Gallery 和全部测试目标构建通过 |
| 完整 CTest（串行） | **50/50 通过，无跳过** |
| 硬件与软件渲染 | 实际 Apple M1 Metal 设备及 raster 路径通过 |
| AppKit / C API | 输入、模态生命周期、文本/数值辅助功能、动态/静态异常处理测试通过 |
| 安装后 SDK 运行库审计 | 通过；没有 Homebrew 或用户构建目录动态库依赖 |
| 独立 SDK 使用方 | `find_package(OneUI CONFIG REQUIRED)` 编译、链接、运行通过，返回版本 0.1.0、UTF-8 ABI 40 |

本轮未验证 Intel、macOS 13 实际运行、Rust 绑定、完整中文输入法交互、VoiceOver、托盘/通知、
真实文件拖入、多屏切换、休眠恢复、签名/公证及安装包分发。AppKit 自动化模态测试不等于所有
文件面板已经人工验收。Linux 仍需真实硬件 GPU、目标发行版及 ARM64 验证；**发布状态仍未就绪**。
本轮没有生成正式发行包，旧 Linux SDK 包不包含本轮更改。

当前 Mac 构建与验证日志保存在 `out/native-release-review/current.log`；此前 Linux 关键验收记录
合并保存在 `out/native-release-review/previous-validation.log`，上文按轮次命名的旧日志已按产物规则收敛。


## 2026-10-08：Mac 性能实验台首轮基线与修复

本轮在同一 Apple M1 / macOS 15.5 / 60 Hz 内屏上比较，1050×720 逻辑尺寸、2× 缩放，
实际 2100×1440 像素。每个场景预热 3 秒、采样 5 秒，标准/高压各正反顺序两轮；
表中为两轮均值。CPU 按全部 8 个逻辑核心归一化，不等于单核口径，也不是功耗测量。
后台应用及温度未完全控制，短测不足以证明小幅差异的统计显著性。

| 负载 / 场景 | CPU 前 → 后 | 回调 Hz 前 → 后 | P95 帧间隔 ms 前 → 后 |
| --- | --- | --- | --- |
| 标准 / 静止连接页 | 5.540% → 0.025% | 60.00 → 0.00 | 20.01 → 0.00 |
| 标准 / 表格滚动 | 5.868% → 4.275% | 59.99 → 59.99 | 22.67 → 19.19 |
| 标准 / 图表 | 6.095% → 6.451% | 59.97 → 60.00 | 20.96 → 17.63 |
| 标准 / 粒子 | 5.975% → 6.627% | 60.01 → 59.96 | 19.40 → 18.49 |
| 高压 / 静止连接页 | 5.618% → 0.026% | 60.00 → 0.00 | 19.93 → 0.00 |
| 高压 / 表格滚动 | 5.794% → 4.786% | 59.99 → 59.98 | 21.58 → 23.59 |
| 高压 / 图表 | 5.962% → 6.280% | 59.99 → 60.00 | 18.67 → 19.20 |
| 高压 / 粒子 | 5.989% → 6.471% | 60.00 → 60.00 | 19.64 → 18.01 |

静止页采样期由每轮约 300 次绘制降为 **0 次**，因此其 0 Hz 代表没有无用绘制，不能当作掉帧。
动画场景保持约 60 Hz。列表进程 CPU 降低；图表和粒子没有表现出明确的 CPU 改善，部分短测样本
高于基线。每帧 CPU 墙钟耗时也没有全面下降，不能把本轮写成“所有场景更快”或“性能已对齐”。

保留的修复：指标字体改用持久覆盖属性，消除每帧在默认字体和等宽字体之间切换导致的重绘循环；
共享桌面调度提供剩余等待时间，Mac 不再在每轮工作后额外等待完整 16 ms，并读取显示器刷新周期；
Metal 仅在尺寸/缩放变化时更新 layer 属性。新增的 P95/P99 和物理尺寸字段用于后续同条件对比。

共享 circle mesh 桥接已支持 Mac 的显式实验构建，18 组缩放、透明叠加、多批次和裁剪测试与参考
结果逐像素相同。Metal 小数设备像素裁剪存在差异时先整体回退，没有部分绘制后再叠加回退的问题。
由于当前短测未证明稳定 CPU 收益，最终 Demo 的 `ONEUI_LAB_MESH_AVAILABLE` 仍为 OFF；
SDK 的 `ONEUI_ENABLE_CIRCLE_MESH_EXPERIMENT` 默认也为 OFF，本轮测试库显式开启以运行验证。
全局 MSAA 试验虽降低曲线路径 CPU 绘制耗时，但粒子成本和 mesh 画质一致性不达标，未保留。
CPU 路径回退热点尚未消除，仍需后续优化，不能声称已全部转到 GPU。

验证：Mac 全量 **51/51 CTest 通过**，包括新增 Metal 图形读回、连续帧和尺寸变化测试；Demo
遥测/字体传播自检通过；Windows MSVC 对当前 Demo 完成语法检查（存在既有转换警告），
未在本轮重新完成 Windows/Linux 的运行基准。高刷新率、多屏切换、最小化/休眠的完整验收仍待完成。
新增像素测试比较的是 GPU 表面读回，不是屏幕呈现延迟或 GPU 时间戳。

原始汇总及二进制哈希：`out/native-release-review/performance/{baseline,candidate}.csv` 与相应
`*-environment.json`；逐轮工作目录在成功后清理，只保留两份汇总。


## 2026-10-08：Mac 活跃场景采样与小路径缓存

继续在同一 M1 / Metal / 60 Hz / 1050×720 / 2× 缩放下检查图表和粒子。
本轮每个场景预热 3 秒、采样 8 秒，标准/高压正反顺序各一次；进程 CPU 仍按 8 个逻辑核心归一化。
两轮短测、未隔离后台负载，不据此宣称小幅 CPU 差异有统计显著性。

| 负载 / 场景 | 整机 CPU 前 → 后 | 每帧绘制墙钟 ms 前 → 后 | 回调 Hz 后 |
| --- | --- | --- | --- |
| 标准 / 图表 | 6.282% → 6.340% | 6.695 → 6.897 | 60.00 |
| 标准 / 粒子 | 6.494% → 6.580% | 6.453 → 5.943 | 60.00 |
| 高压 / 图表 | 6.291% → 6.225% | 7.183 → 7.025 | 60.00 |
| 高压 / 粒子 | 6.394% → 6.211% | 7.410 → 7.088 | 59.99 |

保留共享适配器的小路径身份缓存：每线程最多 16 个路径，各不超过 512 点及 512 命令，
用完整几何相等判断，避免按 CanvasPath 地址缓存导致直接修改命令后出现旧画面。
大路径不参与，LRU 淘汰不修改调用者/已提交绘制持有的路径。该改动帮助静态路径复用
Skia 缓存，但当前总 CPU 未表现出一致、显著的改善；每帧墙钟改善也不等于 GPU 时间或功耗下降。

采样确认图表热点包含 `SoftwarePathRenderer::onDrawPath`、抗锯齿填充和描边展开；
粒子主要热点为逐个圆形的 Ganesh 绘制指令处理。窗口实际后端仍为 Metal。
尝试局部 MSAA 后图表主线程绘制更短，但进程总 CPU 未明显下降、GPU 缓存上升，试验已撤回；
mesh 顶点写入改法也未保留。没有减少点数、粒子数或动画刷新率来降低负载。

私有 mesh 对小数设备坐标裁剪增加安全内区例外：整批几何（含 AA 边缘及浮点余量）
完全处于裁剪框内时允许绘制；触边时仍在任何提交前整体回退。27 组整数/小数裁剪、
内区/触边、缩放、透明叠加和数量用例逐像素相同。默认 Demo mesh 仍关闭，未推广到正式 SDK 默认路径。

验证：Mac Release 全量 **52/52 CTest 通过**、Demo 遥测自检通过；Windows MSVC 对
共享路径实现及新增缓存测试完成语法检查。本轮未重新运行 Windows/Linux 性能基准。
原始对比覆盖固定目录中的 baseline/candidate CSV 和二进制哈希；前一轮汇总继续保留于上节。
发布性能验收仍未完成，重点遗留为动态图表 CPU 栅格化回退及大量粒子的每帧提交成本。


## 统一托盘与应用菜单（ABI 41）

业务层用 `NativeMenuItem` 同时描述托盘菜单与应用菜单栏：`children` 表示子菜单，
`MenuCheck::Check/Radio`、`checked` 和 `radioGroup` 表示勾选、单选状态，
`KeyChord` 表示快捷键。`KeyModifierPrimary` 在 Mac 是 Command，在 Windows/Linux 是 Ctrl。

| 能力 | Windows | macOS | Linux |
| --- | --- | --- | --- |
| 托盘菜单 | Win32 原生菜单 | NSStatusItem / NSMenu | AppIndicator / DBusMenu |
| 应用菜单栏 | OneUI 窗口客户区 | 系统顶部 NSMenu，随窗口激活切换 | OneUI 窗口客户区 |
| 子菜单、禁用、分隔、勾选、单选 | 支持 | 支持 | 支持 |
| 自定义 RGBA 图标 | HICON | NSImage，支持 template hint | 临时 PNG，托盘销毁时自动回收 |
| 原生悬浮提示 | 支持 | 支持 | 只设置 indicator title，桌面环境不保证显示 tooltip |

使用 `WindowCapabilityMenuBar` 和 `WindowCapabilityTrayTooltip` 区分菜单栏能力与
原生提示能力；Linux 托盘仍须桌面运行 StatusNotifierHost，缺少宿主时启用托盘返回失败。
窗口内菜单支持 F10、方向键、Enter、Escape、滚轮及长菜单滚动；应用模态边界阻止后台菜单命令。

```cpp
oneui::NativeMenuItem save{100, L"保存"};
save.shortcut = {"s", oneui::KeyModifierPrimary};
oneui::NativeMenuItem file{0, L"文件"};
file.children = {save};
window->setOnMenuAction([](int id) { /* id == 100: 保存 */ });
window->setMenuBar({file});
window->setOnTrayAction([](int id) { /* 独立的托盘回调 */ });
window->setTrayMenu({file});
window->setTrayEnabled(true);
window->setTrayIcon({1, 1, {255, 80, 30, 255}});
```

所有操作在窗口 UI 线程执行；跨线程使用 `post()`。菜单和图标数据在调用期间复制。
菜单最多 1024 个节点、8 层，文本最多 4096 个宽字符；叶子 ID 全树唯一，
`0xE001/0xE002` 为默认托盘命令保留。同级同组最多一个选中的单选项。
非法模型返回 `false`，保留原菜单。点击只报告 ID，业务层修改 `checked` 后重新提交模型。
同一菜单中不能重复快捷键；托盘快捷键不注册系统全局热键。

三端自定义托盘菜单都完整替换默认菜单；`setTrayMenu({})` 恢复本地化的显示/退出项。
`setMenuBar({})` 移除应用菜单栏；Mac 恢复之前的应用菜单。Mac 自动提供 About/Hide/Quit
应用菜单；Quit 经过窗口的强制退出请求回调，业务可处理未保存内容。
RGBA 图标为非预乘 alpha、每边 1..256 像素；空图标恢复系统默认图标。

旧 C 托盘结构体和函数签名保留。C++ 菜单结构体和 Window 虚表有新增成员，
升级动态库时必须重新编译 C++ 客户端，不可只替换旧程序旁的库文件。
