# 共享移动展示组件

这些组件依赖 OneUI 的公开 Widget/Canvas API，供 Android、iOS 与原生 HarmonyOS 的产品页面复用。账户、网络与 VPN 的产品策略留在消费者；系统宿主与通用交互留在 OneUI。

| 头文件 | 能力 |
|---|---|
| `oneui/mobile/presentation.h` | 语义颜色、宽度测量文字、操作按钮、协议勾选行、带标签输入行、分组、列表行、单调时钟倒计时 |
| `oneui/mobile/navigation.h` | 带选择语义的主导航、分段选择 |
| `oneui/mobile/puzzle_slider.h` | 有界 PNG 拼图展示、人工拖动、键盘调整与源图横坐标回调 |
| `oneui/mobile/widget_snapshot.h` | 有界的可见控件名称、角色、几何和状态快照 |

```cpp
auto palette = oneui::mobile::controlPalette(dark);
auto input = std::make_shared<oneui::TextField>(L"输入设备名称");
input->setAccessibleName(L"设备名称");
oneui::mobile::styleTextField(*input, palette, fontScale);
page->add(oneui::mobile::labeledInput(input, L"设备名称", palette, fontScale));
page->add(oneui::mobile::segmentedControl(
    {L"第一项", L"第二项"}, selected, palette,
    onSelectionChanged, fontScale, busy));
```

`labeledInput` 可在输入行末端放入操作控件；主导航与分段选择在正常字号下提供至少 48 个逻辑像素的触控高度。产品负责业务可用状态、验证、加载和错误内容。`ListRow`、`TabButton` 提供按压取消与键盘激活，滚动手势由共享宿主取消旧按压。

`pageText(value, size, color, weight, availableWidth)` 明确为 Legacy Stack 保留测量后的行高。Label 的默认22逻辑像素高度不能作为大标题的高度；将 preferredSize 清零也不会令 Legacy Stack 自动调用 measure。多行说明传入实际正文可用宽度，并在宽度或字号改变后重新测量。调用者须扣除页面内边距和滚动条预留区，不能依赖字符数猜高度。`checkRow` 统一协议勾选的语义颜色、触控高度与辅助说明，具体协议文本和链接仍由产品传入。

`Cooldown` 采用单调时钟。通过 `nextChangeMillis()` 取得下一次文字变化的期限，产品在 flush 中调用 `ApplicationServices::wakeAfter(ms)`。服务合并为最早的一个期限；Android 只保留一个延迟唤醒，并在暂停、失焦与销毁时取消回调。期限到达使 ApplicationHost 重新检查展示状态，无须用户触摸，也不按秒轮询服务或重建整个表单。iOS/HarmonyOS 宿主仍沿用已有绘制 tick；这不代表其耗电或运行时已验证。

`ApplicationHost` 根据编辑器所有者和冲突检查保留输入会话；移除、禁用、外部改写或后台化后的旧输入不能继续写入。键盘使可用区域变小时，`TextInputSession::revealCaret` 优先显示能容纳的完整输入框，并在极矮视口收缩边缘间距；超高的多行编辑器继续跟随光标。避让仅响应编辑会话、内容/选区、字号几何或视口变化，普通绘制不把手动滚动位置拉回编辑器。密码字段不提供选择手柄几何，验证密码输入可见性应检查实际输入框、脱敏编辑会话和系统 IME，而不是要求选择几何非空。

Android `NativeApplication.inspect` 返回 `NativeUiNode[]`，必须在宿主 UI 线程调用。快照最多 512 节点、深度 64，每个名称最多 256 个宽字符，不返回编辑值、密码、验证码或业务请求内容。便携 N-API 的 `inspect` 仅编译于 `ONEUI_NODE_TEST`。此接口提供语义检查数据，不代表系统读屏桥已经完成。

Android 的 `OneUiActivity` 接受手指、鼠标主键及明确来自触摸屏的未标明工具类型事件，触笔保留独立类型。系统栏保持真实 OS 图标、安全区和输入法区域；原生绘制引起外观版本变化时，宿主也需同步系统栏。API 应用回执与最终合成帧是两种证据，主题验证要检查实际系统图标和手势条的可见性。

消费者使用匹配的预编译 SDK。Android 构建和模拟器操作、便携主机控件检查、系统服务 mock、真实账号验证、VPN 流量以及 iOS/HarmonyOS 原生构建分别记录结果，不能互相替代。

`PuzzleSlider` 仅负责展示和人工输入，不识别缺口、不自动求解，也不请求验证码服务。产品传入背景、拼图 PNG data URI、源图尺寸及纵坐标，再调用 `setDisplayWidth` 设置布局宽度；释放时回调源图横坐标，键盘方向键调整、Enter/Space 提交，取消按压不会提交。产品自行处理服务事务、刷新、过期、错误和取消。

`ImageView::setPngDataUri` 在 OneUI 内严格解码有界静态 PNG：限制编码长度、像素尺寸和帧数，失败返回 false。验证码 ID、一次性证明、手机号和 provider 协议不进入控件；挑战图片只在内存显示，不写入截图、日志或缓存。消费者须使用包含此新增方法实现的匹配 SDK，不能只替换头文件后链接旧库。

## Android 远程展示 SDK 的完整来源

远程控制组件与其依赖随 SDK 的 `include/oneui/mobile` 一同发布，包括虚拟文本编辑器、远程键盘条、
工具条、画面尺寸/邮箱、缩放和平移、滚轮、底部弹层及表单/文件行。产品正常构建直接使用 SDK 头文件，
不应隐式读取另一份工作区中的未提交头文件。只有显式 SDK 开发实验可以指定源码覆盖层。

`Widget::setFocusOnPointer(false)` 允许辅助触摸动作保留当前编辑器；View 与 OverlayHost 按实际被按下的子控件
传递该策略。默认仍获取指针焦点，键盘和辅助功能焦点保持可用。取消按压不得触发点击或改变编辑会话。
`TextInputSession::key` 必须校验当前会话，仅把按键交给当前有焦点的编辑器；陈旧会话不能修改另一个字段。
Android 的空上下文编辑器可通过 `Bridge.key` 接收删除和左右方向键，普通字段保留本地编辑行为。
外部提交并清空预编辑后必须创建新会话，旧 composing 缓冲区不能重新写回。

OneUiActivity 的服务桥最多保留 16 个 in-flight 请求，用 `requestsLimited` 取出尚可处理的数量。
超过并发容量的工作留在原生有界队列，完成回执释放容量后继续调度，不丢弃请求并制造重建循环。
共享请求包括显示键盘、编辑器退格和窗口模式；业务请求继续交由产品处理。
沉浸式窗口重新应用于 resume，安全区变化通过 `safeAreaChanged` 通知产品；IME 与导航区不重复扣除。
GPU 画布保留输入焦点，关闭系统默认焦点高亮，避免 Android 在 GPU 内容上叠加焦点着色。

Java 与原生的 inspect 节点构造签名须一起发布。滑块 range 信息只包含语义范围及值；显式 setProgress
另行校验有限数值、节点索引、名称、目标类型和禁用状态。inspect 本身仍是有界只读快照。

构建器可在独立固定目录重建 SDK，并复用已核验的 Skia 依赖：

```powershell
python -B scripts/build-android-application-sdk.py --abi x86_64 `
  --ndk C:/Android/sdk/ndk/29.0.14206865 --sdk C:/Android/sdk `
  --work-root D:/OneUIBuild/android-sdk `
  --skia-source E:/Dependencies/skia --skia-build E:/Dependencies/skia-x86_64
```

arm64-v8a 需单独指定匹配的 Skia 输出。Skia 与 yyjson 源码按原有依赖说明准备；不得混用其他修订的头文件和库。
独立输出目录由 owner 标记绑定源码工作区，不覆盖共享安装目录，也不设构建产物大小上限。
Java 构建编译全部适配器源码，清除本构建器拥有的旧 class，按固定 ZIP 顺序、时间戳和权限生成 JAR；
不从旧 JAR 替换几个 class 后声称整个 SDK 来自当前源码。
verification.json 记录源码提交及脏状态、Skia 提交、原生输入和归档哈希、Java 源码/class/JAR 哈希与实际编译器。
同一 Java 输入应可重建相同 JAR。源码可追溯、编译通过、原生测试、实际 Android 合成/输入以及公网验收分别提供证据。
