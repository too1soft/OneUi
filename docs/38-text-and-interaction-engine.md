# 文字与通用交互升级：工作区契约与验收状态

本文描述正在实施的升级，不是发布完成声明。C ABI 版本和兼容规则仍以
[集成指南](c-abi-integration.md) 与代码常量为准。原有 C 结构没有追加字段；
本轮新增独立结构、opaque handle 和符号，不引入 GPUI 运行时。

## 当前状态与发布阻塞

- 共享 SkParagraph 布局、字素编辑、密码遮罩、作用域命令、作用域订阅及
  C / Rust 增量接口已进入工作区，并有自动测试；仍在回归和集成收尾。
- 固定 ICU 基础修订保持不变，通过[可复现的 OneUI 维护补丁](../third_party/patches/README.md)
  修复方向覆盖、括号后的组合标记和 BD16 栈容量问题。原有 30 个失败已全部修复；
  完整 Unicode 15.1 Bidi 数据及新增的 15 个括号边界回归用例通过，没有豁免失败用例。
- MSYS2 当前安装的 Skia 为 milestone 143，与固定源码 milestone 150 不匹配，且
  没有提供 SkParagraph 所需的内部头。MinGW 的旧基线成功不代表本轮文字引擎构建成功；
  新配置会明确失败，不能拼接另一修订的头文件绕过。
- SDK 许可/体积/符号隔离及宿主 ICU/HarfBuzz 共存审计、Gallery 展示和完整原生交互验收
  尚未完成。不得使用旧 SDK 的通过记录作为新文字引擎的发布证据。
- Mac、Linux ARM64、麒麟/UOS 的缺失机器继续列为待原生验收；WSLg 不替代真机验收。

## 依赖和布局边界

采用 Skia `1f26101197bff9fcd939a791beb3094297436d59` 的 SkParagraph、SkShaper、
SkUnicode 和 DEPS：HarfBuzz `9cb1fee51069b206effb4736e443b038d230789d`、
ICU `364118a1d9da24bb5b770ac3d762ac144d6da5a4`（74.2，Unicode 15.1）。
没有另加分段库。补丁保留 ICU 公共接口，只修改内部实现；构建脚本校验基础提交和
补丁前后源码 SHA-256，可重复应用，遇到未知修改则保留现场并报错。
OneUI 配置只读校验补丁状态；GN 必须重建完整 `skunicode_icu` 归档，而非只重建 `icu`。
维护记录见补丁目录，改变依赖或补丁必须重新运行全部一致性测试。

`src/text/` 是私有层。塑形、字体回退、行度量、点击、光标、选择矩形和绘制共用布局结果，
首次 paint 前也可命中。显式映射 UTF-8、UTF-16 和本机宽字符下标；第三方类型不进入公开 API。
固定字体测试发现的连字命中崩溃由 OneUI 的字素光标几何命中路径规避，不调用出错的
SkParagraph 坐标命中接口。颜色更新只改变 paint，不重新塑形。

缓存按硬段落复用，键包括文本、字体、语言、方向、宽度、行高、缩放和截断选项。
当前每 UI 线程最多 512 个缓存段落，缓存键源文本预算为 1 MiB；这不是总内存上限，
不包含仍由控件持有的布局、glyph 和字体缓存。`Layout::stats()` 是私有测试/诊断入口，
记录排版次数、命中数与缓存键字节数；没有同条件实测，不作 GPUI 性能比较声明。

平台字体来自 DirectWrite/GDI、Fontconfig/FreeType、CoreText。固定测试字体只用于验收，
不默认打包给最终用户。显示覆盖取决于宿主字体；缺字数量由私有布局诊断读取，
不记录原文，公开诊断接口仍待完善。终端继续按固定单元格绘制，不套普通段落 Bidi。

## 文字设置与编辑迁移

`TextOptions` 包含 `direction`（Auto/LTR/RTL）、`locale`（默认 `und`）和
`wrap`（NoWrap/WordWrap）。`Label::setTextOptions`、`TextField::setTextOptions`
及继承的 TextArea 接受该设置。Label 的旧 wrap/max-lines 入口继续保留；
TextArea 默认不软换行，调用方显式启用 WordWrap。

```cpp
oneui::TextOptions options;
options.direction = oneui::TextDirection::Auto;
options.locale = "zh-CN";
options.wrap = oneui::TextWrapMode::WordWrap;
area->setTextOptions(options);
```

- 旧 `caretIndex` / selection 范围仍使用本机 `std::wstring` 单元下标，不能当作 UTF-8。
- 新 `TextPosition` 使用 UTF-8 字节偏移及 Upstream/Downstream 亲和方向。
  C/Rust 新接口拒绝字节中间、字素中间或越界的位置，不把字节下标直接传给 SkParagraph。
- 水平移动按视觉顺序；上下移动保持目标横坐标；删除按逻辑字素边界。
  选择可以有多个视觉矩形，复制保持原始逻辑顺序。
- preedit 只进入呈现布局，提交作为一次撤销事务；取消不修改正式值/历史。
  焦点会话带失效序号，迟到的旧目标提交不得进入新编辑器。各平台的原生 IME 竞态仍需验收。
- 密码按字素遮罩；明文布局不进共享文字缓存，不进入 Canvas 文本或 surrounding-text。
  复制/剪切命令在密码模式禁用。它不是内存加密，调用方持有的原始字符串仍需自行管理。

## 命令与快捷键

控件和窗口各有 `CommandScope`；ID 是稳定、非空、无内嵌 NUL 的 UTF-8 字符串。
注册返回 move-only `Subscription`，销毁或 `reset()` 立即注销；同作用域重复 ID/快捷键报错。
可执行查询返回 NotFound/Disabled/Enabled，执行成功返回 Executed。
按钮、菜单可调用同一个 `Window::executeCommand` 入口。

```cpp
// token 必须存活到不再需要这个命令时。
auto token = panel->commands().registerCommand(
    "app.refresh", [] { /* refresh model */ }, [] { return true; },
    oneui::KeyChord{"r", oneui::KeyModifierPrimary});
```

第一版是单组组合键；没有多段快捷键、表达式或 JSON 参数。字母键名不区分 ASCII 大小写，
导航键使用 `left`、`pageup` 等名字，功能键使用 `f1`…`f24`。Primary 在 Windows/Linux
映射 Control，在 macOS 映射 Meta/Command；原始 Control、Meta、virtual key、scan code 不改写。

路由为原始键盘回调 → 活动焦点叶到祖先的用户命令/内置命令 → 旧键盘处理。
命中 Disabled 不再穿透。模态边界阻止背景/窗口命令，组合期间不执行应用快捷键。
内置编辑 ID 为 `edit.copy`、`edit.cut`、`edit.paste`、`edit.select_all`、`edit.undo`、`edit.redo`。
公开 C++ 回调不应跨原生事件边界抛异常；C 边界捕获异常，Rust 回调设 panic 边界。

## State 与 Binding 生命周期

保留 `subscribe`/`unsubscribe`，新增 `subscribeScoped` 返回 RAII 订阅。
State 先销毁时 Binding 弱引用失效，不再访问旧对象。State 复制只复制当前值，不复制订阅；
移动转移状态身份。所有操作仍限定 UI 线程，跨线程走 post/worker handle。

通知持有稳定的值与订阅快照。嵌套 set 顺序入队，每次调用前检查订阅有效性；
回调中注销立即生效，新订阅不补收本轮事件。State 销毁清空后续通知。
这是生命周期安全，不是隐式线程安全或全局实体容器。

## C / Rust 入口

| 能力 | C | safe Rust |
| --- | --- | --- |
| 控件命令注册 | `oneui_widget_register_command_utf8` | `Widget::register_command[_when]` |
| 窗口命令注册 | `oneui_window_register_command_utf8` | `Window::register_command[_when]` |
| 注销 | `oneui_command_registration_destroy` | `CommandRegistration` Drop |
| 查询/执行 | `oneui_{widget,window}_{query,execute}_command_utf8` | `query_command` / `execute_command` |
| 文字设置 | `oneui_widget_set_text_options_utf8` | `set_text_options` |
| 文字位置 | `oneui_text_field_{get,set}_position_utf8` | `text_position` / `set_text_position` |

C 注册传入回调上下文及 destroy 回调后，成功、重复或无效参数失败均由该调用接管清理；
不得再次手动释放上下文。注销取消后续调用；正在执行的回调退出后才释放其捕获。
注册 handle 在作用域先销毁后仍须显式 destroy。Rust 注册对象不实现 Send/Sync。
符号清单由 `scripts/check-abi-sync.cmake` 与 oneui-sys 同步核对。

## 验证与发布约束

2026-09-03 工作区自动验证记录（不是发布通过记录）：

| 路径 | 结果 |
| --- | --- |
| Windows x64 MSVC bundled-static CTest | 27/27 通过，包含完整 Unicode 与补丁应用回归 |
| Ubuntu 24.04 x86_64 WSLg X11 CTest | 33/33 通过，包含完整 Unicode 与补丁应用回归 |
| WSLg 原生 Wayland 连接 CTest | 32 通过、1 系统剪贴板用例因缺少交互焦点显式跳过；跳过不计验收 |
| Windows Rust | fmt、workspace/all-targets/all-features 严格 clippy；动态/静态各 70 用例通过 |
| ABI / 导出 / UI 红线 | C/Rust 常量与符号清单一致，HEAD 的旧 C 符号无缺失，UI redline 通过 |
| 官网 | npm ci 后生产构建通过；构建日志未出现 Google 字体元数据重试 |
| MinGW / SDK / 其他原生机器 | MinGW 配置明确报告缺少匹配文字依赖；新 SDK 及未提供机器待验收 |

补丁后日志位于忽略的 `out/text-patched-*.log`；先前回归和网站日志为
`out/text-current-*.log`、`out/text-website-*.log`。Rust 静态链接现在跟踪
OneUI、文字层及 Skia/ICU 等全部静态归档，依赖重建会触发重新链接，不沿用旧静态测试二进制。
Windows 开发构建 DLL 实测 28,135,936 字节；这是当前开发配置，不是压缩 SDK 包体积或最终发行基线。

```sh
cmake -P scripts/fetch-text-test-assets.cmake
cmake --build build/native --parallel 6
ctest --test-dir build/native --output-on-failure
cmake -P scripts/check-abi-sync.cmake
```

测试资产见 `tests/text-assets.cmake`：固定提交、SHA-256、许可标识。
Noto 字体为 OFL-1.1；Unicode 数据及许可为 Unicode-3.0。资产下载到忽略目录；
配置时缺失/哈希不匹配直接失败，不跳过必需测试。

Unicode 测试覆盖 1,187 个 grapheme、1,826 个 word、91,707 个 Bidi character 和
770,241 个 Bidi class 用例。Word 的 15 个冒号相关用例按 ICU 74 root locale 的
ICU-22112/ICU-22127 明确 tailoring 校验，不声称是未定制的纯 UAX word profile；
Bidi 全量通过，没有豁免。测试同时比较直接 ICU 和 SkUnicode 的 level，区分依赖与适配层错误。
额外回归覆盖 62/63/64 层括号、规范等价括号、溢出前已解析配对的回退、嵌套隔离、
跨段落及显式层级重置。补丁脚本测试验证首次应用、重复应用、只读检查和冲突拒绝，
只操作构建目录里的独立 fixture，不重置真实依赖工作区。

`tests/support/test_ui_context.h` 提供可控时钟、任务队列、内存剪贴板、事件回放和
RecordingCanvas。模拟测试证明的是控件契约，不能替代平台中文输入、候选窗跨 DPI、
多窗口、销毁后回调及宿主第三方库共存验收。

普通 SDK 默认必须具备完整文字依赖，接受审计后的体积增长，不再以历史约 5 MB 为硬上限，
也不拆轻量版。动态 SDK 不携带开发用大静态归档。Windows ICU 数据已接入内嵌初始化；
无旁置数据的干净消费者和最终许可清单仍须随新包验证。
`check-package-size.ps1 -AuditOnly` 只报告实测，不返回发布通过结论；
发布检查使用显式审计后的 `-MaxBytes`，不允许静默套用旧限额或跳过体积审核。

MinGW 匹配构建、SDK 审计和缺失原生环境处理前，不推送为“全部完成”，
不发布全面平台/完整文字支持声明。
