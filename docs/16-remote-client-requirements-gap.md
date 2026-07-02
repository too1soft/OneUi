# OneUI 面向远程控制客户端的差距分析

日期：2026-05-25

> **历史快照**：下方正文是 2026-05-25 时点的差距分析，其中不少"未看到公开 API / 无专用组件"的判断**已被落地**（如 `RealtimeFrameView`、`RemoteInputRegion`、`Monitor`、多窗口生命周期、`PointerButton` 全集等）——落地情况见文末进度日志与源码。阅读正文表格时请勿当作现状。

本文基于 `remote` 项目的真实产品诉求，对 `oneui` 当前方向和缺口做一次产品级核对。目标不是把 OneUI 做成大而全的 GUI 框架，而是先保证它能承载远程控制类客户端：轻量、原生、Win7+、可跨平台演进、低延迟、可维护，并且未来能替换掉 Tauri/Qt/Electron 这类壳。

## 总结

OneUI 当前方向总体没有跑偏。

已经对齐的方向：

- 原生 C++ 桌面 UI，而不是 WebView/Tauri/Electron。
- Win32 后端先行，并明确 Windows 7 是一等目标。
- 自绘控件，避免平台控件外观和行为碎片化。
- Skia raster 作为基础绘制层，适合稳定、可控、可打包的早期路线。
- 产品包倾向 MSVC `/MT`、静态运行时、vendored static Skia，符合轻量分发诉求。
- 已有基础控件、布局、Overlay/Popup、Clipboard、行为测试雏形，能支撑普通配置页和管理页。

但它还不足以直接支撑远程控制客户端。远程控制软件的 UI 热点不在普通表单，而在“实时远程会话窗口”：视频帧展示、输入捕获、坐标映射、全屏、多显示器、状态叠层、跨线程帧提交和低延迟调度。这些能力目前没有形成公开 API，也没有验收测试。

优先级建议：先补远程客户端专用的基础原语，再继续扩展通用组件目录。

## 我们的核心 UI 需求

远程客户端近期至少需要这些能力：

- 主窗口：设备码、临时验证码、连接表单、质量/分辨率设置、运行日志、状态提示。
- 独立远程会话窗口：可普通窗口、最大化、全屏，后续支持多会话。
- 实时视频显示：承载远程核心解码出来的帧，支持原始比例、适应窗口、拉伸、居中留黑边。
- 会话状态叠层：FPS、延迟、码率、分辨率、连接状态、丢帧/解码错误。
- 输入捕获：鼠标移动、左/中/右键、双击、滚轮、键盘按下/抬起、组合键、特殊键。
- 坐标映射：窗口坐标、视频画面坐标、远端屏幕坐标之间稳定转换。
- 焦点和释放：窗口失焦、切出、停止会话时释放全部按键/鼠标状态，避免远端卡键。
- 多显示器和 DPI：本机/远端多屏、负坐标屏幕、不同缩放比例。
- 日志查看：高频追加但不强制自动滚到底，可复制、可筛选、可限流。
- 打包：Win7+ 无额外运行时安装，包体积要有硬指标。

## 当前能力对照

| 需求 | 当前 OneUI 状态 | 缺口/风险 | 优先级 |
| --- | --- | --- | --- |
| Win32 原生窗口 | 已有 `Window::create(title, width, height)` | 缺少 WindowOptions、窗口句柄抽象、关闭回调、尺寸/DPI 事件、窗口状态控制 | P0 |
| 多窗口/远程会话窗口 | 理论上可以创建多个窗口，但 API 和消息循环不支持产品级多窗口模型 | `WM_DESTROY` 直接 `PostQuitMessage`，任意窗口关闭可能退出整个应用；缺少 owned window、modal、fullscreen | P0 |
| 全屏/无边框 | 未看到公开 API | 远程软件必须支持专用远程窗口和全屏 | P0 |
| 实时视频 surface | 无专用组件 | 目前 `Canvas` 只有形状和文本；每次 `WM_PAINT` 创建 raster surface，不适合 2K/4K 高频帧展示 | P0 |
| 帧提交/跨线程调度 | 无公开 dispatcher | 远程核心通常在 IO/解码线程产出帧，必须安全投递到 UI 线程 | P0 |
| 鼠标输入 | 目前 `MouseButton` 只有 Left | 缺右键、中键、XButton、双击、进入/离开、捕获边界、绝对坐标映射 | P0 |
| 键盘输入 | `Key` 枚举很小，仅有少量编辑键和 A/C/V/X | 缺 key up、scan code、virtual key、numpad、F1-F24、组合键、重复标记、系统键，容易造成重复输入和特殊键失败 | P0 |
| 输入区域/远程输入控制 | 无专用抽象 | 远程会话需要把 UI 输入和远端注入协议分离，避免 TextInput 与 KeyDown 双发 | P0 |
| DPI/多显示器 | 文档列为方向，未看到完整 API | 远控坐标非常依赖 per-monitor DPI、显示器枚举、负坐标、客户区缩放 | P0 |
| 状态叠层 | 可用普通控件拼 | 需要低开销 overlay，不应触发布局重算影响视频帧 | P1 |
| 日志面板 | 有 List/Table/ScrollView | 缺虚拟化日志、复制、暂停自动滚动、过滤、按级别着色 | P1 |
| 表单控件 | Button/TextField/Select/Checkbox/Slider/Tabs 等已有 | 可支撑主窗口 MVP，但 Select、TextField、IME、焦点仍需加强 | P1 |
| 菜单/右键菜单/托盘 | 组件清单有规划，未看到实现 | 远程软件常用托盘、菜单、右键命令、快捷入口 | P1 |
| Dialog/Toast/Tooltip | 规划中，Popup 基础已有 | 连接失败、权限提示、确认断开、错误提示需要这些能力 | P1 |
| 图标/工具栏/状态栏 | 规划中 | 远程窗口需要工具栏、图标按钮、状态栏、分辨率/画质快捷切换 | P1 |
| 可访问性 | 有语义 API 草案 | 平台 bridge 未完成；远程 canvas 本身可先标记为自定义区域 | P2 |
| 跨平台 | 只有 Win32 可用，Linux/macOS 是骨架 | 不能对外宣称跨平台已完成；应写成 Win32 first，后端接口为跨平台预留 | P2 |
| 包体积 | MSVC 静态包方向正确，dist 中 MSVC SDK 约 3.5MB，MinGW/UCRT 包约 11MB | 远程客户端应禁止把 MinGW/UCRT 动态依赖作为产品默认路径 | P0 |

## 必须补齐的产品级原语

### 1. WindowManager 和 WindowOptions

现有窗口 API 太薄，适合 gallery，不适合远程客户端。

建议新增：

```cpp
struct WindowOptions {
    std::wstring title;
    int width = 1280;
    int height = 800;
    bool resizable = true;
    bool visible = false;
    bool borderless = false;
    bool fullscreen = false;
    bool topmost = false;
};

class Window {
public:
    virtual NativeWindowHandle nativeHandle() const = 0;
    virtual void setTitle(std::wstring title) = 0;
    virtual void setFullscreen(bool enabled) = 0;
    virtual void setBorderless(bool enabled) = 0;
    virtual void close() = 0;
    virtual void requestRedraw() = 0;
    virtual void post(std::function<void()> callback) = 0;
};
```

验收标准：

- 主窗口和远程会话窗口可以同时存在，关闭会话窗口不退出主窗口。
- 支持普通窗口、最大化、全屏切换。
- 支持窗口尺寸变化事件和客户区尺寸查询。
- 支持 UI 线程安全投递。

### 2. RealtimeFrameView / VideoSurface

远程画面不应该被当作普通 `Image` 或普通 `Canvas` 绘制。它是高频、低延迟、可丢帧的实时 surface。

建议新增：

```cpp
enum class PixelFormat {
    Bgra8888,
    Rgba8888,
    Nv12
};

enum class ScaleMode {
    ActualSize,
    Fit,
    Fill,
    Stretch
};

struct VideoFrame {
    const void* data = nullptr;
    int width = 0;
    int height = 0;
    int stride = 0;
    PixelFormat format = PixelFormat::Bgra8888;
    uint64_t frameId = 0;
    uint64_t timestampUs = 0;
};

class RealtimeFrameView : public Widget {
public:
    void submitFrame(const VideoFrame& frame);
    void setScaleMode(ScaleMode mode);
    Rect contentRect() const;
};
```

关键要求：

- 支持从非 UI 线程提交帧，内部只保留最新帧，旧帧可以丢弃。
- 支持 2560x1440、3840x2160 的 BGRA 路径。
- 支持 `contentRect()`，用于鼠标坐标映射。
- 支持帧时间戳，让上层计算端到端延迟。
- Skia raster 可作为第一版实现，但 API 不能绑定死 raster；后续可在 Win32 后端内部切换 GDI/DIB、D3D、OpenGL 或平台特定快速路径。

验收标准：

- 2560x1440 30fps 连续提交 5 分钟，UI 不阻塞，内存不持续增长。
- 当消费者来不及绘制时只显示最新帧，不排队堆积。
- 窗口缩放时坐标映射仍准确。

### 3. RemoteInputRegion / RawInputModel

当前 `Widget` 输入模型适合普通控件，不适合远控。远控需要物理按键级别的事件，而不是只有少量语义 Key。

建议新增：

```cpp
enum class PointerButton {
    Left,
    Right,
    Middle,
    X1,
    X2
};

struct PointerEvent {
    Point windowPosition;
    Point contentPosition;
    Point normalizedPosition;
    PointerButton button;
    bool pressed = false;
    int wheelDeltaX = 0;
    int wheelDeltaY = 0;
};

struct RawKeyEvent {
    uint32_t virtualKey = 0;
    uint32_t scanCode = 0;
    bool pressed = false;
    bool repeat = false;
    bool extended = false;
    bool alt = false;
    bool ctrl = false;
    bool shift = false;
    bool win = false;
};
```

关键要求：

- 区分 `KeyDown/KeyUp` 和 `TextInput`，远控默认只转发 raw key，不把字符输入再重复转发一次。
- 支持特殊键：Esc、Tab、CapsLock、Shift、Ctrl、Alt、Win、Insert、Delete、Home、End、PageUp、PageDown、方向键、F1-F24、PrintScreen、Pause、NumPad。
- 支持组合键：Ctrl+C、Ctrl+V、Alt+Tab、Win+R、Ctrl+Alt+Del 的策略至少要可配置。
- 鼠标支持相对移动和绝对坐标两种模式。
- 会话窗口失焦、停止、崩溃恢复时要触发 `releaseAllInputs()`。

验收标准：

- 主控输入一个 `1`，被控只出现一个 `1`。
- Ctrl+C/Ctrl+V、Alt+Tab、方向键、Delete、F5、NumPad 均能产生可验证事件。
- 鼠标坐标在 Fit/Fill/Stretch/ActualSize 下都能映射到远端屏幕正确位置。

### 4. Monitor/DPI API

远程控制对坐标敏感，不能只依赖窗口客户区。

建议新增：

```cpp
struct MonitorInfo {
    int index = 0;
    Rect bounds;
    Rect workArea;
    float scale = 1.0f;
    bool primary = false;
    std::wstring name;
};

std::vector<MonitorInfo> enumerateMonitors();
```

验收标准：

- 支持负坐标副屏。
- 支持 per-monitor DPI 变化事件。
- 远程窗口跨屏移动后，视频显示和输入映射不漂移。

### 5. 远程客户端必需组件

已有组件足够覆盖一部分普通表单，但还缺这些更贴近远程客户端的组件：

- `Icon` / `IconButton`：远程窗口工具栏需要。
- `Toolbar` / `CommandBar`：全屏、缩放、画质、断开连接等命令。
- `StatusBar`：FPS、延迟、码率、分辨率、连接状态。
- `Toast` / `InlineStatus`：连接失败、重连中、权限不足。
- `Dialog` / `Modal`：断开确认、无人值守授权、安全提示。
- `ContextMenu` / `Menu`：托盘菜单、会话菜单。
- `SystemTray`：远程软件常驻托盘。
- `LogView`：虚拟化日志、暂停滚动、复制选中、级别过滤。
- `PasswordField` / `OtpCodeField`：临时验证码、访问密码。
- `SegmentedControl`：画质/模式切换。
- `ResizablePanel`：主窗口日志区、配置区可调整。

这些不需要一次全部做漂亮，但需要 API 稳定、行为可靠。

## 性能和包体积边界

OneUI 目前 MSVC bundled static SDK zip 约 3.5MB，这个方向适合产品化。MinGW/UCRT 包约 11MB，且容易引入 DLL 依赖，不适合作为远程客户端默认分发路径。

建议写入硬性门槛：

- Windows 产品构建默认使用 MSVC `/MT`。
- 禁止要求用户安装 VC Runtime、UCRT、MSYS2、Skia DLL、字体包。
- UI 库 SDK 压缩包目标小于 5MB；远程客户端最终 UI 层增量目标小于 8MB。
- CI 必须保留 runtime import audit。
- 每次引入第三方依赖必须记录许可证、静态链接方式、体积增量。

## 方向风险

### 风险 1：把普通控件目录做得太宽，远程会话原语却没做

远程客户端最先卡住的不会是按钮、表格或卡片，而是视频 surface、输入捕获、坐标映射、全屏窗口和线程调度。组件目录可以继续，但 P0 应该转向这些产品原语。

### 风险 2：Skia raster 被误用为所有实时画面的唯一热路径

Skia raster 适合控件绘制和 Win7 兼容起步，但远程视频可能需要更快的 blit 或 GPU 路径。建议保持公共 API 是 `RealtimeFrameView`，内部第一版可以 raster，后续按平台替换实现。

### 风险 3：跨平台表述过早

当前真实可用后端是 Win32。文档里应继续明确 Linux/macOS 是 roadmap，不要让业务侧误以为现在已经可用。

### 风险 4：键盘模型过于语义化

普通 UI 的 `Key::Enter`、`Key::A` 不足以远控。远控必须拿到 virtual key、scan code、key up/down、repeat、extended、modifier 状态，否则特殊键和组合键会长期出问题。

### 风险 5：窗口生命周期仍是 demo 模型

`WM_DESTROY -> PostQuitMessage` 适合单窗口 demo，不适合远程客户端。主窗口、远程会话窗口、弹窗、托盘必须有应用级生命周期管理。

## 建议路线

### P0：远程客户端可用底座

- WindowOptions、WindowManager、多窗口生命周期。
- 全屏、无边框、置顶、关闭事件。
- UI 线程 dispatcher。
- RealtimeFrameView，支持 BGRA 帧提交和最新帧覆盖。
- RemoteInputRegion，支持 raw mouse/key down/key up。
- DPI/monitor API。
- 包体积和运行时依赖审计。
- 行为测试：多窗口、输入事件、坐标映射、停止会话释放输入。

### P1：远程客户端主窗口组件

- Icon/IconButton/Toolbar/StatusBar。
- Dialog/Toast/Tooltip/ContextMenu。
- LogView 虚拟化和暂停滚动。
- PasswordField/OtpCodeField。
- SystemTray。
- ResizablePanel。
- 更完整 TextField：IME、复制粘贴、选择、Undo/Redo。

### P2：跨平台和工程硬化

- Linux/macOS 后端开始实现，不再只是 skeleton。
- 平台 accessibility bridge。
- 视觉快照测试。
- 复杂文本 shaping、字体 fallback。
- 高 DPI 和多显示器自动化测试。

## 最小验收清单

OneUI 要被 `remote` 项目接入前，建议至少通过这些测试：

- `remote_shell_demo`：一个主窗口和一个远程会话窗口同时运行。
- `video_surface_2k_30fps`：2560x1440 BGRA 30fps 提交 5 分钟，无明显卡顿、无内存增长。
- `video_surface_4k_smoke`：3840x2160 至少 60 秒 smoke test。
- `input_mapping_fit_fill_stretch`：不同缩放模式下点击四角和中心，映射坐标误差小于 1 像素或 0.1%。
- `keyboard_raw_events`：数字键、字母键、方向键、F 键、组合键、NumPad、Delete、Tab、Esc 都能产生 down/up。
- `keyboard_no_duplicate_text`：输入 `1`、`abc` 不重复。
- `release_all_on_focus_lost`：按住 Ctrl/Shift/鼠标键后切出窗口，能发出释放事件。
- `multi_window_lifetime`：关闭远程窗口不退出主窗口。
- `win7_runtime_audit`：产品构建不依赖用户额外安装运行时。
- `package_size_gate`：产物体积超过阈值时 CI 失败。

## 结论

OneUI 适合作为我们后续统一 UI 库的方向，但当前还处在“能做普通桌面工具”的阶段，离“能承载远程控制客户端”还差一层实时交互基础设施。

下一步不建议继续优先扩普通组件目录，而应先把 P0 的远程底座补齐：多窗口/全屏、实时视频 surface、raw input、坐标映射、dispatcher、DPI/monitor、包体积审计。只要这些底座稳定，主窗口的普通表单和设置页用现有组件就能先跑起来，后续再逐步补 Toolbar、Dialog、Toast、Tray、LogView 等产品组件。
### 2026-05-25 Developer 1 状态

- 已完成第一步最小切片：`WindowOptions` 公共 API、Win32 `Window::create(WindowOptions)`、`close/requestRedraw/post/nativeHandle/clientSize/setTitle/setFullscreen/setBorderless` 骨架。
- Win32 多窗口生命周期已从任意 `WM_DESTROY` 直接 `PostQuitMessage` 改为最后一个 native window `WM_NCDESTROY` 后才退出消息循环，关闭远程会话窗口不会直接结束仍存在的主窗口。
- `Window::post` 当前是基于 `PostMessage(WM_APP + 1)` 的 per-window callback 队列骨架；完整应用级 dispatcher、关闭回调、DPI/monitor 事件仍待后续补齐。

### 2026-05-25 Package size gate 状态

- 已新增 `scripts/check-package-size.ps1`，默认检查 `dist/OneUI-SDK-msvc-bundled-static.zip` 不超过 5MB。
- 该 gate 对齐远程产品路径：MSVC bundled-static SDK 是默认分发路径，MinGW/UCRT 动态依赖包不应作为远程客户端产品默认包。

### 2026-05-25 RemoteInputRegion 状态

- 已新增 `RemoteInputRegion` 最小运行时骨架，面向远程会话输入区，而不是普通文本/按钮控件。
- 当前公开 `PointerButton`、`RemotePointerEvent`、`RawKeyEvent`、`RemoteInputScaleMode`，支持窗口坐标到 content/normalized/remote 坐标的映射。
- 支持 `dispatchRawKey(...)`、`dispatchPointer(...)` 和 `releaseAllInputs()`，并且 `RemoteInputRegion` 失焦时会自动释放已按下的 pointer/key 状态，降低远端卡键风险。
- 当前仍是纯 OneUI 层抽象；right/middle/XButton 原生消息接入和更多特殊键平台细节还待后续 P0 slice。

### 2026-05-25 RealtimeFrameView 状态

- 已新增 `RealtimeFrameView` 最小运行时 API，面向远程会话实时画面，而不是普通图片控件。
- 公开 `PixelFormat`、`ScaleMode`、`VideoFrame`、`VideoFrameSnapshot`，支持 `submitFrame(...)`、`setScaleMode(...)`、`contentRect()` 和 `latestFrame()`。
- 当前支持 BGRA/RGBA 行拷贝和最新帧覆盖语义；旧帧不会排队堆积，测试覆盖了 stride、frameId、timestampUs 和 caller buffer 变更后的内部快照稳定性。
- `contentRect()` 已覆盖 ActualSize/Fit/Fill/Stretch，可给 `RemoteInputRegion` 做远端坐标映射参考。
- 当前仍是 P0 skeleton：`paint()` 还没有真实像素 blit；`Nv12` 只是 API 预留；非 UI 线程提交后的 dispatcher/invalidate 语义还需要下一步接到 `Window::post`。

### 2026-05-25 Monitor/DPI 状态

- 已新增 `include/oneui/platform/monitor.h` 和 Win32 `enumerateMonitors()`。
- `MonitorInfo` 包含 `index`、`bounds`、`workArea`、`scale`、`primary`、`name`，其中 bounds/workArea 使用 `Rect`，保留负坐标副屏信息。
- Win32 后端使用 `EnumDisplayMonitors` / `GetMonitorInfoW`，并动态加载 `Shcore.dll` 的 `GetDpiForMonitor`；旧系统或 API 缺失时回退到 1.0 scale，保持 Win7 兼容。
- 当前仍缺 DPI 变化事件、窗口跨屏移动后的自动通知，以及与 `Window` / `RealtimeFrameView` 的联动。

### 2026-05-25 本轮验收记录

- UCRT 构建通过，CTest 6/6 通过：control、overlay_host、scroll_view、realtime_frame_view、remote_input_region、monitor。
- MSVC bundled-static 产品构建通过，CTest 6/6 通过。
- 产品 runtime audit 通过：`oneui.dll` 只依赖系统 DLL；`oneui_gallery.exe` 只依赖 `KERNEL32.dll` 和 `oneui.dll`。
- SDK package 通过：`dist/OneUI-SDK-msvc-bundled-static.zip` 约 3.57 MB，低于 5 MB gate。
- SDK consumer smoke 通过；Gallery 已从产品 SDK 启动。
