# OneUI 入门指南

这份文档面向第一次接触 OneUI 的 C++ 桌面应用开发者。目标不是罗列内部实现，而是说明 SDK 里有什么、怎么接入、怎么运行 Gallery、当前哪些能力可以依赖，以及遇到常见问题时先查哪里。

> 当前状态：OneUI 仍处于 MVP 阶段。Windows Win32 后端和 MSVC 产品 SDK 是当前主线；Linux/macOS 目前只有平台骨架，尚不是可运行后端。应用启动 API 仍在收敛，Gallery 是现阶段最可靠的可运行示例。

## 1. OneUI 现在是什么

OneUI 当前是一个自绘的原生桌面 UI 框架/组件库，主要由这些部分组成：

- `include/oneui/` 下的公开 C++ 头文件。
- Windows 动态库 `oneui.dll`。
- MSVC 产品 SDK 中的导入库 `oneui.lib`。
- CMake package：应用侧可以使用 `find_package(OneUI REQUIRED)`。
- Gallery 示例程序：展示已经验收或正在收敛的控件行为。
- 静态打包目标：产品 SDK 尽量不要求终端用户安装 MSYS2、Skia 或 Visual C++ Runtime。

你可以先把它理解成：

```text
你的应用 exe
  -> 链接 OneUI::oneui
  -> 运行时加载 oneui.dll
  -> 创建平台窗口
  -> 挂载 root View
  -> 组合布局容器和控件
  -> 用 State<T> / Binding<T> 连接 ViewModel
```

## 2. SDK 目录结构

当前产品 SDK 包通常位于：

```text
dist/OneUI-SDK-msvc-bundled-static.zip
```

解压后结构类似：

```text
OneUI-SDK-msvc-bundled-static/
  bin/oneui.dll
  lib/oneui.lib
  include/oneui/
  cmake/OneUIConfig.cmake
  docs/*.md
  examples/gallery/
```

开发者需要解压后的 SDK 目录、CMake，以及支持 C++17 的编译器。Windows 上建议使用和 SDK 匹配的 MSVC 工具链。终端用户通常只需要你的应用 `.exe` 和同目录的 `oneui.dll`。

## 3. 接入方式

### 3.1 消费产品 SDK

这是推荐给应用开发者的方式：

1. 获取或构建 `dist/OneUI-SDK-msvc-bundled-static.zip`。
2. 解压到稳定目录，例如 `D:\SDKs\OneUI-SDK-msvc-bundled-static`。
3. 在应用项目里把 SDK 根目录传给 `CMAKE_PREFIX_PATH`。
4. 运行时把 `bin/oneui.dll` 放到应用 exe 旁边。

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH=D:\SDKs\OneUI-SDK-msvc-bundled-static
cmake --build build
```

### 3.2 在 OneUI 仓库内开发

框架开发者或调试 OneUI 自身时，常用命令是：

```powershell
.\scripts\build-oneui-msvc-bundled.ps1
.\scripts\run-gallery.ps1
```

开发构建可能使用不同 toolchain 或动态依赖；产品 SDK 需要通过 runtime audit 确认只依赖系统 DLL 和 `oneui.dll`。

## 4. 最小 CMake 接入

```cmake
cmake_minimum_required(VERSION 3.16)
project(oneui_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OneUI REQUIRED)

add_executable(oneui_app main.cpp)
target_link_libraries(oneui_app PRIVATE OneUI::oneui)
```

如果构建成功但运行时报找不到 DLL，把 SDK 的 `bin/oneui.dll` 复制到你的 exe 旁边，或把 `bin/` 加入 `PATH`。

## 5. 最小应用心智模型

公开、稳定、面向应用开发者的启动封装仍在整理中。现阶段不要凭空假设存在类似 `Application app; app.run();` 的稳定 API；请以 Gallery 为准。

```cpp
#include <oneui/oneui.h>

int main() {
    // 1. 创建平台窗口。
    // 2. 创建 root View 或 OverlayHost。
    // 3. 组合 Stack/Grid/FormField/Button 等控件。
    // 4. 把 root 挂到窗口。
    // 5. 进入平台消息循环。
}
```

建议阅读顺序：

1. `examples/gallery/main.cpp`：窗口创建、root view、消息循环。
2. `examples/gallery/gallery_view.cpp`：布局容器、控件组合、状态绑定。
3. `include/oneui/oneui.h`：当前公开 API 汇总。

## 6. 运行 Gallery

从 SDK 包运行：

```powershell
cd D:\SDKs\OneUI-SDK-msvc-bundled-static\examples\gallery
.\run-gallery.ps1
```

从仓库根目录运行：

```powershell
.\scripts\run-gallery.ps1
```

Gallery 是“当前运行时行为应该长什么样”的可执行样本。如果一个组件还没有出现在 Gallery 中，即使头文件已经存在，也应该把它当成较低稳定度，除非测试和文档明确说明它已验收。

## 7. 第一个界面怎么组织

```cpp
struct ProjectViewModel {
    oneui::State<std::wstring> name{L"OneUI app"};
    oneui::State<int> platform{0};
};

auto root = std::make_shared<oneui::Stack>(oneui::StackDirection::Column);
root->setGap(12.0f);
root->setPadding(oneui::Insets{16.0f});

auto nameInput = std::make_shared<oneui::TextField>(L"请输入项目名称");
nameInput->bindText(vm.name);

auto nameField = std::make_shared<oneui::FormField>();
nameField->setLabel(L"项目名称");
nameField->setRequired(true);
nameField->setChild(nameInput);

auto save = std::make_shared<oneui::Button>(L"保存");
save->setVariant(oneui::ButtonVariant::Primary);
save->setOnClick([&] {
    // 保存 ViewModel。
});

root->add(nameField);
root->add(save);
```

推荐模式：

- ViewModel 持有 `State<T>`。
- View 创建控件并通过 `bindXxx` 绑定状态。
- 业务动作放在 ViewModel 或应用服务里，通过 `setOnClick` / `setOnChanged` 调用。
- 样式优先使用 `Theme` 默认值，局部差异再用 `StyleOverride`。

## 8. 已有控件概览

详见 [14-component-reference.md](14-component-reference.md)。当前可重点尝试：

- 基础：`Widget`、`View`、`State<T>`、`Binding<T>`、`Theme`、`Canvas`。
- 布局：`Stack`、`Grid`、`Wrap`、`DockView`、`SplitView`、`ScrollView`、`OverlayHost`。
- 表单：`TextField`、`Select`、`Checkbox`、`RadioGroup`、`Switch`、`Slider`、`FormField`、`ValidationMessage`。
- 导航与数据：`Tabs`、`List`、`Table`。
- 反馈与展示：`Button`、`Label`、`Card`、`Badge`、`Separator`、`ProgressBar`、`Popup`。

## 9. 常见问题排查

### 9.1 `find_package(OneUI REQUIRED)` 找不到包

先确认 `CMAKE_PREFIX_PATH` 指向 SDK 根目录，而不是 `cmake/` 子目录：

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH=D:\SDKs\OneUI-SDK-msvc-bundled-static
```

### 9.2 链接时报找不到 `oneui.lib`

检查：

- SDK 是否完整解压，`lib/oneui.lib` 是否存在。
- 应用和 SDK 是否使用匹配的架构，例如 x64 应用不要链接 x86 SDK。
- 是否通过 `target_link_libraries(app PRIVATE OneUI::oneui)` 链接，而不是手写库路径。

### 9.3 运行时报找不到 `oneui.dll`

Windows 默认不会自动从 SDK 的 `bin/` 搜索 DLL。解决方式：

- 把 `oneui.dll` 放到你的 exe 同目录。
- 或在调试环境里把 SDK 的 `bin/` 加入 `PATH`。

### 9.4 TextField 中文输入不完整

这是已知限制。当前 `TextField` 已支持 caret、基础选择区、密码显示模式、横向滚动裁剪、copy/cut/paste API、Ctrl+A/C/X/V、Left/Right/Home/End/Delete/Backspace 基础键盘编辑、`undo()` / `redo()` 历史入口，以及 Win32 `SystemClipboard`。但 IME composition、Linux/macOS 系统剪贴板 bridge、Ctrl+Z/Ctrl+Y 快捷键接入、复杂文本布局仍未完成。

### 9.5 ScrollView 水平滚动状态

`ScrollView` 现在除了垂直滚动外，已经新增：

- `setContentWidth(float)`：设置内容宽度。
- `horizontalScrollOffset()` / `setHorizontalScrollOffset(float)`：读取和设置水平偏移。
- `maxHorizontalScrollOffset()`：读取当前最大水平偏移。
- 水平 scrollbar thumb 会在内容横向溢出时绘制，并支持鼠标拖拽；拖动会按 viewport 与内容宽度比例换算为 `horizontalScrollOffset`，并自动 clamp 到有效范围。
- 键盘骨架：`Up` / `Down` 纵向滚动，`Left` / `Right` 横向滚动，`Home` / `End` 回到起点或跳到最大偏移。

限制也要明确：鼠标滚轮仍是纵向滚动；垂直 thumb 仍是基础绘制且暂不支持拖拽；触控板精细 delta、惯性滚动、滚动条样式化和完整可访问性语义仍待后续 slice。

### 9.6 Popup/Select 行为还不完整

`PopupPlacement` 已经是可复用几何定位器；`OverlayHost` 的挂载、移除、层级、基础事件转发、焦点边界和外部指针阻断已有行为测试。`Popup` 现在有 `PopupInteractionMode::Modeless`、`LightDismiss`、`Modal` 三个薄预设，并可通过 `popup->overlayOptions(layer)` 生成对应 `OverlayOptions`。Gallery 的 Overlay/Popup 页面已经有三列对照示例，能帮助你看清 background click、outside dismiss、modal block 的区别。

`Select` 当前仍是最小内置下拉，public API 未变。内部 popup 状态已收敛为私有 `LightDismissModel` / `PopupLightDismissReason`，light-dismiss 行为基线已经验收：点击 field/dropdown 外部会关闭下拉，不改变 `selectedIndex`，不触发 `onChanged`；点击其它控件可继续派发；点击另一个 `Select` 会关闭前一个并打开后一个。下拉几何已经通过内部 adapter 复用 `PopupPlacement`，但还没有真正挂进共享 `OverlayHost` / `Popup` 运行时。长列表滚动、typeahead、完整键盘行为、可访问性和完整浮层迁移仍未完成。

## 10. 推荐阅读路径

1. 先运行 Gallery。
2. 读本文档，理解 SDK、CMake、DLL 和最小应用模型。
3. 读 [13-authoring-guide.md](13-authoring-guide.md)，理解布局、样式、MVVM 和事件。
4. 读 [14-component-reference.md](14-component-reference.md)，按组件查用法和限制。
5. 读 [07-component-inventory.md](07-component-inventory.md)，了解更完整的规划和缺口。
