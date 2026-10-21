# OneUI 移动端支持

状态：**Android 已接入共享 Skia GPU 控件、两级导航、基础布局／基础控件／图表／粒子，以及系统主题、系统栏外观、普通文字输入和输入页字号体验；完整移动试验台及 SDK 仍在开发，尚不可正式发布。**

用户已决定推进 Android、iOS 和原生 HarmonyOS 支持。已有桌面 SDK 和官网
`0.1.0-preview.1` 下载包没有加入移动能力，也没有被本轮源码变更覆盖。
移动版将使用同一套控件、版本分发和预编译依赖管理，业务项目不重复编译框架。

## 阶段与能力边界

| 编号 | 阶段 | 交付与验收 |
|---|---|---|
| MOBILE-001 | 共用运行时 | 生命周期、多指身份/取消、坐标与安全区域、键盘避让、Surface 代次、按需帧；C++/C/Rust 行为一致 |
| MOBILE-002 | Android 原生宿主 | 实际控件渲染、GPU Surface 重建、触控滚动、IME、返回操作、无障碍；真机输入/后台/功耗验收 |
| MOBILE-003 | iOS 原生宿主 | UIKit/Metal、触摸/手势、键盘与安全区域、Scene 生命周期、VoiceOver；模拟器与真机分别记录 |
| MOBILE-004 | 原生 HarmonyOS 宿主 | ArkUI/XComponent、生命周期、触控、输入法、无障碍、渲染与平台版本限制；不能以 Android 兼容运行代替 |
| MOBILE-005 | 按需系统插件 | 权限、相机、传感器、通知、文件、网络状态等按实际产品接入；支付按服务商和平台分别实现 |
| MOBILE-006 | 统一试验台和分发 | 同一试验台的移动布局、三端自动构建、签名、预编译包、下载清单及原生验收证据 |

顺序是公共层 → Android 实际交互闭环 → iOS / HarmonyOS 原生接入 → 插件与发布。
没有承诺整套移动支持已经完成。原生通知、相机、陀螺仪和支付没有因为出现
移动构建选项就自动可用；没有实现时不提供伪成功接口。

## 最终验收基线：统一试验台与 PC 同等完整度

用户在 2026-10-09 明确要求：移动端也必须做齐性能试验台，对齐 PC 端的完整度。
Android、iOS、原生 HarmonyOS 的最终交付均为 **OneUI 试验台**，统一“布局 / 性能 / 功能”
及稳定页面 ID；`tests/mobile/android` 的 GPU 方块和 Runtime 回归只证明第一阶段，
不计为试验台页面完成，不替代最终验收。

PC 的 [能力清单](../examples/performance_lab/capabilities.json) 和
[页面目录](../examples/performance_lab/demo_catalog.cpp) 是覆盖来源，当前共 27 页。
下面是必需覆盖的当前清单，不是已完成清单；PC 后续新增能力也必须进入移动端对齐矩阵。
PC 尚未完成的迁移或语言入口应保留为缺项，不能据此降低此前统一试验台计划的目标。

| 页面 ID | 移动端必须覆盖的体验与验证 |
|---|---|
| `layout/basic` | Stack/Grid/Wrap/Panel/滚动；方向、间距、空间分配、触控滚动 |
| `layout/forms` | 表单校验/提交、自适应重排、软键盘避让、焦点及草稿保留 |
| `layout/pages` | 完整列表/详情/编辑流程、筛选/空态、返回与未保存确认、导航和编辑状态保留 |
| `layout/workspace` | 分栏、标签排序/关闭、终端工作区；窄屏切换与平板多栏共用状态 |
| `layout/docking` | 同一控件/所有者迁移、分栏、排序、取消回滚；系统窗口能力逐平台核实 |
| `performance/overview` | 同源综合场景、原负载档位、可比较的独立基准模式 |
| `performance/charts` | 实时图表、追加/暂停/恢复；离页停止采样 |
| `performance/particles` | 原粒子路径、批量/网格模式与负载控制、画质及绘制耗时 |
| `performance/lists` | 万行虚拟列表/表格、快速触控滚动、稳定 ID 与选择 |
| `performance/materials` | 字体、段落、渐变、阴影、裁剪与原材质场景 |
| `performance/idle` | 动效、减少动态效果、交互结束空闲停绘和后台停止 |
| `performance/renderers` | 实际 CPU/GPU 后端、设备与回退原因；同负载/物理尺寸隔离比较 |
| `performance/stability` | 原稳定性场景、资源有界、反复切页与宿主重建、停止/取消 |
| `features/controls` | 全部基础控件；触摸/禁用/选择/进度/Begin-Commit-Cancel 真实事件 |
| `features/text` | 中文输入法、组合文本、Emoji/双向文字、选区、撤销重做、密码、多行与字体 |
| `features/data` | 列表/表格/树、多选、展开折叠、重排、滚动与原选择模型 |
| `features/drawing` | 图片、图标、像素更新/清空/恢复、Canvas 曲线/透明度/渐变/裁剪 |
| `features/notifications` | 弹层和 Toast；原生通知权限、替换/撤销、点击/按钮、运行中及冷启动唤起 |
| `features/menus` | 多级/勾选/单选菜单、命令路由与可用快捷键；托盘单独报告平台支持 |
| `features/windows` | 原生宿主实际/请求状态、全屏、尺寸变化、返回/关闭取消；置顶等逐项核实 |
| `features/multiwindow` | 生命周期、所属关系/模态、共享状态、无失效回调；原生多窗口/多场景逐项核实 |
| `features/files` | 系统文件打开/保存/目录、剪贴板、权限/取消；拖入与分享入口分别标明实际能力 |
| `features/terminal` | 原四组模拟会话、分栏、搜索、日志、文件/监控及内容保留；软键盘/触控可操作 |
| `features/remote` | 原本地模拟画面、帧/区域更新、缩放/坐标映射、文字/键码/释放；离页停播 |
| `features/reactive` | 状态/计算属性/绑定、UI 投递、异步任务/取消、NativeHost 所有权 |
| `features/theme` | 浅深主题、密度、语言切换、长文字/字号放大、状态样式及内容保留 |
| `features/diagnostics` | 语义树、焦点、读屏、布局快照、诊断、真实后端及 ABI/错误语义 |

### 完成条件

- **MLAB-A1 · 覆盖完整。** 按 `capabilities.json` 的每组公开能力、关键交互、语言和测试逐项对照，
  三端分别记录“未实现 / 已实现待验收 / 通过 / 失败 / 系统限制待确认”。不能只对齐页面标题，
  不能用空页、静态截图、原生按钮拼成的假控件页或删除入口通过检查。
- **MLAB-A2 · 同一套真实控件。** 复用 SDK 控件、页面模型、测试场景和状态；每页包含可操作体验、
  参数控制、结果反馈、默认收起的接入代码。平台原生服务通过标准接口接入，页面壳适配手机/平板，
  不直接缩小 PC 窗口。导航在窄屏使用抽屉/层级返回，宽屏保留两级侧栏。
- **MLAB-A3 · 移动交互完整。** 触摸、滚动惯性、手势取消/竞争、软键盘与组合输入、文本选择、
  系统返回、读屏、字号放大、安全区、横竖屏均有实际交互验证；硬件键鼠在支持设备上单独验证。
  不把旧鼠标单击映射当作触控完成，不把 Android 模拟器结果代替 iOS/原生 HarmonyOS。
- **MLAB-A4 · 生命周期与状态正确。** 惰性建页并保留编辑/选择/滚动状态；离页停止动画、采样与任务。
  前后台、Surface/渲染上下文重建、尺寸变化、宿主关闭取消均无误点击或失效回调。
  同一进程内迁移保留原控件及所有者；系统结束进程后的恢复另用可持久化业务状态验证，不能声称保留旧指针。
- **MLAB-A5 · 性能可比较。** 复用 PC 的场景、负载、画质检查和既有基准门槛，自动基准排除导航与代码面板。
  记录设备/系统、后端、物理尺寸、刷新率、负载、温度/节流状态；区分回调频率、CPU 绘制、GPU 和实际呈现。
  手机与 PC 不要求相同 CPU 百分比或任意固定 FPS；各目标设备的帧预算、耗电/温升和回归阈值须在验收前固定，
  不得测完再放宽。空闲/后台停止不必要帧，原 1800 秒稳定性测试及生命周期循环不得持续资源增长。
  OS 不能直接启动基准子进程时使用原生测试启动机制隔离运行；缺失后端/遥测项明确报告，不能伪造比较。
- **MLAB-A6 · 平台差异不掩盖缺项。** 托盘、置顶、任意跨系统窗口拖放等按 OS/版本/设备能力分别核实，
  保留页面和可查询能力结果，返回明确“不支持/权限拒绝/失败”。可用的替代交互单独展示，不能冒充原能力；
  系统限制需证据和用户验收后才能作为例外，AI 不得自行删项或标为完成。
- **MLAB-A7 · 接入与发布完整。** 与 PC 一样提供真实 C++、UTF-8 C、安全 Rust 示例，适用处提供 `.one`，
  片段参与目标平台编译。保持统一生命周期/错误语义和 ABI 规则；官网提供匹配版本的预编译 SDK、
  依赖/最低系统说明和试验台安装方式，消费者不重编框架。安装、升级、权限和必要注册均有实际证据。
- **MLAB-A8 · 三端分别验收。** Android、iOS、原生 HarmonyOS 各有匹配构建哈希和固定版本报告，
  逐页交互结果、真实截图、原生输入/通知/文件/辅助功能证据及手机/平板适配记录。模拟器与真机分开；
  仍有通用能力缺项、失败或未经用户接受的系统限制时，不宣布该平台达到 PC 同等完整度。

终端、远程画面、连接流程继续使用 PC 试验台约定的本地模拟数据，并明确标记；
控件交互和平台服务验收仍使用真实实现。相机、传感器、权限和后续支付插件要在同一试验台
“功能”分类中纳入对应场景与失败/取消验收，不另建长期独立产品 Demo。

执行顺序保持公共层 → Android 完整试验台 → iOS / 原生 HarmonyOS 同一目录对齐。
阶段性测试记录继续有效，但只能证明其明示范围；最终完成以 MLAB-A1～A8 为准。

### 基础能力优先级

按用户最新方向，先补齐影响所有页面的宿主能力，再继续页面迁移：

| 能力 | 当前 Android 状态 | 后续验收 |
|---|---|---|
| 系统深浅色、应用跟随/覆盖、状态栏/导航栏外观 | 公共 C++/C/Rust 接口及 Android 适配已接入；API 36 模拟器通过 | 最低系统和 ARM64 真机；iOS/HarmonyOS 适配未实现 |
| 安全区域、系统返回、前后台与 Surface 重建 | 现有宿主回归已覆盖，并纳入外观回归 | 平板/多窗口、键盘避让、进程被终止后的业务恢复 |
| IME、文本选区、系统字号与无障碍 | 部分实现 | 普通输入、组合协议与输入页字号已接；普通字段手柄已接；候选窗/读屏/全目录字号待补 |
| 权限、剪贴板/文件/分享、通知唤起 | 移动端未完成 | 原生权限允许/拒绝/取消、后台限制及唤起 |
| 相机、传感器、支付 | 移动端未实现 | 后续按实际平台和服务商分别接入 |

此顺序不取消 PC 对齐清单。移动系统缺少的托盘等能力仍须明确报告限制，不伪造桌面行为。

### 外观 SDK 与 Android 系统栏

- 公共接口：[C++ Appearance](../include/oneui/mobile/appearance.h)、
  [版本化 C ABI](../include/oneui/mobile/appearance_c.h)、
  [安全 Rust 包装](../bindings/rust/oneui-mobile/src/appearance.rs)。旧 Runtime V1 结构和桌面 ABI 43 不变。
- 分开读取 `system_theme` 与 `effective_theme`：固定深色不改变系统的真实主题值。
  默认跟随系统；未知系统主题明确返回 0，界面回退浅色；不擅自更改设备设置。
- `setOptions` 仅接受请求。没有连接原生宿主时 `host_available=0`；
  接入层成功应用后才确认当前 `revision`，`applied_revision` 不代表 GPU 已呈现。
  相同状态不产生新版本，没有查询定时器；所有调用与销毁限创建线程。
- 每栏独立的不透明 `0xAARRGGBB` 背景色，图标支持自动/浅色/深色；自动按相对亮度选择对比度。
  非法枚举、透明色被原子拒绝，保留原样式。当前不提供隐藏系统栏和内容穿透的全屏接口。
- Android 可复用的接入层是
  [SystemAppearance.java](../platform/android/java/net/too1/oneui/SystemAppearance.java)，
  安装到 SDK `share/oneui/android/java`。Activity 把 Surface 放入 `content()`，
  首次启动、配置变化和恢复前台时把 `systemTheme()` 交给 C++ `observe`；
  读取已解析的颜色/图标调用 `apply`，成功后确认相同 revision。
  此 Java 类不持有 C++ 裸指针；具体 JNI 连接示例见 `mobile/android/host.cpp` 和 `LabActivity.java`。
- Android 15+ edge-to-edge 下，在透明系统栏后绘制真实保护背景，设置实际系统图标样式。
  状态栏、导航栏、刘海、横屏侧边安全区仅扣减一次；Surface 已位于安全区内，
  C++ viewport 边距传零。应用内标题区由 OneUI 控件绘制，示例将其与顶部颜色同步，
  不是系统窗口标题栏。[Android 官方说明](https://developer.android.com/develop/ui/views/layout/edge-to-edge-manually)
- `features/theme` 的六个按钮提供跟随/浅色/深色与三套配色；页面展示系统主题、应用模式与接入结果。
  切页、旋转、前后台和 Surface 重建保留状态；进程退出后默认跟随系统。
  语言切换、系统字号、主题持久化尚未接入，不能将该页算作完整通过。

真实、参与编译的接入示例：
[C++](../examples/performance_lab/mobile/appearance_snippet.cpp)、
[C](../tests/mobile/appearance_c_tests.c)、
[Rust](../bindings/rust/oneui-mobile/tests/appearance.rs)。原生宿主同样需要接入，不能只创建公共状态对象便宣称具备系统服务。

自动验证：公共构建新增 `oneui_mobile_appearance_tests`、`oneui_mobile_appearance_c_tests`；
Rust 增加外观行为及不可跨线程的 compile-fail 检查；
`scripts/test-android-appearance.py --adb <adb> --serial emulator-5554`
通过真实触摸、系统夜间模式和截图像素核对外观、安全区域、旋转/后台/Surface 与空闲停绘。
脚本恢复设备夜间模式及旋转设置，覆盖固定报告 `out/mobile-current/android-appearance-test.json`。

## 当前源码

- `include/oneui/mobile/c_api.h`：独立移动 ABI 1，旧桌面 ABI 43 及结构布局不变。
- `include/oneui/mobile/runtime.h`：无渲染器依赖的 C++ 状态机。
- `include/oneui/mobile/widget_host.h`：嵌入现有 `Widget`，使用宿主提供的逻辑坐标 `Canvas`。
- `src/platform/android/mobile_input_android.cpp`：NDK 触摸事件到共享运行时的转换。
- `bindings/rust/oneui-mobile`：独立的安全 Rust 包装，使用已编译库，不自动编译 SDK。

`Runtime` 不持有原生窗口或 GPU 资源，不开后台线程，不偷偷创建事件循环。
`WidgetHost` 当前把首触点接到旧控件的鼠标交互入口；多指数据已保留，但触摸
滚动目前只有可选的 ScrollView 竖向拖动及横向控件竞争；惯性、长按、双击、
嵌套滚动接力与缩放竞争尚未完成，不能称完整手势支持。
移动根布局与桌面布局可以不同；现有控件和业务状态可以保留同一实例。

## 运行时协议

1. 在宿主 UI 线程创建运行时。所有调用与销毁都在该线程；C 返回 `WRONG_THREAD`，
   Rust 类型不可 `Send` / `Sync`。没有跨线程回调或隐式调度。
2. 提供物理像素尺寸、像素/逻辑单位比例、安全边距和键盘底部遮挡。键盘与底部安全区
   取最大遮挡，不重复相加。如果 OS 已经缩小 Surface，键盘遮挡传零。
3. 获取有效 Surface 后 `setSurface(true)`，前台可交互时进入 `ACTIVE`。
   失焦进入 `INACTIVE`，隐藏进入 `BACKGROUND`，最终退出进入 `DESTROYED`。
4. 原生指针输入使用稳定 ID 与单调纳秒时间。输入坐标是物理像素，出队坐标是
   全 Surface 的逻辑坐标，不减安全区。`content_x/y` 提供布局起点。
5. 立即消费输入，再检查 `needs_frame`。只在需要时申请系统下一帧；帧回调调用
   `takeFrame` / `take_frame`。`EMPTY` 表示不绘制，不自行建立空闲帧循环。
6. 丢失 Surface 时先 `setSurface(false)` 再释放 GPU 资源；恢复后重新建立资源，
   再 `setSurface(true)`。`surface_generation` 可识别必须重建的上下文。
7. 暂停、旋转、安全区域变化、Surface 丢失会清除排队中的点击并发出取消。
   即使后台不绘制也要消费取消，清除控件按下状态，不能把取消变成抬起点击。
8. 指针上限 16，待消费事件上限 128；溢出报告 `CAPACITY` 并取消整段交互。
   宿主应在每个原生事件后消费，不让输入无限积压。

所有枚举值、浮点数、比例、坐标、边距与时间顺序均校验。返回 `OK` 只表示该状态
操作被接受；不代表系统权限、相机启动、支付或帧呈现成功。
GPU 呈现失败由宿主调用 `invalidate` 重试，并按平台规则重建资源。

## 构建和接入

桌面默认不开启移动模块。可在已有桌面配置增加
`-DONEUI_BUILD_MOBILE_RUNTIME=ON`，定向构建 `oneui_mobile_widget_tests`，
不需要构建所有 Demo 或重编 Skia。

仅编译状态机及 C/C++ 测试，无 Skia / Rust / Android SDK 依赖：

```sh
cmake -S . -B out/mobile-current/host -DONEUI_MOBILE_RUNTIME_ONLY=ON -DBUILD_TESTING=ON
cmake --build out/mobile-current/host --config Release
ctest --test-dir out/mobile-current/host -C Release --output-on-failure
```

固定目录复用，不按日期复制。执行前遵守项目产物预算；预算例外只适用于用户批准的工作轮次。
本轮移动验证已获用户批准，总量上限为 35 GiB；没有下载 SDK、模拟器镜像或重建桌面 Skia。

有真实 Android NDK 时可交叉编译运行时与原生输入适配器：

```sh
cmake -S . -B out/mobile-current/android-arm64-v8a \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 \
  -DONEUI_MOBILE_RUNTIME_ONLY=ON -DBUILD_TESTING=OFF
cmake --build out/mobile-current/android-arm64-v8a
```

上述命令仅生成原生基础库。实际 GPU 控件使用下文的 Android 试验台构建选项；普通输入已使用独立 TextInputSession/EditorSurface 接入，完整 IME 验收仍未完成。
`android-24` 是此阶段的配置示例，尚未经过最低系统真机认证。
iOS / OHOS 工具链同样只能使用 `ONEUI_MOBILE_RUNTIME_ONLY=ON` 构建公共层；
完整 SDK 配置会明确报错，防止错误地链接 Cocoa 或 Linux X11/GTK。

Rust 指定预编译库所在的绝对目录后运行：

```sh
ONEUI_MOBILE_LIB_DIR=/absolute/path/to/library cargo test \
  --manifest-path bindings/rust/Cargo.toml -p oneui-mobile
```

多配置生成器的库通常在 `out/mobile-current/host/Release`。
默认静态链接；共享库使用 `ONEUI_MOBILE_LINK_KIND=dylib` 并配置系统库搜索路径。
Android / OHOS 静态链接需显式指定匹配的 `ONEUI_MOBILE_CXX_RUNTIME` 和工具链库路径。
不能把桌面 `.lib`、macOS `.dylib` 或 Linux `.so` 当作移动目标库。

原生库可用 `cmake --install out/mobile-current/host --config Release --prefix <固定SDK目录>`
安装；CMake 消费者使用 `find_package(OneUIMobile CONFIG REQUIRED)` 与
`OneUI::mobile_runtime`，不会触发源码下载。Widget 桥目前只在源码工程内启用，
尚未作为移动渲染 SDK 对外发行。

Windows 控件宿主验收也可直接消费冻结的桌面 SDK，避免重编框架和依赖。在 MSVC 环境执行：

```powershell
cmake -S tests/mobile/sdk-host -B out/mobile-current/widget-sdk -G Ninja `
  -DCMAKE_BUILD_TYPE=Release -DONEUI_SDK_DIR=E:/project/byname/oneui/out/sdk-current/oneui-modern-x86_64
cmake --build out/mobile-current/widget-sdk --parallel 1
ctest --test-dir out/mobile-current/widget-sdk --output-on-failure
```

此配置的两组测试使用既有控件与 RecordingCanvas 验证生命周期、触控竞争，以及共享基础布局页
的参数/主题/网格几何。它不能替代下面的 Android 实际控件渲染验收。

## Android 统一试验台：基础布局接入

产品宿主位于 `examples/performance_lab/mobile/android`，应用名为“OneUI 试验台”，包名
`net.too1.oneui.lab`。当前只接入 `layout/basic` 的操作区，**这页仍是部分实现**：
完整导航、代码面板、多语言片段和移动可访问性尚未完成，其余 26 页也未接入。
不能把基础布局的 8 组回归当成 MLAB-A1～A8 全部通过。

Activity 只提供 Surface、系统栏安全区、生命周期和原始触摸事件。页面、按钮、滑块、
Stack/Grid/Wrap、ScrollView、绿色浅深主题，均为与 PC 相同的 C++ OneUI 控件。
Skia Ganesh/GLES3 绘制，SkParagraph/HarfBuzz/ICU 排版，Android 系统字体经 FreeType 加载。
没有使用 Android Button/TextView 模拟正常运行的 OneUI 页面；Android TextView 仅用于宿主创建失败提示。

| 当前交互 | 行为与边界 |
|---|---|
| 方向按钮 | 同一 Stack 横/纵切换，结果文字更新；预留高度随方向和间距调整 |
| 间距滑块 | 0～40，实际调整 Stack/Grid；横向手势交给滑块，保留值 |
| 列数滑块 | 1～4，实际调整六个网格块；行高和行间距参与预留高度，后续控件不能重叠 |
| 竖向滚动 | 超过 8 个逻辑像素后取消子控件按下，直接移动 ScrollView；松手停止，尚无惯性 |
| 主题按钮 | 切换共享浅深主题；保留控件身份、方向、参数和滚动位置 |
| 原生生命周期 | Surface/GL 上下文重建、后台恢复与横竖屏保留同一页面对象；后台和空闲停绘 |

`LayoutPage` 从桌面平台依赖中抽到 `pages/basic_layout_page.cpp`，原桌面工厂也调用它。
发现并修复的网格高度问题同时覆盖 PC 与 Android：只设置 flex-basis、未设置最小高度时，
Yoga 在无约束内容测量中可把 Grid 算成零高；现在用实际行数与间距确定高度，并加入回归断言。
窄到单列的 Wrap、高字号、平板导航和原生读屏仍需后续补齐。

构建只给负责 OneUI 的会话使用，业务消费者等待官网移动预编译包：

```powershell
python -B scripts/build-skia-android.py `
  --ndk C:/Users/Administrator/AppData/Local/Android/Sdk/ndk/29.0.14206865
python -B scripts/build-android-probe.py --workbench `
  --sdk C:/Users/Administrator/AppData/Local/Android/Sdk `
  --java-home "C:/Program Files/Eclipse Adoptium/jdk-17.0.18.8-hotspot" `
  --yoga-source build/msvc-bundled-static-release/_deps/oneui_yoga-src
python -B scripts/test-android-lab.py `
  --adb C:/Users/Administrator/AppData/Local/Android/Sdk/platform-tools/adb.exe `
  --serial emulator-5554
```

Skia 复用仓库已固定的 `1f26101197bff9fcd939a791beb3094297436d59` 源码与已安装 NDK，
输出复用 `out/mobile-current/skia-x86_64`；不重新下载依赖。Yoga 指向已有固定源码。
OneUI 复用 `out/mobile-current/android-x86_64`，通过 `ONEUI_BUILD_ANDROID_LAB=ON` 进入
独立移动构建分支，不链接桌面 Window 后端。默认桌面构建和 SDK 下载包不变。

2026-10-09 的 x86_64 debug APK 为 `out/mobile-current/android-x86_64/oneui-mobile-lab.apk`，
7,246,306 字节（约 6.91 MiB），SHA-256：
`3cc8fba4f59c221697522c3e32e220a9da3581185d0cf98e0d8dcbf801504ad3`。
这不是通用 APK、ARM64 产物或正式签名发行包，不能据此承诺完整移动 SDK 体积。

固定报告 `out/mobile-current/android-lab-test.json` 记录 8 组通过：空闲停绘、方向按钮、
滑块/手势竞争、滚动/网格几何、主题保留、20 次 Surface 重建、后台恢复、横竖屏。
窗口物理尺寸 1080×2400，Surface 1080×2205，scale 2.625；横屏 Surface 2268×943。
重建前后 PSS 为 77,826 / 85,727 KiB，增加约 7.72 MiB，符合预先设定的 64 MiB 冒烟保护阈值。
该短循环不证明没有泄漏，也不能替代 1800 秒稳定性、真机温升和功耗验收。
计数 `frames` 表示成功提交 EGL 交换的次数，**不是系统实际呈现 FPS**。

测试诊断广播只在 debug 宿主注册，并要求调用者具备系统 `DUMP` 权限；测试脚本只操作
明确指定的模拟器。诊断读取不重启 Activity、不主动请求帧；前台广播避免读取 Android 缓存进程
延迟投递的旧状态。正常页面没有轮询日志或持续帧回调。

## Android 导航与同源页面（2026-10-09 后续接入）

`examples/performance_lab/mobile/workbench.*` 从 PC 的 `pageCatalog()` 读取完整 27 项目录。
手机使用可滚动的两级目录，内容宽度达到 840 逻辑像素后保留侧栏；判定依据为当前 Surface
宽度，不判断设备型号。未接入的 21 项显示“待接入”且禁用，不创建空白页面冒充实现。
六种实际体验仍需完成各自能力组与移动原生验收，不能把入口已接入计作整页完成。

| 已接入 ID | 真实实现与本阶段边界 |
|---|---|
| `layout/basic` | 复用 PC 基础布局页面；方向、间距、列数和滚动状态保留 |
| `features/controls` | 从 PC 控件页抽出同一 `createBasicControlsPage`；按钮、禁用、勾选、开关、单选、选择、滑块与进度；完整能力组验收仍待补齐 |
| `performance/charts` | 复用 `plots.hpp` 的三条曲线及 360 / 1200 / 4000 点档位；开始／暂停、切换负载 |
| `performance/particles` | 同源 500 / 2500 / 10000 粒子和五种路径；Android mesh 实验路径未启用，明确回退到批量绘制 |
| `features/theme` | 系统主题查询/跟随/覆盖、上下系统栏颜色及图标、应用标题区示例；本地化、字号等未完成 |
| `features/text` | 原 OneUI 单多行输入、预编辑/提交、选区手柄与撤销、密码命令、键盘避让、输入页 SP 字号；完整 IME/读屏待补 |

### 当前按钮与生命周期契约

- **目录／布局／性能／功能：** 点击后进入对应目录；已接入项惰性创建，其他项不响应。
  切换前取消指针手势并停用旧页，保留原页面所有者、控件实例、参数、选择与滚动状态。
  导航变更在输入分发结束后执行，不在控件回调中拆卸正在分发事件的树。
- **深色／浅色：** 应用共享主题，保持各页状态。移动壳将单列交互控件的触摸框提高到至少
  48 逻辑像素，不改桌面控件默认尺寸；字体随系统字号放大仍是待完成项。
- **查看接入代码／收起代码：** 默认收起；展开后停用被遮住的页。直接展示仓库六个 C++
  片段，CMake 将相同源文件加入 Android 目标实际编译；尚无移动控件 C / Rust 对应入口时
  明示待补齐，不生成虚构示例。公共 Runtime 的 C / Rust 包装不等于控件语言接入已完成。
- **系统返回：** 先关闭目录，再关闭代码面板；其他页面回到保留的基础布局页，根页退出
  Activity。API 33 以上注册系统 Back 回调，旧版类型引用隔离在独立内部类；最低 API 的
  实际运行和预测返回动画仍待验收。
- **开始／暂停、负载、绘制路径：** 使用原 PC 绘图模型。离页、窄屏目录遮挡、代码面板、
  失焦、后台或 Surface 不可用时停止；恢复时按保留的运行状态继续，重置时间基点避免跳变。
  移动模型不启动桌面后台进程监控线程，绘图不会另行复制一套随机负载。

当前是可交互场景，不是已验收的移动独立基准：视口几何不同，不能直接和 PC 帧率比较；
成功 EGL 交换次数、动画回调次数与实际呈现 FPS 分别记录，GPU 耗时与功耗尚未采集。
稳定性 1800 秒、最低 Android、ARM64、真机及 iOS / HarmonyOS 验收仍然缺失。

新增回归 `scripts/test-android-navigation.py` 只使用真实触摸和系统返回操作，诊断接口只读：
惰性建页、代码与目录返回、遮挡停绘、隐藏页停绘、后台恢复、图表暂停、8 轮往返缓存有界、
横屏侧栏与主题保留。固定报告 `out/mobile-current/android-navigation-test.json`；原有
`android-lab-test.json` 继续覆盖参数、滚动、20 次 Surface 重建和旋转。最终 APK 的哈希、
大小与逐项结果以 `out/mobile-current/validation.json` 为准；上文早期 APK 数字仅为历史证据。

实现按职责分为移动壳（导航、页面所有权）、场景页（负载与动画）、Android JNI（宿主和输入）。
PC `control_pages.cpp` 的基础控件职责迁到 `basic_controls_page.cpp`，由两端共同调用；
没有新增第二套基础控件演示。普通桌面 `Model` 仍默认开启进程采样，移动实例显式关闭。

## Android 集成测试宿主

`tests/mobile/android` 是用于验证移动宿主链路的最小测试消费者，不是第二套产品 Demo，
也不是已移植完整控件的试验台。它包含 Activity、SurfaceView、JNI 和 EGL/GLES 2 绘图区，
接入真实 C++ Runtime。文字和操作按钮使用 Android 原生控件，绿色方块只是 GPU 测试图形；
不能把截图解释成 OneUI 控件、Skia、IME 或无障碍桥已移植。

- Activity 前台、失焦、后台、销毁对应 Runtime 生命周期；原生对象只在 UI 线程访问。
- Surface 销毁前取消输入、释放 EGL/NativeWindow；重建后保留 Runtime 和触点位置，代次递增。
- 只在 Runtime 请求绘制时注册一次 Choreographer 回调，空闲时停止；呈现失败停止并显示错误，
  可通过“重建绘制表面”恢复，不启动无限失败重试。
- 触控带稳定触点 ID、单调时间及物理坐标，支持取消；不将次触点误作主触点释放。
- 系统栏由 Android 宿主留出空间，因此 Surface 内安全边距为零。旋转调整同一宿主的尺寸。

本机已发现 Android NDK `29.0.14206865`、API 36、Build Tools 36.1.0、JDK 17 和 Pixel 8
x86_64 AVD。脚本仅使用已安装工具，直接打测试 APK，无需下载 Gradle 或复制 SDK：

```powershell
python -B scripts/build-android-probe.py `
  --sdk C:/Users/Administrator/AppData/Local/Android/Sdk
```

产物固定为 `out/mobile-current/android-x86_64/oneui-mobile-probe.apk`。
API 28 是此测试宿主的最低配置，并非 SDK 最低系统兼容承诺；当前使用标准 Android debug key，
不可作为正式签名发行。构建前仍需满足本轮产物预算。安装时必须使用 `adb -s <模拟器序列号>`
明确指定设备，不默认操作 adb 列表中的用户真机。

安装后可运行有界的模拟器回归（已有 Python 与 Pillow）：

```powershell
python -B scripts/test-android-probe.py `
  --adb C:/Users/Administrator/AppData/Local/Android/Sdk/platform-tools/adb.exe `
  --serial emulator-5554
```

脚本拒绝真机序列号，只启动测试包；恢复旋转设置并清理设备端临时 XML。
画面像素在内存中检查，不积累截图。固定报告为 `out/mobile-current/android-test.json`。

## 已执行的基础验证（2026-10-09）

| 环境 | 实际结果 |
|---|---|
| Windows x64 / MSVC 19.50 | C++ Runtime 与 C ABI 两组行为测试通过；生命周期、输入取消、边界与线程规则 |
| Rust / 预编译运行时 | 运行时集成测试、禁止跨线程传递的 compile-fail 文档测试通过 |
| Windows / 冻结桌面 SDK | WidgetHost 控件所有权、原实例保留、空闲停绘、取消点击、次触点隔离测试通过 |
| Android NDK 29 / x86_64 / API 28 编译基线 | 原生 Runtime、NDK 输入适配器、JNI/EGL 库与 Java APK 实际编译；debug APK 签名校验通过 |
| Pixel 8 模拟器 / Android API 36 | 空闲 3 秒不增帧；触控后停绘；连续 3 次 Surface 重建保持位置；后台停绘与恢复；旋转后继续绘制，5 组检查通过 |

模拟器报告 `Android Emulator OpenGL ES Translator (NVIDIA GeForce RTX 5080/PCIe/SSE2)`。
这是主机 GPU 加速的模拟器证据，不代表 Android 真机的功耗、性能或兼容性。
测试包约 120 KiB，只含基础运行时与 GPU 方块，不能据此估计完整 OneUI 移动 SDK 的体积。
已修复直接 javac 编译缺少 `core-lambda-stubs.jar`，以及窗口内容建立前访问 InsetsController 的启动错误。

宿主行为参考 [SurfaceHolder.Callback](https://developer.android.com/reference/android/view/SurfaceHolder.Callback)
与 [Choreographer](https://developer.android.com/reference/android/view/Choreographer)。

## 尚需完成的验收

本轮具体结果统一记录在 `out/mobile-current/validation.json`；没有实际执行的项目
必须是 pending，不能用代码检查或桌面单元测试替代原生设备验收。

- Android：其余 21 个页面入口的实际实现、已接入页面的完整能力组覆盖与语言示例、完整手势/惯性、中文候选实测/选区连续滚动、TalkBack、全目录字号放大、arm64 和最低系统兼容、真机后台/旋转/GPU 恢复及功耗。
- iOS：iPhone SDK、UIKit/Metal 宿主、Scene/IME/安全区域/VoiceOver、签名及真机。
- HarmonyOS：对应 SDK、XComponent 宿主、ArkTS 桥、原生设备、输入法/无障碍。
- 三端：统一试验台、系统能力插件、预编译分发与可复核构建来源。

原生参考：[Android 输入](https://developer.android.com/ndk/reference/group/input)、
[NativeWindow](https://developer.android.com/ndk/reference/group/a-native-window)、
[UIKit 生命周期](https://developer.apple.com/documentation/uikit/managing-your-app-s-life-cycle)。

## Android 文字输入基础增量（2026-10-09）

`features/text` 为第六个真实页面，仍是局部覆盖：普通 `TextField` / `TextArea`、
单多行编辑、全选、撤销重做、系统键盘避让、页面实例及已提交文字保留。
新增 `oneui/mobile/text_input.h` 的 `TextInputSession`，原控件及桌面 ABI 43 不变；
Android SDK 适配器 `platform/android/java/net/too1/oneui/EditorSurface.java` 使用实际
`InputConnection` 连接原 C++ 控件，没有用隐藏 EditText 替代 OneUI 绘制。

- JNI 边界统一 UTF-16 索引；Windows 的 16 位 wchar_t 与 Android 的 32 位 wchar_t
  分别验证中文、emoji、选区及码点删除。非法代理项、半个代理对的选区被拒绝。
- `setComposingText` 只显示预编辑，提交才进入控件撤销栈；切换焦点、外部撤销/改文、
  后台关闭连接后，旧 session 不能写入新编辑器。预编辑被取消不冒充已保存草稿。
- Android `SystemAppearance` 对系统栏/刘海与 IME 使用最大遮挡值，一次调整 Surface；
  C++ viewport 不再扣减键盘高度。Surface 尺寸变化保留焦点，布局后滚动到光标。
- Android `TypedValue.applyDimension(SP, 16, metrics)` 换算本页字体，不把 fontScale
  当作屏幕 DPI，也不自行假设高版本系统线性缩放。200% 系统字号与输入框高度一起调整。
  **目前只覆盖输入页的文字/输入框；全目录字号、按钮字号和 TalkBack 仍待完善。**
- `WidgetHost::paintWhenNeeded` 在动画检查后才调用 Canvas 工厂；光标闪烁之间没有
  无效 GPU 帧。`callbacks` 是动画检查次数，`frames` 是成功交换次数，都不等于显示器
  实际呈现 FPS。当前仍有按刷新率检查光标的回调，低频唤醒优化待后续调度接口。

接入顺序：同一 UI 线程创建 WidgetHost、控件根和 TextInputSession；EditorSurface.Bridge
转发 snapshot/apply。用户点击后 `sync(true)`，其他编辑/布局后 `sync(false)`；后台/销毁前
先 suspend Java 连接，再 reset C++ session。取消原控件指针状态后调用
`restoreComposition()` 恢复仍有效的预编辑，布局变化后 `revealCaret()`。
可编译片段是 `examples/performance_lab/mobile/text_snippet.cpp`，随 Android 目标编译。
**这一控件宿主入口的 C/Rust 包装仍待接入**，不把已有 Runtime/Appearance 语言包装算作完成。

验证入口：

```powershell
python -B scripts/build-android-ime-test.py --sdk <AndroidSdk> --java-home <JDK17>
python -B scripts/test-android-text.py --adb <AndroidSdk>/platform-tools/adb.exe --serial emulator-5554
```

测试夹具在单独 APK 中，包含实际 Android Instrumentation 和 InputMethodService；
真实系统 IME 窗口、触摸按键、InputConnection、JNI 和 C++ 控件均参与测试，
结束恢复原输入法、字号/旋转设置并卸载夹具。夹具用于可复现的组合输入和键盘区域验证，
**不能替代 Gboard/其他中文输入法的候选窗、手写、语音、浮动键盘体验验收**。
本机 Gboard 验证期间曾显示浮动手写工具栏，系统报告 IME 底部遮挡为 0；不伪造为停靠键盘。
固定报告 `out/mobile-current/android-text-test.json`、`ime-protocol.log`；
C++ 同一测试源 `tests/mobile/text_input_tests.cpp` 分别运行 Windows 和 Android。

仍未支持：选区边缘连续滚动/放大镜、全方向键与复杂双向文字、自动填充、
读屏、富内容/手写/语音及复杂输入法兼容；自定义编辑器不是 TextField 时不开放本桥。
iOS/HarmonyOS 的原生输入与字体接入、C/Rust 控件宿主和移动预编译发行仍未完成。

原生依据：[InputConnection](https://developer.android.com/reference/android/view/inputmethod/InputConnection)、
[Android 14 非线性字号](https://developer.android.com/about/versions/14/features#non-linear-font-scaling)。

## Android 密码与编辑菜单增量（2026-10-10）

密码仍由原 OneUI `TextField::setPasswordMode(true)` 绘制。公共 C++
`TextInputSession::snapshot()` 在敏感模式只返回每 UTF-16 单元一个遮罩字符及选区；
`apply()` 拒绝写入敏感字段，避免将遮罩覆盖原文。`editSensitive()` 提供提交、
预编辑、选区、组合区域、完成、按 UTF-16/码点删除和移动光标命令。
原文只在 C++ 编辑路径内保留，不作为 JNI 查询结果传给 Java。
输入的字符本身仍会经过用户选用的输入法，不能把这一设计解释为防止恶意键盘记录。

Android `SensitiveConnection` 不创建含密码的 Editable；周边文字、选中文字、
提取文字和快照查询返回空结果。使用密码输入类型和不参与个性学习的请求标志，
拦截直接调用的复制/剪切；支持粘贴、全选、方向键扩选和预编辑。
非法 UTF-16、切换密码模式、外部改文、切换焦点及后台恢复后的旧连接均受校验。

`EditorSurface.Bridge` 追加 `sensitive()` 与 `containsEditor()`，默认返回不支持；
已有普通输入宿主源码可继续使用，但要接入密码命令和长按菜单必须实现这两个方法。
每次原生触摸处理更新焦点并 `sync(false)` 后调用 `onEditorTouch(event)`；其返回 true 时不再因抬手弹出键盘。应用返回事件先调用 `dismissEditingActions()` 关闭菜单，再处理页面返回。菜单使用 Android 浮动
ActionMode，普通选区支持全选/复制/剪切/粘贴，密码仅全选/粘贴。
普通字段的选词与拖柄见下述后续增量；密码仍仅菜单，放大镜或自动填充尚未接入。

试验台 `功能／文字输入与字体` 新增密码框、全选密码及清空密码按钮。结果与诊断
仅记录长度、选区和变化次数，不输出内容；演示中请使用合成数据。
同一进程中的旋转、Surface 重建和后台恢复保留已提交内容；进程终止后不承诺恢复密码。

验证命令（先构建当前 APK 和独立 IME 夹具）：

```powershell
python -B scripts/test-android-secure-input.py --adb <AndroidSdk>/platform-tools/adb.exe --serial emulator-5554
```

固定结果为 `out/mobile-current/android-secure-test.json` 与 `secure-protocol.log`。
测试分为真实 InputConnection 协议和系统长按菜单两部分，不以 mock 或伪文字接口替代。
这些 API36/x86_64 证据不覆盖最低 API28、ARM64、iOS/HarmonyOS，也不构成安全认证。

原生接口依据：[EditorInfo](https://developer.android.com/reference/android/view/inputmethod/EditorInfo)、
[InputConnection](https://developer.android.com/reference/android/view/inputmethod/InputConnection)。

本轮回归还发现一次旧构建的模拟器 ANR：栈停在键盘尺寸变化后的
`eglChooseConfig → QemuPipeStream`，不是文字替换算法。已去掉同一 ANativeWindow
尺寸变化时重复销毁/创建 EGL 与 Ganesh 的路径；新窗口、Surface 真正销毁或已有
渲染错误仍完整重建。`gpuContexts` 记录成功创建上下文的次数，`gpuResizes` 记录
同一窗口接受的尺寸更新；两者不是帧率。输入测试要求键盘开合保留 GPU 上下文。
原始最小堆栈保存在 `out/mobile-current/input-anr.log`。驱动级阻塞与真机功耗仍需
真实设备验证，不能用一次模拟器回归宣布全部解决。

上一轮密码/菜单候选在 API36 模拟器通过 39 组回归：布局 8、导航 9、外观 8、
普通输入 8、密码与原生菜单 6；所有报告对应同一个 APK SHA256，见
`out/mobile-current/input-validation.json`。Windows 冻结 SDK 的 3 个 CTest
与 Android wchar32 输入测试通过。测试夹具已从模拟器卸载，输入法、字号和旋转设置已恢复。
这只是当前 Android 增量证据：完整 27 页仍有缺项，不能代替三端发布验收。

## Android 光标与选区手柄增量（2026-10-10）

`TextInputSession::selectionGeometry()` 返回会话、UTF-16 两端索引、裁剪区域及
两个光标矩形；坐标与 `Widget::frame()` 一致。几何来自 `TextField` 的同一排版，
不在 Java 重新测量文本。无焦点、不可编辑、完全被裁剪、密码模式或预编辑期间
返回零会话。`selectAtPoint(session, SelectionEdge, Point)` 支持插入光标、anchor、
caret 三种模式；保留另一端并允许交叉，拒绝旧会话及非有限坐标。

Android `EditorSurface.Bridge` 的 `selectionGeometry()`、`selectAt()` 默认不支持；
宿主实现时把逻辑坐标乘以实际渲染 scale，返回 View 内物理像素；同时实现
`cancelPointers()`，取消 native pointer 并 pumpInput，不清除焦点或选区。通过
`EditorSurface.beforeEditorTouch(event)` 先让手柄处理事件；返回 true 后不得再
向 OneUI 转发该触摸流。普通触摸仍沿用 native pointer → `sync(false)` →
`onEditorTouch(event)` 的顺序。`cancelSelectionGesture()` 在尺寸变化前取消进行中
拖动；后台/销毁沿用 `suspend()`。手柄用 Android 主题 Drawable，颜色通过
`setSelectionHandleColor()` 跟随 OneUI 明暗 accent。只在几何变化时更新叠层。

普通字段点击显示插入手柄，长按按系统 Locale 分词选中并显示原生菜单。
长按从收到 DOWN 后使用系统超时/移动阈值计时，避免忙布局后的旧时间戳立即触发；
选词成功后取消底层拖选，接管剩余触摸流，避免后续 MOVE 把整个单词缩成一个字。
双端拖动保持逻辑 anchor/caret，取消和第二根手指恢复开始选区，前提是会话仍有效。
外部编辑、切页或后台不向失效会话回写。手柄触摸范围为 48dp；不可见端点隐藏。
`getSelectionHandleBounds()` 仅提供可见触摸矩形，不包含文字内容。

密码的现有公共几何查询刻意关闭，本轮保持其点击定位及全选/粘贴菜单，
不通过导出原文或猜测字宽生成密码手柄。选区边缘自动连续滚动、放大镜、
TalkBack、复杂双向文字及不同输入法的候选体验仍需后续原生验证。
C/Rust 控件宿主尚未接入本交互，不能把 C++ helper 当成三语言已完整覆盖。

定向测试：`scripts/test-android-selection.py` 使用独立 instrumentation 夹具，
通过 Android 输入分发注入真实触摸事件，校验单多行、插入手柄、双端扩缩、交叉、
取消、多指及 Unicode 边界。公共几何同源测试在 Windows wchar16 和 Android wchar32
分别运行。固定结果：`out/mobile-current/android-selection-test.json`、
`selection-protocol.log`；实际构建与完整剩余范围以当前 `input-validation.json` 为准。

本轮同一 API36/x86_64 APK 已通过 54 组完整回归：选区 21、布局 8、导航 9、
系统外观 8、普通输入 8。密码协议已通过，但密码与原生菜单套件尚未全部通过；
测试中发现布局/输入法时序问题，随后模拟器系统输入法服务超时，冷启动因 C 盘
空间不足被拒绝。原生菜单及密码生命周期仍须恢复环境后重验，不能沿用前一 APK
的通过记录。Windows 冻结 SDK 3/3 CTest 与 Android wchar32 输入测试通过。
选区包含单/多行、跨端、取消、多指、公开尺寸变化取消回调、
Unicode 和后台隐藏；该回调测试不代表已验证系统旋转过程中的拖动。
测试驱动等待实际前台状态、系统主题确认、键盘绑定/显示及最新窗口几何，
不把固定休眠后的旧状态作为结果。长按使用系统超时与移动阈值；模拟器计时与
IME 候选交互、真机响应延迟/功耗仍需单独验证，不能视为移动端发布验收完成。

叠层接口依据：[Android ViewOverlay](https://developer.android.com/reference/android/view/ViewOverlay)。
手柄尖端定位参考 [Android Editor 的 selection handle hotspot](https://android.googlesource.com/platform/frameworks/base/+/master/core/java/android/widget/Editor.java)，
按原生图形内的尖端位置对齐光标，避免把图形外沿误作文字边界。
