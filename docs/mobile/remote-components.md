# 远程控制共用组件与平台适配

本文记录 OneUI 可复用的远程画布、触控和桥接边界。实现不含 QuickDesk 设备码、访问码、信令地址、身份认证或业务流程；这些留在产品。它补充 `docs/mobile-support.md` 的平台验收要求，不宣布 Android、iOS 或其他系统达到完整对等。

## 依赖方向

```text
产品 C++ OneUI 页面 ── RemoteInteraction / FrameMailbox / RealtimeFrameView
        │                              ▲
        └─ ApplicationServices          │ owned RGBA
                    │                  │
Android OneUiActivity                  │
    ├─ PointerCapture ── 局部 dp 多指样本
    ├─ 可选 EngineBridge ── 无视图 FlutterEngine ── 产品 Dart 协议
    └─ 可选 WebRTC FrameReceiver ── I420 → NV12 → JNI mailbox
```

公共头文件可用于不同宿主；Java/WebRTC/Flutter 依赖只属于对应可选适配。当前消费者示例为 `E:/project/byname/remote/clients/quickdesk-oneui/mobile`。复用能力不等于其他消费者已经完成接入或跨平台验收。

## 核心组件

| 组件/入口 | 责任 | 不属于此层 |
| --- | --- | --- |
| `include/oneui/mobile/remote_interaction.h`，`RemoteInteraction` | 纯 C++ 多指状态，Direct/Trackpad，适应留边映射，鼠标/滚轮语义输出，手势取消与释放 | USB/QuickDesk 编码、网络、设备身份、像素缓冲、OS 权限 |
| `include/oneui/mobile/frame_mailbox.h`，`FrameMailbox` | 有界帧校验、自有 RGBA 存储、最新待取帧替换、线程互斥、序号和 `take`/`clear` | 解码器、发送队列、信令、无界帧历史 |
| `include/oneui/mobile/pixel_conversion.h`，`nv12ToRgba` | BT.601 limited-range NV12 → RGBA；检查尺寸、偶数边长、stride 和缓冲容量 | 色彩协商、HDR、网络、硬件零拷贝 |
| `include/oneui/controls/realtime_frame_view.h`，`RealtimeFrameView` | 显示已拥有生命周期的 RGBA/BGRA 帧，缩放/绘制由 OneUI 负责 | 接管外部解码缓冲所有权或决定远控权限 |

`FrameMailbox::submit` 按行复制 RGBA，`submitNv12` 转换到自有紧密 RGBA；当前最大边界为 5120×2880。`take` 移出当前待取帧，下一帧可以替换尚未取走的帧。该设计控制的是 mailbox 的待取队列，不意味着渲染器、解码器和当前调用栈总共只能持有一个缓冲。显示消费者应使用 `submitOwnedFrame` 传递同一共享所有者，避免借用已经释放的内存。

## 手势与取消语义

`RemoteInteraction` 默认 Trackpad；Direct 把本地位置映射到按比例适应的远程画面。单指移动输出位置，轻点输出主键按下/释放；两次轻点间隔小于 300ms 且距离小于 24 时，第二次按住进入主键拖拽。未移动且停留至少 500ms 的手势在松手时输出次键。双指状态只由内部有序指针表的首项输出滚动增量。

`cancel` 在拖拽时发出主键释放并清理指针。改变 viewport、模式、sink 或禁用时会取消；viewport 内容没有改变时不会仅因新视频帧取消。产品还必须在断线、后台、工具覆盖、仅查看和销毁前调用取消/释放，并在发送协议输入前核对当前会话与控制权。共享类不替产品推断授权。

## Android 可选适配

### 画布缩放与会话工具

`RemoteViewport` 保有适应比例、缩放倍率和受限平移，并提供同一变换的逆映射。
`RemoteInteraction::setZoomViewport` 可接入该状态。双指间距变化缩放画面，放大后的
双指移动平移画布，直接触控通过逆变换返回远端像素。适应留边区不会产生直接点按。
变换不会随每帧更换而重置，前后台和手势取消会释放已按下的鼠标。

`ZoomableFrameView` 使用相同变换绘制并裁剪 `RealtimeFrameView` 的帧。
消费者在控件尺寸变化后更新本地命中区域，以处理系统输入法、方向和工具面板
引起的视口变化。该更新通过 `Widget::setOnSizeChanged` 绑定。

`ToolbarButton` 提供图标与标签动作、选中、焦点和按压状态。
`action_icons.h` 提供键盘、触控板和指针的矢量图形。按钮文本和业务动作由消费者传入。
这些新增头文件可随源码 SDK 使用。固定 SDK 消费者应把新增头文件作为有界覆盖，
并保持所有翻译单元使用同一组件定义。

| 路径 | 责任与使用约束 |
| --- | --- |
| `platform/android/java/net/too1/oneui/mobile/OneUiActivity.java` | Android Activity、Surface、OneUI 原生应用工厂、系统外观/字号、IME 与 Back 接入；产品子类提供业务请求处理 |
| `platform/android/java/net/too1/oneui/mobile/PointerCapture.java` | 在画布内开始时捕获 MotionEvent；像素按 density 变为局部 dp；输出最多十个移动样本与 Down/Move/Up/Cancel。产品控制有效范围和是否启用 |
| `platform/android/flutter/java/net/too1/oneui/flutter/EngineBridge.java` | 建立 FlutterEngine/Dart 默认入口与 MethodChannel；ready 前最多等待 16 个请求；限制输入事件容量；关闭时移除 handler 并销毁引擎。没有 FlutterView |
| `platform/android/webrtc/java/net/too1/oneui/webrtc/FrameReceiver.java` | 可选 WebRTC VideoSink；`toI420` 后经 YuvHelper 转 NV12，复用 direct buffer；向 JNI 提交后通知变更；支持 enable、切 track 与 close |
| `platform/android/webrtc/frame_receiver_jni.cpp` | 校验 direct buffer 并调用 `FrameMailbox::submitNv12`；不把网络或认证引入 OneUI 核心 |

`FrameReceiver` 不持有帧锁调用 `removeSink`，避免解码回调与移除 sink 互相等待。产品须在 mailbox 所有者销毁前关闭 receiver，并在暂停时禁用接收、结束时分离 track。原始 mailbox 地址是受同进程寿命约束的桥，不是可持久化句柄，也不是跨进程接口。

`EngineBridge` 是可选传输适配，不是 UI 技术替代。产品提供 channel 名、请求 action、协议包和事件语义。使用 Flutter 插件不改变 OneUI 页面所有权；此接入无需 FlutterView、`runApp` 或 WebView。

系统栏与内容色仍由共享宿主负责：`SystemAppearance` 同时更新 legacy decor 的亮色栏标志与 Android 30+ `WindowInsetsController` 图标标志，避免配置热切换恢复旧 flags；`OneUiActivity` 在配置变化、焦点恢复等生命周期边界重新应用当前外观。原生状态已 acknowledge 不代表 Android 随后不会覆盖窗口标志，因此这些边界仍需重申。该修改不属于产品全局视觉 token。

## Windows 文件选择原语

`include/oneui/platform/windows/file_picker.h` 提供 `FileSelection` 与 `pickFile`：在调用方安排的 STA helper 中初始化 COM，使用系统 IFileOpenDialog，配置真实文件/目录与初始位置，返回选中（1）、取消（0）或失败（-1）及 HRESULT。它不嵌套 OneUI 渲染事件循环。

helper 进程创建/结束、异步回调上下文、取消轮询和文件传输业务属于产品适配。QuickDesk 桌面 `native_file_picker.cpp` 使用此原语；系统选择器本身不负责发文件、管理会话或创建 Android 权限流程。不能由 Windows 的选择器接入推导 Android 文件能力已验收。

## QuickDesk 消费者边界

QuickDesk 共享协议实现位于 `clients/quickdesk/protocol-dart`，包名 `quickdesk_protocol`；旧 `clients/quickdesk/quickdesk-android` 保留原 API 路径的兼容 export。OneUI 不依赖该包，移动产品以 headless Dart isolate 调用其会话和数据通道。

移动产品承担连接代际取消、信令/API/访问码、配置持久化、远端输入消息映射、视图与工具状态。数据通道中未释放按键/鼠标的清理同样属于产品协议层；OneUI 的手势取消不能代替释放全部协议按键。

消费者通过现有 `TextFieldStyleOverride` 设定背景、正文、占位符、边框与光标颜色，不为本次接入新增一套全局视觉 token。最终字段占位符浅色 `#636b78`/白底约 5.38:1，深色 `#a6aebb`/`#2e3137` 约 5.83:1。首页说明使用 OneUI Label 的换行和测量 API，以当前内容宽度计算首选高度；这些是产品页面材质和布局的使用方式，不改变协议或平台桥边界。

## 验证边界

文档整理未独立构建、截屏或执行远控。本轮消费者证据位于 remote 项目 `.cache/quickdesk-build/android-smoke.json`：Pixel_8 API36 x86_64 模拟器接受 1042 帧；Direct 轻点已观察到 fixture 主键按下/释放各增加 1，Enter 键事件增加 2；仅查看前后输入请求均为 0；后台 releaseAck、帧暂停和前台恢复已记录。arm64-v8a 仅编译，未运行。

真实输入/会话 smoke 对应 APK SHA256 `b014f16a46f1a54ff692490baf47bb50131ee8bf87d51917df106c59cc18201e`。最终 APK SHA256 `f0a7d1434121db25642ede260baab5f51cb35c892a26c4da5817f780eb3cdbb5` 追加共享系统栏 flags 重申、字段主题对比度与首页说明测量换行修复，同一 UI 修订的格式化前构建验证了深色热切换与 1.3 字号首页；最终仅格式化重编包未再次启动，未重新执行全部前包输入 smoke。最终连接/深色截图和前输入包的真视频会话截图，分别保存在 remote 的 `.impeccable/review/native-android-connect.png`、`native-android-dark.png`、`native-android-session.png`。两项材质问题的定向复核均 resolved，ship 判定仅针对这两项修复，不表示全端认证。

共享 C++ 测试二进制实际返回 exit 0，无成功文字输出；消费者 Dart analyze 无问题、连接取消并发测试 2 项与旧 Android 兼容测试 28 项通过。QA app 数据已清除，系统字号 1.0/night no 已恢复。上述结果不证明 Windows 消费端已全面使用 `RemoteInteraction` 或 `FrameMailbox`；当前未证实其直接复用。

remote 的 `docs/design/remote-oneui-android-prototype.html` 已在 IAB 1280×720、390×844 走查四页与主要成功/失败/取消/恢复流程，无 console 错误；HTML detector 为 0 anti-pattern、2 圆角 advisory。该模拟原型不构成 native gate，浏览器长按/多指、读屏与 IME 未实测。MyStudio 独立 Android 原型 `XS6CFaQOU7fY24I7AkRY` v1、remote guide v7 与本组件手册 `Ezi_-nP_j1ssPzWOi7bW` 已同步，桌面原型 v5 保留；云端附件不表示原生客户端已加载。

完整移动平台门槛见 `docs/mobile-support.md`。Android 被控、录屏/辅助功能/输入注入权限、文件传输、音频播放、物理手机、公网/NAT、展开布局、屏幕旋转、硬件键鼠、辅助功能、持续性能/功耗或其他移动系统仍未验收。QuickDesk 原应用拥有相关代码不等于新 OneUI 消费者已完成接入。


## 原生表单、语义节点与固定来源 HTTP

`presentation.h`、`navigation.h`、`puzzle_slider.h` 提供主题表单、列表、协议勾选、主次按钮、
导航与人工拼图。触控目标的最小尺寸不随小字号缩小；按钮、分段和标签提供按压/焦点反馈。
`insetGroup` 保留子控件高度，避免 Legacy Stack 中列表重叠；`readingText` 把长正文分为
240 字符语义块，使有界检查标签仍能覆盖完整文档。

Android `OneUiActivity.fieldInputType` 提供字段语义入口，共享 `EditorSurface` 设置 IME；
密码模式始终覆盖普通用途。`NativeAccessibility` 将 C++ 节点接入 Android 虚拟节点、焦点、
点击、编辑、悬停与滚动；密码值不返回，焦点不触发勾选。激活沿用宿主真实触摸处理链。
`widgetSnapshot` 按根视口与 ScrollView 裁剪，隐藏在导航后的内容不参与命中；检查接口仍不含
编辑器值、平台 Token 或业务载荷。非密码的当前编辑内容仅通过系统辅助功能提供，不进检查日志。

可选 `platform/android/network/java` 提供固定来源 HTTPS 与 `PkceProof`：有界请求/响应、
默认 TLS、禁止重定向和自动重发、主动取消、独立随机 state/nonce/verifier 与 S256；它没有
账号、短信、订单或平台地址策略。接入产品自行管理短时事务、同意、会话与凭据归属。

`scripts/update-android-java-sdk.py` 更新现有两 ABI Java SDK；
`update-android-inspection-sdk.py` 复用已有 Ninja 编译命令，仅重编检查 JNI 对象并替换匹配
组合归档成员，保留依赖及其 ABI。必须同时消费匹配头文件、Java 与原生 SDK，不以新头文件
冒充旧归档兼容。

网云远程的当前 x86_64/API36 消费者验证了 17 项隔离 HTTP/Keystore 账户行为、TalkBack
启用时的 28 项语义操作和 0.85 字号下 48dp 目标。arm64 仅编译，其他消费者未据此重跑；
不扩大为人工听读、物理设备、视觉 CAPTCHA 替代、真实短信或整个框架的发布认证。

## 修饰键与实体键盘

`remote_keyboard.h` 提供 `RemoteModifierKeys` 和 `remoteModifierRow`。消费者持有按键状态，
通过 sink 接收 USB HID 按下与松开，切换面板、焦点、前后台和会话时调用 `release`。
按键行提供 Ctrl、Alt、Shift、Win / ⌘ 的选中外观及辅助功能勾选状态。

`RemoteHardwareKeyboard` 的 `Sink` 接收 usage、pressed、locks，locks 中 bit 1 表示
Caps Lock，bit 2 表示 Num Lock。原有双参数 BiConsumer 构造方式继续可用。
失焦会释放已按住的键，恢复后到达的旧重复事件要等新的首次按下才能重新激活。

网云远程 RustDesk 适配复用引擎对目标系统的物理键映射。API36 模拟器连接真实 Windows
RichTextBox 测试窗口，已核对 Shift 大写、重复输入、小键盘符号、Ctrl+A、屏幕 Shift
选区、后台松开及中文提交。专用 fixture 的 HoldFocus 选项用于隔离其他程序抢焦点。
