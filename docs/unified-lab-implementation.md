# OneUI 统一试验台：实现与验收记录

本次迁移仍在进行，不能据此宣布三端全部上线就绪。试验台继续使用
`examples/performance_lab` 与 `oneui-performance-lab`，应用名称为 **OneUI 试验台**。

## 已接入的结构

- 两级导航包含布局、性能、功能，默认 `layout/basic`；27 个稳定页面 ID 见
  `examples/performance_lab/demo_catalog.cpp`。
- 页面按需创建并保留对象；离开页面调用停用钩子。代码面板默认收起，窄窗口使用导航抽屉。
- 启动参数、外壳、页面工厂、性能场景和桌面注册分别成模块。
- `--page <id>` 直达页面；原 `--view`、自动基准和稳定性命令仍走独立场景。
- CPU/GPU 对照及稳定性页面已接入独立子进程测试：复用旧场景与固定尺寸，
  页面离开时取消自己启动的测试。报告覆盖固定文件，不按轮次累积。
- 能力目录覆盖 124 个公开头文件，关联 27 个页面、关键交互、语言限制和测试入口；
  [完整目录](unified-lab-capabilities.md)由 `capabilities.json` 生成。
- 代码面板直接嵌入参与编译的源码，已移除 20 处未编译内嵌副本。当前有 50 个页面示例引用：
  27 个 C++、10 个 UTF-8 C、10 个 Rust 和 3 个 `.one`；共享源码按实际文件去重编译。
  `.one` 经过生成和 C++ 模板实例化；Rust 检查只属于 Demo 构建，不改变独立 SDK 的依赖。
- 多窗口页包含共享状态、关闭确认/取消、异步父子模态、关闭后投递抑制及退出策略。
  显式退出在演示中要求先成功启用托盘，提供恢复和退出入口。
- 托盘页已修正重复菜单 ID，补齐勾选、单选、隐藏/恢复与显式关闭到托盘。
- `run.ps1` 无场景参数时进入基础布局；显式旧场景参数仍进入兼容模式。普通构建只编 Demo，
  `-Test` 才编行为测试目标。
- 终端工作区已迁移并通过三端自动化回归；统一构建不再依赖旧终端入口。
  删除旧 `examples/terminal_workbench` 被自动审批以 `blocked by policy` 拒绝，源文件与约 252 MiB
  构建仍在磁盘；不能报告已经删除，也不换工具绕过。待手动清理见固定的 `blocked-cleanup.json`。
  Gallery、远程 Gallery 与声明式独立 Demo 仍待完成覆盖后删除。

## 终端工作区迁移对应

| 原行为 | 统一页面 / 实现 | 回归 |
| --- | --- | --- |
| 四组 Rust 终端、输入回调、关闭重开、会话筛选 | `features/terminal`；`terminal/sessions.cpp`、`oneui-lab-owner` | 原对象、输入和选区保留；100 次主题/重开循环 |
| 文件筛选、多选、新建、删除、模拟上传下载 | `terminal/files.cpp` | 按名称保留选择、模拟操作；不访问文件系统 |
| 分栏、折叠比例、窄屏页签、命令栏、可见内容搜索 | `views/TerminalWorkbench.one`、`terminal/workspace.cpp` | 640/1360 宽度切换、分栏和搜索状态 |
| 监控资源、进程与连接信息、显式开始/暂停 | `terminal/activity.cpp` | 离页取消、返回不自动启动、隐藏页空闲重绘为零 |
| C++ NativeHost 保留 Rust 所有者 | `terminal_tests.cpp` | 宿主释放后注销 Rust 回调；C++ 控件可继续存活 |
| 原终端跨系统窗口迁移 | `layout/docking`；`terminal_dock_tests.cpp` | 30 次跨窗迁移、取消、失败回滚、拆出与合并后所有者一次释放 |

新实现将文件模型、会话所有权和采样循环分成独立职责；新 C++ 实现模块各小于 250 行，
没有再将工作区业务追加到统一入口。Rust 回显有固定文本/网格上限，C 桥隔离 panic。
原 `workbench.h` 为 184 行压缩源码、正常格式化后 641 行；其旧副本因上述删除审查仍保留，
本次属于迁移已验证、重复旧实现删除受阻，不能将旧模块清理计为完成。
终端仍是本地回显示例，不是 PTY、ANSI 解释器或 SSH 客户端；真实接入由调用方实现。

## 桌面接口

- `Application`：窗口注册、退出策略、异步关闭确认、激活排队、弱 dispatcher。
- `NotificationService`：权限、稳定 ID、替换/撤销、发送结果和操作回调；与托盘解耦。
- `ToastHost`：应用内队列、同 ID 更新、超时、关闭和操作按钮。
- `Window`：状态观察、显示/隐藏、恢复、最大化、置顶、约束、所属窗口与输入限制。
- `ModalSession`：异步父子模态、父窗口输入恢复及结果交付。
- `DockWorkspace`：共享 C++ 布局模型，C ABI 和兼容 Rust 门面。
- `DockSession`：标签、分栏、独立系统窗口与拖放事务，保留原 Widget 和控制器所有者。
  Prepare/Commit 资源失败会回滚；空的自有浮动窗口在迁移完成后延迟关闭。
- `Tabs::setOnDetachRequested`：拖出标签栏时的入口，C 和 Rust 有对应回调。

新增 C 接口集中在 `application`、通知与窗口增量声明，以及
`dock_model_c.h`、`dock_session_c.h`；ABI 为 43，未改旧 C 结构体布局。
源代码与 DLL/lib 必须来自同一稳定构建。

ABI 43 新增 `oneui_dock_hit_test` 和 `oneui_dock_session_set_style_sheet`，
Rust 停靠手势已委托 SDK；三端使用同一套几何规则。新结构仅新增，未更改旧结构布局。
`DockSession::setStyleSheet` 为现有与未来浮动窗口更新样式；共享样式生命周期由会话保留。
生产 SDK 包含 688 个 C 导出，其中 19 个 DockSession 导出。

## 页面生命周期与模块拆分

连接工作区现在提供 `suspend/resume/close`：离页取消模拟保存及样式监听，保留草稿和控件，
返回创建新的消息通道，旧任务不能提交到新的页面代次；加载动画不会自动恢复。
`oneui-lab-workspace-tests` 覆盖 30 次快速返回、取消/重新保存、隐藏空闲重绘和终态关闭。
Windows、实机 Mac、Linux/Xvfb 的 C++ 和 `.one` 两种路径均已通过。
Windows 另回归两种入口各 300 个连接流程循环、84 个编写方式/主题/宽度场景。
原 `connection_workspace.hpp` 的模型、视图和样式责任已迁至同目录的
`connection_models.hpp`（369 行）、`connection_views.hpp`（114 行）、
`connection_styles.hpp`（96 行）；工作区协调器为 169 行。统计为 clang-format 后物理行数。

远程页增加 RGBA/BGRA、共享帧所有者、局部更新、光标模式和完整输入反馈，以及 C/Rust 示例。
NV12 只有保留枚举、尚未支持显示，页面验证拒绝结果并保留原帧，不做隐式格式转换。
三端 `oneui-lab-remote-tests` 验证 60 次完整帧/局部更新/所有者切换、坐标映射及重复释放；
另在真实窗口中验证播放、离页帧 ID 停止、隐藏空闲重绘为零、返回不自动播放、
30 次快速切换和页面销毁后不执行旧回调。

模态生命周期增加三端回归：最后一个同级模态结束后恢复父窗口原焦点与文字选区；
拒绝同一子窗口重复注册和模态所属循环，销毁会话解除临时所属关系但保留调用者的窗口。
移除焦点控件、关闭确认中注销窗口，以及关闭回调销毁 Application 的重入路径均已验证。
对应 `tests/application_modal_tests.cpp`，Windows、实机 Mac、Linux/Xvfb 均通过。

## 完整页面组合迁移

`layout/pages` 默认展示“应用外壳”，另一标签保留原列表与详情示例。页面均按需创建，
切换保留编辑器对象、草稿和选区。原远程组件 Gallery 的主要控件已归入这两个统一页面：

| 原场景 | 统一体验及边界 |
| --- | --- |
| AppShell、TopBar、NavItem、标题栏、状态条 | `layout/pages` 应用外壳；窄宽切换保留同一组控件 |
| 设备识别码、连接方式、开关、按钮 | 本地模拟连接与校验结果；生成识别码；不连接网络 |
| 搜索框、分段按钮、Badge、图标按钮 | 真实筛选设备富列表、空结果、密度切换和事件反馈 |
| 最近设备与状态动作 | VirtualList 富项目；查看详情及复制状态 |
| 原窗口最小化/最大化/关闭 | 组合页仅预览标题栏事件；真实系统操作在 `features/windows` |
| 远程画面及输入 | `features/remote`，共享帧所有权、局部更新和输入映射见该页 |

AppShell 隐藏侧栏时从绘制/输入树移除并保留原对象；恢复时重新挂载。
AppShell 和 TopBar 更新其他插槽时保留存续子树，避免重置编辑焦点与输入状态。
Rust 增加安全 `AppShell`、`TopBar` 封装，使用 19 个既有 C 导出；没有新增 C 导出或改动旧 C 结构体，
ABI 版本仍为 43。三个语言的真实片段为 `snippets/{cpp,c,rust}/product.*`，随试验台编译。

`oneui-lab-product-tests` 在 Windows、实机 Mac、Linux/Xvfb 检查输入校验、生成/筛选事件、
30 次主题/宽度切换、30 次组合页切换、侧栏绘制和焦点、工具栏插槽增删及失效回调。
Rust `oneui --test shell` 三端验证安全插槽 API、清除和 30 次折叠恢复。
NavItem、IconButton 支持 Enter/Space；StatusStrip 用方向键选择动作，Tab 继续离开。
回调先复制再执行，允许处理器销毁所属控件；禁用、隐藏和键盘回调销毁路径三端回归通过。
这些是控件和真实窗口中的程序化回归，不替代原生输入法、辅助技术或跨屏 DPI 验收。

空闲测试已改用 `rendererInfo().paints`，且要求基准计数大于零，避免关闭开发工具时
`developerSnapshot().frames` 恒零导致误判。连接、终端和远程页在三端重新通过隐藏停止采样/绘制测试。

## 图片与绘图页

`features/drawing` 增加双轴对齐、原对象像素更新/清空/恢复、图标选择和描边，
以及 Canvas 曲线、线性/径向渐变、裁剪、透明度参数。图像仍为本地生成，不读取用户文件或联网。
C++ 显示可编译 ImageView 和自绘 Widget 片段；C/Rust 显示现有 ImageView 入口，
不虚构不存在的 Canvas FFI。`oneui-lab-media-tests` 校验图片选项、状态保留、Canvas 参数和
绘制状态配对，并用非零实际绘制计数验证空闲停止；三端结果见固定验收目录。
Windows 深/浅、宽/窄截图经过一轮检查；下方参数通过页内滚动访问。

## 原生拖放

| 后端 | 实现 | 当前证据与限制 |
| --- | --- | --- |
| Windows | OLE IDataObject/IDropSource/IDropTarget；保留文件拖入 | 系统鼠标拖入与 Esc 回滚通过；Win7 完整构建、跨屏 DPI/IME 尚待本轮验收 |
| macOS | AppKit NSDraggingSession 与接收代理；保留文件拖入 | 实机系统鼠标跨窗合并、Esc 回滚、边缘分栏、拖出新窗口通过；跨屏 DPI/IME 与长时资源验证待完成 |
| X11 | Xdnd 进入/位置/选择数据/完成握手 | Xvfb 的真实指针及协议测试通过；WSLg 手势测试有间歇失败，需复核；未替代物理 Linux 验收 |
| Wayland | wl_data_device 的同进程数据拖放，使用原始按钮抓取 serial | WSLg 构建和事务测试通过；真实拖放待验收。连续顶层拖窗尚不可用，外部释放保持原宿主，保留显式拆出/合并 |

调用 `nativeDragSupported(host)` 查询数据拖放入口，调用
`nativeDragLimitation(host)` / `oneui_dock_session_native_drag_limitation_utf8`
获取限制说明。支持数据拖放不代表支持连续拖动整个顶层窗口。

Mac 原生手势验收由用户授权辅助功能后在 Terminal 启动，经过 CGEvent → AppKit
NSDraggingSession → 原窗口接收代理。验收发现并修复了粘贴板将 MIME 字符串当作 UTI
而拒绝载荷的问题，改为 `org.oneui.dock-v1` 并检查写入结果。四种手势同时断言控件身份、
文字、选区、目标分组、宿主数量与迁移回调；取消用例还确认真正创建了原生拖放载荷。
源码为 `tests/macos_dock_drag_tests.mm`，证据为
`out/native-release-review/unified-mac-native-drag.log`。测试需要授权时可显式使用
`--request-input-access`；普通测试运行不会主动申请系统权限。

`DockSession` 只接受当前同进程会话的临时令牌，不把外部字符串当作面板 ID。
外部宿主由调用者持有；应用必须比会话活得更久。嵌入页面停用时应隐藏宿主面板，
防止保留页面的旧几何成为投放目标。

## 2026-10-08 本轮验证

- Windows、Mac、X11/Wayland WSLg：迁移保留控件身份、内容、选区和焦点；拒绝、取消、
  回滚、孤立面板和独立窗口关闭路径的 C++ 测试。
- Windows、Mac：Rust DockSession 所有者保留/一次释放，14 个工作区模型测试。
- Windows：1082 个 ABI 布局检查与 592 个编译器函数签名检查通过。
- Windows：COM 冷激活、重复启动、注册升级及卸载路径通过；这不是实际点击系统通知的验收。
- Windows、Mac、Linux/Xvfb：独立 CPU/GPU 子进程完成，Unicode/空参数/引号/反斜杠传递、
  子进程取消回收与启动失败检查通过。Linux 的 GPU 回退仍标为不能作硬件对照。
- Windows：真实页面“开始测试”触发两轮 CPU/GPU 报告，后端分别为 raster/OpenGL，物理尺寸一致；
  “停止测试”触发取消，退出后无残留子进程。
- Windows：`scripts/test-lab-ui-win32.py` 通过观察窗口关闭确认、取消、关闭后投递、
  模态父窗口输入限制/恢复、全应用退出预检和取消。测试经 Win32 消息队列驱动实际控件；
  不代替物理指针、IME 或跨屏 DPI 验收。另修复页面析构操作已注销窗口导致的异常退出。
- Windows（软件 / OpenGL）、实机 Mac、Linux/Xvfb：`oneui-lab-terminal-tests` 覆盖上述终端迁移。
  Windows 另经 WM_CHAR 与鼠标消息验证终端输入和标签后的焦点路由；组合文字检查为合成状态，
  不声称操作系统输入法候选窗已验收。Linux 已修复 Rust 动态库 SONAME，Mac 使用 `@rpath` 库标识。
- 固定证据目录：本地 `out/native-release-review`，Mac
  `/Users/tuyikeji/.cache/oneui-release-review`。
- Windows 最新生产 SDK 身份见 `out/native-release-review/production-sdk-state.json`；
  开发工具和测试截图开关关闭，未引入强制 WinRT 导入。

Mac 的通知原生验收已由用户配合完成：授权弹框、通知实际显示、运行中正文点击、
`open` / `acknowledge` 操作回调、撤销及退出后冷启动唤起。冷启动前 PID 为 92424，
退出后经通知启动为 92466，未带原页面启动参数，页面仍显示正确唤起结果。
系统点击与视觉结果为用户实机确认，进程退出/重建由 SSH 核对；记录位于
`out/native-release-review/unified-mac-native-acceptance.json`。权限拒绝及替换的通知中心
去重、完整安装升级卸载仍未验收，不把成功请求当作视觉证据。

该验收发现 Mac Demo 原应用包签名不完整且签名标识与 Bundle ID 不同，系统通知守护进程
因此拒绝授权。现在 CMake 将匹配 SDK 和 Rust 运行库放入 `Contents/Frameworks`，
只使用包内相对 RPATH，从内到外签名并验证完整应用包。可用
`scripts/check-macos-lab-bundle.py --bundle <app> --output <report>` 复查身份、签名及依赖闭包。
默认本地 ad-hoc 签名，可配置 `ONEUI_LAB_MACOS_SIGNING_IDENTITY`；尚未做 Developer ID
签名、公证或 Gatekeeper 发布验收。当前包验证通过，记录为
`out/native-release-review/unified-mac-bundle.json`。

## 弹层交互迁移与回调生命周期

`features/notifications` 新增 `PopupShowcase`：三种外部点击策略、六种首选位置、
边界翻转、Esc 开关、`State<bool>` 绑定开关及双层模态确认。内部编辑器保留原控件、
文本和选区；切页关闭全部弹层，返回后参数和草稿仍在。通知页和 Toast 的回调在
停用或销毁后不再执行；系统通知服务仍与应用内弹层独立。

`OverlayHost` 在点击外部关闭时可能被回调移除自身或销毁宿主。原实现继续访问
已失效的容器引用；现使用受管控件引用、生命周期检查及输入顺序快照。滚轮分发中
移除其他弹层时跳过失效项，鼠标释放保留回调接收者直到返回。该职责从原 970 行
宿主拆为宿主/键盘编排 447 行、指针路由 286 行、焦点管理 294 行（格式化后的物理行数）。

窗口事件、模态会话、通知载荷对象改由桌面后端编译，避免 Windows 纯控件核心
因导出窗口类型而意外要求系统剪贴板实现。公开 C 结构和 ABI 版本不变。
新增 `overlay_host_lifetime_tests` 覆盖自移除、宿主销毁、释放回调和滚轮中修改层级；
`oneui-lab-popup-tests` 覆盖真实页面交互、30 次主题/生命周期切换、失效回调和实际
渲染计数空闲不增长。C++、UTF-8 C、Rust 弹层片段参与试验台构建；C/Rust 暂无独立
Esc 策略 setter，示例只展示其已有入口。

本轮 Windows、实机 Mac、Linux/Xvfb 均通过上述三项 SDK 回归和页面回归；Windows
另通过应用生命周期/父子模态测试及最新 ABI 检查。宽暗/窄亮页面已检查，Mac 当前
试验台打开到 `features/notifications`。本轮代码变更后的包身份验证单独记录，
原先用户确认的通知/拖放实测继续绑定其原测试产物，不能混作最新版实测。

用户确认暂时没有可用 Linux 实机，当前继续代码与自动回归；保留全部原生验收缺口。

## 必须继续完成

- 按已建立目录补齐每个能力组的全部交互与语言示例；当前所有页仍标为迁移中，
  `generate-lab-catalog.py --strict` 必须拒绝发布完成状态。
- Gallery、远程 Gallery、声明式迁移；迁移完成后清理对应旧入口、脚本、打包引用。
- 全部页面完整交互及普通页空闲验收仍待完成。三端 27 页 × 两种主题/宽度共 162 项
  CPU 启动/截图冒烟已通过；统一脚本为 `scripts/test-lab-pages.py`，仅复用一个截图文件。
  本次 Mac/Linux 临时启用 QA 截图开关，完成后恢复 OFF；不把截图冒烟作为输入法或 DPI 验收。
- 三端通知权限、实际通知点击/按钮、退出后唤起及安装升级卸载的原生证据。
- X11/Wayland 的物理环境跨窗拖放；Mac/X11/Wayland 的 IME、跨屏 DPI、
  资源上下文重建及长时间资源稳定性。
- Wayland 可用顶层拖拽协议接入与协议缺失时的完整降级说明。
- 模态及多窗口页面的 Mac/Linux 原生交互验收。
  等价停靠菜单已接入，包含拆出、合并、分栏、取消和标签排序。
- 三端统一 Demo 和 Rust 示例已编译；统一安装打包与 Win7 兼容回归仍待完成。
- 最终产物复核与清理。自动审批拒绝的可再生目录列于
  `out/native-release-review/blocked-cleanup.json`，尚未删除，不能算作已清理。
