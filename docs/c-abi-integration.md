# OneUI C ABI 接入

OneUI 使用版本化 C ABI 为 Rust、Go、C#、Python FFI 和不同 C++ ABI 的产品提供边界。本文说明
当前 ABI v20 的形状、所有权、线程和迁移规则；完整函数签名只以
`include/oneui/oneui_c_api.h` 为准。

## 当前版本

```c
#define ONEUI_UTF8_ABI_VERSION 20u
```

运行时检查：

```c
if (oneui_utf8_abi_version() != ONEUI_UTF8_ABI_VERSION) {
    /* The headers/import library and loaded oneui.dll do not match. */
    return ONEUI_ABI_MISMATCH;
}
```

safe Rust `Window::new` 自动执行同一检查并返回 `AbiVersionMismatch`。

## 边界红线

C ABI 不能暴露：

- C++ class、template、STL container/string；
- exception 或跨边界 unwind；
- lambda / `std::function`；
- RTTI、compiler-specific layout；
- 产品专用 route/state；
- 调用方无法判断所有权的裸内存。

公开形状只使用：

- opaque handles，例如 `OneUiWindow*`、`OneUiWidget*`、`OneUiStyleSheet*`；
- 固定布局 POD struct/enum；
- 带长度 UTF-8 view；
- pointer + count 数组；
- 函数指针 callback + caller-owned `user_data`；
- 显式 destroy。

## 字符串

### 新 API

```c
typedef struct OneUiUtf8String {
    const char* ptr;
    size_t len;
} OneUiUtf8String;
```

约定：

- `len` 是 byte 数，不包含 NUL；
- 内容必须是有效 UTF-8；
- 在 setter/create 调用中 OneUI 会拷贝内容，函数返回后输入 buffer 可释放；
- 返回字符串一般使用 caller buffer + required length；
- 不依赖 NUL，因此允许普通 Rust/Go string slice；
- 新增 ABI 只允许使用 UTF-8 版本。

### 旧 Windows 兼容入口

少量 `wchar_t*` 函数仍保留给已有 Windows 调用方。它们不是可移植 ABI 的扩展方向，也不应该
被新语言绑定采用。

## 结构化数组

List/Tree/Tabs/Table/Terminal 等使用结构化数组。例如 Table：

```c
typedef struct OneUiTableColumnUtf8 {
    OneUiUtf8String header;
    float width;
} OneUiTableColumnUtf8;

typedef struct OneUiTableRowUtf8 {
    const OneUiUtf8String* cells;
    size_t cell_count;
} OneUiTableRowUtf8;
```

调用 `oneui_table_set_columns_utf8` / `oneui_table_set_rows_utf8` 时，OneUI 在返回前拷贝全部
array 和 string。调用方不需要把这些输入 buffer 保持到下一帧。

返回数组通常采用：

```c
size_t count = oneui_table_selected_indices(table, NULL, 0);
int* values = malloc(count * sizeof(int));
oneui_table_selected_indices(table, values, count);
```

具体返回值和 buffer 规则以函数注释为准。

## 句柄与所有权

### Window

```c
OneUiWindow* window = oneui_window_create_utf8(&options);
oneui_window_initialize(window);
oneui_window_show(window);
int code = oneui_window_run(window);
oneui_window_destroy(window);
```

- create 返回 caller-owned window handle；
- initialize 在当前 UI 线程创建 HWND；
- run 驱动该窗口消息循环；
- destroy 释放窗口 wrapper 与原生资源。

### Widget

```c
OneUiWidget* root = oneui_stack_create(OneUiStackDirectionColumn);
OneUiWidget* label = oneui_label_create_utf8(text);
oneui_stack_add(root, label);
oneui_window_set_content(window, root);

oneui_widget_destroy(label);
oneui_widget_destroy(root);
```

`oneui_widget_destroy` 释放调用方 wrapper 引用；挂载到 View/Stack/Window/OverlayHost 后，底层 C++
树保存共享所有权。测试覆盖“先 destroy wrapper，再初始化/快照”仍保持树存活的场景。

但有 callback 时要额外注意：callback storage 通常由语言包装对象拥有。如果销毁包装对象，必须
先清除原生 callback；safe Rust 会自动处理。

### StyleSheet / Tray

`oneui_style_sheet_destroy`、`oneui_tray_destroy` 与各自 create 配对。不要用
`oneui_widget_destroy` 释放非 Widget handle。

## 子项挂载规则

- setter/add API 将底层 child shared ownership 挂到 parent；
- 重复设置 content 会替换 parent 持有的旧 child；
- caller wrapper 的 destroy 不等同于从 parent remove；
- overlay 使用专用 remove API；
- callback user_data 不会被 OneUI 自动 free。

## 回调

通用形状：

```c
typedef void (*OneUiVoidCallback)(void* user_data);
typedef void (*OneUiIntCallback)(int value, void* user_data);
typedef void (*OneUiBoolCallback)(int value, void* user_data);
```

安装：

```c
oneui_button_set_on_click(button, on_click, state);
```

清除：

```c
oneui_button_set_on_click(button, NULL, NULL);
```

规则：

- callback 一般在 owning window thread 触发；
- `user_data` 必须在 callback 被清除或控件不再触发之前保持有效；
- callback 不可抛异常/longjmp/unwind 经过 OneUI；
- callback 内可调用同一窗口的非阻塞 UI API，但应避免递归打开消息循环；
- 高频 callback（pointer move、ratio changed、terminal update）不要同步做磁盘/网络重活。

窗口客户端尺寸变化使用逻辑像素，并由后端在 UI 线程按帧合并后派发：

```c
oneui_window_set_on_client_size_changed(window, on_client_size, state);
// 销毁 state 前清除回调。
oneui_window_set_on_client_size_changed(window, NULL, NULL);
```

连续拖动窗口时不会为每一条平台 `WM_SIZE` 消息同步执行产品布局；回调收到的是该帧最新宽高。

### Rust panic 边界

safe Rust 为 dispatcher、按钮、value/pointer/list/tree/menu/table/terminal 等 callback 安装
`catch_unwind` 边界。panic 不会越过 C ABI；应用可用 `set_callback_panic_handler` 接收 component、
event 和 panic message。

observer 只用于诊断，不应在当前 dispatch 中重试或修改正在回调的控件。

## 线程模型

### UI 线程

- window create/initialize/run 在同一 UI 线程；
- layout、paint、input、control callback 在该线程；
- 普通 widget setter 应在 owning UI 线程执行。

### 后台线程

C ABI：

```c
oneui_window_post(window, callback, user_data);
```

或使用 `oneui_window_post_owned` 传递带 cleanup 的 owned payload。窗口关闭后 post 会失败/取消；
调用方必须按函数契约处理 cleanup。

Rust 优先使用：

- `UiDispatcher::dispatch`；
- `WidgetHandle` / `LabelHandle` / `TextFieldHandle`；
- `VirtualListHandle` / `TableHandle` / `TerminalViewHandle`。

## Window 能力

ABI 当前覆盖：

- create/initialize/show/activate/run/close/request-close/minimize/maximize；
- title、borderless、fullscreen/topmost/resizable options；
- placement get/set 与可见 work-area 验证；
- logical client size、physical pixel size、DPI scale；
- default font、StyleSheet 与运行时 refresh；
- title-bar drag metrics 和 interactive insets；
- raw key callback 与合并后的客户端尺寸变化 callback；
- clipboard、file/folder dialog、confirm、prompt；
- tray 与 notification；
- UI-thread post 与 animation frame；
- layout snapshot JSON。

## 控件覆盖

当前 C ABI 不再只是 Window/Button/TextField。主要前缀包括：

```text
oneui_widget_*            oneui_stack_*             oneui_split_view_*
oneui_overlay_host_*      oneui_popup_*             oneui_dialog_*
oneui_panel_*             oneui_scroll_view_*       oneui_app_shell_*
oneui_product_shell_*     oneui_top_bar_*            oneui_title_bar_*
oneui_label_*             oneui_button_*            oneui_icon_button_*
oneui_text_field_*        oneui_checkbox_*          oneui_switch_*
oneui_radio_group_*       oneui_select_*            oneui_tabs_*
oneui_list_*              oneui_virtual_list_*      oneui_tree_view_*
oneui_table_*             oneui_menu_*              oneui_reorderable_grid_*
oneui_card_*              oneui_badge_*             oneui_progress_bar_*
oneui_sparkline_*         oneui_state_view_*        oneui_status_strip_*
oneui_toast_*             oneui_log_view_*          oneui_terminal_view_*
oneui_realtime_frame_view_*  oneui_remote_input_region_*
```

当前 header 包含 465 个公开 `oneui_*` 函数声明。数量用于说明覆盖规模，不代替逐函数契约。

## ABI v20 重点

相对 v13，本工作区把 ABI 推进到 v20，重点包含：

- `OneUiTableColumnUtf8` / `OneUiTableRowUtf8`；
- Table 结构化数据、滚动、选择、命令、重排和 drag；
- Tabs icon/compact/width/close/context/reorder；
- Sparkline；
- Checkbox；
- TextField prefix/suffix icon 与 submit；
- InteractiveSurface pointer move/hover；
- WindowTitleBar accessory；
- title-bar interactive insets；
- client logical-size changed callback；
- `oneui_realtime_frame_view_submit_frame_owned` 与恰好一次 release callback；
- safe Rust `RealtimeFrameView` / `RealtimeFrameViewHandle` / `RemoteInputRegion`。
- `oneui_remote_input_region_set_on_text_input_utf8` 与 safe Rust committed-text/IME 回调；
- 远端光标默认/隐藏/预乘 RGBA 位图、远端坐标和线程安全 `RemoteInputRegionHandle`；
- widget frame/tooltip；
- Stack content extent；
- SplitView ratio committed；
- Menu clear；
- window layout snapshot JSON。

## 布局快照

```c
size_t required = 0;
int query = oneui_window_layout_snapshot_utf8(window, NULL, 0, &required);
if (query == -2 && required > 1) {
    char* json = malloc(required);
    if (oneui_window_layout_snapshot_utf8(window, json, required, &required) == 1) {
        /* use JSON */
    }
    free(json);
}
```

返回值：

- `1`：成功；
- `-2`：buffer 缺失或过小，`required_len` 包含末尾 NUL；
- `0`：无效 window。

快照路径会同步提交与呈现一致的布局，序列化 content + overlay。内容包括 parent/child 关系、actual/
preferred frame、style node、resolved style box、可见/禁用/焦点和安全语义摘要。

隐私约束：TextField/密码内容不写入 JSON；只记录长度等安全元信息。测试覆盖 secret 不泄漏。

## 错误与返回值

C ABI 没有统一 `errno` 对象；每个函数使用下列之一：

- null handle 表示 create 失败；
- `0/1` 表示 false/true 或失败/成功；
- 负值表示需要 buffer/特定失败；
- size query 返回 required count；
- void setter 对无效 handle 通常安全忽略。

调用方必须阅读函数注释，不能把所有 `int` 都解释为相同错误码。

## 动态加载建议

动态语言/插件应：

1. 加载与应用架构一致的 `oneui.dll`；
2. 获取 `oneui_utf8_abi_version`；
3. 与绑定生成时版本严格比较；
4. 只解析该版本声明的 symbol；
5. 在卸载 DLL 前关闭 window、清除 callback、销毁全部 handle；
6. 不把某个版本的 struct size 强行用于另一个 ABI 版本。

## 修改 ABI 的检查表

每次新增/修改公开能力必须同时完成：

- [ ] 更新 `include/oneui/oneui_c_api.h`；
- [ ] 实现 `src/capi/oneui_c_api.cpp`；
- [ ] 必要时提升 `ONEUI_UTF8_ABI_VERSION`；
- [ ] 同步 `oneui-sys::UTF8_ABI_VERSION` 与 extern/POD；
- [ ] 增加 safe Rust wrapper 与 Drop 清理；
- [ ] 增加 C ABI behavior test；
- [ ] 增加 Rust test；
- [ ] 更新组件清单/参考/C ABI 文档；
- [ ] 用实际 DLL 跑 ABI version 与 symbol smoke test。

## 不要做的事

- 不要给新 API 增加 `wchar_t*` 版本而没有 UTF-8 版本；
- 不要跨 ABI 返回 `std::string` / `std::vector`；
- 不要保存调用方临时 slice pointer，除非 API 明确声明借用期限；
- 不要让 Rust panic/C++ exception 穿过 callback；
- 不要从 worker 直接调用普通 widget setter；
- 不要在产品层用 raw Win32 控件补一个应当属于 OneUI 的通用组件；
- 不要在 ABI 版本不匹配时“尝试继续”。
