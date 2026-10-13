# WP4：工作区交互组件设计

状态：**design-2，已评审；S2 验收通过、ABI 44 冻结；S3→S8 实施结果另见各步报告**。日期：2026-10-10。评审基线：7257eb4；S2 基线：b48df20；权威裁决见 `I/docs/codex/reports/WP-4-design-review.md`。

OneUI 基线 `ee52acbee6a3a387bf875325026a177f1203ea77`，工作树 `E:/project/byname/oneui-views`，分支 `codex/workspace-components`。iShell 只读基线 `61d784be647dc828004ab3865d37d0b6eafbd706`。本文所有行号对应这两个快照；`I/` 表示 `E:/project/tauri/ishellpro-arch/`，`O/` 表示当前 OneUI 工作树。

依据：任务书第 0–3 节、第 5 节 WP4/WP6、第 9 节，`I/docs/codex/reports/WP-3-design-review.md` 的 D6/D8，以及 `WP-3-review.md`。已阅读 OneUI 文档 55–59、S1–S6 报告及 `docs/benchmarks/language-neutral-views/` 的 README、results、listperf。旧 S4–S6 报告中的 ABI 45、列表性能失败属于历史候选记录；以最终验收和当前源码为准：**ABI 44，列表性能已通过**。

S2 检查点已经通过，Claude 授权连续实施 S3→S8，完成后停在最终检查点。不得重配置 capture、启动 iShell 或修改原 OneUI 工作区。本文记录设计契约；各步报告区分实际实现、测试证据及未达标项。第 10 节为已确认裁决。产物预算按 S2 后续裁决，仅统计本工作树 build/，上限 30 GiB，不统计或清理主仓库其他会话的目录。

## 1. 现状调查

### 1.1 分屏、标题、拖动：精确参考行为

| 证据 | 行为与边界；WP4 处理 |
|---|---|
| `I/native-shell/src/terminal_pane_ui.rs:141–169`；`terminal_pane_ui/layout_motion.rs:45–113` | 200ms，`t=clamp((now-start)/200,0,1)`，`ease=1-(1-t)^3`。第一次 rAF 只置 warmed，下一次 rAF 才设 start（该次 t=0）。运动前获取 ResizeHold、最小尺寸置 0、设置起始比例；结束一次性 settle、恢复最小尺寸、释放 hold、执行 done。调度失败直接 finish；禁用系统动画或空轨道立即应用终态。不是 CSS 默认 transition，也不是有超调的物理弹簧。 |
| `I/native-shell/src/local_terminal.rs:249–262,272–321` | UI 线程嵌套 hold 计数；每终端一个 resize key，BTreeMap 只保存最后一次 rows/columns。最外层 hold 释放才逐终端调用 session.resize。hold 的作用是阻止 PTY/SSH 重排，不是停止显示区几何变化或停止终端输出。 |
| `I/native-shell/src/product_shell/terminal_workspace_layout.rs:106–173`；`product_shell.rs:761–778` | 初始比例 clamp 到 0.001…0.999；gap=5，divider hit=10，叶最小宽 220/高 130。拖动比例即时显示，最后变化后静止 120ms 才释放 resize hold；指针仍按下时静止也可能释放。ratio committed 负责持久化。这与“整次拖动仅一次 resize”并不等同，见 Q4。 |
| `I/native-shell/src/terminal_pane_ui.rs:238–267`；`terminal_pane_ui/layout_motion.rs:195–226` | 子树最小尺寸按轴求和加 gap，交叉轴取 max；分屏内容尺寸取 viewport 与树 minimum 的较大值，使用可滚动内容承载，隐藏滚动条 chrome。最大化 minimum=0。不能简单压扁全部窗格以适应窄窗口。 |
| `I/native-shell/src/product_shell/terminal_workspace_layout.rs:204–237,280–350` | 新建同主机/本地会话由应用决定，先准备会话与布局；新叶由边缘展开。关闭先走会话确认，再将父比例折至 0（第一子）或 1（第二子），最后关闭该会话并折叠树；兄弟会话不销毁。PaneHeader 的关闭操作在只有一个可见叶时不执行。 |
| `I/native-shell/src/terminal_pane_ui/pane_actions.rs:31–71,133–212` | 中央交换 leaf，边缘先摘源、折叠原父，再在目标旁按 0.5 插入；源取得焦点。最大化只对祖先轨道显示 1/0，`commit=false`，保存比例不变；还原反向播放。均分将**每个 split**设 0.5，不是让不平衡树所有叶面积相等。普通移位/交换当前没有额外分屏缓动，不能新增飞行动画改变体验。 |
| `I/native-shell/src/terminal_pane_ui.rs:349–398,429–623` | 标题 29px、激活线 1px，内容 padding 上下8/左右16；按钮24×24，徽标17×17，拖柄36×26。拖柄阈值4px，标题拖动8px；标题单击激活、双击最大化、右键更多，拖柄单击移动菜单。按钮子元素自行消费输入，不能同时启动标题拖动。width<320 隐藏快捷分屏与最大化；路径保留，标题自然宽 ceil 后 clamp 36…120，受 `max(width-(compact?128:184),36)` 约束。 |
| `I/native-shell/src/terminal_pane_ui/layout_motion.rs:287–400` | 标题只在多叶、广播成员或 overview 时显示。标题正文实际优先显示“源/接收者”角色，并非总显示会话名；会话/地址/类型/连接状态在 tooltip，隐私模式由应用脱敏。metadata 行保持隐藏；返回实时输出只在 viewing_history 时可见。overview 的按钮文案/动作是应用策略，不能被默认分屏命令覆盖。 |
| `I/native-shell/assets/ishell_product.css:4603–4606,4628–4629` | 非 active/hover 时前四按钮与 grip opacity=0，仍保留命中与 tab 顺序；focus-visible/hover 显示。广播成员按钮持续显示。pane 预览只有一块强调色，落点 opacity .16/border2/radius10，抬起无落点 .06/border1，无文字、无方向 chip。 |
| `I/native-shell/src/terminal_pane_ui/pane_drag.rs:166–321` | 最大化禁止窗格拖动；标签拖出还要求非 overview 且源标签仅1会话。Started/Updated/**Dropped** 均重新命中最后坐标；Cancelled 不提交。落在标签栏：多窗格中的 pane 才能成为新标签；单叶或来自标签的拖动无操作。普通 pane 落自身/空白保留原地抬起预览，标签无目标则不显示。释放后延迟操作，若当前树不等于拖动开始快照则丢弃，不能复活被关闭的窗格。 |
| `I/native-shell/src/terminal_pane_ui.rs:77–103`；`pane_drag.rs:323–408` | **iShell 标签拖出不是 DockSession 的 TabMerge 模式**：命中另一 pane 后，无论中央或边缘，按归一化距离选最近 Left/Right/Top/Bottom，创建半区；不交换。源是当前树成员则禁止。目标树扩展后原单叶标签消失，终端仍是同一对象。pane 拖到标签栏先折叠动画，再成为独立标签并保持源焦点。 |
| `I/native-shell/src/terminal_dock_interaction.rs:8–27`；`pane_drag.rs:411–464` | 预览跟随是指数逼近 `a=1-exp(-dt/45)`；首帧dt=16ms，后续 clamp 0…50；x/y/w/h 差值均<0.4时直接吸附。改目标从当前显示矩形继续，无超调。每手势最多一个排队 rAF，token 防旧回调修改新手势；系统减少动画直接吸附。 |
| `I/native-shell/src/terminal_pane_ui/pane_drag.rs:5–80` | 相邻导航：方向边距 gap<-1 排除；横向查 y 重叠，纵向查 x 重叠。分数=`(交叉轴重叠>0?0:10000)+max(gap,0)+abs(交叉轴中心差)*.25`，取最小；等分保留树 DFS 首个叶。最大化时不导航/交换；少于2叶快捷键未处理；多叶但无邻居仍消费已匹配的快捷键。交换后焦点随源 key 移动。快捷键映射归应用。 |
| `I/native-shell/src/terminal_pane_ui/pane_drag.rs:83–163`；`layout_motion.rs:289–305` | 广播角色三态为 none/source/receiver；运行状态 Off/Running/Paused 与角色正交。Off 时点 active=广播给本标签其余叶，点其他叶=active 为源、该叶为接收者；点源结束；点接收者移除，最后一个移除则结束；点非成员加入，保持 Running/Paused。源未连接拒绝，接收者未连接跳过。单叶不能建立组。 |

广播 tooltip 必须保留五个分支（从 `layout_motion.rs:293–302` 提取）：

1. 源：“广播源：此窗格的输入会同步到其他成员 · 单击结束广播”。
2. 接收者：“正在接收广播 · 单击移出广播”。
3. 已有广播的非成员：“加入广播，接收广播源的输入”。
4. Off 的 active：“从此窗格广播到本标签的所有窗格”。
5. Off 的非 active：“从当前窗格广播到此窗格”。

这些文案由 VM 提供以便本地化，组件不推断广播安全策略。WP6 必须保留：源不随焦点变化、接收者自己输入暂停、多行粘贴先确认、成员断开暂停；WP4 模拟 VM 用这些场景做契约验证，不把协议转发搬进 OneUI。

### 1.2 工作台、SFTP、监控

| 证据 | 精确行为及边界 |
|---|---|
| `I/native-shell/src/terminal_workbench_layout.rs:88–109,337–359,435–473` | 默认 root H=.699；左 V=.53（Terminal/SFTP），右 V=.5（Monitor/AI）。工作台 gap8。Terminal dock min340×180、float min320×180；其他 dock280×150、float240×160。内容注册一次，后续移动 ID；compact 仍保留浮动工具。split changed 更新预览模型，committed 才存盘。 |
| `I/native-shell/src/terminal_dock_interaction.rs:83–214,1023–1175` | 拖柄/标题阈值7px，标题双击最大化；Alt 禁用落点、显式转浮动。compact 的 docked 模块不可拖，已有 float 可拖。落点排除自身和其他 float。docked 无有效目标且无 Alt 则原样返回；已有 float 可自由移动。释放时重新读取坐标和 Alt，只有快照未变才提交。Esc/capture cancel 回滚显示，不写持久状态。 |
| 同文件 `:1069–1141`；`O/src/core/dock_hit_test.cpp:71–90` | 已浮动源用严格引导：42px 边缘或四个中央方向按钮；docked 源用最近物理边。方向按钮42×36，相对目标中心 L(-68,-18)、R(26,-18)、T(-21,-58)、B(-21,22)。与 pane 无文字预览不同，dock 有方向图标及“松开平分/浮动/Esc”说明。真实 float 跟指针即时走，只有目标预览指数跟随。 |
| `I/native-shell/src/terminal_dock_interaction.rs:858–941,1177–1256` | 菜单浮动宽上限560/高620，原尺寸无效时400×360；工作台内置顶，不是 OS topmost。浮动8向 resize 从 original+总delta 算，碰最小值后反向可立即恢复；release 最新点提交，陈旧快照不能覆盖已关闭面板。SFTP/Monitor 最大化分别映射产品 presentation/focused；Terminal/AI 使用 manual_maximized。Terminal 没有“收起面板”。这些产品差异须 VM 决策。 |
| `O/src/core/dock_geometry.cpp:83–123` | clamp 后 width=min(max(width,minW),viewportW)，height同理，x/y夹在工作台内；工作台比最小值小时以可用宽高为限。resize 西/北保持对边；东/南受工作台边界约束。窗口变窄时的显示 clamp 不覆盖已保存的 float bounds。 |
| `I/native-shell/src/terminal_workbench_state.rs:84–198`；`O/src/core/dock_workspace.cpp:69–172,187–207` | 快照保存 root、floating(bounds/pinned/order)、hidden 的 before/after/target/edge/ratio/旧float。版本1，字符串上限65536 bytes，四模块必须齐，恢复几何校验；maximized 不存盘。show 优先恢复完全相同 after 对应的 before，否则靠 target 恢复，否则放 root 右侧 ratio .7；无 root 则独叶。redock 复用锚点。排序按 pinned/order；顺序溢出按当前绘制序重编号。 |
| `I/native-shell/src/terminal_sftp/controller.rs:930–977` | 判定宽先减2并 clamp≥0；height取面板记录高度。并排 iff W≥860、H>0、W≥1.6H。并排 browser 填充、tree224宽、details292宽且 W≥1100；非并排只显示 selected_pane（0 browser/1 tree/2 details），含详情页，因此不能把“<1100永不显示详情”写死。并排隐藏切换器。 |
| 同文件 `:932–943,978–980` | 窗口宽≤900影响标题 padding8/gap5，否则12/10；面板 W≥620、W<400 还有局部紧凑分支。窗口断点不能机械改成组件断点；WP5 保留这部分产品策略。 |
| `I/native-shell/src/terminal_monitor_v9.rs:645–720`；`terminal_monitor_overview.rs:257–300`；`terminal_monitor_v9_resources.rs:88–150`；`terminal_monitor_activity.rs:202–218` | width≤0忽略，内容宽=width-39；内容≤330 narrow，标题<338 compact。CPU/网络行按实际字体测量宽换行；overview 按350/290下限与文本宽，activity 按max(required,340)换行。**本快照未找到资源整页按超宽阈值切多列的实现**；不编造既有阈值。WP4 提供能力与试验台示例，产品阈值见 Q6。 |

### 1.3 OneUI 的复用边界

| 当前入口 / 源码证据 | 决定 |
|---|---|
| `include/oneui/layout/split_view.h:17–47`；`src/core/split_view.cpp:176–190,210–243,324–403` | 复用 divider 几何、命中、可访问性和键盘规则。方向步长 .025；Home/End 到约束边界，Ctrl/Alt/Win不处理；双击=.5；释放先处理末坐标；Esc恢复dragBegin且无commit。注意旧 resetInteractionState 调 finish（commit），并非 cancel：不能默默改旧 SplitView，PaneGrid 的 capture-loss 规则见 Q4。 |
| `src/core/split_view.cpp:324–350` | 可用尺寸不足时 SplitView 按两边minimum占比，而 `dockGeometry` 不足时仍按原ratio，两者有差异。PaneGrid 沿用 iShell 的内容 minimum+滚动面方案，常态不触发冲突；不能混用这两个 fallback。 |
| `include/oneui/controls/tabs.h:52–65`；`src/core/tabs.cpp:554–576,647–654,1091–1097` | 复用 setOnTabDragOut，阈值hypot≥5，离开strip转交，phase0/1/2/3；优先于 detachRequested。适配时立即把 index 固定为业务 key；后续重排不得改变源。当前 `.one` 未接通 drag-out，需要注册。 |
| `include/oneui/layout/dock_model.h:12–122`；`src/core/dock_{node,geometry,hit_test,workspace}.cpp` | 已有共享纯模型/命中/几何/C ABI，Rust workspace_* 是兼容 facade，不应复制 Rust 算法。复用常量与变换语义；当前 DockNode 验证深度≤64、叶≤1024（dock_node.cpp:98–115），递归几何重复求minimum，不能直接满足任意深度及运动预算，需迭代索引和缓存。 |
| `include/oneui/layout/dock_session.h:19–76`；`src/core/dock_session_drag.cpp:39–98,130–147` | DockSession 拥有布局、窗口和原生拖动，并在未处理的 OS 拖动后创建新窗口；TabMerge 20%边区与 iShell 标签就近边不同。**不作为 PaneGrid/DockWorkspace 控制器**。复用独立的准备/提交思想与身份测试，跨窗口拖动不进入 WP4。 |
| `include/oneui/layout/dock_view.h:11–30`；`src/core/dock_view.cpp:43–79` | DockView 是上下左右中区域布局，不是树形停靠工作台。继续保留，不能换名宣称已覆盖。 |
| `docs/55-native-host-and-terminal-workbench.md:31–49`；`docs/56-declarative-terminal-workspace.md:23–61`；`docs/57-workspace-layout-components.md:21–79`；`docs/58-declarative-workspace-stage.md:5–11` | 复用 NativeHost/主题opt-in、稳定Tabs、Workspace/DockPanel区域语义。DockPanel 只是标题/正文/页脚与收起保留；55–57 的 C++ 生成说明已被59的 IR路径取代。1050窗口断点不是容器查询。 |
| `include/oneui/layout/stack.h:43–101`；`include/oneui/view.h:14–32`；`bindings/rust/oneui/src/layout/stack.rs:22–67` | Stack 有 add、方向、flex；View已有reconcileChildren/prepareChildren事务，**没有公开单项 insert/remove**。复用children事务补足，不再clear+add。Yoga已有measure/arrange缓存（src/core/stack_yoga.cpp），重排须同步失效。 |
| `include/oneui/animation.h:10–58`；`include/oneui/ui_motion_diagnostics.h:20–41` | 复用 motionEnabled、EaseOutCubic与时钟。MotionAudit 默认1000/60≈16.67ms，WP4 必须显式传16.0，不能偷换验收阈值。Pane preview 的45ms指数跟随独立于FloatTransition。 |
| `include/oneui/oneui_view_c_api.h:114–154,218–232`；`src/capi/oneui_c_api_view_host.cpp:51–100` | ABI44已有多标量invoke、子VM notifier、host owner租约。scope参数已存在但当前非NULL返回INVALID_ARGUMENT。attach需空host和未挂载content，detach立即释放owner。动态定位需完善索引，而非改VTable。 |
| `src/views/tree.cpp:70–92`；`tools/viewc/lower_attributes.cpp:90–97`；`src/views/format/ir_graph.cpp:104–113` | repeat目前创建Column包装，host全局按name入map，编译器和reader均禁止动态host。WP4必须同时改编译器、reader、运行时和热重载，不能只去掉编译报错。 |
| `src/views/format/schema.cpp:60–77`；`src/views/notifications.cpp:133–185`；`src/views/lists.cpp:49–66`；`src/views/record_properties.cpp:5–74` | schema拒绝递归list；列表有精确delta、prepare/commit、超4096或1MiB折叠RESET。组件列表适配目前仅Select/Tabs/Table/Chart。PaneGrid要新增领域适配及整体场景校验，不能依赖JSON或逐帧读取整张表。 |
| `include/oneui/controls/slider.h:13–35`；`include/oneui/controls/card.h:11–20`；`include/oneui/ui_component_schema.h:7–55` | Slider已有Begin/Update/Commit/Cancel（Cancel还原初值）；Card是容器，不是可点击按钮。二者未声明式注册。G1–G5见59-s1-report第3节。 |
| `src/core/style_sheet.cpp:216–225,639–707`；`src/core/style_adapter.cpp` | 有ease-out-cubic和简单选择器、CSS变量/作用域；没有容器尺寸条件求值管线。不能把 `@container` 原文交旧解析器就算支持，也不引入浏览器CSS引擎。 |
| `src/platform/win32/window_win32.cpp:100–123,2857–2866` | NVIDIA成功present后第2/3/4/6/8/12/16帧启动空线程+glFinish，吸收线程普查。保留；不重新调查shader、图集或资源加载。`C:/Users/Administrator/.claude/projects/E--project-tauri-ishellpro/memory/nvidia-gl-thread-census-stall.md` 已读。 |

## 2. 分层、状态与共同事件规则

依赖保持 `core ← views ← capi/window`。core负责几何、手势、动画、控件与样式；views负责schema数据适配、稳定实例、作用域和事件打包；C ABI只提供语言边界；VM持有分屏/停靠状态、业务权限、广播、确认框、持久化和异步会话。PaneGrid不得持有SSH/PTY/文件路径管理服务。普通标题、SFTP、监控、AI外壳必须是 `.one`，只有原生画面放NativeHost。

| 状态 | 唯一归属 |
|---|---|
| groups、root、节点及ratio、pane/session key、active group/pane、maximized、closing、float bounds/pinned/order、恢复锚点、请求处理结果 | VM权威状态；maximized/closing也是VM状态，但默认不跨进程持久化 |
| 鼠标capture、阈值检测、hover、预览rect、display ratio、动画时钟/轨道、渲染几何缓存、暂存的目标快照、当前字号算出的viewport | 组件内部瞬时状态；不能写成第二份业务模型或存盘 |
| NativeHost的Widget及外语callback owner | 内容租约；应用内容模块创建，host持有；L4只保存业务key，不能构建普通控件 |
| 容器尺寸、匹配条件、样式缓存、布局epoch | 引擎内部；不要求VM接收每帧宽高再发通知 |

所有表中的事件均经 ABI44 `invoke(vm, command_id, OneUiValue*, argc)` 传输。签名里的 `key/edge/mode` 都是utf8，序号/尺寸整数为i64，几何为f64，开关为bool；不传C++对象、JSON命令、裸指针、任意脚本。静态属性枚举由viewc校验，动态值由runtime校验。命令名绑定 `@event="commandName"`，按位置一一匹配，不在模板内调用带表达式的函数。

PaneGrid/DockWorkspace涉及预览提交的请求尾部统一 `(request:i64, revision:i64)`，request在组件实例内单调递增（正数）；revision是开始交互时的场景版本。VM只在版本仍匹配且业务准许时修改状态，并更新 `ackRequest/ackAccepted`，拒绝也必须ack。回调返回OK只表示调用成功，**不等于接受意图**。同一次VM修改先完成全部数据，再notify各字段/列表，最后notify场景revision；在回调退出后的flush发布。读回调禁止重入、flush与attach禁止在invoke内进行，沿用WP3。PaneHeader按钮没有待提交的几何预览，只发业务key；VM在执行时读取当前权威状态，异步确认另保存自己的request/revision，不能沿用过期选择。

收到请求前只显示预览；等待ack时保留旧权威树、释放pointer capture，等待态可Esc取消本地预览，不能因超时偷偷提交。推荐iShell VM在同步命令中完成几何确认；需异步确认的关闭走独立closing状态。新revision、源消失、窗口关闭、失去有效目标使旧手势取消；迟到ack不得覆盖新快照。接受后组件从当前显示几何过渡到新权威几何。所有副作用（创建/关闭会话、广播、存盘）由VM负责。

## 3. `.one` 组件与VM契约

### 3.1 PaneGrid / Pane：按身份持有内容，按树分配几何

新增 `PaneGrid`、`Pane`、`PaneHeader`。PaneGrid有两个输入集合：扁平nodes/groups是几何数据，直接子Pane集合是**全工作区稳定内容池**；不随活动标签筛选池。非活动group的Pane保留但不参加绘制/命中。叶的 `pane` 指向池key，几何树改变不改变内容作用域。

```xml
<template view-model="TerminalWorkspace">
  <Column grow="1" wrap="false">
    <Tabs view-id="sessionTabs" :items="tabs" v-model="activeGroup"
          reorderable="true" drag-domain="terminal" drag-kind="single-pane-tab"
          :drag-items="tabSources" @reorderRequested="reorderTabs" />
    <PaneGrid view-id="terminalGrid" :nodes="nodes" :groups="groups"
              :active-group="activeGroup" :active-pane="activePane"
              :maximized="maximized" :revision="revision"
              :ack-request="ackRequest" :ack-accepted="ackAccepted"
              :navigation-serial="navigationSerial" :navigation-edge="navigationEdge"
              :navigation-mode="navigationMode" drag-domain="terminal"
              gap="5" divider-hit="10" motion-ms="200" resize-idle-ms="120"
              @dropRequested="dropRequested" @ratioRequested="ratioRequested"
              @focusRequested="focusRequested" @swapRequested="swapRequested"
              @resizeSettled="resizeSettled" @transitionSettled="transitionSettled">
      <Pane v-for="pane in panes" :key="pane.id" view-id="panes"
            :pane-key="pane.id" :closing="pane.closing" :close-request="pane.closeRequest"
            min-width="220" min-height="130">
        <PaneHeader :pane-key="pane.id" :number="pane.number" :title="pane.title"
                    :path="pane.path" :active="pane.active" :visible="pane.headerVisible"
                    :broadcast-role="pane.broadcastRole" :broadcast-hint="pane.broadcastHint"
                    :viewing-history="pane.viewingHistory" :can-close="pane.canClose"
                    @activateRequested="pane.activate" @splitRequested="pane.split"
                    @maximizeRequested="pane.maximize" @closeRequested="pane.close"
                    @broadcastRequested="pane.broadcast" @liveRequested="pane.live"
                    @menuRequested="pane.menu" />
        <NativeHost host="terminal" resize-policy="settled" />
      </Pane>
    </PaneGrid>
  </Column>
</template>
```

以上新增 `drag-*`、`reorderable`、Pane标签和resize-policy尚未实现。Pane结构槽固定为一个可选PaneHeader和一个内容根（可为导入 `.one`）；无需新 scoped-slot 语言：v-for直接提供pane词法scope。多层内容用Column，空池显示PaneGrid的静态 `empty` 槽，例如 `<EmptyState slot="empty" title="暂无会话"/>`（与Pane平级且最多一个）；该槽在active group没有可见pane时出现，不受暂存pane影响。对Pane/DockItem集合，REPT成为**透明的结构集合**，不生成Column；其他v-for仍保持WP3语义。编译器通过组件的children-kind判定，不靠运行时猜测标签。

对应schema（无手写数字ID）：

```text
model TerminalWorkspace {
  r nodes: list<WorkspaceNode>
  r groups: list<WorkspaceGroup>
  r panes: list<TerminalPane>
  r tabs: list<oneui.v1.TabItem>
  r tabSources: list<TabSource>
  rw activeGroup: utf8
  r activePane: utf8
  r maximized: utf8
  r revision: i64
  r ackRequest: i64
  r ackAccepted: bool
  r navigationSerial: i64
  r navigationEdge: utf8
  r navigationMode: utf8
  command reorderTabs(source:utf8, before:utf8)
  command dropRequested(source:utf8, target:utf8, edge:utf8, request:i64, revision:i64)
  command ratioRequested(split:utf8, ratio:f64, request:i64, revision:i64)
  command focusRequested(from:utf8, target:utf8, request:i64, revision:i64)
  command swapRequested(source:utf8, target:utf8, request:i64, revision:i64)
  command resizeSettled(pane:utf8, cols:i64, rows:i64, generation:i64)
  command transitionSettled(pane:utf8, request:i64, revision:i64)
}
model WorkspaceNode key id {
  r id: utf8
  r kind: utf8
  r axis: utf8
  r ratio: f64
  r first: utf8
  r second: utf8
  r pane: utf8
}
model WorkspaceGroup key id {
  r id: utf8
  r root: utf8
}
model TabSource key id {
  r id: utf8
  r pane: utf8
  r enabled: bool
}
model TerminalPane key id {
  r id: utf8
  r number: i64
  r title: utf8
  r path: utf8
  r active: bool
  r headerVisible: bool
  r broadcastRole: utf8
  r broadcastHint: utf8
  r viewingHistory: bool
  r canClose: bool
  r closing: bool
  r closeRequest: i64
  command activate(pane:utf8)
  command split(pane:utf8, edge:utf8)
  command maximize(pane:utf8)
  command close(pane:utf8)
  command broadcast(pane:utf8)
  command live(pane:utf8)
  command menu(pane:utf8, x:f64, y:f64)
}
```

`WorkspaceNode`是结构投影；可使用应用命名model，注册表对字段名/类型做结构校验并纳入COMP/SCHM完整哈希，不把用户model强行改成保留的oneui.v1命名。所有key非空UTF-8、不含NUL；无目标使用空字符串，不发不存在的nullable值。

PaneGrid绑定属性的类型与schema一一对应。`gap/divider-hit/motion-ms/resize-idle-ms`为静态f64有限非负；默认分别5/10/200/120，iShell模板固定此值；motion曲线固定ease-out-cubic，motionEnabled仍是最高层开关。`activeGroup/activePane/maximized`是key，maximized空=未最大化；`navigationEdge`仅left/right/top/bottom或初始空，mode=focus/swap，serial=0不动作。

### 3.2 拖动、落点与方向导航

| 能力 | 属性/事件/槽与执行规则 |
|---|---|
| Pane拖动与DropSurface | PaneHeader自动注册到最近PaneGrid的drag-domain；也可在普通内容上用 `DragHandle :source="id" domain="terminal" kind="pane" threshold="4"`（default槽一个控件）。`DropSurface domain="terminal" role="tab-strip"`有default槽包Tabs；Tabs的drag-domain使其自动注册tab-strip，不能重复注册。组件内部传几何，不把每次pointer move经VM往返。 |
| 最终落点 | `dropRequested(source:utf8,target:utf8,edge:utf8,request:i64,revision:i64)`。edge=left/right/top/bottom/center/tab-strip；center=交换，tab-strip时target为Tabs的view-id，其他为pane key。源来自tab时用TabSource.pane转为内容key，edge永不center；从pane到tab-strip仅当前group多叶允许。VM用权威membership复核，不相信拖动缓存。 |
| Tabs独立对外事件 | 可选 `@dragOut="onDragOut"` 签名 `(key:utf8,phase:i64,x:f64,y:f64)`，坐标为所在window逻辑坐标。已接drag-domain时这是观察事件，不应另发一次drop。`@reorderRequested(source:utf8,before:utf8)`，before空=末尾；VM决定列表MOVE。关闭沿用 `@close(key:utf8)`。无自定义槽。 |
| DragHandle独立观察 | 可选 `@dragStateChanged(source:utf8,phase:i64)`，phase0开始/1移动/2释放/3取消；默认不接入VM热路径。DropSurface `@dropRequested`使用相同五参数，仅未被域协调器消费时发，确保一个释放只有一个请求。 |
| 方向导航 | core暴露 `focusAdjacent(edge)`、`swapAdjacent(edge)`，只查已布局的可见叶并产生上表事件，不直接改active/tree。模板通过navigationSerial/Edge/Mode驱动：同一帧内逐次执行，最多8次，超出丢弃并记录诊断；相同serial幂等。VM在每次命令回调退出/flush边界发布完整三元组，运行时入队而不等到paint覆盖；同一回调内未发布的中间值不可观察，不能由serial差值猜测方向。无邻居不发事件，最大化忽略；按1.1评分、DFS tie-break。 |

DragHandle的schema只需 `r id:utf8` 和可选 `command dragStateChanged(source:utf8,phase:i64)`；DropSurface事件绑定到所属工作区VM的dropRequested。domain/view-id/kind/role为静态字符串，不是业务类型注册。作用域限定同一View、同一window，禁止跨进程/跨窗口/文件拖入，也不调用OS拖放。不同域互不命中。单个域同一时刻只有一个drag，新的Started先取消旧手势。

几何命中按1.1/1.2精确算法，Pane中央为开区间(.28,.72)²；刚好.28/.72属于边缘，最近边tie优先Left/Right/Top/Bottom。取消包括Esc、capture loss、源失效、revision变化；末尾释放坐标必须再命中。禁用动画立即吸附；预览不参与输入、布局minimum或无障碍tab顺序。

### 3.3 PaneHeader

新增属性：`pane-key:utf8,number:i64,title:utf8,path:utf8,active:bool,broadcast-role:utf8,broadcast-hint:utf8,viewing-history:bool,can-close:bool,can-split:bool,can-maximize:bool`；通用visible/disabled/name/description沿用。role=none/source/receiver，默认none；can-*默认true，由VM在单叶/overview/业务繁忙时约束。默认title/path空、number=1、active/history=false。

事件与TerminalPane schema一致。split右侧快捷键发 `(pane,"right")`；默认更多菜单提供向右/向下、最大化/还原、均分、选择其他目标、相对移动/交换，菜单本体用 `.one` 导入并由VM驱动：增加 `command balance()`、`command chooseTarget(pane:utf8,edge:utf8)` 与3.2的dropRequested即可，不在core塞入主机选择业务。`menuRequested`坐标仅定位弹层；键盘激活使用拖柄bounds的右下角。关闭/广播/实时输出都只发意图。

槽：`identity`可替换标题/路径，`actions`可追加操作，`menu`是静态模板插槽、VM控制visible；默认拖柄/徽标/既有操作必须保持可访问名称。overview由应用传入动作标签/菜单与command绑定（采用单独导入的OverviewHeader模板组合），不让SDK认识“设为广播源”等产品动作。hover/按下/紧凑模式为组件内态；active、广播角色、headerVisible、history由VM提供。精确显隐与320断点见1.1，使用opacity隐藏与visible=false须严格区分。

### 3.4 DockWorkspace / DockItem

避免C++同名：现有纯模型仍叫 `oneui::DockWorkspace`，新控件C++名 `DockWorkspaceView`，`.one`名 `DockWorkspace`。

```xml
<DockWorkspace view-id="tools" :nodes="dockNodes" :root="dockRoot"
               :placements="placements" :revision="dockRevision"
               :maximized="maximizedTool" :compact-active="compactTool"
               :ack-request="dockAck" :ack-accepted="dockAccepted" gap="8"
               @dockRequested="dockRequested" @floatRequested="floatRequested"
               @ratioRequested="dockRatioRequested"
               @boundsRequested="boundsRequested" @pinRequested="pinRequested"
               @raiseRequested="raiseRequested" @maximizeRequested="maximizeRequested"
               @visibilityRequested="visibilityRequested" @redockRequested="redockRequested">
  <DockItem pane-key="terminal" dock-min-width="340" dock-min-height="180"
            float-min-width="320" float-min-height="180" hideable="false">
    <PanelHeader><DragHandle source="terminal" domain="tools" kind="dock" threshold="7"/></PanelHeader>
    <PanelBody><TerminalWorkspace/></PanelBody>
  </DockItem>
  <!-- SFTP / Monitor / AI 同样是导入的 .one；允许 DockItem v-for -->
</DockWorkspace>
```

域默认等于DockWorkspace的view-id；DockItem沿用DockPanel的PanelHeader/Body/可选Footer槽与语义排序，保留所有内容，不能用NativeHost包整块普通工具UI。TerminalWorkspace是静态import调用，不是把业务VM指针藏在属性中。

```text
model DockPlacement key id {
  r id: utf8
  r mode: utf8
  r x: f64
  r y: f64
  r width: f64
  r height: f64
  r pinned: bool
  r order: i64
}
// 下列成员加入页面VM（与dockNodes:list<WorkspaceNode>等绑定字段一起）。
// dockRoot/maximizedTool/compactTool:utf8；dockRevision/dockAck:i64；dockAccepted:bool。
// placements:list<DockPlacement>；所有字段只读，VM通过command更新。
```

mode=docked/floating/hidden；docked必须恰好在root可达叶中，floating不在root，hidden不显示；x/y/w/h仅floating参与几何校验，所有标量有限。`compact-active`空表示正常布局，否则显示该docked工具，并保留可见float；不存在key诊断，不任意换工具。maximized优先于compact，float置顶顺序为(pinned,order)，同序时按稳定key；VM负责raise写入新order。

完整语义命令签名（布局类带request/revision）：

```text
command dockRequested(source:utf8,target:utf8,edge:utf8,request:i64,revision:i64)
command dockRatioRequested(split:utf8,ratio:f64,request:i64,revision:i64)
command floatRequested(pane:utf8,x:f64,y:f64,width:f64,height:f64,request:i64,revision:i64)
command boundsRequested(pane:utf8,x:f64,y:f64,width:f64,height:f64,request:i64,revision:i64)
command pinRequested(pane:utf8,pinned:bool,request:i64,revision:i64)
command raiseRequested(pane:utf8,request:i64,revision:i64)
command maximizeRequested(pane:utf8,request:i64,revision:i64)
command visibilityRequested(pane:utf8,visible:bool,request:i64,revision:i64)
command redockRequested(pane:utf8,request:i64,revision:i64)
```

VM保存DockRestore锚点和版本化快照；该历史状态不参加当前绘制，无需每帧送入 `.one`。恢复时VM先验证snapshot再投影成nodes/placements；拒绝时保持旧快照并提供错误状态。可以调用已有纯Dock模型C接口计算next，也可任意语言实现等价领域状态；视图不能调用它自动执行请求。持久化不含content、会话、凭证，maximized默认不存盘，保持iShell现状。

DockWorkspace的 `nodes` 同WorkspaceNode，root/maximized/compact-active为utf8 key；revision/ack-request为i64，ack-accepted为bool。gap为非负f64默认8，divider-hit默认8（沿用SplitView的最小8px命中）；边缘/浮动拖动规则见1.2。DockItem的pane-key必填且稳定，四个min为有限非负f64，默认dock280×150/float240×160；hideable默认true。split divider只改变内部显示ratio并实时刷新子容器，release或键盘静止时请求dockRatioRequested；VM接受后更新node.ratio并持久化，取消恢复旧ratio。参考中的临时ratio模型因此落到组件瞬时态，用户可见行为保持。DockItem默认操作菜单用PanelHeader中的 `.one` 菜单绑定上述命令，hideable=false不提供隐藏意图；收起/浮动/置顶也可由应用自己的菜单命令触发，组件不得先改placement再通知。

T25 追加合同：`raise-on-press:bool` 默认 true，保留按下浮窗先请求 Raise 的旧 SDK 行为。
需要 Esc 连同持久化 order 一并保持的宿主可设 false；此时指针 drag/resize 仅预览，
显式 `Raise` 和 `Pin` 请求仍生效。`preview-dock-text`、`preview-float-text`、
`preview-keep-text` 为可绑定 UTF-8 操作提示；SDK组合可访问面板名并绘制不捕获输入的提示条。
样式 part 为 `dock-workspace-preview`（背景/边框）和 `dock-workspace-caption`（背景/文字）；
提示背景保持不透明、目的地区域保持半透明，松手/Esc立即移除反馈。

### 3.5 Stack增删与多列重排

模板继续写 `<Column><Card v-for="item in items" :key="item.id">…</Card></Column>`。schema为 `r items:list<Item>`、`model Item key id { r id:utf8 ... }`。VM INSERT/REMOVE/MOVE delta决定结构，Stack不发业务增删事件；无新增“在模板中执行insert()”语法。default槽保留任意子项。

原生补 `Stack::insert(size_t index, shared_ptr<Widget>)`、`Stack::remove(shared_ptr<Widget>) -> bool`，复用prepareChildren；同parent插入已存在child视为MOVE（index为移除后的目的序，合法0…剩余长度），remove不存在返回false。跨parent insert拒绝已挂载child，须显式remove再insert，复杂跨父移动由内部CompositionTransaction原子执行。remove不destroy仍由外部持有的Widget；移除含焦点/IME子项先commit composition再移焦点，MOVE保留IME/选区。保存相同parent的flex条目，跨parent需由目标模板明确给flex；清理被移除的weak flex记录并使Yoga缓存失效。不得用clearChildren模拟。

监控多列推荐**同一Row wrap=true、子项统一flex basis/min**，不按列数创建不同v-for树。容器查询调整basis、gap、方向或layout-order；同一结构scope/Widget保留，读取顺序仍为数据顺序（视觉order变化同时同步键盘顺序）。确有跨列容器重排时内部事务迁移保留对象，VM不创建Stack。超宽具体阈值Q6。

### 3.6 D8：字体、媒体、Slider/Card、逐项主题预览

| 组件 | `.one`示例、属性与插槽 | `.oneschema`数据/语义命令；状态归属 |
|---|---|---|
| FontPicker | `<FontPicker :items="fonts" :selected="fontId" :loading="fontsLoading" :error="fontError" preview-text="Aa 字体 0123" @selectionRequested="selectFont" @retryRequested="retryFonts"/>`。items结构{id,family,label,available}；可选preview-text、size，default空，无任意控件插槽；内部搜索、虚拟候选列表、行预览。 | `fonts:list<FontItem>,fontId:utf8,fontsLoading:bool,fontError:utf8`；FontItem各字段utf8，available bool；`selectFont(key:utf8),retryFonts()`。VM/字体服务枚举，组件只筛选与预览；选择无效key拒绝，字体临时缺失显示已选名称与fallback、不改配置。query/highlight/popup为内态，选中family为VM状态。 |
| MediaPicker | `<MediaPicker :asset="asset" :label="assetLabel" :loading="mediaLoading" :error="mediaError" accept="image" @chooseRequested="chooseMedia" @clearRequested="clearMedia" @cancelRequested="cancelMedia"/>`。可选empty/preview/error静态槽；默认预览组合MediaPreview。 | asset/label/error utf8，loading bool；`chooseMedia(),clearMedia(),cancelMedia()`。VM调用平台picker与业务导入服务；请求generation、晚到结果丢弃、取消/权限/格式错误归VM。清除只清引用，绝不自动删除文件。组件按钮在loading/无asset时正确disable，重试再次choose。 |
| MediaPreview | `<MediaPreview :source="asset" fit="cover" position="center" :opacity="opacity" @loadFailed="previewFailed"/>`；source为本地资源标识/路径utf8，fit=contain/cover/stretch，position=center或九宫格枚举，opacity f64[0,1]；empty/error槽。 | `asset:utf8,opacity:f64; command previewFailed(source:utf8,code:utf8,message:utf8)`。source更换内部取消旧解码，generation隔离回调，ImageView同一对象；缓存和解码瞬态归组件，所选资源归VM。image第一期静态图；不隐式下载URL、不加入视频编解码需求，见Q8。 |
| Slider | `<Slider :minimum="lo" :maximum="hi" :step="step" :value="level" @interaction="levelRequested"/>`；f64范围，有限、min<max、step>0，default0/1/.01；无槽。 | `r lo/hi/step/level:f64; command levelRequested(phase:i64,value:f64)`；0Begin/1Update/2Commit/3Cancel。内态保存begin/local preview；VM选择实时应用或commit应用，Cancel恢复begin、通知VM回滚其预览。`v-model="level"`可选且要求rw，通过setter请求写入；与interaction并用仅setter改变值、interaction负责事务边界，不双重执行副作用。程序性set不发interaction。 |
| Card | `<Card><Button :text="title" @click="select"/></Card>`。default槽一个内容根（多个需Column），background/border/radius/padding/shadow沿用CSS，无业务事件。 | title utf8、`command select()`由真正Button发出。Card不伪装可点击；选中态由外层Surface的VM绑定class表现，键盘Enter/Space仍走Button。注册Card通用布局/样式属性与工厂，不增加未定义的selected属性或主题专用业务Card。 |
| ThemeScope | `<ThemeScope :tokens="item.tokens"><Card>…</Card></ThemeScope>`，default槽一根；tokens记录{id,rgba}，颜色i64为RRGGBBAA，严格0…0xffffffff。主题卡外侧选择/焦点标记留在原应用主题。 | `model ThemeItem key id {r id:utf8;r tokens:list<ColorToken>;command select()}`；ColorToken的id=utf8 token名、rgba=i64。逐项delta只失效该scope；color白名单从SDK公开语义token表固定生成，禁止任意CSS文本/布局属性/URL。不改全局theme、不覆盖NativeHost内容专属配色。无事件，选择事件来自Button。 |

FontItem schema完整字段：`id:utf8,family:utf8,label:utf8,available:bool`，key=id；ColorToken key=id。所有list都沿用WP3稳定VM身份/租约。FontPicker查找区保留IME，上下键移动候选、Enter请求选择、Esc关闭不更改，无结果/加载中/错误可读；加载失败不清空旧选择。MediaPicker键盘顺序为选择→清除→取消（可用时），预览有替代文本，错误码可本地化；文件路径省略显示但tooltip可完整查看。

默认值与空态：FontPicker的size是预览字号f64（逻辑px，>0，默认14），preview-text默认“Aa 0123”，selected/error默认空、loading=false；items必填，selected用FontItem.id而不是family字符串。MediaPicker的asset/label/error默认空、loading=false，accept仅image，空asset使clear不可用；MediaPreview默认source空、fit=contain、position=center、opacity=1，九宫格为top-left/top/top-right/left/center/right/bottom-left/bottom/bottom-right。ThemeScope空tokens继承父主题，重复/未知token或越界颜色诊断并保留上一有效局部主题。上述控件的通用visible/disabled及可访问name/description沿用注册表，业务状态不通过CSS class反向解析。

逐项主题建议用SDK semantic tokens（surface/ink/muted/line/accent等）映射iShell的12套palette；命名映射在模板/VM契约固定，不开放任意style对象。字体、媒体、主题预览后台服务经dispatcher回UI；cache以资源identity+revision或token值哈希缓存，失效与释放有明确计数。通用组件不写用户设置。

## 4. 任意深度分屏树与增量事务

### 4.1 模型不递归，语义可递归

`nodes:list<WorkspaceNode>`用id引用子节点；kind=leaf/split。leaf须pane非空、first/second为空，axis=""、ratio=0.5为规范占位；split须pane为空、axis=horizontal/vertical、first/second不同且存在，ratio严格0<r<1。groups的root指向节点，允许空root表达空group；所有live nodes从某group恰好可达一次，无环、无多父、无悬挂、无重复pane key。全局pane池可保留未放入group的“正在创建/暂存”内容，但这些不绘制、不参与导航。

节点key独立于位置/first-second路径、数组index和pane key。交换的是叶引用或叶位置，不给源会话换key；内部split也有稳定key用于ratio命令。业务key绝不强转成u64：core用每view的intern表分配内部DockId，逆表用于事件；key生命周期内ID不复用。空字符串仅作缺省/无目标。

不扩展 `.oneschema` 为递归类型。新内部 `DockTreeIndex`持连续节点+key→index+parent索引，使用迭代DFS三色校验、postorder算minimum、preorder算rect；没有固定“支持2/4/16层”的特判。受每view节点总量预算约束（沿用100,000），深度不另限64；禁止把一层split对应一层C++调用栈/原生View树。PaneGrid用**平铺的持久Pane和divider层**绘制二叉几何，因此1000层模型不生成1000层事件冒泡/Yoga递归。

旧DockNode ABI与验证限制不改动；抽出共享分割/命中/浮动纯函数供两者使用，DockTreeIndex是同一Dock语义的索引表示，不另造冲突规则。旧DockWorkspace reducer适用于现有四工具；需>64深度的VM使用扁平状态变换，或后续明确扩展旧纯模型，不能暗中经受限DockNode roundtrip截断。总量超限返回LIMIT，保留旧场景。任意深度不承诺100,000可见终端还能满足16ms，测量负载见7.3。

### 4.2 不用RESET表达正常树编辑

例：初始groups[G].root=L1，nodes=[L1(leaf,P1)]，panes=[P1]。在P1右侧新建P2：

1. VM分配稳定P2、L2、S1，panes INSERT P2；nodes INSERT L2(leaf,P2)、S1(split,H,.5,L1,L2)；groups G UPDATE(root=S1)。P1/L1的key和VM地址不变。
2. 每个列表独立revision递增，发送准确before/after/index/count/keys；groups UPDATE仅root字段，pane头的active等用子notifier字段notify。最后场景revision递增。不能对根 `notify(ALL_PROPERTIES)`。
3. Runtime读取所有输入到WorkspacePatch，验证场景revision起止相等与引用闭合，准备新Pane/Host、divider/索引存储、children计划，再无分配commit。任一步失败：已准备的新scope释放，旧场景/租约/焦点不动；VM收到诊断后修复数据再notify。**这一组件级原子性是WP4新增工作**，不把WP3标量setter的非事务行为当作保证。
4. 启动1→.5轨道（右侧新叶），按第7节预热/运动；P2内容attach准备好后才进入计时运动，不逐帧新增VM/host。初始化属于Preparing，单列输入到首次反馈/内容ready的延迟，不可把任意准备慢帧冒充唯一的WarmFrame。内容失败时VM撤销新叶并显示错误，不无限等待ready。

交换P1/P2：修改两叶的pane字段或互换父的子引用，nodes UPDATE（字段级），pane池不发REMOVE/INSERT；比例修改只UPDATE一个split.ratio。移动到其他group：修改原树并折叠父、新目标插入split、UPDATE两个group root；panes列表不变。数组MOVE只改变索引排序，不代表几何拓扑变化，不能以数组index作为node身份。

关闭：VM确认后设置 `P2.closing=true,closeRequest=N`，先保留树与pane租约；组件将P2父折到终点并发 `transitionSettled(P2,N,revision)`；VM复核N后 REMOVE P2、L2、S1，将G.root UPDATE L1。一次flush完成最终结构，原session在内容卸载之后关闭，兄弟内容保持。标签独立化用同一两阶段折叠，但最终只改groups/nodes，不删除pane池或session。外部断开可直接移除叶（无动画紧急路径），需满足内容模块的明确关闭授权，不能把临时失效当作用户关闭。

仅RESET/越界delta恢复才遍历全部key并按key+VM/vtable比对；一般UPDATE只读脏字段、INSERT只acquire新VM、MOVE不acquire已有VM。缓存UTF-8→UTF-16转换与节点引用，沿用WP3 listperf结论，不能在动画帧重走record_properties整表转换。

## 5. D6：动态NativeHost定位、租约和内容不重建

### 5.1 scope足够表达身份，缺少的是路径解析

ABI44已有签名保持：

```c
ONEUI_API OneUiViewResult oneui_view_find_host(
    OneUiView *view, OneUiViewNotifier *scope,
    OneUiUtf8String name, OneUiWidget **out);
```

`scope=NULL`仍表示root；非NULL必须属于view且尚未closed，表示实例化的词法row scope。VM的connect可保存自己的notifier，但应用内容协调器不能可靠地从业务key取得它，尤其同VM绑定多个View或同集合出现两次。因此推荐**新增一个解析导出**，不改VTable：

```c
typedef struct OneUiViewScopeSegment {
    OneUiUtf8String instance_id; /* slash-free v-for view-id, qualified by call-id/ prefixes */
    OneUiUtf8String key;         /* exact stable list key; no path escaping */
} OneUiViewScopeSegment;

ONEUI_API OneUiViewResult oneui_view_resolve_scope(
    OneUiView *view,
    const OneUiViewScopeSegment *segments,
    size_t segment_count,
    OneUiViewNotifier **out_scope);
```

UI线程、回调外调用，输入数组/UTF-8仅借到返回；成功输出一个owned notifier reference，用现有notifier_release释放。0段返回root scope；失败清NULL。非法UTF-8/空段=INVALID_ARGUMENT；无显式view-id在编译期拒绝；无匹配=NOT_FOUND；view或旧scope已关闭=CLOSED；跨view scope=INVALID_ARGUMENT。segment_count≤64（模板词法嵌套预算，**不是分屏树深度**），总路径bytes≤1MiB；不使用字符串斜杠拼接业务key，因此key内的 `/`、Unicode无需自造转义。组成instance_id的原始repeat/call view-id禁止含 `/`；限定名前缀属于编译器命名空间，不由业务key推导。带工作区集合的导入及其供给槽都必须声明稳定call view-id，IR reader独立验证能力位与调用标记。

示例路径 `[("panes","session-A")]`，再 `find_host(view, scope, "terminal")`。嵌套v-for可为 `[("workspaces","W"),("panes","A")]`。同一parent scope内repeat的显式view-id唯一；导入调用点必须显式稳定view-id并参与路径命名空间（限定名使用编译器展开的 `call-id/repeat-id`，此处斜杠只在instance_id定义中，不解析业务key）。组件树的slot路径不等于Dock几何的left/right路径，交换/跨group移动不会改变以上路径。

每个渲染row实例拥有独立scope connection，即使两个repeat引用同一个VM；不能仅用(vm,property,key)合并notifier。host map改为 `(scope identity, static host name)`，同scope重名编译诊断；host名称仍静态、不可CALL改名。scope移动只更新逻辑位置，VM identity换代则旧scope关闭，旧notifier永远不重新指向新row。不得返回悬空widget句柄：borrowed host限当前稳定结构期使用，结构flush/reload后重新find；Rust HostRef不能提供可长存的裸host Widget。S2采用C侧身份检查和失效句柄墓碑提供同等失效保护：移除后立即解除内容租约，显式flush/reload/下一次lookup清掉句柄的native共享引用，旧句柄返回CLOSED且地址不得复用给新host，直至View销毁。每View最多发行100000个host身份（包括失效墓碑），超限返回LIMIT；不把墓碑当作内容停车场。Rust引用仍不能越过View寿命。这个上限及无新增release-host导出的取舍列入S2冻结报告。

### 5.2 复用内容租约，不增host-move导出

现有 `oneui_view_host_attach(host,content,owner,retain_owner,release_owner)` 已提供两种所有权：native Widget共享引用与foreign callback owner引用。WP4保留签名，并把内部HostLease明确为**内容租约**：attach成功恰好retain一次，detach/真正销毁后release一次；先从树拆内容、断开观察，再release owner。失败不泄漏引用，回调不跨ABI unwind。

PaneGrid的扁平内容池使以下操作**无需detach/attach**：

| 操作 | Widget / owner / scope行为 |
|---|---|
| 分屏 | P1的Pane/NativeHost/TerminalView保持；只给新P2创建一次内容。几何变化不重启PTY。 |
| 交换、边缘移位、跨标签 | 只改变几何索引和可见group；原Pane/Host与内容租约不变，焦点按源key重建父焦点链；不销毁/重新注册输入回调。 |
| 最大化、还原、隐藏group | 相同对象切显示几何；非可见终端可继续接收输出，渲染调度按可见性暂停。 |
| 关闭一个pane | 按4.2先动画、后移除；内容协调器延后到flush完成才结束该session。其他pane地址、连接数、选区、scrollback保持。 |
| DockItem停靠↔float↔compact | 同window内由内部children事务移同一内容根；其下所有NativeHost租约不变。普通表单状态同样保留。 |
| View/window关闭 | 取消动画、清事件门、取消待投递的settled事件、拆树、释放内容租约，再释放VM；外部异步任务看到closed后丢弃结果。 |

未attach的新host由内容模块在 `instantiate/flush` 返回后的UI任务解析并attach，不能在VM getter/connect/invoke里attach。所有需要同时迁移的父关系先准备，旧树保持到commit，提交无分配；不采用“先detach所有，再逐个attach，失败丢半棵树”。跨window/跨View移动不在本次契约，返回BUSY或由应用另建显式迁移流程，不能偷用DockSession原生拖动。

真正需要临时离树的内容仍由其原NativeHost实例作为停车容器持有，不把owner单独释放后再retain。没有增设take/put lease/host_move C导出的必要。静态host的D4热重载BUSY规则保留；动态host可仅在**同稳定scope/name且模板所属位置未改变**时复用。修改模板使附着host改名/删除/迁移到另一模板位置仍BUSY并保留整视图；运行时Dock树更新属于组件数据变化，不是模板reload，不应触发静态host移动判定。

## 6. 容器查询与Yoga

### 6.1 OneUI支持的CSS子集（拟新增）

```css
.sftp-frame { container-name: sftp; container-type: size; }
@container sftp (width >= 860px) and (height > 0px) and (aspect-ratio >= 8/5) {
  .sftp-columns { flex-direction: row; }
  .sftp-tree { display: flex; width: 224px; flex-shrink: 0; }
  .sftp-browser { display: flex; flex-grow: 1; flex-basis: 0px; }
  .sftp-details { display: none; }
  .sftp-switch { display: none; }
}
@container sftp (width >= 1100px) and (height > 0px) and (aspect-ratio >= 8/5) {
  .sftp-details { display: flex; width: 292px; flex-shrink: 0; }
}
```

默认窄模式由三页容器的 `:selected`/保留式条件与VM selectedPane控制；宽模式只覆盖**布局可见性**。必须有一个专用 `ContainerPage :selected="selected"`（bool、default槽、无事件，schema仅r selected:bool）：它把selected产生的默认display作为低优先级样式，而不是通用 `visible=false` 的硬禁用，宽查询才能恢复全部页。另一可行写法是每页 `:container-selected`布局属性；推荐前者，避免全局改变visible的优先级。功能属性visible/disabled仍为硬约束，不被CSS强制开启。ContainerPage是容器查询的小型配套组件，纳入S5。

仅支持命名容器、size、width/height/aspect-ratio比较、px数值、有限正整数比值、and；不支持JS、任意calc、style query、or/not与浏览器全语法。严格诊断未知语法/属性，包含CSS源行列。width/height测量容器**内边框尺寸**（border-box减border，不减padding），单位逻辑px，选择器作用于最近同名祖先的后代；容器本身不能查询自身后改变自己的被测尺寸。这个明确的OneUI语义使SFTP的W=frame.width-2精确可表达；容器height须按参考传给size的相同frame口径，iShell模板用无垂直边框的测量壳与既有边框绘制层，避免误减2改变1.6边界。border/padding不应重复扣除。

`container-type:size`要求父布局能独立分配两轴（basis/grow/固定约束）。内容驱动高度的容器不能查询height或ratio，编译可判定时拒绝，运行时不满足独立尺寸条件则诊断并使用无条件样式。查询不发VM事件；媒体/monitor布局也使用同一机制，不把尺寸通知送回Rust计算分支。

支持的查询内布局输出：display:none/flex、flex-direction、flex-wrap、flex-basis/grow/shrink、gap、width/height/min-width/min-height、max-width/max-height（主轴上限，经 StackFlex::max 应用；跨轴给出 CONTAINER_CONSUMPTION 诊断），以及现有绘制/文字属性；同步实现严格消费检查与Yoga/style_adapter应用，不能只解析不应用。`display:none`保留Widget与VM，但不参与布局/命中/可访问树；移出正在编辑节点前按正常隐藏政策提交composition并迁焦点到切换控件。

### 6.2 布局后求值，最多一次重算

1. 新layout epoch先应用无条件样式，父布局/Yoga分配各query container的独立尺寸，得到稳定测量矩形。
2. 用该矩形快照对索引过的相关条件求值；aspect用 `W*5 >= H*8`，H=0必false，不做除零。不用整数取整，≥860、≥1100边界精确。
3. 条件向量改变才失效对应后代style与Yoga measure/arrange cache；批量应用后最多再layout一次。第二次不重新求条件，避免一帧死循环。首期禁止依赖式嵌套：若祖先query规则会改变后代query container的可用尺寸，则编译可判定时拒绝，动态布局时给出依赖诊断并保持上一有效匹配。可以嵌套固定独立尺寸的容器，也可有多个并列容器；不声称单次额外Yoga全树计算能解决任意级联查询。SFTP/监控示例用并列独立测量壳，不依赖此被排除能力。
4. `size`隔离使后代显示/换行无法改变祖先被查询的尺寸，因此**跨帧也不会A/B振荡**。仅有“一帧一次”不能证明跨帧收敛；不符合size隔离条件就拒绝，不能每帧偷偷翻转。
5. scrollbar在测量壳内预留稳定gutter，不把出现/消失的scrollbar反馈成query width。样式删除/热更新重建依赖索引，错误CSS保留上一有效表。

推荐不加默认滞回，保留iShell在860/1100/1.6的精确切换；未来可显式扩展容器hysteresis，但WP4不以此掩盖依赖环。Yoga仍负责PaneHeader、普通表单与监控wrap；PaneGrid自定义二叉几何作为Yoga的一个叶式布局容器，子Pane的内层Yoga收到最终rect，不能再让Yoga自动排列它的兄弟pane。

## 7. 动画、尺寸结算与性能

### 7.1 状态机与中断

`Idle → Preparing → WarmFrame → Running → FinalLayout → Settled → Idle`。

Preparing只由VM已接受的新状态触发，分配新节点/内容、验证引用、建立轨道。WarmFrame恰好一个预热帧，下一rAF t=0开始200ms时钟。Running对预分配轨道做float插值，最小尺寸只在显示层临时放开，VM保存ratio不被0/1覆盖。FinalLayout恢复minimum、内容extent和query最终布局，再等真实viewport报告；Settled按有效pane发送一次最新尺寸并清hold。减少动画/调度失败跳到FinalLayout；退出不投递任何业务settled事件。

关闭确认发生在动画前，关闭组件不能提前destroy会话。最大化轨道不改保存ratio。并发动画推荐单workspace事务epoch：新接受的布局从当前display ratio重定向，旧轨道取消但**不中途释放共享尺寸hold**；最终新epoch才结算。外部revision变化取消手势，动画按最新权威快照重定向。被外部删除的pane立即断事件且取消对应尺寸结算；禁止为了补旧动画完成回调恢复它。此中断策略是新增规范，见Q4。

### 7.2 `resizeSettled`必须接到真实TerminalViewport

`TerminalView::reportViewport`当前在paint中报告实际字体度量后的整格尺寸（`src/core/terminal_view.cpp:1387–1414`），不能在PaneGrid里用宽/默认字符宽重新估算；零/不足一个整格时沿用最后有效viewport，不能发1×1破坏终端状态。

在core增加轻量 `ContentViewportSource` 观察/hold接口；TerminalView适配真实viewport结果，views的NativeHost适配器连接它，PaneGrid只认识该接口，不认识PTY。`resize-policy="settled"`使新host采用此通道：paint持续更新最新候选，但运动/resize hold期间不向外发尺寸变更。非终端原生内容可仅使用像素 `@boundsSettled(pane:utf8,width:f64,height:f64,generation:i64)`；不能伪造cols/rows。该事件可在PaneGrid按需注册，绑定VM相同签名命令。

FinalLayout后必须等待**最终rect对应的viewport观测**（layout epoch标记）；按pane key+generation+cols/rows去重，投递到paint栈退出后的UI队列，经invoke发resizeSettled。VM调用一次session.resize；若尺寸未变则0次，已删除/隐藏/无有效格尺寸的pane不发。内容模块迁移时不再同时注册legacy viewport回调去resize，避免语义事件和旧回调双发。观察者列表与回调owner寿命独立管理；不要覆盖用户输入、paste、focus回调。

“一次”指每个发生实际尺寸变化的终端、每个最终settled epoch一次，不是整个workspace共一次。动画中终端数据帧可以持续到达并绘制旧grid；只冻结网格调整/PTY resize。divider按参考的120ms静止结算，静止后再拖动进入新epoch，详Q4；最终release和120ms timer同一generation竞争时只能一个结算。键盘连续微调也合并120ms。取消回到原尺寸，若上次尚未结算则不发重复resize。

### 7.3 为什么可达，以及如何证实

设计减少热帧工作：stable Pane/Host无重建；nodes的反向索引和minimum按树编辑时更新；每帧迭代一次O(N)分配几何，不递归重复求子树minimum；轨道/rect/命中缓冲复用；不每帧跨ABI读VM、不转换UTF-8、不重新enumerate字体、不同步解码图片、不做session.resize、不起线程；预览只一个非交互overlay。容器查询只重算受影响子树且至多一次附加layout。

这些是机制依据，**不是已经达到16ms的实测结论**。计划release负载：1→2首次分屏、2→4、4→8、8→16、16叶均分/最大化/还原/关闭；同时包含tab并入、拖动预览与monitor/SFTP容器切换。结构正确性另测深度1000和极端节点预算，不把1000个终端作为16ms承诺。若16叶或参考负载不达标，不降帧数/关功能掩盖，分段计时后修复。

测量要求：

- Windows x64、NVIDIA OpenGL与软件后端分开，固定字体/DPI/1600×1000及900以下窄窗口；使用优化release二进制（原生/O2 /DNDEBUG，Rust --release），记录exe/DLL/IR hash、GPU/驱动、显示刷新率和系统motion设置。
- DevTools `ONEUI_DEVTOOLS=1 ONEUI_DEVTOOLS_OVERLAY=0`，逐帧记录schedule、update/layout/query/paint/submit、帧间隔、motion epoch、phase、是否预热、resize计数。正式报告不把CPU paint当屏幕present延迟；VSync等待和实际工作耗时分别列。
- 门槛以后续iShell默认后端为准（本机NVIDIA OpenGL），软件后端仅参考。预算为每个非预热的运动帧work/submit完整耗时≤16.0ms，max和慢帧数是门槛，P50/P95/P99辅助；rAF间隔/丢呈现帧另列，60Hz的16.67ms不是工作耗时。每场景100次、5轮，暖身结束后100%保留原始有界记录，首次分屏另用20个全新进程，不能用已分屏进程代表首次。
- 启动标记记录驱动吸收checkpoint；首次操作在启动ready后立即执行。另测用户在第16次present之前请求分屏的竞态，若仍卡顿列失败，不把60–200ms任意标成“预热”排除。不新增glFinish到普通交互热路径。
- 源/目标TerminalView地址、owner retain/release、创建次数、VM acquire/连接数、session resize数作非计时诊断；动画结束一次、取消零次或最终变化一次，500次拆并闭循环计数归零。停止采样和光标后稳定500ms无新增动画帧。

## 8. ABI、IR和兼容策略

ABI44的OneUiValue/VTable/ListChange/字符串借用规则保持。按Q1裁决，唯一新增导出oneui_view_resolve_scope直接并入ABI 44，不升45；WP3/WP4一起合入main。合入时若main已占用44，由Claude统一取main+1。S2检查点冻结ABI。

除5.1的resolve_scope外不新增C导出。Stack insert/remove仅为引擎内部C++能力，供声明式运行时使用；不增加Rust layout/stack.rs增删API。ContentViewportSource为引擎内部接口，后续组件统一经IR接入。

IR推荐保持major1，新增**必需能力段** `WSC1`（目录kind=19、flags=1，固定记录16bytes：version:u32=1、feature_bits:u32、reserved0:u32=0、reserved1:u32=0）。bits分别0动态scope host、1透明Pane/DockItem集合、2容器查询；未知bit或未知version拒绝。使用相关功能才写该段，旧IR字节不变。旧reader对未知required kind拒绝（ir_reader.cpp:54–55），不能仅升被旧reader忽略的minor（:13）。HOST现有字段布局不改，flags增加bit1=scope-local；编译器/reader一致校验其只能出现在受支持的稳定key集合下，原static实例规则继续执行。COMP新增组件/属性/事件完整契约；所有语言重新生成与schema/COMP hash严格匹配。

为避免动态host泛化成普通UI逃生口，首期只允许PaneGrid的Pane或DockWorkspace的DockItem稳定集合下重复host；静态嵌套模板仍按现有规则。动态host改名、key复用换VM、模板调用重排与reload BUSY均有负例。新编译器产生含WSC1的内容，旧runtime必须明确VERSION错误，不是加载半张页面。

## 9. 验收示例、实施步骤与文件预算

### 9.1 试验台“终端工作区”

新增独立 `examples/workspace_interactions/` 目录和独立exe，完全不修改 `examples/performance_lab/`；页面 `.one` + `.oneschema`，Rust模拟VM通过WP3生成trait，C++实验台入口只负责挂载IR。不得新增应用普通UI桥接C++。终端内容模块创建TerminalView一次，模拟本地会话支持输入echo、连续计数输出、历史/实时切换、连接/断开、resize计数；不启动真实shell或SSH、不读写用户工作区配置。

同一页提供1/2/4/16窗格、分屏方向与目标选择、header所有操作、pane↔tab、最大化/还原/均分、方向focus/swap、广播源/接收/暂停、Dock四工具、float/8向resize/置顶/返回原位/收起/compact，以及容器SFTP和监控多列。设置抽屉包含FontPicker、MediaPicker/Preview、Slider、12项ThemeScope/Card，不为D8另造不接VM的静态截图。

自动化由两层组成：core纯几何/手势单元测试用确定性clock；集成测试给**原生根控件事件路由**注入Pointer/Key/Text/Composition事件，经capture和hit-test真实触发，再检查VM与绘制结果。窗口只承载渲染，不用SendInput、系统鼠标键盘、WM消息注入替代测试入口。不能只直接调用VM命令证明拖动命中正确。同步通过warm/running/settled与hostReady谓词，禁止固定“等待N帧”猜完成。

| 验收ID | 场景与断言 |
|---|---|
| A1 tree | 深度1/64/65/1000、比例边界、环/悬挂/重复key/NaN、最小尺寸与滚动；INSERT/UPDATE/MOVE/REMOVE trace无正常RESET，身份与connect/release符合预算。 |
| A2 divider | 5px gap/10px命中含边界；双击=.5、步长.025、Home/End/修饰键、终点鼠标位置、Esc恢复；capture loss按裁决。无body输入被divider偷走。 |
| A3 drag | .28/.72及四角tie、自己/空白、源关闭、revision变化、释放最后点；pane转tab、多叶tab拒绝拖出、单叶tab中央就近边合入；预览无字无多重overlay；16/8/50ms dt轨迹一致。 |
| A4 header/navigation | 319/320px显隐、单叶/广播/overview、hover/active/focus-visible；title/path长文与隐私；几何不规则树邻居选择、无邻居、交换源焦点、IME与history保留。 |
| A5 broadcast | Off建立组、源结束、最后接收者移除、断线/自行输入暂停、focus不改源、多行粘贴确认取消/确认；确认前无广播副作用。 |
| A6 dock | docked释放自身/外部不float、Alt末态、strict42px及四导引、float实时跟手、8向resize逆向、Esc、过期手势；compact保留float、置顶仅工作台内；保存/恢复锚点和无效snapshot。 |
| A7 host | 分屏/交换/跨tab/收起/最大化500循环，现存TerminalView/owner地址不变；两View同VM、两repeat同list、Unicode/slash key、scope移除失效、附件BUSY reload、attach失败回滚、窗口先关、晚到回调。 |
| A8 query/Stack | W859.99/860/860.01、1099.99/1100/1100.01，W=1.6H两侧、H0；selected details窄模式可见；尺寸独立性/滚动条/gutter、恶意振荡拒绝；Stack move/跨父失败/焦点IME、Yoga与Legacy几何一致。 |
| A9 D8 | 字体空/失败/晚到/被删选中/键盘IME；媒体成功/取消/失效路径/格式不支持/异步代际/clear不删文件；Slider四阶段/键盘/程序set无交互回调；12主题只改一项scope、全局颜色不变。 |
| A10 perf | 按7.3输出真实release帧数据、首分屏/resize/内存与创建计数；GPU/软件分开，不使用WP3的软件列表数据代替。 |

截图矩阵：浅/深×1600×1000/900以下，补充浮动、预览、SFTP三种形态与主题画廊，不超过20张/100MiB。WP4示例证明组件；WP6再跑 `terminal-*`、`terminal-panes-*`、`terminal-split-motion`、`terminal-workbench-*` 和WP0现场回归，验证iShell像素/输入安全与内存≤基线+5%。本稿不宣称执行过这些测试。

### 9.2 分八步，每步可独立验收

| 步骤 | 内容与预计文件（新文件除标明既有外） | 验证与产出 |
|---|---|---|
| S1 契约与扁平几何 | `include/oneui/layout/workspace_tree.h`、`src/core/workspace_tree_{validate,layout}.cpp`；从既有split_view/dock_geometry/dock_hit_test抽共享纯规则至`split_geometry.*`；`tests/workspace/tree_tests.cpp` | 冻结nodes/groups、事件枚举、深度/ratio规则；A1与旧Dock/SplitView特征对比。产出schema/契约测试和S1报告，不修改iShell。 |
| S2 动态scope与内容租约 | `src/views/workspace_{scope,children}.cpp`、`src/capi/oneui_c_api_view_scope.cpp`；修改tree/lists/reload与HOST reader/lower；Rust `view/scope.rs`、host.rs；WSC1和sys符号表 | A7、旧WP3生命周期/reload/黄金IR回归、C11/ABI/Rust签名同步；**Claude检查点：ABI冻结**。 |
| S3 PaneGrid与尺寸运动 | `include/oneui/layout/pane_grid.h`、`src/core/pane_grid_{layout,motion,resize}.cpp`、`split_divider_interaction.*`；`src/views/workspace_properties.cpp`；TerminalView viewport观察逻辑抽到`terminal_viewport.cpp` | A1/A2、7.1确定性clock、真实viewport结算；hold一进一出与新pane初始化失败。先完成性能冒烟及明确数据，未达标不得进下步掩盖。 |
| S4 PaneHeader/drag/navigation | `include/oneui/controls/pane_header.h`、`src/core/pane_{header,drag,navigation}.cpp`、`workspace_drag_domain.*`；从既有Tabs提取拖动领域实现；`src/views/workspace_events.cpp` | A3/A4/A5；声明式端到端和语义trace，tab/pane对象身份；参考轨迹/文案/阈值矩阵。 |
| S5 Stack与容器查询 | `src/core/stack_children.cpp`、`src/core/container_query_{parse,evaluate}.cpp`、`container_page.cpp`；style_sheet/style_adapter/stack_yoga接入 | A8；先拆出既有style_sheet选择器解析/规则块扫描至独立领域文件，再新增query；编译诊断、布局pass计数、循环拒绝和CSS热重载。 |
| S6 DockWorkspace | `include/oneui/layout/dock_workspace_view.h`、`src/core/dock_workspace_{view,gesture,placement}.cpp`；`src/views/dock_properties.cpp`、注册表/工厂接入 | A6、锚点roundtrip、事件拒绝/过期、compact/8向resize；和既有DockSession/DockView无行为干扰。 |
| S7 D8补齐 | `font_picker.*`、`media_picker.*`、`media_preview.*`、`theme_scope.*`按core/views职责目录；`src/views/control_picker_events.cpp`；既有slider/card注册 | A9、各组件独立.one/schema例子、C++/Rust共IR，空/错/取消/异步/无障碍；无业务文件管理代码。 |
| S8 实验台与全套验收 | `examples/workspace_interactions/`内拆模板/schema、Rust vm/content/fixtures；`tests/workspace/`按A组拆；`docs/benchmarks/workspace-interactions/`固定报告 | A1–A10、release原始数据/截图/计数、CPP core独立链接、WP3回归；最终报告review后停止，WP6迁移另授权。 |

注册接入共同涉及 `include/oneui/ui_component_schema.h`、`src/views/format/component_registry.inc`、`src/views/component_factory.cpp`、`component_properties.cpp`、`events.cpp`、viewc validator/lower、`cmake/OneUISources.cmake`/`OneUIViewTests.cmake`及Rust sys/supported-symbols。数据表可以分领域include，不能对不同入口维护两份组件真相。新文件按职责每个≤500物理行（含注释空行）；不是拆成part1/part2。超过500重新划边界，超过1000必须拆；`.one`≤400行。

已知需治理的接入点：`src/core/style_sheet.cpp`当前1246行（CSS解析/匹配/值处理），S5实际迁出规则解析职责；`src/core/tabs.cpp`超过1000行，S4迁出拖动而非继续附加控制器；TerminalView现有大文件S3迁出viewport报告/观察领域。公共大Rust lib.rs仅模块导出/ABI常量接入，新safe API在view/layout子模块；本阶段只是计划，不能声称已拆分。S1先记录实际实施前行数，验收附迁出职责/前后行数和回归结果。

构建规则：不在沙箱重配 `build/capture`，需新配置仅 `build/codex-workspace` 一个固定用途目录且先检查预算。公共include成员/布局变化后capture和release都clean后重编，正常终端由Claude复核；直接MSVC仅重放真实配置，不伪称Ninja通过。项目跨盘30GiB，相关旧产物的核查、占用确认、清理按build-artifact-hygiene；无法删除的按绝对路径/大小/原因列报告，不停工。截图≤20/100MiB，日志≤50MiB，固定基准目录覆盖更新，fixture在finally回收。

## 10. 已确认裁决（Claude，WP-4-design-review）

| ID | 裁决与执行约束 |
|---|---|
| Q1 | 仅新增resolve_scope，并入ABI44；不改VTable、不增host-move和Stack C导出/Rust增删API；S2冻结。 |
| Q2 | 扁平schema、迭代DockTreeIndex、平铺Pane；只限总量，不限几何深度；旧DockNode ABI不变，共用纯规则。 |
| Q3 | VM确认→closing→折叠settled→VM delta删除→内容卸载后释放session；外部断开独立紧急路径。 |
| Q4 | **行为规范补充**：divider保留120ms静止结算，每次静止独立epoch；PaneGrid capture loss按cancel，旧SplitView不变；并发动画重定向并共享hold，最终epoch一次结算。 |
| Q5 | 16个可见pane，release每个非预热运动帧work+submit≤16.0ms，仅排除一个WarmFrame；以iShell默认后端（本机NVIDIA OpenGL）为门槛，软件仅参考；另列rAF/present间隔。 |
| Q6 | SDK示例≥1100两列、≥1600三列、basis约360、gap12；监控多列不属于既有行为一致性要求。WP5由Claude决定iShell是否启用，默认保持单列。 |
| Q7 | 仅命名size隔离，布局后最多重算一次，精确阈值无默认滞回，拒绝依赖式嵌套；ContainerPage纳入S5。 |
| Q8 | 本地静态图片，VM选择/导入、异步预览；ThemeScope仅白名单颜色token；无视频/远程媒体。 |
| Q9 | 强事务只保证workspace组件一级，用故障注入验证，不泛称整个引擎具有强保证。 |
| Q10 | 保留驱动吸收，不延迟输入；覆盖预热完成前分屏，若卡顿提交trace，由Claude裁决startup-ready门控。 |

其他裁决：navigation同帧逐次执行最多8次并诊断溢出（发布边界见3.2）；透明集合由children-kind决定，S1加负例；动态host仅PaneGrid/Pane、DockWorkspace/DockItem；大文件分别在S3/S4/S5按职责拆分并记录前后行数；新示例独立exe，完全不碰performance_lab。

## 11. design-1阶段核验（历史记录）

### WP5 设置页接入补充

FontPicker 的 `trigger-label` 可本地化按钮文案，已选字体仍通过 accessibility value 暴露。
MediaPicker 支持 `choose-label`、`clear-label`、`cancel-label`，32px 紧凑布局可用于表单行。
其内部按钮分别使用 `media-picker-choose`、`media-picker-clear`、`media-picker-cancel` 样式类，
字体按钮使用 `font-picker-trigger`；主题切换重新解析内部按钮样式。
MediaPreview 的既有本地静态图片契约不变，视频导入和全局视频播放仍由应用服务承担。

以下为早期设计记录，不能替代上述能力的实现验证；WP5 测试结果在 iShell 交接报告中列明。

已完成只读现状调查与可实施契约；没有运行编译、运行时交互、性能或截图测试，因此本稿的性能与验收均为待执行计划。图MCP索引查询和MyStudio ensure_project被禁止审批策略拒绝，未继续调用受限工具，未换HTTP绕过；workspace_context已读，agent_kit.version=5与本地相同。MyStudio **未同步**（无task/execution/handoff写入）。版本状态：files_updated=false，http_verified=未单独执行，client_loaded=本会话已读取v5且workspace_context可用；未声称已上报回执。

本任务不产生构建/实验/截图临时文件，仅保留正式设计稿及 `WP-4-design-commit-msg.txt`。不触碰既有capture配置和原工作区产物；WP3历史受阻项见 `docs/benchmarks/language-neutral-views/artifacts.md`，本次未把历史数字冒充新审计或宣称清理。进入实现前按9.2执行全项目产物审计。UI技能读取到 `.impeccable/design.json` 比 DESIGN.md 旧；它不是本任务视觉权威，未顺带重建，后续可单独document刷新。

交付文件已检查UTF-8无BOM、无连续四问号乱码、无替换字符/U+200B、代码围栏配对；I/O前缀的具体引用路径存在。Git分支仍为codex/workspace-components，状态仅上述两个新增文件，源码无diff；未写Git索引或提交。此为文档静态核验，不替代实现后的编译与行为验收。

提交由Claude代为执行，提交信息附文件清单和Co-Authored-By。此记录仅对应design-1；design-2已获准S1→S2，实施证据另见S1/S2报告。每步提交信息用WP-4-S<N>-commit-msg.txt，交叉文件则合并并说明原因。

WP5 settings FontPicker extension: `confirm-selection` keeps a candidate as a
local preview until Apply; Cancel/Escape leave the VM value unchanged. Optional
`monospace-items` is a `oneui.v1.StringItem` list of OS-reported fixed-pitch keys;
`monospace-only` sets the initial filter and the user may disable it. The
`apply-label`, `cancel-label`, `filter-label`, `search-label`, and `retry-label`
properties localize the popup. `retryRequested` remains available after success
so newly installed fonts can be enumerated again. The component never discovers
fonts or persists preferences itself. Existing callers keep immediate selection
unless they opt into confirmation.

The popup paints an opaque surface inside its enclosing scroll viewport. Search,
selection and secondary buttons inherit the active semantic palette when their
optional component tokens are absent; this includes partial dark palettes.

### PaneHeader action captions

`split-label`, `maximize-label`, `close-label`, `live-label`, and `menu-label` accept UTF-8 literal or bound text. They update both the native button's accessible name and tooltip without replacing the button or changing drag routing. Broadcast feedback continues to use `broadcast-label` and `broadcast-hint`. Caption changes preserve content leases and native control identity.

### WP6 header fidelity follow-up

PaneHeader retains the separate left drag handle and right menu action. The right actions follow broadcast / split / maximize / menu / close / live order; the 29px header is followed by a 1px active rule. Header parts consume the inherited style scope through `pane-header-part` selectors: `pane-header-button`, `pane-header-broadcast-on`, `pane-header-grip`, `pane-header-number`, `pane-header-title`, and `pane-header-path`. The number/title/path nodes also expose `pane-header-active` or `pane-header-inactive`. Part colors, font size/weight/family and number radius are authored by the application and refresh on theme replacement. `menu-label` applies to both menu entry points. No C ABI or wire-format change.

PaneGrid divider paint can be themed with `pane-grid-part.pane-grid-divider` background and `:hover`, `:pressed`, `:focus` states. PaneHeader exposes `pane-header-part.pane-header-rule` for the full-width bottom rule; its grip remains visible on inactive panes. Neither styling changes geometry or content ownership.

### PaneHeader session tooltip

`tooltip` accepts literal or bound UTF-8 text independently of the visible title,
path and broadcast hint. The declaration runtime preserves an explicitly authored
tooltip (including an empty one) when active, title, path or other header settings
change. Omitted tooltips retain the component's title/path default. Session privacy
and connection-state wording remain application model/VM policy. This adds no
public class layout or C ABI change.

### Native tab strips in a declarative workspace

`ViewMount::register_workspace_tabs(domain, id, tabs)` registers an existing
native Tabs in the mounted view's WP4 domain. The Tabs must belong to the same
window; registration holds a weak reference. Forward its native drag-out events
with `View::workspace_tab_drag`, supplying the stable single-pane source on
Started and window coordinates on every event. Flush pending workspace state
before Started. WP4 owns preview, release hit testing, revision rejection and
cancellation for both directions; the application only applies accepted intents.
Maximized panes and multi-pane tab sources cannot begin a drag.

`pane-grid-part.pane-grid-preview { background: var(--accent); }` supplies the
preview accent, refreshed with the style scope. The component applies its own
translucency (41/255 for a valid destination), outline and 45 ms exponential
tracking. The release point is evaluated directly, independent of the lagging
preview. These C API additions do not change public class layout.

Named element metrics and mount focus lookup include content nested inside
Popup. Native View focus requests route through Popup to its content, allowing
an anchored DataMenu to receive keyboard input after it opens.

### Declarative session Tabs (T11/T12)

Tabs v3 appends `decorations` (keyed rows: `id`, `icon`, packed RGBA `rgba`),
`item-min`, `item-max` and `edit-key`. It appends keyed `contextRequested`,
`overflowChanged` and `editFinished` events without changing earlier IDs or
`oneui.v1.TabItem`. Decoration rows follow item order. Compact document tabs
keep logical order, allow wheel scrolling to every item and bring a newly
activated or edited item into view; repaint preserves the user's scroll offset.

`ViewMount::register_workspace_tab_surface` also accepts the declarative host
Panel, retaining the existing same-window and drag-domain checks. Forward drag
events outside VM callbacks, preserving phase order and release coordinates.
Reject a changed model revision while deferring Started; WP4 rejects revisions
changed during the gesture. Pure model merging inserts an external leaf in a
half split at the nearest edge; it does not replace either session.

The public Tabs layout replaces obsolete compact packing storage with viewport
width state. Consumers must clean and rebuild their existing SDK configuration.

`DragHandle` also emits `activated(source,x,y,count,button)` for a click below
the drag threshold, right click, or Enter/Space. A child control consumes its
own input first; a completed drag does not activate. This appended event leaves
the existing drag event and phase values unchanged.

Dock divider paint can be styled with
`dock-workspace-part.dock-workspace-divider` and its `:pressed` / `:focus`
states. Transparent normal background keeps the authored gap visible; styling
does not alter divider geometry or its hit target. `interactionActive()` and
`hasDockTarget()` expose read-only interaction state for native input contracts.
### Monitor connection composition contracts

`Scroll.scrollbar-inset` exposes the native inset (default 5); zero reserves only the authored scrollbar width. `VirtualList.trailing-width` and `trailing-gap` expose native rich-row metrics (defaults 58 and 8). `VirtualList.activate-on-single-click` (default false) lets a single click fire `@activate`, matching completion popups that accept on click; it restores the default when removed during reload. Structured rows may supply `trailing_rgba: i64` in unsigned 32-bit RGBA range; omitted values retain the native default. These properties support bindings and restore defaults when removed during reload. Monitor templates use compact metrics without a native content host.

VirtualList also clamps an obsolete scroll offset when an enlarged viewport is painted or clicked, so filtering followed by expansion cannot leave hit testing at an old row. Runtime fixture covers both paths.

`oneui_view_element_metrics` reports VirtualList scroll position/extent. SegmentedControl validates the existing Tabs `text-inset` style adapter, allowing narrow labels to keep readable space.
