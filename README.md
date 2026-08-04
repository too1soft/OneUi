# OneUI

[English](README_EN.md) | 简体中文

[![License: MIT](https://img.shields.io/badge/License-MIT-2ea44f.svg)](LICENSE)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599c.svg)](CMakeLists.txt)
[![Platform](https://img.shields.io/badge/platform-Windows-0078d4.svg)](#平台状态)
[![Status](https://img.shields.io/badge/status-early%20development-f59e0b.svg)](#项目状态)

OneUI 是一个面向桌面应用的原生、自绘 UI 框架。它使用 C++17 构建，以 Skia raster
作为当前渲染基础，通过保留式控件树、布局系统、响应式状态和 CSS-like 样式表组织界面，
不依赖浏览器或 WebView。

项目当前以 Win32 为主线，目标是为工具软件、运维客户端和企业桌面应用提供稳定、紧凑、
可测试的原生 GUI 基础。OneUI 同时提供 C++ API、稳定 UTF-8 C ABI 和安全 Rust 绑定。

> **项目状态：早期开发阶段。** Win32 后端、Gallery、组件库和 MSVC SDK 已可运行，
> 但 API 仍可能调整。Linux 与 macOS 目前只有平台骨架，尚不能用于生产应用。

## 为什么选择 OneUI

- **真正的原生桌面运行时**：没有 HTML、JavaScript、浏览器进程或 WebView 依赖。
- **一致的自绘控件**：控件行为和视觉由 OneUI 管理，不受系统控件主题差异影响。
- **清晰的平台边界**：核心控件与 Win32 窗口、输入、DPI、IME、剪贴板和呈现层分离。
- **面向复杂工具界面**：包含产品外壳、虚拟列表、树、表格、终端和实时画面等能力。
- **跨语言接入**：C++ 应用可直接使用，Rust 应用通过 UTF-8 C ABI 和安全包装层接入。
- **可分发 SDK**：MSVC 产品构建使用静态 MSVC runtime 与 vendored static Skia，
  终端用户无需另外安装 MSYS2、Skia 或 Visual C++ Runtime。

## 项目状态

OneUI 已经超出最初的窗口与 Button 原型，目前包括：

| 范围 | 当前能力 |
| --- | --- |
| 基础 | `Widget`、`View`、`Canvas`、动画、`State<T>` / `Binding<T>` |
| 布局 | Stack、Grid、ReorderableGrid、Wrap、Dock、Split、Scroll、Panel、Overlay、App/Product Shell |
| 表单 | Button、IconButton、TextField、Checkbox、Switch、RadioGroup、Slider、Select、FormField |
| 导航与数据 | Tabs、List、VirtualList、TreeView、Table、Menu、NavItem；列表、树和网格均可报告稳定的重排请求 |
| 反馈与容器 | Card、Badge、ProgressBar、Dialog、Popup、Toast、StateView、StatusStrip |
| 专用视图 | TerminalView、LogView、RealtimeFrameView、RemoteInputRegion |
| 平台与互操作 | Win32 window、DPI/monitor、clipboard、IME 路径、UTF-8 C ABI、Rust bindings |

这些组件的成熟度并不完全相同。准备在产品中使用某个组件前，请查阅
[组件参考](docs/14-component-reference.md)和[组件清单](docs/07-component-inventory.md)。

## 平台状态

| 平台 | 状态 | 说明 |
| --- | --- | --- |
| Windows / Win32 | 主线可运行 | 当前开发、测试、Gallery 和 SDK 发布平台 |
| Windows 7+ | 兼容目标 | API 与依赖选择以兼容为目标，仍需持续做真实系统验收 |
| Linux | 平台骨架 | X11/Wayland 后端尚未达到可用状态 |
| macOS | 平台骨架 | Cocoa 后端尚未达到可用状态 |

## 架构

```text
Application (C++ or Rust)
        |
        +-- C++ API / UTF-8 C ABI / safe Rust bindings
        |
OneUI retained widget tree
        +-- state and binding
        +-- layout and focus
        +-- CSS-like style sheet and typed adapters
        +-- input dispatch and accessibility semantics
        |
Canvas abstraction
        |
Skia raster renderer
        |
Platform backend (Win32 today)
```

核心代码不应包含产品专用分支；可复用控件必须先进入 OneUI，并带有行为测试和稳定的
跨语言边界。架构细节见[架构说明](docs/01-architecture.md)和
[OneUI 契约红线](docs/24-oneui-contract-red-lines.md)。

## 仓库结构

```text
include/oneui/        公开 C++ API
src/core/             跨平台控件、布局、样式和状态实现
src/platform/         平台后端；当前实现为 Win32
src/capi/             UTF-8 C ABI
bindings/rust/        oneui-sys 与安全 Rust 包装层
examples/gallery/     组件 Gallery
tests/                控件、布局、终端、C ABI 与后端契约测试
docs/                 设计、组件、接入和迁移文档
scripts/              Skia、OneUI、SDK、审计和测试脚本
```

## Windows 构建

### 前置条件

- Git、Python 3、PowerShell
- CMake 3.16 或更高版本
- Visual Studio / Build Tools，包含 MSVC C++ 工具链与 Windows SDK
- Ninja（Visual Studio 安装中通常已包含）

首次构建需要下载并编译 Skia。以下路径请按本机 Visual Studio 和 Windows SDK
安装位置调整：

```powershell
git clone https://github.com/too1soft/OneUi.git
cd OneUi

$vs = "C:\Program Files\Microsoft Visual Studio\2022\Community"
$windowsSdk = "C:/Program Files (x86)/Windows Kits/10"
$env:PATH = "$vs\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;$env:PATH"

.\scripts\build-skia-static.ps1 `
  -Fetch -SyncDeps -Generate -Build `
  -WinVc ($vs.Replace("\", "/") + "/VC") `
  -WinSdk $windowsSdk

.\scripts\build-oneui-msvc-bundled.ps1 -VsInstall $vs
```

需要代理时，可给 Skia 脚本增加：

```powershell
-Proxy http://127.0.0.1:7897
```

构建产物：

```text
build/msvc-bundled-static/oneui.dll
build/msvc-bundled-static/oneui.lib
build/msvc-bundled-static/examples/gallery/oneui_gallery.exe
```

运行 Gallery：

```powershell
$env:PATH = "$(Resolve-Path .\build\msvc-bundled-static);$env:PATH"
& .\build\msvc-bundled-static\examples\gallery\oneui_gallery.exe
```

## 最小 C++ 示例

```cpp
#include <oneui/oneui.h>

#include <memory>

int main() {
    auto root = std::make_shared<oneui::Stack>(oneui::StackDirection::Column);
    root->setGap(12.0f);
    root->setPadding(oneui::Insets{16.0f});

    auto title = std::make_shared<oneui::Label>(L"Hello, OneUI");
    auto button = std::make_shared<oneui::Button>(L"Continue");
    root->add(title);
    root->add(button);

    auto window = oneui::Window::create(L"OneUI App", 720, 480);
    window->setContent(root);
    window->show();
    return window->run();
}
```

完整的窗口与控件组合方式请以
[`examples/gallery`](examples/gallery)为准。

## SDK 与 CMake 接入

生成可分发 SDK：

```powershell
.\scripts\package-sdk.ps1 -Toolchain msvc-bundled-static
```

输出位于：

```text
dist/OneUI-SDK-msvc-bundled-static.zip
```

应用侧 CMake：

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_oneui_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
find_package(OneUI REQUIRED)

add_executable(my_oneui_app main.cpp)
target_link_libraries(my_oneui_app PRIVATE OneUI::oneui)
```

配置时让 `CMAKE_PREFIX_PATH` 指向解压后的 SDK 根目录，并把 `bin/oneui.dll`
放在应用可执行文件旁边。详见[入门指南](docs/12-getting-started.md)。

## Rust 绑定

`bindings/rust/oneui-sys` 提供原始 UTF-8 C ABI，`bindings/rust/oneui` 提供所有权明确的
安全包装层。先构建 C++ 库，再运行 Rust 测试：

```powershell
$env:ONEUI_LIB_DIR = (Resolve-Path .\build\msvc-bundled-static)
$env:PATH = "$env:ONEUI_LIB_DIR;$env:PATH"
cargo test --manifest-path .\bindings\rust\Cargo.toml
```

新 Rust 代码应使用 UTF-8 API；旧 `wchar_t*` ABI 仅用于兼容已有 Windows 调用方。

## 测试与质量检查

```powershell
ctest --test-dir .\build\msvc-bundled-static --output-on-failure

.\scripts\audit-runtime.ps1 `
  -Binary .\build\msvc-bundled-static\oneui.dll `
  -Mode product

.\scripts\check-ui-red-lines.ps1
.\scripts\test-sdk-consumer.ps1
```

行为测试覆盖控件状态、布局、浮层、滚动、树、终端、实时画面、C ABI、DPI 和 Win32
后端契约。新增或修复行为时必须同时增加相应测试。

## 当前限制

- Linux 和 macOS 后端尚不可用。
- 公开 API 和 ABI 仍处于 `0.x` 收敛阶段。
- 复杂文本 shaping、完整 IME、平台可访问性桥接和视觉快照仍需完善。
- 部分高级控件已具备基础能力，但交互和可访问性尚未达到稳定版承诺。
- OneUI 不是浏览器 CSS 实现；StyleSheet 提供 CSS-like selector/state 和类型化适配器。

## 路线图

近期重点：

1. 稳定 Win32 后端、文本输入、DPI、焦点、可访问性和 SDK 契约。
2. 继续完善虚拟化数据组件与终端等高性能视图。
3. 建立可运行的 Linux 后端并通过同一套平台契约测试。
4. 在 ABI 和组件行为稳定后发布首个公开版本。

完整规划见[路线图](docs/02-roadmap.md)。

## 参与贡献

欢迎提交 Issue、设计讨论和 Pull Request。提交代码前请遵循以下原则：

- 可复用能力进入 OneUI，不在框架中加入特定产品名称或特殊分支。
- 样式通过 token、StyleSheet 和 typed adapter 表达，不在业务控件里堆写死常量。
- 输入、DPI、焦点、IME、剪贴板等平台差异必须停留在平台边界。
- C ABI 使用带长度的 UTF-8 字符串，并明确所有权、线程和回调生命周期。
- 行为修改需要测试；性能关键列表和实时视图需要有可重复的性能验证。

推荐先阅读[设计语言](docs/06-design-language.md)、
[组件开发指南](docs/13-authoring-guide.md)和
[下游产品接入规范](docs/17-downstream-product-integration-standard.md)。

## 许可证

OneUI 使用 [MIT License](LICENSE)。
