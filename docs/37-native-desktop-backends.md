# Linux / macOS 原生后端：实现、构建与验收

这是当前跨平台实现的状态记录，不是全部平台已经支持的发布声明。Windows 保持既有主线；
Linux 与 macOS 属于同一期开发，只有完成各自原生验收后才能提升支持等级。

下面的构建记录是文字升级前的跨平台阶段基线。本轮新增的强制文字依赖、Unicode 一致性
失败和 MinGW 依赖阻塞见[文字引擎状态](38-text-and-interaction-engine.md)；不能沿用旧包的通过结论。

## 支持矩阵

| 目标 | 已实现 | 已构建 / 自动测试 | 已原生验收 |
| --- | --- | --- | --- |
| Windows x64，MSVC bundled-static / MinGW | Win32，共享 Skia Canvas | 本机回归通过 | 本轮未重做完整人工输入/多屏验收 |
| Ubuntu 24.04 x86_64 X11 | Xlib / XIM / RandR / selection / XImage | Ubuntu 24.04.4 WSLg 已构建、CTest/Rust 通过 | 待真实 Ubuntu 桌面；WSLg 的 X11 可能由 XWayland 提供 |
| Ubuntu 24.04 x86_64 Wayland | xdg-shell / shm / xkbcommon / data-device / text-input-v3 | WSLg 原生 Wayland 连接、Gallery、CTest/Rust 已运行 | 待真实桌面与 IME；无交互序号的剪贴板测试明确跳过，不计通过 |
| Ubuntu 24.04 ARM64 | 同一 Linux 源码路径 | 未构建 | 待 ARM64 机器 |
| 银河麒麟桌面 V10 x86_64 / ARM64，默认 X11 | 同一 X11 源码路径 | 未在发行版基线上构建 | 待对应发行版、架构、输入法 |
| 统信桌面 UOS 20 x86_64 / ARM64，默认 X11 | 同一 X11 源码路径 | 未在发行版基线上构建 | 待对应发行版、架构、输入法 |
| macOS 13+ Intel / Apple Silicon | Cocoa Objective-C++ 源码已接入 | 未构建；尚无 Mac SDK/机器验证 | 待两个架构的 Mac |

当前开发环境：Windows x64；WSL Ubuntu 24.04.4 x86_64，WSLg，`DISPLAY=:0`、
`WAYLAND_DISPLAY=wayland-0`，Clang 18。这不是 ARM64、国产发行版或 macOS 的验收证据。
MSVC bundled-static 的 DLL 通过 product 运行库审计；本轮 MinGW 回归使用已有
`msys2-dynamic` 开发配置，依赖 `libskia.dll`，仅通过 development 审计，不能作为独立发布包。
测试数量以本次构建的 `ctest -N` / Cargo 输出为准，不在概览中维护易漂移的数量。

## 后端选择与能力

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
| 原生文件对话框 / 托盘 | 保留已有实现 | 未实现 | 未实现 | 未实现 |

能力位是运行时信息，不是验收标记；Wayland 的剪贴板能力会随输入序号可用性变化。
缺少 IME 协议不等于已经支持中文输入。Wayland `workArea` 暂按输出边界报告，不能推断
面板保留区；旧版协议包缺少 fractional-scale-v1 时只使用输出整数缩放。
Linux/macOS 不支持的对话框/托盘 C 接口返回失败/null，不用空实现报告成功。
X11 当前按会话 Xft.dpi 处理逻辑缩放，混合 DPI 显示器的策略仍需真机验证；
XImage 提交要求 32 位 BGRA-compatible TrueColor 存储，不支持的 visual 明确报错。
Wayland 客户端装饰、Cocoa 全屏/还原与输入法替换范围也必须接受真实窗口系统的验证。

## 实现边界

- `src/platform/shared/` 包含 Skia Canvas、路径、文字测量/字体缓存及桌面调度器；OS GUI 头只在平台实现出现。
- `src/platform/native_services.cpp` 隔离现有 Win32 文件对话框/托盘；`src/capi` 只依赖 OneUI 抽象。
- Cocoa 使用 NSApplication/NSWindow/NSView、CoreText、NSPasteboard；Skia 栅格经 CoreGraphics 展示。
- X11 使用 Xlib、XIM、RandR 和系统光标，通过 XImage 提交像素。
- Wayland 使用显式缓冲释放、frame callback、共享内存、客户端标题栏；绘制内容坐标仍从客户区原点开始。
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
  libfontconfig1-dev libfreetype-dev fonts-noto-cjk
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
