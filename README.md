# OneUI

[English](README_EN.md) | 简体中文

[![License: MIT](https://img.shields.io/badge/License-MIT-2ea44f.svg)](LICENSE)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599c.svg)](CMakeLists.txt)
[![Platform](https://img.shields.io/badge/backend-Win32-0078d4.svg)](#平台与成熟度)
[![UTF-8 ABI](https://img.shields.io/badge/UTF--8%20ABI-v16-6f42c1.svg)](include/oneui/oneui_c_api.h)
[![Version](https://img.shields.io/badge/version-0.1.0-f59e0b.svg)](CMakeLists.txt)

OneUI 是一个 **Windows 优先、原生、自绘、保留式** 的桌面 UI 框架。它以 C++17
实现，使用 Skia raster 渲染，通过 `Widget` / `View` 树、布局容器、响应式状态和
CSS-like 样式表构建界面，不依赖浏览器、HTML 或 WebView。

当前仓库同时提供：

- 公开 C++ API；
- 版本化的 UTF-8 C ABI（当前 `ONEUI_UTF8_ABI_VERSION = 18`）；
- `oneui-sys` 原始 Rust FFI 和 `oneui` 安全 Rust 包装层；
- Win32 窗口、输入、DPI、剪贴板、文件对话框、托盘和 Skia 呈现后端；
- Gallery、远程组件 Gallery、SDK 打包脚本和 13 个 CTest 行为/契约测试目标。

> **当前版本是 0.1.0 开发版。** Win32 主线已经可以承载真实桌面产品，但公开 API、
> C ABI 和组件细节仍可能在 `0.x` 阶段调整。Linux 与 macOS 目录目前只是明确报错的
> 平台骨架，不是可运行后端。

## 适用场景

OneUI 重点服务高信息密度的原生工具界面，例如：

- SSH/终端、运维和远程控制客户端；
- 企业控制台、配置工具和内部桌面应用；
- 包含大量列表、树、表格、分栏、浮层和快捷键的工作台；
- 需要由 C++ 核心与 Rust 产品层共同维护的 Windows 应用；
- 不希望引入浏览器运行时，同时需要统一视觉和可测试交互的桌面产品。

## 当前能力总览

| 范围 | 已实现能力 |
| --- | --- |
| 基础运行时 | `Widget`、`View`、逻辑/物理像素、焦点链、命中测试、键盘/鼠标/滚轮、动画帧、tooltip、无障碍语义元数据 |
| 响应式状态 | `State<T>`、`Binding<T>`、窗口线程 dispatcher、后台任务投递、控件线程安全 handle |
| 渲染 | `Canvas`、Skia raster、文字/图元/渐变/阴影/像素帧、clip 与稳定 viewport bounds |
| 样式 | CSS-like selector、class、伪状态、custom properties、typed style adapter、颜色/透明度过渡 |
| 布局 | Stack、Grid、Wrap、Panel、ScrollView、SplitView、OverlayHost、ReorderableGrid、AppShell、ProductShell |
| 表单 | Button、IconButton、TextField/TextArea、Checkbox、Switch、RadioGroup、Slider、Select、FormField |
| 导航与数据 | Tabs、List、VirtualList、TreeView、Table、Menu、NavItem；选择、激活、上下文菜单、重排和稳定拖拽 ID |
| 反馈与容器 | Card、Badge、IconBadge、ProgressBar、Sparkline、Dialog、Popup、Toast、StateView、StatusStrip |
| 专用视图 | TerminalView、LogView、RealtimeFrameView、RemoteInputRegion、WindowTitleBar |
| 平台服务 | Win32 Window、窗口状态持久化、DPI/显示器、剪贴板、文件/目录选择、confirm/prompt、托盘、全局 raw-key |
| 可观测性 | 真实控件 frame、Stack 内容尺寸、交互 trace、隐私安全的 OneUI 布局树 JSON 快照 |
| 互操作 | C++ API、459 个公开 C ABI 函数声明、UTF-8 ABI v16、safe Rust wrappers、回调 panic 边界 |

完整逐组件状态、语言覆盖和限制见
[组件清单](docs/07-component-inventory.md)与
[组件参考](docs/14-component-reference.md)。

## 本次工作区新增/增强能力

当前代码相对上一基线重点增加：

- `Sparkline`：归一化时间序列绘制，并提供 C ABI 与 Rust 安全包装；
- `Tabs`：紧凑宽度、按文本测量、图标、滚轮溢出、关闭、上下文菜单与受控重排请求；
- `Table`：结构化 UTF-8 行列、可见行绘制、平滑滚动、单选/多选、键盘命令、编辑/删除请求、内部重排与外部稳定 ID 拖拽；
- 布局诊断：窗口同步提交布局后导出 JSON；包含节点关系、实际/首选 frame、样式盒和脱敏语义信息；
- 标题栏：可插入交互附件，并可显式划分可拖拽区与可点击区；
- 通用交互：tooltip、`InteractiveSurface` pointer move/hover、`TextField` submit、`SplitView` ratio committed；
- 浮层与命中：嵌套 OverlayHost、越界 popup、滚动容器内 Select 的路由与 viewport 翻转；
- Rust：新增 `WidgetHandle`、`TextFieldHandle`、`SparklineHandle`、`TableHandle`、布局快照和交互 trace 等产品集成能力。

## 平台与成熟度

| 平台 | 状态 | 说明 |
| --- | --- | --- |
| Windows / Win32 | 当前唯一可运行后端 | 开发、Gallery、测试、SDK 和 Rust 产品接入均以此为主线 |
| Windows 7 API 级别 | 源码兼容目标 | CMake 定义 `_WIN32_WINNT=0x0601`；最终系统兼容性仍取决于所选 MSVC/Skia 构建产物 |
| Linux | 未实现 | `Window::create` 与剪贴板明确抛出未实现错误 |
| macOS | 未实现 | `Window::create` 与剪贴板明确抛出未实现错误 |

组件成熟度分为三层：

1. **主线**：已被 Gallery、C ABI/Rust 产品或行为测试覆盖，可用于当前 Win32 产品；
2. **可用**：核心交互已实现，但仍有明确的高级能力缺口；
3. **实验性**：API 已存在，适合验证和继续演进，不应解读为稳定版承诺。

精确分类以[组件清单](docs/07-component-inventory.md)为准。

## 架构

```text
Application (C++ / Rust / another FFI language)
        |
        +-- public C++ API
        +-- UTF-8 C ABI v16
        +-- oneui-sys + safe oneui Rust wrappers
        |
OneUI retained widget tree
        +-- state, binding and selection model
        +-- layout, focus, hit testing and overlays
        +-- CSS-like style sheet and typed adapters
        +-- callbacks, animation and accessibility metadata
        |
Canvas abstraction
        |
Skia raster renderer
        |
Win32 backend
        +-- window/message loop/raw key/IME path
        +-- DPI/monitor/clipboard/file dialogs/tray
        +-- logical-to-physical presentation
```

核心代码保持产品无关：可复用的控件、布局、输入和样式能力应先进入 OneUI，再通过 C ABI
或 Rust 包装被产品使用。详见[架构说明](docs/01-architecture.md)和
[契约红线](docs/24-oneui-contract-red-lines.md)。

## 仓库结构

```text
include/oneui/        公开 C++ 头文件与 oneui_c_api.h
src/core/             跨平台控件、布局、样式、状态和绘制逻辑
src/platform/win32/   当前可运行的 Win32 后端
src/platform/linux/   未接线的平台骨架
src/platform/macos/   未接线的平台骨架
src/capi/             C ABI 实现与跨边界生命周期管理
bindings/rust/        oneui-sys 与安全 oneui crate
examples/gallery/     C++ 组件 Gallery
examples/remote_component_gallery/ 远程/产品组件示例
tests/                行为、C ABI、后端和平台契约测试
docs/                 当前规范、组件参考、设计记录和接入说明
scripts/              Skia 构建、SDK、审计、红线与 smoke test
website/              Nuxt 文档/组件展示站点源代码
```

## Windows 构建

### 前置条件

- Git、Python 3、PowerShell；
- CMake 3.16 或更高版本；
- Visual Studio / Build Tools（MSVC C++ 工具链与 Windows SDK）；
- Ninja；
- 首次构建需要下载并编译 vendored Skia。

### 推荐：MSVC + 静态 Skia

```powershell
git clone https://github.com/too1soft/OneUi.git
cd OneUi

$vs = "C:\Program Files\Microsoft Visual Studio\2022\Community"
$windowsSdk = "C:/Program Files (x86)/Windows Kits/10"

.\scripts\build-skia-static.ps1 `
  -Fetch -SyncDeps -Generate -Build `
  -WinVc ($vs.Replace("\", "/") + "/VC") `
  -WinSdk $windowsSdk

.\scripts\build-oneui-msvc-bundled.ps1 `
  -VsInstall $vs `
  -Arch x64 `
  -Configuration RelWithDebInfo
```

如需代理，给 Skia 脚本增加 `-Proxy http://127.0.0.1:7897`。

主要产物：

```text
build/msvc-bundled-static/oneui.dll
build/msvc-bundled-static/oneui.lib
build/msvc-bundled-static/oneui.pdb            # RelWithDebInfo 时
build/msvc-bundled-static/examples/gallery/oneui_gallery.exe
build/msvc-bundled-static/examples/remote_component_gallery/oneui_remote_component_gallery.exe
```

运行 Gallery：

```powershell
$env:PATH = "$(Resolve-Path .\build\msvc-bundled-static);$env:PATH"
& .\build\msvc-bundled-static\examples\gallery\oneui_gallery.exe
```

MinGW/UCRT 预设和其他 Skia 模式见[入门指南](docs/12-getting-started.md)。

## 最小 C++ 示例

```cpp
#include <oneui/oneui.h>

#include <memory>

int main() {
    auto root = std::make_shared<oneui::Stack>(oneui::StackDirection::Column);
    root->setPadding(oneui::Insets{16.0f});
    root->setGap(12.0f);

    auto title = std::make_shared<oneui::Label>(L"Hello, OneUI");
    auto button = std::make_shared<oneui::Button>(L"Continue");
    button->setOnClick([] { /* application action */ });

    root->add(title);
    root->add(button);

    auto window = oneui::Window::create(L"OneUI App", 720, 480);
    window->setContent(root);
    window->show();
    return window->run();
}
```

`oneui/oneui.h` 汇总常用 API；未纳入总头文件的高级组件可直接包含其公开头文件，例如
`<oneui/controls/virtual_list.h>`、`<oneui/controls/sparkline.h>` 或
`<oneui/controls/window_title_bar.h>`。

## CMake / SDK 接入

生成 SDK：

```powershell
.\scripts\package-sdk.ps1 -Toolchain msvc-bundled-static
```

SDK 包输出到 `dist/OneUI-SDK-msvc-bundled-static.zip`。应用侧：

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_oneui_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
find_package(OneUI REQUIRED CONFIG)

add_executable(my_oneui_app main.cpp)
target_link_libraries(my_oneui_app PRIVATE OneUI::oneui)
```

配置时把 `CMAKE_PREFIX_PATH` 指向解压后的 SDK，运行时把 `bin/oneui.dll` 放在应用旁边。

## C ABI

[`include/oneui/oneui_c_api.h`](include/oneui/oneui_c_api.h) 是唯一权威 ABI 声明：

- opaque handle + POD struct + 显式 destroy；
- 新 API 使用带长度的 `OneUiUtf8String`；
- 数组和字符串在调用期间被 OneUI 拷贝；
- callback 使用函数指针和调用方 `user_data`；
- ABI v16 在加载时可由 `oneui_utf8_abi_version()` 校验；
- 旧 `wchar_t*` 入口仅为现有 Windows 调用方保留，不应继续扩展。

生命周期、线程、回调和组件映射见 [C ABI 接入指南](docs/c-abi-integration.md)。

## Rust 绑定

`bindings/rust/oneui-sys` 原样映射 C ABI；`bindings/rust/oneui` 提供所有权、回调清理、
UI 线程 dispatcher、panic 捕获和线程安全 handle。

```powershell
$env:ONEUI_LIB_DIR = (Resolve-Path .\build\msvc-bundled-static)
$env:PATH = "$env:ONEUI_LIB_DIR;$env:PATH"
cargo test --manifest-path .\bindings\rust\Cargo.toml
```

安全层目前覆盖窗口/dispatcher、样式表、布局容器、浮层、常用输入控件、Tabs、List、
VirtualList、TreeView、Table、TerminalView、LogView、Sparkline 和文件对话框等产品主线。
详见 [Rust bindings README](bindings/rust/README.md)。

## 测试与质量门禁

```powershell
cmake --build .\build\msvc-bundled-static
ctest --test-dir .\build\msvc-bundled-static --output-on-failure

$env:ONEUI_LIB_DIR = (Resolve-Path .\build\msvc-bundled-static)
$env:PATH = "$env:ONEUI_LIB_DIR;$env:PATH"
cargo test --manifest-path .\bindings\rust\Cargo.toml

.\scripts\check-ui-red-lines.ps1
.\scripts\audit-runtime.ps1 `
  -Binary .\build\msvc-bundled-static\oneui.dll `
  -Mode product
.\scripts\test-sdk-consumer.ps1
```

13 个 CTest 目标覆盖控件、选择模型、Overlay、ScrollView、Stack、实时画面、远程输入、
TerminalView、TreeView、Panel、C ABI、显示器与 Win32 后端契约。Rust crate 另有 safe-wrapper
和回调生命周期测试。

## 文档导航

从 [docs/README.md](docs/README.md) 开始。常用入口：

- [架构](docs/01-architecture.md)
- [入门与构建](docs/12-getting-started.md)
- [组件清单](docs/07-component-inventory.md)
- [组件参考](docs/14-component-reference.md)
- [样式表](docs/20-style-sheet.md)
- [可访问性](docs/15-accessibility.md)
- [C ABI 接入](docs/c-abi-integration.md)
- [Rust 绑定](bindings/rust/README.md)
- [平台后端契约](docs/28-platform-backend-contract.md)
- [TerminalView](docs/33-terminal-view.md)
- [TreeView](docs/34-tree-view.md)

## 已知限制

- 只有 Win32 后端可运行；Linux/macOS 尚未实现。
- `0.1.0` 仍在收敛期，公开 API 与 ABI 版本可能变化。
- StyleSheet 是受控 CSS-like 子集，不是浏览器 CSS 引擎。
- OneUI 已保存无障碍角色、名称、描述、值和状态，但 Win32 UI Automation 平台桥仍未完成。
- 文本输入支持基础编辑、选择、撤销/重做、粘贴、密码和提交；复杂 shaping 与完整 IME 场景仍需持续验证。
- Table 已支持虚拟化绘制和命令回调，但暂不内置排序、筛选、列 resize 或单元格编辑器。
- Tabs/Table/Tree/ReorderableGrid 的重排回调只报告请求；产品数据成功更新后再提交新顺序。
- RealtimeFrameView 当前绘制 BGRA/RGBA 像素，支持所有权移交的免复制提交和 Rust 后台线程最新帧合并；NV12 仅保留协议枚举，尚未实现转换。
- 布局 JSON 快照用于几何/语义诊断，不等同于像素截图测试。

## 贡献原则

- 可复用能力进入 OneUI，不在框架层添加具体产品名或条件分支；
- 平台差异停留在 `src/platform`，核心控件不直接依赖 Win32；
- 新跨语言能力先定义 UTF-8 C ABI，再补齐安全 Rust 生命周期；
- 输入、焦点、滚动、重排、浮层和线程行为必须有可重复测试；
- 样式通过 token、StyleSheet 与 typed adapter 表达，避免产品侧 magic number；
- 修改 ABI 时同步更新版本、`oneui-sys`、安全包装、测试和文档。

## 许可证

OneUI 使用 [MIT License](LICENSE)。
