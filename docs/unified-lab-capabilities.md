# OneUI 试验台能力目录

本表由 `examples/performance_lab/capabilities.json` 生成。头文件归属和示例编译通过不等于所有交互、语言入口及原生平台已验收。

构建试验台会检查 C++、UTF-8 C、Rust 示例，并实例化 `.one` 模板。每页仍有迁移验收项；`generate-lab-catalog.py --strict` 会拒绝将当前目录视为发布完成。

| 页面 | 能力组 | C++ | C · UTF-8 | Rust | .one | 迁移 |
| --- | --- | --- | --- | --- | --- | --- |
| `layout/basic` | Stack / Grid / Wrap / Panel / ScrollView | [源码](../examples/performance_lab/snippets/cpp/basic_layout.cpp) | [源码](../examples/performance_lab/snippets/c/basic_layout.c) | [源码](../examples/performance_lab/snippets/rust/basic_layout.rs) | 待补 / 见限制 | 迁移中 |
| `layout/forms` | 表单与自适应 | [源码](../examples/performance_lab/snippets/cpp/forms.cpp) | 待补 / 见限制 | 待补 / 见限制 | [源码](../examples/performance_lab/snippets/one/FormSnippet.one) | 迁移中 |
| `layout/pages` | 完整页面组合 | [源码](../examples/performance_lab/snippets/cpp/product.cpp) | [源码](../examples/performance_lab/snippets/c/product.c) | [源码](../examples/performance_lab/snippets/rust/product.rs) | [源码](../examples/performance_lab/snippets/one/FormSnippet.one) | 迁移中 |
| `layout/workspace` | 工作区分栏 | [源码](../examples/performance_lab/snippets/cpp/workspace.cpp) | 待补 / 见限制 | 待补 / 见限制 | 待补 / 见限制 | 迁移中 |
| `layout/docking` | 停靠与浮动 | [源码](../examples/performance_lab/snippets/cpp/docking.cpp) | [源码](../examples/performance_lab/snippets/c/docking.c) | [源码](../examples/performance_lab/snippets/rust/docking.rs) | 待补 / 见限制 | 迁移中 |
| `performance/overview` | 综合性能 | [源码](../examples/performance_lab/snippets/cpp/renderer.cpp) | 待补 / 见限制 | 待补 / 见限制 | 待补 / 见限制 | 迁移中 |
| `performance/charts` | 图表 | [源码](../examples/performance_lab/snippets/cpp/charts.cpp) | 待补 / 见限制 | 待补 / 见限制 | 待补 / 见限制 | 迁移中 |
| `performance/particles` | 图元与批量绘制 | [源码](../examples/performance_lab/snippets/cpp/particles.cpp) | 待补 / 见限制 | 待补 / 见限制 | 待补 / 见限制 | 迁移中 |
| `performance/lists` | 虚拟化 | [源码](../examples/performance_lab/snippets/cpp/lists.cpp) | 待补 / 见限制 | 待补 / 见限制 | 待补 / 见限制 | 迁移中 |
| `performance/materials` | 文字与材质 | [源码](../examples/performance_lab/snippets/cpp/materials.cpp) | 待补 / 见限制 | 待补 / 见限制 | 待补 / 见限制 | 迁移中 |
| `performance/idle` | 动效与空闲 | [源码](../examples/performance_lab/snippets/cpp/idle.cpp) | 待补 / 见限制 | 待补 / 见限制 | 待补 / 见限制 | 迁移中 |
| `performance/renderers` | CPU/GPU 对照 | [源码](../examples/performance_lab/snippets/cpp/renderer.cpp) | 待补 / 见限制 | 待补 / 见限制 | 待补 / 见限制 | 迁移中 |
| `performance/stability` | 稳定性 | [源码](../examples/performance_lab/snippets/cpp/renderer.cpp) | 待补 / 见限制 | 待补 / 见限制 | 待补 / 见限制 | 迁移中 |
| `features/controls` | 基础控件 | [源码](../examples/performance_lab/snippets/cpp/controls.cpp) | [源码](../examples/performance_lab/snippets/c/controls.c) | [源码](../examples/performance_lab/snippets/rust/controls.rs) | 待补 / 见限制 | 迁移中 |
| `features/text` | 文字输入与字体 | [源码](../examples/performance_lab/snippets/cpp/text.cpp) | [源码](../examples/performance_lab/snippets/c/text.c) | [源码](../examples/performance_lab/snippets/rust/text.rs) | 待补 / 见限制 | 迁移中 |
| `features/data` | 列表/表格/树及重排 | [源码](../examples/performance_lab/snippets/cpp/data.cpp) | 待补 / 见限制 | 待补 / 见限制 | 待补 / 见限制 | 迁移中 |
| `features/drawing` | 图片、图标与 Canvas 绘制 | [源码](../examples/performance_lab/snippets/cpp/image.cpp) | [源码](../examples/performance_lab/snippets/c/image.c) | [源码](../examples/performance_lab/snippets/rust/image.rs) | 待补 / 见限制 | 迁移中 |
| `features/notifications` | 弹层/Toast/系统通知 | [源码](../examples/performance_lab/snippets/cpp/notifications.cpp) | [源码](../examples/performance_lab/snippets/c/notifications.c) | [源码](../examples/performance_lab/snippets/rust/notifications.rs) | 待补 / 见限制 | 迁移中 |
| `features/menus` | 菜单与托盘 | [源码](../examples/performance_lab/snippets/cpp/menus.cpp) | 待补 / 见限制 | 待补 / 见限制 | 待补 / 见限制 | 迁移中 |
| `features/windows` | 窗口行为 | [源码](../examples/performance_lab/snippets/cpp/windows.cpp) | 待补 / 见限制 | 待补 / 见限制 | 待补 / 见限制 | 迁移中 |
| `features/multiwindow` | 应用生命周期/多窗口 | [源码](../examples/performance_lab/snippets/cpp/application.cpp) | [源码](../examples/performance_lab/snippets/c/application.c) | [源码](../examples/performance_lab/snippets/rust/application.rs) | 待补 / 见限制 | 迁移中 |
| `features/files` | 剪贴板与文件交互 | [源码](../examples/performance_lab/snippets/cpp/files.cpp) | 待补 / 见限制 | 待补 / 见限制 | 待补 / 见限制 | 迁移中 |
| `features/terminal` | 终端与日志 | [源码](../examples/performance_lab/snippets/cpp/terminal.cpp) | [源码](../examples/performance_lab/snippets/c/terminal.c) | [源码](../examples/performance_lab/snippets/rust/terminal.rs) | [源码](../examples/performance_lab/snippets/one/TerminalSnippet.one) | 迁移中 |
| `features/remote` | 远程画面与输入 | [源码](../examples/performance_lab/snippets/cpp/remote.cpp) | [源码](../examples/performance_lab/snippets/c/remote.c) | [源码](../examples/performance_lab/snippets/rust/remote.rs) | 待补 / 见限制 | 迁移中 |
| `features/reactive` | 状态绑定与异步 | [源码](../examples/performance_lab/snippets/cpp/reactive.cpp) | 待补 / 见限制 | 待补 / 见限制 | 待补 / 见限制 | 迁移中 |
| `features/theme` | 主题与本地化 | [源码](../examples/performance_lab/snippets/cpp/theme.cpp) | 待补 / 见限制 | 待补 / 见限制 | 待补 / 见限制 | 迁移中 |
| `features/diagnostics` | 运行时/无障碍/诊断/互操作 | [源码](../examples/performance_lab/snippets/cpp/diagnostics.cpp) | 待补 / 见限制 | 待补 / 见限制 | 待补 / 见限制 | 迁移中 |

## Stack / Grid / Wrap / Panel / ScrollView · `layout/basic`

关键行为：主轴与空间分配；网格列数；缩窄自动换行；横纵滚动。

Grid/Wrap 目前只有 C++ 入口；C/Rust 示例演示 Stack。

测试入口：[yoga_behavior_tests.cpp](../tests/yoga_behavior_tests.cpp)、[scroll_view_behavior_tests.cpp](../tests/scroll_view_behavior_tests.cpp)。

公开头文件：[layout/stack.h](../include/oneui/layout/stack.h)、[layout/grid.h](../include/oneui/layout/grid.h)、[layout/wrap.h](../include/oneui/layout/wrap.h)、[layout/panel.h](../include/oneui/layout/panel.h)、[layout/scroll_view.h](../include/oneui/layout/scroll_view.h)、[geometry.h](../include/oneui/geometry.h)、[color.h](../include/oneui/color.h)。

## 表单与自适应 · `layout/forms`

关键行为：校验与提交；窄窗口重排；焦点与草稿保留。

FormGrid/FormRow 组合由 C++ Compose 和 .one 提供。

测试入口：[gallery_tests.cpp](../examples/performance_lab/gallery_tests.cpp)、[workspace_lifecycle_tests.cpp](../examples/performance_lab/workspace_lifecycle_tests.cpp)。

公开头文件：[controls/form_field.h](../include/oneui/controls/form_field.h)、[controls/validation_message.h](../include/oneui/controls/validation_message.h)、[layout/adaptive_panes.h](../include/oneui/layout/adaptive_panes.h)。

## 完整页面组合 · `layout/pages`

关键行为：列表/详情往返；表单提交；保留选择及焦点；侧栏响应式；应用外壳工具栏/状态栏/分段导航；本地设备筛选与无结果状态；临时识别码与模拟连接反馈；主题和窄宽切换保留编辑状态；导航/图标按钮键盘触发、状态动作方向键选择。

AppShell/TopBar 的 C++、UTF-8 C 与安全 Rust 示例参与编译；高级 Compose/.one 页面组合为 C++。所有连接数据均为本地模拟。

测试入口：[gallery_tests.cpp](../examples/performance_lab/gallery_tests.cpp)、[workspace_lifecycle_tests.cpp](../examples/performance_lab/workspace_lifecycle_tests.cpp)、[product_tests.cpp](../examples/performance_lab/product_tests.cpp)、[shell.rs](../bindings/rust/oneui/tests/shell.rs)。

公开头文件：[layout/app_shell.h](../include/oneui/layout/app_shell.h)、[layout/product_shell.h](../include/oneui/layout/product_shell.h)、[layout/top_bar.h](../include/oneui/layout/top_bar.h)、[layout/sidebar_nav_bridge.h](../include/oneui/layout/sidebar_nav_bridge.h)、[controls/nav_item.h](../include/oneui/controls/nav_item.h)、[controls/tile.h](../include/oneui/controls/tile.h)、[controls/card.h](../include/oneui/controls/card.h)、[ui_compose.h](../include/oneui/ui_compose.h)、[ui_declarative.h](../include/oneui/ui_declarative.h)、[ui_template_support.h](../include/oneui/ui_template_support.h)、[ui_component_schema.h](../include/oneui/ui_component_schema.h)。

## 工作区分栏 · `layout/workspace`

关键行为：分栏方向与比例；键盘调整及撤销；标签重排/关闭/编辑；原终端工作区迁移。

基础控件有三种语言入口；完整终端工作区仍在迁移。

测试入口：[control_behavior_tests.cpp](../tests/control_behavior_tests.cpp)。

公开头文件：[layout/dock_view.h](../include/oneui/layout/dock_view.h)、[layout/split_view.h](../include/oneui/layout/split_view.h)、[controls/tabs.h](../include/oneui/controls/tabs.h)、[ui_keyed_tabs.h](../include/oneui/ui_keyed_tabs.h)。

## 停靠与浮动 · `layout/docking`

关键行为：同一控件拆出/合并；边缘分栏；拖放取消及回滚；宿主关闭与所有者保留。

DockSession 与共享模型有 C++/C/Rust；原 Rust 手势模型尚待提升。 共享手势命中与浮动窗口主题同步已接入；14 项 Rust 布局测试覆盖兼容门面。

测试入口：[dock_session_tests.cpp](../tests/dock_session_tests.cpp)、[dock_session.rs](../bindings/rust/oneui/tests/dock_session.rs)、[dock_gesture_tests.cpp](../tests/dock_gesture_tests.cpp)。

公开头文件：[layout/dock_model.h](../include/oneui/layout/dock_model.h)、[layout/dock_session.h](../include/oneui/layout/dock_session.h)、[dock_model_c.h](../include/oneui/dock_model_c.h)、[dock_session_c.h](../include/oneui/dock_session_c.h)。

## 综合性能 · `performance/overview`

关键行为：固定原场景；绘制与回调采样；负载切换。

原场景 CLI 独立运行；普通页面不承担自动基准。

测试入口：[renderer_tests.cpp](../examples/performance_lab/renderer_tests.cpp)。

公开头文件：[platform/dpi.h](../include/oneui/platform/dpi.h)。

## 图表 · `performance/charts`

关键行为：追加采样；暂停及恢复；隐藏页停止采样。

TimeSeriesChart 主要为 C++；Sparkline 有 C/Rust。

测试入口：[control_behavior_tests.cpp](../tests/control_behavior_tests.cpp)。

公开头文件：[controls/time_series_chart.h](../include/oneui/controls/time_series_chart.h)、[controls/sparkline.h](../include/oneui/controls/sparkline.h)。

## 图元与批量绘制 · `performance/particles`

关键行为：不同图元路径；负载调整；绘制耗时。

Canvas 是 C++ API；不为 C/Rust 展示不存在的绘图接口。

测试入口：[particle_tests.cpp](../examples/performance_lab/particle_tests.cpp)。

公开头文件：[canvas.h](../include/oneui/canvas.h)。

## 虚拟化 · `performance/lists`

关键行为：万行滚动；可见区域绘制；稳定 ID 与选择。

虚拟列表有 C++/C/Rust。

测试入口：[control_behavior_tests.cpp](../tests/control_behavior_tests.cpp)。

公开头文件：[controls/virtual_list.h](../include/oneui/controls/virtual_list.h)。

## 文字与材质 · `performance/materials`

关键行为：渐变与阴影；段落排版；材质参数。

底层 Canvas/TextLayout 为 C++；控件样式经 C/Rust 设置。

测试入口：[effects_tests.cpp](../examples/performance_lab/effects_tests.cpp)、[text_layout_tests.cpp](../tests/text_layout_tests.cpp)。

公开头文件：[text.h](../include/oneui/text.h)、[style.h](../include/oneui/style.h)、[style_adapter.h](../include/oneui/style_adapter.h)。

## 动效与空闲 · `performance/idle`

关键行为：有限动效；减少动态效果；空闲停止重绘。

高级动画组合为 C++/.one；C/Rust 可驱动基础控件。

测试入口：[motion_tests.cpp](../examples/performance_lab/motion_tests.cpp)。

公开头文件：[animation.h](../include/oneui/animation.h)、[effect_transition.h](../include/oneui/effect_transition.h)、[style_transition.h](../include/oneui/style_transition.h)、[controls/reveal.h](../include/oneui/controls/reveal.h)、[ui_motion_diagnostics.h](../include/oneui/ui_motion_diagnostics.h)。

## CPU/GPU 对照 · `performance/renderers`

关键行为：CPU/GPU 独立子进程；实际后端及回退；相同物理尺寸；取消及子进程回收。

原生互操作平台相关；不把 CPU 绘制耗时当作呈现帧率。

测试入口：[process_tests.cpp](../examples/performance_lab/process_tests.cpp)、[desktop_renderer_tests.cpp](../tests/desktop_renderer_tests.cpp)。

公开头文件：[native_interop.h](../include/oneui/native_interop.h)。

## 稳定性 · `performance/stability`

关键行为：场景切换；有界资源采样；停止后台任务。

短时完成不能替代长时间泄漏验收。

测试入口：[lifecycle_tests.cpp](../examples/performance_lab/lifecycle_tests.cpp)。

公开头文件：[subscription.h](../include/oneui/subscription.h)。

## 基础控件 · `features/controls`

关键行为：点击/禁用；选择与键盘；进度联动；交互 Begin/Commit/Cancel。

ButtonBridge 是 C++ 产品组合 helper；RadioGroup 的 Rust 入口目前为 raw。

测试入口：[control_behavior_tests.cpp](../tests/control_behavior_tests.cpp)、[interaction_c_api_tests.cpp](../tests/interaction_c_api_tests.cpp)。

公开头文件：[controls/button.h](../include/oneui/controls/button.h)、[controls/button_bridge.h](../include/oneui/controls/button_bridge.h)、[controls/icon_button.h](../include/oneui/controls/icon_button.h)、[controls/checkbox.h](../include/oneui/controls/checkbox.h)、[controls/switch.h](../include/oneui/controls/switch.h)、[controls/radio_group.h](../include/oneui/controls/radio_group.h)、[controls/slider.h](../include/oneui/controls/slider.h)、[controls/select.h](../include/oneui/controls/select.h)、[controls/progress_bar.h](../include/oneui/controls/progress_bar.h)、[controls/interactive_surface.h](../include/oneui/controls/interactive_surface.h)、[controls/separator.h](../include/oneui/controls/separator.h)。

## 文字输入与字体 · `features/text`

关键行为：中文/Emoji/双向文字；选区/撤销/重做；密码及多行；字体枚举。

系统 IME 仍须原生验收；TextInputBridge 为 C++ helper。

测试入口：[text_layout_tests.cpp](../tests/text_layout_tests.cpp)、[system_fonts_tests.cpp](../tests/system_fonts_tests.cpp)。

公开头文件：[controls/text_field.h](../include/oneui/controls/text_field.h)、[controls/text_input_bridge.h](../include/oneui/controls/text_input_bridge.h)、[controls/label.h](../include/oneui/controls/label.h)、[fonts.h](../include/oneui/fonts.h)。

## 列表/表格/树及重排 · `features/data`

关键行为：稳定 ID；多选与范围选择；展开/折叠；内部重排与外部 ID 拖放。

SelectionModel 为 C++，C/Rust 通过控件选择接口。

测试入口：[control_behavior_tests.cpp](../tests/control_behavior_tests.cpp)、[interaction_c_api_tests.cpp](../tests/interaction_c_api_tests.cpp)。

公开头文件：[controls/list.h](../include/oneui/controls/list.h)、[controls/table.h](../include/oneui/controls/table.h)、[controls/tree_view.h](../include/oneui/controls/tree_view.h)、[selection_model.h](../include/oneui/selection_model.h)、[input/item_drag.h](../include/oneui/input/item_drag.h)、[layout/reorderable_grid.h](../include/oneui/layout/reorderable_grid.h)。

## 图片、图标与 Canvas 绘制 · `features/drawing`

关键行为：RGBA 图像；缩放与双轴对齐；圆角；图标；像素更新、清空与恢复；曲线、透明度、线性/径向渐变与裁剪。

ImageView 有 C++/C/Rust；Canvas 自绘只有 C++，页面提供真实片段，性能对照见粒子页。

测试入口：[control_behavior_tests.cpp](../tests/control_behavior_tests.cpp)、[media_tests.cpp](../examples/performance_lab/media_tests.cpp)。

公开头文件：[controls/image_view.h](../include/oneui/controls/image_view.h)、[controls/icon_view.h](../include/oneui/controls/icon_view.h)、[icon.h](../include/oneui/icon.h)、[ui_icon_names.h](../include/oneui/ui_icon_names.h)。

## 弹层/Toast/系统通知 · `features/notifications`

关键行为：权限查询/请求；同 ID 替换/撤销；默认点击/操作按钮；Toast 队列/超时/关闭；弹层焦点约束；弹层外部点击放行/关闭/阻止；Esc 策略与状态绑定；位置翻转/嵌套焦点恢复；切页关闭弹层并保留编辑状态。

系统服务及权限依赖平台；Accepted 不等于已显示。 三种 Popup 模式、状态绑定、位置翻转和双层焦点已迁入；C/C++/Rust 片段参与编译。C/Rust 暂无独立 Esc 策略 setter。

测试入口：[application_lifecycle_tests.cpp](../tests/application_lifecycle_tests.cpp)、[win32_notification_activation_tests.cpp](../tests/win32_notification_activation_tests.cpp)、[overlay_host_behavior_tests.cpp](../tests/overlay_host_behavior_tests.cpp)、[overlay_host_lifetime_tests.cpp](../tests/overlay_host_lifetime_tests.cpp)、[popup_tests.cpp](../examples/performance_lab/popup_tests.cpp)。

公开头文件：[notification.h](../include/oneui/notification.h)、[controls/toast_host.h](../include/oneui/controls/toast_host.h)、[controls/toast.h](../include/oneui/controls/toast.h)、[controls/dialog.h](../include/oneui/controls/dialog.h)、[controls/popup.h](../include/oneui/controls/popup.h)、[controls/state_view.h](../include/oneui/controls/state_view.h)、[controls/badge.h](../include/oneui/controls/badge.h)、[controls/icon_badge.h](../include/oneui/controls/icon_badge.h)、[controls/status_strip.h](../include/oneui/controls/status_strip.h)、[layout/overlay_host.h](../include/oneui/layout/overlay_host.h)。

## 菜单与托盘 · `features/menus`

关键行为：应用菜单栏；多级/勾选/单选；托盘隐藏恢复；快捷键。

Mac 顶部菜单；Windows/Linux 窗口内菜单。托盘依赖桌面环境。

测试入口：[native_menu_tests.cpp](../tests/native_menu_tests.cpp)、[native_menu_c_api_tests.cpp](../tests/native_menu_c_api_tests.cpp)。

公开头文件：[native_menu.h](../include/oneui/native_menu.h)、[controls/menu.h](../include/oneui/controls/menu.h)、[command.h](../include/oneui/command.h)。

## 窗口行为 · `features/windows`

关键行为：实际/请求状态；全屏/置顶/调整尺寸；尺寸约束/位置恢复；关闭取消/异步确认。

系统可能拒绝位置、聚焦及窗口状态请求；Wayland 限制需呈现。

测试入口：[application_lifecycle_tests.cpp](../tests/application_lifecycle_tests.cpp)、[desktop_input_tests.cpp](../tests/desktop_input_tests.cpp)。

公开头文件：[platform/window.h](../include/oneui/platform/window.h)、[platform/window_state.h](../include/oneui/platform/window_state.h)、[platform/monitor.h](../include/oneui/platform/monitor.h)、[controls/window_title_bar.h](../include/oneui/controls/window_title_bar.h)、[layout/title_bar_bridge.h](../include/oneui/layout/title_bar_bridge.h)、[title_bar_presentation_c.h](../include/oneui/title_bar_presentation_c.h)。

## 应用生命周期/多窗口 · `features/multiwindow`

关键行为：退出策略；所属窗口/异步模态；类型化共享状态；关闭后无失效投递；通知冷启动。

三种语言 API 已有；真实通知点击与跨屏/IME 验收仍待完成。

测试入口：[application_lifecycle_tests.cpp](../tests/application_lifecycle_tests.cpp)、[dock_session_tests.cpp](../tests/dock_session_tests.cpp)。

公开头文件：[application.h](../include/oneui/application.h)、[modal_session.h](../include/oneui/modal_session.h)。

## 剪贴板与文件交互 · `features/files`

关键行为：复制粘贴；打开/保存/目录；系统文件拖入。

对话框与剪贴板依赖原生桌面会话。

测试入口：[file_drop_tests.cpp](../tests/file_drop_tests.cpp)、[x11_file_drop_tests.cpp](../tests/x11_file_drop_tests.cpp)。

公开头文件：[clipboard.h](../include/oneui/clipboard.h)。

## 终端与日志 · `features/terminal`

关键行为：四组 Rust 所有者终端；关闭重开保留原对象和内容；主副终端分屏、可见文字搜索、模拟命令与日志；内存文件多选、筛选、目录、上传下载和删除；窄屏终端／文件／监控／日志切换；分栏与折叠保留比例；显式采样、离页取消、返回保留状态且不自动恢复采样。

终端工作区已迁入本页：C++ 模板宿主 + Rust 控件所有者，所有连接、文件和监控均为内存模拟。Windows、Mac 与 Linux/Xvfb 已通过对象状态、100 次循环、跨窗所有者迁移和隐藏页生命周期回归；实际系统输入法、跨屏 DPI 与打包后装载仍需独立验收。

测试入口：[terminal_view_behavior_tests.cpp](../tests/terminal_view_behavior_tests.cpp)、[terminal_tests.cpp](../examples/performance_lab/terminal_tests.cpp)、[terminal_dock_tests.cpp](../examples/performance_lab/terminal_dock_tests.cpp)。

公开头文件：[controls/terminal_view.h](../include/oneui/controls/terminal_view.h)、[controls/log_view.h](../include/oneui/controls/log_view.h)。

## 远程画面与输入 · `features/remote`

关键行为：RGBA/BGRA 全帧与共享缓冲所有者；局部更新与 NV12 明确拒绝；缩放与远程坐标映射；默认/隐藏/位图光标；文字、原始键码、滚轮和释放事件；按需播放，离页停止且不自动重启。

全部数据本地模拟，无网络和解码器。NV12 是保留枚举，目前提交会被拒绝；共享所有者、局部更新、播放离页停止和输入释放已验证。真实系统 IME 输入仍需原生验收。

测试入口：[control_behavior_tests.cpp](../tests/control_behavior_tests.cpp)、[desktop_input_tests.cpp](../tests/desktop_input_tests.cpp)、[remote_tests.cpp](../examples/performance_lab/remote_tests.cpp)、[remote_page_tests.cpp](../examples/performance_lab/remote_page_tests.cpp)。

公开头文件：[controls/realtime_frame_view.h](../include/oneui/controls/realtime_frame_view.h)、[controls/remote_input_region.h](../include/oneui/controls/remote_input_region.h)、[platform/key_mapping.h](../include/oneui/platform/key_mapping.h)。

## 状态绑定与异步 · `features/reactive`

关键行为：State/Computed/Binding；UI dispatcher；取消后台任务；NativeHost 所有权。

C++ 响应式层与 Rust 自有状态；C 使用 setter/callback。

测试入口：[reactive_lifetime_tests.cpp](../tests/reactive_lifetime_tests.cpp)。

公开头文件：[reactive.h](../include/oneui/reactive.h)、[ui_reactive.h](../include/oneui/ui_reactive.h)、[ui_native_host.h](../include/oneui/ui_native_host.h)、[ui_app.h](../include/oneui/ui_app.h)、[ui_declarative_app.h](../include/oneui/ui_declarative_app.h)。

## 主题与本地化 · `features/theme`

关键行为：浅深主题；密度；本地化；继承与状态样式。

高层主题组合为 C++/.one；C/Rust 可设置样式与本地化。

测试入口：[style_scope_tests.cpp](../tests/style_scope_tests.cpp)。

公开头文件：[style_sheet.h](../include/oneui/style_sheet.h)、[localization.h](../include/oneui/localization.h)、[material3_tokens.h](../include/oneui/material3_tokens.h)、[ui_density.h](../include/oneui/ui_density.h)、[ui_native_theme.h](../include/oneui/ui_native_theme.h)、[ui_theme.h](../include/oneui/ui_theme.h)。

## 运行时/无障碍/诊断/互操作 · `features/diagnostics`

关键行为：焦点链与语义；布局快照；开发工具；ABI/错误语义；稳定 UI 所有权。

开发工具可在 SDK 构建时关闭；原生辅助技术须分别验收。

测试入口：[developer_tools_tests.cpp](../tests/developer_tools_tests.cpp)、[c_api_behavior_tests.cpp](../tests/c_api_behavior_tests.cpp)。

公开头文件：[widget.h](../include/oneui/widget.h)、[view.h](../include/oneui/view.h)、[developer_tools.h](../include/oneui/developer_tools.h)、[error.h](../include/oneui/error.h)、[export.h](../include/oneui/export.h)、[oneui.h](../include/oneui/oneui.h)、[oneui_c_api.h](../include/oneui/oneui_c_api.h)、[capi/base.h](../include/oneui/capi/base.h)、[capi/controls.h](../include/oneui/capi/controls.h)、[capi/data.h](../include/oneui/capi/data.h)、[capi/layout.h](../include/oneui/capi/layout.h)、[capi/remote.h](../include/oneui/capi/remote.h)、[capi/window.h](../include/oneui/capi/window.h)、[ui.h](../include/oneui/ui.h)、[ui_dev.h](../include/oneui/ui_dev.h)、[ui_focus.h](../include/oneui/ui_focus.h)、[ui_layout_diagnostics.h](../include/oneui/ui_layout_diagnostics.h)。
