# OneUI 入门指南

本文覆盖当前 Win32 主线的源码构建、Gallery、测试、C++ SDK、C ABI 与 Rust 接入。

## 1. 当前支持范围

- 可运行平台：Windows / Win32；
- 语言：C++17、UTF-8 C ABI v16、Rust 2021；
- 渲染：Skia raster；
- 推荐产品构建：MSVC + vendored static Skia + static MSVC runtime；
- 开发预设：MSYS2 MINGW64/UCRT64；
- Linux/macOS 当前不可运行。

## 2. 获取源码

```powershell
git clone https://github.com/too1soft/OneUi.git
cd OneUi
```

目录中不直接包含已编译 Skia。首次构建需要联网获取依赖。

## 3. 工具要求

推荐环境：

- Windows 10/11 开发机；
- PowerShell 7 或 Windows PowerShell 5.1；
- Git；
- Python 3；
- CMake 3.16+；
- Ninja；
- Visual Studio / Build Tools with C++ 和 Windows SDK；
- 足够磁盘空间用于 Skia 源码和中间产物。

MSVC 脚本不会猜测所有 Visual Studio 安装位置。请显式传 `-VsInstall`。

## 4. 构建 vendored Skia

```powershell
$vs = "C:\Program Files\Microsoft Visual Studio\2022\Community"
$windowsSdk = "C:/Program Files (x86)/Windows Kits/10"

.\scripts\build-skia-static.ps1 `
  -Fetch `
  -SyncDeps `
  -Generate `
  -Build `
  -WinVc ($vs.Replace("\", "/") + "/VC") `
  -WinSdk $windowsSdk
```

需要代理：

```powershell
.\scripts\build-skia-static.ps1 `
  -Fetch -SyncDeps -Generate -Build `
  -Proxy http://127.0.0.1:7897 `
  -WinVc ($vs.Replace("\", "/") + "/VC") `
  -WinSdk $windowsSdk
```

默认 x64 输出：

```text
third_party/skia/out/oneui-win-x64-release/
```

x86 使用对应脚本参数和 `oneui-win-x86-release` 输出。Skia 构建完成后，普通 OneUI 增量构建
不需要再次执行 Fetch/SyncDeps。

## 5. 构建 OneUI（推荐）

```powershell
.\scripts\build-oneui-msvc-bundled.ps1 `
  -VsInstall $vs `
  -Arch x64 `
  -Configuration RelWithDebInfo
```

脚本执行：

1. 进入 MSVC `vcvarsall` 环境；
2. 使用 Ninja 配置 `build/msvc-bundled-static`；
3. 启用 bundled static Skia；
4. 使用 `MultiThreaded` 静态 MSVC runtime；
5. 构建 OneUI、Gallery 与测试。

可选 configuration：`Debug`、`Release`、`RelWithDebInfo`。推荐内部产品保留
`RelWithDebInfo` 的 `oneui.pdb`，客户包不必分发 PDB。

主要产物：

```text
build/msvc-bundled-static/oneui.dll
build/msvc-bundled-static/oneui.lib
build/msvc-bundled-static/oneui.pdb
build/msvc-bundled-static/examples/gallery/oneui_gallery.exe
build/msvc-bundled-static/examples/remote_component_gallery/oneui_remote_component_gallery.exe
```

## 6. 只做增量构建

已经配置过 build 目录时：

```powershell
cmake --build .\build\msvc-bundled-static
```

若系统 `cmake` 与生成目录使用的 CMake 不一致，改用 Visual Studio 附带的同一 CMake，或重新
运行 `build-oneui-msvc-bundled.ps1`。

## 7. 运行示例

让 Windows loader 找到 `oneui.dll`：

```powershell
$oneuiBuild = Resolve-Path .\build\msvc-bundled-static
$env:PATH = "$oneuiBuild;$env:PATH"

& .\build\msvc-bundled-static\examples\gallery\oneui_gallery.exe
& .\build\msvc-bundled-static\examples\remote_component_gallery\oneui_remote_component_gallery.exe
```

Gallery 是控件组合和视觉行为的参考；行为契约仍以 `tests/` 为准。

## 8. 运行 C++ / C ABI 测试

```powershell
ctest --test-dir .\build\msvc-bundled-static --output-on-failure
```

当前应发现 13 个测试目标：

```text
oneui_control_behavior_tests
oneui_selection_model_tests
oneui_overlay_host_behavior_tests
oneui_scroll_view_behavior_tests
oneui_stack_behavior_tests
oneui_realtime_frame_view_behavior_tests
oneui_remote_input_region_behavior_tests
oneui_terminal_view_behavior_tests
oneui_tree_view_behavior_tests
oneui_panel_behavior_tests
oneui_c_api_behavior_tests
oneui_monitor_behavior_tests
oneui_backend_contract_tests
```

只运行某类测试：

```powershell
ctest --test-dir .\build\msvc-bundled-static -R "table|control|c_api" --output-on-failure
```

CTest target 名不一定包含单个组件名；Table/Tabs/Sparkline 等主要位于 control behavior tests。

## 9. 最小 C++ 应用

```cpp
#include <oneui/oneui.h>

#include <memory>

int main() {
    auto root = std::make_shared<oneui::Stack>(oneui::StackDirection::Column);
    root->setPadding(oneui::Insets{16.0f});
    root->setGap(10.0f);

    auto title = std::make_shared<oneui::Label>(L"OneUI consumer");
    auto input = std::make_shared<oneui::TextField>(L"Type a command");
    auto action = std::make_shared<oneui::Button>(L"Run");

    input->setOnSubmitted([](const std::wstring& text) {
        // Submit without inserting a newline in single-line mode.
    });
    action->setOnClick([] {
        // Product action.
    });

    root->add(title);
    root->add(input);
    root->add(action);

    auto window = oneui::Window::create(L"My OneUI App", 800, 520);
    window->setContent(root);
    window->show();
    return window->run();
}
```

## 10. 从 SDK 使用 C++

生成 SDK：

```powershell
.\scripts\package-sdk.ps1 -Toolchain msvc-bundled-static
```

输出：

```text
dist/OneUI-SDK-msvc-bundled-static/
dist/OneUI-SDK-msvc-bundled-static.zip
```

SDK 结构：

```text
bin/oneui.dll
lib/oneui.lib
include/oneui/**
cmake/OneUIConfig.cmake
examples/gallery/**
docs/**
```

Consumer CMake：

```cmake
cmake_minimum_required(VERSION 3.16)
project(oneui_consumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OneUI REQUIRED CONFIG)

add_executable(oneui_consumer main.cpp)
target_link_libraries(oneui_consumer PRIVATE OneUI::oneui)
target_compile_definitions(oneui_consumer PRIVATE UNICODE _UNICODE)
```

配置：

```powershell
cmake -S . -B build `
  -DCMAKE_PREFIX_PATH="C:/path/to/OneUI-SDK-msvc-bundled-static"
cmake --build build
```

部署时把 `bin/oneui.dll` 放在 consumer executable 旁边。

### C++ ABI 提醒

公开 C++ API 使用 `std::shared_ptr`、`std::wstring` 等 C++ 类型。跨 DLL 使用 C++ API 时应匹配
编译器家族、运行库和架构。不同语言/不同 C++ ABI/插件边界请使用 C ABI，不要直接链接 C++ 类。

验证 SDK consumer：

```powershell
.\scripts\test-sdk-consumer.ps1 `
  -SdkRoot dist/OneUI-SDK-msvc-bundled-static `
  -VsInstall $vs `
  -Arch x64
```

## 11. C ABI 接入

唯一权威声明：

```text
include/oneui/oneui_c_api.h
```

启动时先检查版本：

```c
if (oneui_utf8_abi_version() != ONEUI_UTF8_ABI_VERSION) {
    /* refuse to continue with a mismatched DLL */
}
```

新代码使用 `OneUiUtf8String`：

```c
const char title_bytes[] = "OneUI C ABI";
OneUiWindowOptionsUtf8 options = {0};
options.title.ptr = title_bytes;
options.title.len = sizeof(title_bytes) - 1;
options.width = 800;
options.height = 520;
options.resizable = 1;

OneUiWindow* window = oneui_window_create_utf8(&options);
OneUiWidget* root = oneui_stack_create(OneUiStackDirectionColumn);
OneUiWidget* button = oneui_button_create_utf8(
    (OneUiUtf8String){"Continue", 8});

oneui_stack_add(root, button);
oneui_window_set_content(window, root);
oneui_window_show(window);
int result = oneui_window_run(window);

oneui_widget_destroy(button);
oneui_widget_destroy(root);
oneui_window_destroy(window);
return result;
```

调用方 handle destroy 与挂载后的底层树生命周期分离；callback `user_data` 生命周期仍必须由调用方
保证。详见 [C ABI 接入](c-abi-integration.md)。

## 12. Rust 接入

先保证 `ONEUI_LIB_DIR` 包含 `oneui.lib` 和 `oneui.dll`：

```powershell
$env:ONEUI_LIB_DIR = (Resolve-Path .\build\msvc-bundled-static)
$env:PATH = "$env:ONEUI_LIB_DIR;$env:PATH"
cargo test --manifest-path .\bindings\rust\Cargo.toml
```

最小 safe Rust 组合：

```rust
use oneui::{Button, Insets, Label, Stack, StackDirection, Window, WindowOptions};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let window = Window::new(&WindowOptions {
        title: "OneUI Rust app".into(),
        width: 800,
        height: 520,
        ..WindowOptions::default()
    })?;

    let root = Stack::new(StackDirection::Column)?;
    root.set_padding(Insets { top: 16.0, right: 16.0, bottom: 16.0, left: 16.0 });
    root.set_gap(10.0);

    let title = Label::new("Hello from Rust")?;
    let mut action = Button::new("Continue")?;
    action.set_on_click(|| {
        // Product action on the window thread.
    });

    root.add(title.as_widget());
    root.add(action.as_widget());
    window.set_content(root.as_widget());
    window.show();
    let exit_code = window.run();
    std::process::exit(exit_code);
}
```

注意：需要回调的 wrapper 必须活到控件不再触发回调。后台线程应保留 `UiDispatcher` 或具体 handle，
不要直接移动 UI wrapper。完整说明见 [Rust bindings](../bindings/rust/README.md)。

## 13. 样式表

```css
:root {
  --surface: #141821;
  --border: #303744;
  --accent: #656aff;
}

button.primary {
  background: var(--accent);
  color: #ffffff;
  border-radius: 6px;
  padding: 0 14px;
  transition-duration: 120ms;
}

button.primary:hover {
  background: #7377ff;
}

table.inventory {
  background: var(--surface);
  border-color: var(--border);
  scrollbar-color: #64748b;
  scrollbar-width: 5px;
}

table.inventory:selected {
  background: #29375f;
}
```

实际受支持属性见 [StyleSheet](20-style-sheet.md)。OneUI 不执行浏览器 CSS layout。

## 14. 布局树诊断

Rust：

```rust
let json = window.layout_snapshot_json()?;
std::fs::write("oneui-layout.json", json)?;
```

C ABI 使用两阶段 buffer：

```c
size_t required = 0;
int status = oneui_window_layout_snapshot_utf8(window, NULL, 0, &required);
if (status == -2 && required > 1) {
    char* json = malloc(required);
    if (oneui_window_layout_snapshot_utf8(window, json, required, &required) == 1) {
        /* inspect/write JSON */
    }
    free(json);
}
```

快照会同步提交布局，包含普通内容与 overlay，但不包含 TextField 明文值。

## 15. 质量门禁

```powershell
.\scripts\check-ui-red-lines.ps1

.\scripts\audit-runtime.ps1 `
  -Binary .\build\msvc-bundled-static\oneui.dll `
  -Mode product

.\scripts\check-package-size.ps1
.\scripts\test-remote-component-gallery-smoke.ps1
```

`audit-runtime` 检查非系统 DLL 依赖。MSVC bundled-static 产品包不应要求终端用户安装 MSYS2、
Skia 或额外 VC runtime。

## 16. 常见问题

### 找不到 oneui.dll

把 build/SDK 的 `oneui.dll` 放到 executable 同目录，或把其目录放入当前进程 PATH。不要依赖系统上
另一份旧 DLL；ABI 版本检查会拒绝不匹配版本。

### Rust 链接到旧 DLL

设置 `ONEUI_LIB_DIR` 后重新构建；`oneui-sys` build script 会把匹配 DLL 复制到 Cargo profile/test
目录，避免旧 DLL 优先被 loader 找到。

### 修改 C ABI 后 Rust 编译/运行失败

同步完成：

1. `ONEUI_UTF8_ABI_VERSION`；
2. `oneui-sys::UTF8_ABI_VERSION`；
3. raw extern 声明；
4. safe wrapper；
5. C ABI 与 Rust tests；
6. 文档。

### 标题栏中的 Tabs/Search 点不动

无边框窗口需要设置 title-bar drag metrics，并用 title-bar interactive insets 排除交互附件区。不要
用全宽 caption hit-test 覆盖控件。

### 页面看着对齐但缩放后错位

读取 logical frame、client pixel size 和 DPI scale；使用布局 JSON 验证实际 frame，不要混用截图物理
像素与 OneUI 逻辑像素。
