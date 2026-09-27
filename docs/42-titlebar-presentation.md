# 可配置标题栏：产品配置与框架行为分离

`WindowTitleBar` 的 `variant` 只追加 `chrome-<名称>` CSS 类，不选择布局、按钮位置或产品分支。未设置 presentation 时保持原默认标题栏。

需要自定义时，应用提交 `TitleBarPresentation`：标题、图标、leading、accessory 和三个窗口按钮的局部逻辑坐标，以及可选的线条图标路径和椭圆按钮颜色。OneUI 继续统一处理绘制、命中、按压取消和窗口回调；Win32 原生命中也使用这些按钮区域。应用不再需要在 SDK 核心增加产品名称判断。

## C++ / C / Rust

```cpp
oneui::TitleBarPresentation presentation;
presentation.title = {100, 0, 240, 38};
presentation.titleFontSize = 13;
presentation.accessory = {360, 3, 180, 32};
// buttons[0..2] 始终为最小化、最大化/还原、关闭；屏幕排列可以不同。
presentation.buttons[2].frame = {8, 5, 24, 28};
presentation.buttons[2].visual = {14, 13, 12, 12};
presentation.buttons[2].icon = {15, 14, 10, 10};
presentation.buttons[2].ellipse = true;
presentation.buttons[2].fill = {220, 60, 50, 255};
presentation.buttons[2].pressedFill = {220, 60, 50, 180};
titleBar->setPresentation(&presentation);
// 恢复默认：titleBar->setPresentation(nullptr);
```

上例只配置一个关闭按钮；其他按钮的空矩形不显示。实际应用应填齐所需按钮、品牌和附件位置。坐标相对标题栏原点，单位是逻辑像素；应用在窗口尺寸或布局模式改变时重新提交，DPI 缩放由框架完成。需要前导控件时显式填写 leading 区域。配置不自动改变标题栏控件本身高度。

- 安全 Rust：`WindowTitleBar::set_presentation(Some(&presentation)) -> bool`；`None` 恢复默认。类型为 `TitleBarPresentation`、`CaptionButtonPresentation`、`CaptionPathCommand`。
- C：`oneui_title_bar_set_presentation`；`OneUiTitleBarPresentation.struct_size` 必须为当前结构大小。结构定义见 `include/oneui/title_bar_presentation_c.h`，已由主 C API 头文件包含。
- ABI 版本为 **39**；头文件、导入库、DLL 和 Rust 绑定必须配套重建。未发布工作区曾使用的 34–38 不作为发布兼容基线；不要仅修改版本常量而混用旧 DLL。
- 配置及路径同步复制，不保留调用者指针或闭包。路径为 16×16 坐标系下的 move/line/close，最多 64 个命令；空路径使用内置图标，非空 maximizedGlyph 在最大化时替换 glyph。
- 非有限数、负尺寸、无效枚举／布尔和过长路径会被拒绝；无效提交保留上一配置。成功提交会取消尚未完成的按压，防止布局切换后误触另一按钮。
- CSS 继续提供普通按钮颜色、悬停与按压样式。`cornerRadius < 0` 继承 CSS；椭圆按钮的 fill/pressedFill 及显式 glyphColor 来自配置。`glyphOnGroupHover` 控制整组悬停显示图标。
- 所有配置操作在所属 UI 线程执行。配置不替代窗口拖动与附件交互区设置；附件区域仍需正确配置 interactive insets。

## 验证与边界

`oneui_title_bar_presentation_tests` 覆盖局部原点偏移、绘制／命中一致、三个语义动作、跨按钮释放、布局更新取消、禁用、恢复默认、CSS 独立性、所有权和 C ABI 无效输入。Windows 专项还验证真实 `WM_NCHITTEST` 的按钮 client 区与 caption 拖动区，以及双击最大化／还原。

下游在 Windows 上对两种按钮风格 × 深浅主题 × 800/1600px 共八个场景做迁移前后截图。宽窗口四图逐像素一致；窄窗口差异局限在监控按钮颜色，未发现迁移区域位置、路径或文本变化。静态图片不证明动态颜色差异的原因，也不代表物理拖动手感、跨屏 DPI、系统辅助技术或真实 macOS 已验收。

产品专属标题栏代码已从本地 SDK 核心移出，已知模式边界扫描为 0 项。检查器不是完整的依赖／许可审计；历史来源注释仍保留。大量粒子批量渲染不属于本次改动。
