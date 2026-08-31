# OneUI Rust Bindings

OneUI 的 Rust workspace 包含两层：

| crate | 作用 |
| --- | --- |
| `oneui-sys` | ABI v21 原始 FFI；不增加所有权或线程策略 |
| `oneui` | 安全包装；RAII、UTF-8、callback 清理、panic 边界、dispatcher 和线程安全 handle |

产品代码应优先使用 `oneui`。只有 safe 层尚未覆盖且生命周期能被明确封装时，才在一个局部模块
中直接使用 `oneui-sys`。

## 前置构建

先构建 C++ DLL/import library：

```powershell
$vs = "C:\Program Files\Microsoft Visual Studio\2022\Community"
.\scripts\build-oneui-msvc-bundled.ps1 `
  -VsInstall $vs `
  -Arch x64 `
  -Configuration RelWithDebInfo
```

设置：

```powershell
$env:ONEUI_LIB_DIR = (Resolve-Path .\build\msvc-bundled-static)
$env:PATH = "$env:ONEUI_LIB_DIR;$env:PATH"
cargo test --manifest-path .\bindings\rust\Cargo.toml
```

`ONEUI_LIB_DIR` 必须包含：

```text
oneui.lib
oneui.dll
```

`oneui-sys/build.rs` 会把匹配 DLL 复制到 Cargo 当前 profile/test executable 目录，避免旧 DLL
比 PATH 更早被 Windows loader 选中。

## ABI 检查

`oneui-sys::UTF8_ABI_VERSION` 当前为 21。`Window::new` 创建窗口前调用
`oneui_utf8_abi_version()`，不匹配时返回：

```rust
Error::AbiVersionMismatch { expected, actual }
```

不要绕过该检查。header/import library 和 runtime DLL 必须来自同一次 OneUI 构建。

## 最小应用

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
    root.set_padding(Insets {
        top: 16.0,
        right: 16.0,
        bottom: 16.0,
        left: 16.0,
    });
    root.set_gap(10.0);

    let title = Label::new("Hello from Rust")?;
    let mut action = Button::new("Continue")?;
    action.set_on_click(|| {
        // Runs on the owning window thread.
    });

    root.add(title.as_widget());
    root.add(action.as_widget());
    window.set_content(root.as_widget());
    window.show();

    let code = window.run();
    std::process::exit(code);
}
```

`action` 必须保持存活，因为它拥有 callback storage。把 Widget 挂进 native tree 后，底层控件不会因
Rust wrapper Drop 立即消失，但 callback wrapper 被 Drop 时会清除 callback。

## 线程模型

### Window 与 UI wrapper

- `Window::new` 在调用线程创建并初始化隐藏原生窗口；
- 该线程是 owning UI thread；
- `show` / `run` / 普通控件 setter 在该线程使用；
- `Window::set_on_client_size_changed` 在 UI 线程按帧合并派发逻辑像素宽高，适合事件驱动响应式布局；
- `Window`/`UiDispatcher` 可在运行时设置和查询沉浸全屏；`Window::set_minimum_client_size` 使用逻辑客户端尺寸；
- 普通控件 wrapper 不应从 worker 随意调用。

### UiDispatcher

```rust
let dispatcher = window.dispatcher();

std::thread::spawn(move || {
    let result = load_data();
    let _ = dispatcher.dispatch(move || {
        // Apply product state on the window thread.
        consume(result);
    });
});
```

窗口关闭后 dispatcher 会拒绝/取消排队工作。`dispatch` 的 closure 必须为 `Send + 'static`。

Dispatcher 还提供：

- `request_animation_frame`；
- owner-bound file/folder dialog；
- `confirm` / `prompt`；
- worker blocking confirm/prompt；
- layout snapshot JSON；
- 创建控件 handle。

## Handle

Handle 用于后台服务向 UI 线程投递受控更新，而不是把完整 UI wrapper 变成跨线程可变。

| Handle | 用途 |
| --- | --- |
| `WidgetHandle` | visible、preferred size、frame、tooltip |
| `LabelHandle` | 合并/投递文本更新 |
| `TextFieldHandle` | 投递文本值 |
| `ProgressBarHandle` | 投递进度 |
| `SparklineHandle` | 投递 sample vector |
| `VirtualListHandle` | 整表 revision、清选择、单行更新 |
| `TableHandle` | 整表 revision、清选择、单行更新 |
| `TerminalViewHandle` | grid/frame/viewport/选择等高频终端状态 |
| `RealtimeFrameViewHandle` | 完整帧所有权移交、脏矩形批次提交和单一待处理 UI 批次 |

示例：

```rust
let trend = oneui::Sparkline::new()?;
let trend_handle = window.sparkline_handle(&trend);

std::thread::spawn(move || {
    let samples = vec![0.18, 0.22, 0.41, 0.35, 0.63];
    trend_handle.set_values(samples)?;
    Ok::<_, oneui::Error>(())
});
```

Label/VirtualList/Table/Terminal handle 内部对部分更新做合并，避免 worker 高频事件无限堆积到窗口
消息队列。

## 当前 safe API

### Window 与平台

- `Window` / `WindowOptions` / `WindowPlacement`；
- title、borderless、corner radius、placement；
- title-bar drag metrics 与 interactive insets；
- raw key；
- client logical/pixel size 与 DPI；
- clipboard；
- file/folder dialog；
- confirm/prompt；
- style sheet / default font；
- layout snapshot JSON。

### 布局与浮层

- `Stack`；
- `SplitView`，含 changed/committed callback；
- `Panel`；
- `ScrollView`；
- `OverlayHost`；
- `Popup`；
- `Menu`；
- `Dialog`；
- `StateView`；
- `ReorderableGrid`。

### 输入与导航

- `Label`；
- `Button` / `IconButton` / `InteractiveSurface`；
- `TextField` / `TextArea`；
- `Checkbox` / `Switch`；
- `Select`；
- `SegmentedControl`；
- `Tabs`；
- `NavItem`；
- `WindowTitleBar`。

### 数据与专用视图

- `List`；
- `VirtualList`；
- `Table`；
- `TreeView`；
- `TerminalView`；
- `LogView`；
- `ProgressBar`；
- `Sparkline`；
- `Icon`。

其他 C ABI 能力可由 `oneui-sys` 使用，但在补 safe wrapper 前必须明确所有权与 callback Drop。

## Tabs

```rust
let mut tabs = oneui::Tabs::new(&[
    "SSH · production".into(),
    "Local shell".into(),
])?;

tabs.set_item_icons(&[
    Some(oneui::IconSymbol::Terminal),
    None,
]);
tabs.set_compact(true);
tabs.set_item_width_range(96.0, 220.0);
tabs.set_closable(true);
tabs.set_reorder_enabled(true);
tabs.set_on_close_requested(|index| {
    // Update product tabs, then call set_items.
});
tabs.set_on_reorder_requested(|source, target| {
    // Persist request; native Tabs does not mutate product data.
});
```

callback storage 由 `Tabs` wrapper 持有，Drop 会先清 callback。

## Table

```rust
let mut table = oneui::Table::new()?;
table.set_columns(&[
    oneui::TableColumn { header: "Name".into(), width: 160.0 },
    oneui::TableColumn { header: "Address".into(), width: 180.0 },
    oneui::TableColumn { header: "Status".into(), width: 0.0 },
]);
table.set_rows(&[
    oneui::TableRow { cells: vec!["Production".into(), "10.0.0.8".into(), "Online".into()] },
    oneui::TableRow { cells: vec!["Staging".into(), "10.0.1.8".into(), "Offline".into()] },
]);
table.set_selection_mode(oneui::SelectionMode::Multiple);
table.set_row_height(32.0);
table.set_on_activated(|row| {
    // Open row.
});
```

后台整表刷新：

```rust
let handle = window.table_handle(&table);
std::thread::spawn(move || {
    let rows = load_rows();
    handle.set_rows(rows)?;
    Ok::<_, oneui::Error>(())
});
```

`set_rows_and_clear_selection` 用于 route/filter 变化；普通 refresh 可保留可用选择。`update_row` 适合实时
状态 patch。

## Layout snapshot

```rust
let json = window.layout_snapshot_json()?;
std::fs::write("oneui-layout.json", json)?;
```

后台任务使用 dispatcher：

```rust
let json = window.dispatcher().layout_snapshot_json()?;
```

快照会同步提交当前布局，包含 content + overlay 的结构、实际/首选 frame、style 和安全语义摘要。
TextField/密码明文不写入 JSON。

## Callback panic 边界

Rust panic 不能穿过 C ABI。safe crate 会捕获 callback panic，并可安装 observer：

```rust
oneui::set_callback_panic_handler(|panic| {
    eprintln!(
        "OneUI callback panic: component={} event={} message={}",
        panic.component,
        panic.event,
        panic.message,
    );
});
```

observer 用于日志/崩溃诊断：

- 不在 observer 内重试 callback；
- 不修改正在 dispatch 的控件；
- 不假设业务事务已经回滚；
- 内部 release 应保留精确 PDB/二进制映射。

## Interaction trace

新增 callback 安装时记录 component/event/caller file/line/column：

```rust
oneui::set_interaction_trace_handler(|trace| {
    eprintln!(
        "{}:{} installed at {}:{}:{}",
        trace.component,
        trace.event,
        trace.file,
        trace.line,
        trace.column,
    );
});
```

该 trace 描述 callback 注册来源，不包含用户输入值。

## 文件对话框

```rust
let filters = [oneui::FileDialogFilter {
    name: "JSON",
    pattern: "*.json",
}];
let selected = window.file_dialog(oneui::FileDialogOptions {
    filters: &filters,
    ..oneui::FileDialogOptions::open("Open config")
})?;
```

还支持 save/select-folder。路径和过滤器通过 UTF-8 ABI；对话框由 window owner 绑定，产品不需要直接
调用 Win32 API。

## 生命周期规则

1. Window 在 UI 线程创建和 run；
2. child 挂载后 native tree 持有底层对象；
3. 需要 callback/直接更新的 Rust wrapper 保持存活；
4. wrapper Drop 先清除 callback，再 destroy caller handle；
5. worker 保留 dispatcher/handle，不保留可变 UI wrapper；
6. window close 后停止继续投递；
7. DLL 在所有 Window/Widget wrapper Drop 后再卸载。

## `oneui-sys` 使用规则

确需 raw FFI 时：

- 在单独模块封装 unsafe；
- 立即把 null/return code 转成 Rust error；
- 使用 `OneUiUtf8String::from_str`；
- 保存 callback Box，原生清除后再 Drop；
- 为 create/destroy 写 RAII wrapper；
- 不从 worker 直接调用 UI setter；
- 增加 safe-layer 测试后尽快上移到 `oneui` crate。

## 测试

```powershell
$env:ONEUI_LIB_DIR = (Resolve-Path .\build\msvc-bundled-static)
$env:PATH = "$env:ONEUI_LIB_DIR;$env:PATH"

cargo fmt --manifest-path .\bindings\rust\Cargo.toml -- --check
cargo test --manifest-path .\bindings\rust\Cargo.toml
```

Rust tests 需要加载与 `oneui-sys` ABI 版本相同的真实 `oneui.dll`，不能只做 compile-only。

## 发布/部署

- Rust executable 旁分发匹配架构的 `oneui.dll`；
- 不分发 `oneui.lib` 给终端用户；
- 内部保留 `oneui.pdb` 以诊断 native callback/crash；
- 升级 OneUI 时同时升级 `oneui-sys`、safe crate 和 DLL；
- 不支持在同一进程混用不同 ABI 版本的 OneUI DLL。
