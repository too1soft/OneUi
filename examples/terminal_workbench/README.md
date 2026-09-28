# 原生连接工作区：`.one` + C++ + Rust

可运行的桌面工作区示例：连接侧栏、可关闭的终端标签、可拖动的分隔线、虚拟化文件表格和三组监控图表。`.one` 写布局，C++ ViewModel 管理状态，Rust 持有四组主／副 TerminalView 和输入框。全部使用 OneUI 原生控件，不包含其他产品的业务代码。

![深色工作区](../../docs/images/terminal-workspace-v4/dark-wide.png)

## 一条命令启动

在 OneUI 仓库根目录运行。需要 VS C++ 工具、Windows SDK、Rust MSVC 工具链和已构建的 Skia（准备步骤见根 README）。

```powershell
.\examples\terminal_workbench\build.ps1 -Test -Run
```

CMake 构建 SDK、模板编译器和 C++ 页面，再用 Cargo 构建 Rust 所有者 DLL。`Cargo.lock` 锁定依赖，不用手工复制 DLL。

后续直接运行：

```powershell
.\examples\terminal_workbench\build-current\bin\oneui-terminal-workbench.exe
# 深色、640px 客户区
.\examples\terminal_workbench\build-current\bin\oneui-terminal-workbench.exe --dark --narrow
# 四组回归 + 截图 + 二进制哈希
.\examples\terminal_workbench\check.ps1
```

## 按这个顺序体验

1. 点击左侧服务器图标展开连接列表，或用顶部选择框切换模拟连接；加号打开下一条连接。
2. 切换、关闭、重新打开标签：终端对象和内容保留。终端标题栏可以搜索当前可见内容、分屏、打开命令栏或清空。
3. 拖动终端／文件、正文／监控之间的分隔线；也可聚焦分隔线后用方向键调整。文件标题左侧箭头收起正文，再次展开恢复比例。
4. 左侧文件图标切换到文件区域，监控图标打开监控。640px 窗口使用“终端 / 文件 / 监控”顶部切换。
5. 文件区支持多选、搜索、双击目录、返回上级、新建、删除和模拟上传／下载；所有文件操作只修改内存。
6. 在终端直接输入，或打开命令栏输入 `help`、`ls`、`status`、`clear`。这是本地回显，不调用系统 Shell。
7. 监控页签切换资源／进程／连接；点击“开始采样”更新模拟图表，再点暂停停止更新。
8. 顶部或左侧设置图标切换主题；F12 查看 OneUI 内置诊断，Ctrl+F12 导出报告。

初始采样暂停，终端光标不闪烁，用于检查空闲时停止重绘。真实中文输入法和跨显示器 DPI 仍需单独验收。

## 从哪里开始改

| 文件 | 职责 |
|---|---|
| `views/TerminalWorkbench.one` | 页面布局、数据绑定、局部 CSS |
| `vm.h` | 可绑定状态与命令 |
| `workbench.h` | 会话、模拟文件、模拟采样等示例业务逻辑 |
| `main.cpp` | 窗口入口、原生集成回归 |
| `rust/src/lib.rs` | Rust 所有者、终端本地回显和命令 |
| `bridge.h` | 示例 v1 C ABI，标量与不透明句柄 |

应用层不调用子控件 `setFrame`。`SplitView`、稳定 ID 的 `Tabs`、`TimeSeriesChart`、图标和 `NativeHost themed` 是 SDK 功能，可在其他 C++ / `.one` 页面复用。

[新增工作区布局组件与验证](../../docs/57-workspace-layout-components.md) · [上一阶段接口](../../docs/56-declarative-terminal-workspace.md) · [跨语言所有权契约](../../docs/55-native-host-and-terminal-workbench.md)
