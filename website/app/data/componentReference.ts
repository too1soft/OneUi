export const navItems = [
  { label: '总览', to: '#overview' },
  { label: '快速开始', to: '#quick-start' },
  { label: '代码审查', to: '#review' },
  { label: '架构模型', to: '#architecture' },
  { label: '完整组件 API', to: '#component-api' },
  { label: '布局系统', to: '#layout-api' },
  { label: '样式系统', to: '#style-api' },
  { label: 'C++ API', to: '#cpp-api' },
  { label: 'C ABI', to: '#c-api' },
  { label: '跨语言调用', to: '#bindings' },
  { label: '远程会话', to: '#remote' },
  { label: '构建与测试', to: '#build-test' },
  { label: 'FAQ', to: '#faq' }
]

export const stats = [
  { label: '定位', value: 'C++17 自绘桌面 UI 框架' },
  { label: '当前可运行后端', value: 'Windows / Win32 / Skia' },
  { label: '公开入口', value: '<oneui/oneui.h>、单组件头文件、oneui_c_api.h' },
  { label: '文档站', value: 'Nuxt 4 + Nuxt UI，独立位于 website/' }
]

export const reviewFindings = [
  {
    level: 'P1',
    color: 'error',
    title: '测试运行时依赖还没有完全收口',
    body: '本地旧构建中的 oneui_monitor_behavior_tests 以 0xc0000135 退出，这在 Windows 上通常表示测试进程找不到某个 DLL。建议在 CTest preset 或测试启动脚本中统一设置 PATH，或者把测试所需 DLL 复制到测试可执行文件旁边。'
  },
  {
    level: 'P1',
    color: 'error',
    title: '平台支持需要明确标注为 Windows 优先',
    body: '仓库里有 Linux/macOS skeleton 文件，但真正可运行、可审查的窗口后端是 Win32。官网、README 和 SDK 包说明都应该明确当前阶段是 Windows 可用，跨平台是后续目标。'
  },
  {
    level: 'P2',
    color: 'warning',
    title: '聚合头 oneui.h 没有包含所有已公开组件',
    body: 'IconButton、Tile、Toast、StatusStrip、TopBar、animation.h、style_transition.h 等头文件存在，CMake 也编译了实现，C ABI 也暴露了部分能力，但 <oneui/oneui.h> 还没有聚合这些头。使用这些新组件时需要直接 include 对应头文件，这会影响新手发现 API。'
  },
  {
    level: 'P2',
    color: 'warning',
    title: 'C++ API 与 C ABI 覆盖范围不完全一致',
    body: 'C ABI 已覆盖窗口、样式、Stack、TopBar、AppShell、ProductShell、OverlayHost、Panel、Label、Icon、IconButton、Switch、TitleBar、NavItem、Badge、Card、Tile、StatusStrip、Toast、RadioGroup、TextField、Button 等，但 Select、Tabs、List、Table、Slider、Popup、远程输入等仍主要通过 C++ API 使用。'
  },
  {
    level: 'P2',
    color: 'warning',
    title: '既有中文文档和示例存在编码风险',
    body: '当前终端读取部分中文文件时出现明显乱码。建议统一源码和文档为 UTF-8，补充 editorconfig、CI 编码检查，并避免不同 shell 默认编码导致文档站或 Gallery 文案损坏。'
  },
  {
    level: 'P3',
    color: 'info',
    title: '远程视频控件仍是基础能力，不是最终高性能视频面',
    body: 'RealtimeFrameView 已有帧提交、缩放和快照能力，但文档应提醒使用方不要把它误认为最终视频热路径。远程桌面场景后续仍需要更明确的帧队列、丢帧策略、GPU 上传和性能指标。'
  }
]

export const architectureRows = [
  ['Widget', '所有控件的基础类。负责 frame、preferredSize、visible、disabled、focusable、mouse/key/text input、accessibility 和 paint 入口。'],
  ['View', '可容纳子控件的 Widget。负责 add/remove child、子控件布局、绘制顺序、命中测试、焦点转发和 Tab 导航。'],
  ['Layout', 'Stack、Grid、Wrap、DockView、SplitView、ScrollView、AppShell 等容器根据 preferredSize 与自身规则给子控件设置 frame。'],
  ['StyleSheet', 'CSS-like 小型样式系统。支持 tag、class、伪状态和有限属性，最终转换为 typed style override 或 StyleBox。'],
  ['Platform', 'Window、Clipboard、Monitor 等平台能力。当前真正落地的是 Win32，Linux/macOS 文件还属于规划骨架。'],
  ['C ABI', '面向 C、Rust、Go、C#、Python、Node、Java 等语言的稳定边界。它不是完整 C++ API 的逐项镜像。']
]

export const quickStartSteps = [
  ['1. 引入头文件', '常规 C++ 应用先 include <oneui/oneui.h>。如果使用新组件，比如 Toast、Tile、TopBar、IconButton，当前还需要直接 include 对应头文件。'],
  ['2. 创建窗口', '准备 WindowOptions，设置 title、width、height、visible、resizable 等字段，然后调用 Window::create。'],
  ['3. 组织控件树', '用 Stack、AppShell、Panel、FormField 等容器组合 Button、TextField、List 等控件。避免一开始就写绝对坐标。'],
  ['4. 绑定状态和事件', '用 State<T> 做简单响应式绑定，用 setOnClick、setOnChanged 等回调处理用户操作。'],
  ['5. 运行消息循环', '调用 window->run() 进入平台消息循环。跨线程更新 UI 时使用 window->post(...) 回到 UI 线程。']
]

export const componentGroups = [
  {
    title: '基础显示与容器',
    description: '这些组件负责文本、图标、卡片、面板、分隔线和状态展示，是所有页面都会用到的基础积木。',
    items: [
      {
        name: 'Label',
        include: '#include <oneui/controls/label.h>',
        purpose: '显示一段只读文本，适合标题、说明、字段名和状态文案。',
        constructor: 'Label(std::wstring text = L"")',
        api: [
          ['setText(std::wstring) / text()', '设置或读取显示文本。'],
          ['bindText(State<std::wstring>&)', '把文本绑定到响应式状态。'],
          ['setColor(Color)', '设置文字颜色。'],
          ['setFontSize(float)', '设置字号。'],
          ['setFontWeight(int)', '设置字体粗细。'],
          ['setAlign(TextAlign)', '设置文本对齐方式。']
        ],
        usage: 'auto title = std::make_shared<oneui::Label>(L"设备列表");\ntitle->setFontSize(18.0f);\ntitle->setFontWeight(600);',
        notes: ['Label 不处理输入事件。', '需要可点击文本时应组合 Button 或自定义控件。']
      },
      {
        name: 'IconView',
        include: '#include <oneui/controls/icon_view.h>',
        purpose: '绘制 OneUI 内置 IconSymbol，适合导航、状态、按钮前缀图标。',
        constructor: 'IconView(IconSymbol symbol = IconSymbol::RemoteAssist)',
        api: [
          ['setSymbol(IconSymbol)', '切换图标符号。'],
          ['setColor(Color)', '设置主线条颜色。'],
          ['setAccent(Color)', '设置强调色。'],
          ['setStrokeWidth(float)', '设置线条宽度。']
        ],
        usage: 'auto icon = std::make_shared<oneui::IconView>(oneui::IconSymbol::Monitor);\nicon->setColor(oneui::Color{37, 99, 235, 255});',
        notes: ['IconSymbol 枚举定义在 icon.h。', '图标是代码绘制，不依赖外部图片文件。']
      },
      {
        name: 'Badge',
        include: '#include <oneui/controls/badge.h>',
        purpose: '显示短状态标签，例如 Online、Error、Beta、同步中。',
        constructor: 'Badge(std::wstring text = L"", BadgeVariant variant = BadgeVariant::Neutral)',
        api: [
          ['setText(std::wstring) / text()', '设置或读取标签文本。'],
          ['bindText(State<std::wstring>&)', '绑定文本状态。'],
          ['setVariant(BadgeVariant)', '设置 Neutral、Success、Warning、Danger、Accent 等语义色。'],
          ['setStyleOverride(BadgeStyleOverride)', '覆盖背景、文字、边框、圆角、内边距等样式。'],
          ['clearStyleOverride()', '恢复默认样式。']
        ],
        usage: 'auto badge = std::make_shared<oneui::Badge>(L"在线", oneui::BadgeVariant::Success);',
        notes: ['Badge 适合短文本，不适合作为按钮。', '变体表达语义，具体颜色可由样式覆盖。']
      },
      {
        name: 'Separator',
        include: '#include <oneui/controls/separator.h>',
        purpose: '绘制水平或垂直分割线，用于分隔工具栏、表单区块和列表组。',
        constructor: 'Separator(SeparatorOrientation orientation = SeparatorOrientation::Horizontal)',
        api: [
          ['setOrientation(SeparatorOrientation) / orientation()', '设置水平或垂直方向。'],
          ['setStyleOverride(SeparatorStyleOverride)', '覆盖颜色、厚度、边距等。'],
          ['clearStyleOverride()', '恢复默认样式。']
        ],
        usage: 'auto line = std::make_shared<oneui::Separator>(oneui::SeparatorOrientation::Horizontal);',
        notes: ['Separator 只负责视觉分隔，不参与业务状态。']
      },
      {
        name: 'ProgressBar',
        include: '#include <oneui/controls/progress_bar.h>',
        purpose: '显示 0 到 1 范围内的进度，适合下载、连接、扫描、安装等过程。',
        constructor: 'ProgressBar()',
        api: [
          ['setValue(double) / value()', '设置或读取进度值，通常会 clamp 到有效范围。'],
          ['bindValue(State<double>&)', '绑定进度状态。'],
          ['setStyleOverride(ProgressBarStyleOverride)', '覆盖轨道、填充、圆角等样式。'],
          ['clearStyleOverride()', '恢复默认样式。']
        ],
        usage: 'oneui::State<double> progress{0.35};\nauto bar = std::make_shared<oneui::ProgressBar>();\nbar->bindValue(progress);',
        notes: ['当前是确定进度条，不是无限 loading 指示器。']
      },
      {
        name: 'Card',
        include: '#include <oneui/controls/card.h>',
        purpose: '带背景、边框、圆角、内边距和阴影的单内容容器。',
        constructor: 'Card()',
        api: [
          ['setContent(std::shared_ptr<Widget>)', '设置卡片内部唯一内容。'],
          ['setBackground(Color)', '设置背景色。'],
          ['setBorder(Color)', '设置边框颜色。'],
          ['setRadius(float)', '设置圆角。'],
          ['setPadding(Insets)', '设置内容内边距。'],
          ['setShadow(ControlShadowStyle)', '设置阴影。'],
          ['setStyleBox(StyleBox) / clearStyleBox()', '通过样式盒统一覆盖视觉属性。']
        ],
        usage: 'auto card = std::make_shared<oneui::Card>();\ncard->setPadding(oneui::Insets{16});\ncard->setContent(content);',
        notes: ['Card 是单子控件容器。多个内容应先放进 Stack，再把 Stack 作为 content。']
      },
      {
        name: 'Panel',
        include: '#include <oneui/layout/panel.h>',
        purpose: '通用面板容器，能力接近 Card，并额外支持边框宽度等布局场景。',
        constructor: 'Panel()',
        api: [
          ['setContent(std::shared_ptr<Widget>)', '设置面板内容。'],
          ['setBackground(Color)', '设置背景。'],
          ['setBorder(Color, float width)', '设置边框颜色和宽度。'],
          ['setRadius(float)', '设置圆角。'],
          ['setPadding(Insets)', '设置内边距。'],
          ['setShadow(ControlShadowStyle)', '设置阴影。']
        ],
        usage: 'auto panel = std::make_shared<oneui::Panel>();\npanel->setPadding(oneui::Insets{20});\npanel->setContent(form);',
        notes: ['Panel 适合页面区域，Card 更适合重复项目或独立信息块。']
      }
    ]
  },
  {
    title: '输入与选择',
    description: '这些组件接收用户输入或修改状态。它们通常有 value、checked、selectedIndex、onChanged 或 onClick。',
    items: [
      {
        name: 'Button',
        include: '#include <oneui/controls/button.h>',
        purpose: '执行命令的标准按钮，支持主按钮、次按钮、禁用、键盘激活、点击回调和动画。',
        constructor: 'Button(std::wstring text)',
        api: [
          ['setText(std::wstring) / text()', '设置或读取按钮文字。'],
          ['bindText(State<std::wstring>&)', '绑定按钮文字。'],
          ['setVariant(ButtonVariant)', '设置 Primary 或 Secondary。'],
          ['setDisabled(bool)', '设置禁用状态。'],
          ['setOnClick(std::function<void()>)', '设置点击回调。'],
          ['setStyleOverride(ButtonStyleOverride)', '覆盖 normal、hovered、pressed、disabled、selected、focusVisible 状态样式。'],
          ['clearStyleOverride()', '清除样式覆盖。']
        ],
        usage: 'auto save = std::make_shared<oneui::Button>(L"保存");\nsave->setVariant(oneui::ButtonVariant::Primary);\nsave->setOnClick([] { /* save */ });',
        notes: ['Button 是可聚焦控件。', '空格和回车可触发按钮行为。']
      },
      {
        name: 'IconButton',
        include: '#include <oneui/controls/icon_button.h>',
        purpose: '只有图标的命令按钮，适合工具栏、标题栏、列表行操作。',
        constructor: 'IconButton(IconSymbol symbol = IconSymbol::Monitor)',
        api: [
          ['setSymbol(IconSymbol) / symbol()', '设置或读取图标。'],
          ['setOnClick(std::function<void()>)', '设置点击回调。'],
          ['setStyleSheet(std::shared_ptr<StyleSheet>, StyleNode)', '应用 CSS-like 样式，默认节点类似 button.icon-button。'],
          ['setDisabled(bool)', '继承 Widget 禁用能力。']
        ],
        usage: 'auto refresh = std::make_shared<oneui::IconButton>(oneui::IconSymbol::Refresh);\nrefresh->setOnClick(reloadDevices);',
        notes: ['当前未被 <oneui/oneui.h> 聚合，需要直接 include。', '只显示图标时，业务层应同时考虑可访问名称。']
      },
      {
        name: 'Checkbox',
        include: '#include <oneui/controls/checkbox.h>',
        purpose: '布尔选择控件，适合“启用某项设置”或多选项。',
        constructor: 'Checkbox(std::wstring text = L"")',
        api: [
          ['setText(std::wstring)', '设置旁边的标签文本。'],
          ['setChecked(bool) / checked()', '设置或读取选中状态。'],
          ['bindChecked(State<bool>&)', '绑定布尔状态。'],
          ['setOnChanged(std::function<void(bool)>)', '状态变化时回调。'],
          ['setStyleOverride(CheckboxStyleOverride)', '覆盖多状态样式。'],
          ['clearStyleOverride()', '恢复默认样式。']
        ],
        usage: 'oneui::State<bool> remember{true};\nauto checkbox = std::make_shared<oneui::Checkbox>(L"记住我");\ncheckbox->bindChecked(remember);',
        notes: ['可用鼠标点击，也支持键盘切换。']
      },
      {
        name: 'Switch',
        include: '#include <oneui/controls/switch.h>',
        purpose: '开关控件，表达立即生效的开/关设置。',
        constructor: 'Switch(std::wstring text = L"")',
        api: [
          ['setText(std::wstring)', '设置说明文字。'],
          ['setChecked(bool) / checked()', '设置或读取开关状态。'],
          ['bindChecked(State<bool>&)', '绑定布尔状态。'],
          ['setOnChanged(std::function<void(bool)>)', '变化回调。'],
          ['setStyleOverride(SwitchStyleOverride)', '覆盖轨道、滑块、焦点等样式。'],
          ['clearStyleOverride()', '恢复默认样式。']
        ],
        usage: 'auto dark = std::make_shared<oneui::Switch>(L"深色模式");\ndark->setOnChanged([](bool enabled) { applyTheme(enabled); });',
        notes: ['Switch 更适合即时开关；需要提交表单时 Checkbox 更直观。']
      },
      {
        name: 'Slider',
        include: '#include <oneui/controls/slider.h>',
        purpose: '在连续或离散范围内选择数字值。',
        constructor: 'Slider()',
        api: [
          ['setRange(double min, double max)', '设置最小值和最大值。'],
          ['setStep(double)', '设置步长，0 或较小值可作为近似连续值。'],
          ['setValue(double) / value()', '设置或读取当前值。'],
          ['bindValue(State<double>&)', '绑定数值状态。'],
          ['setOnChanged(std::function<void(double)>)', '拖动或键盘调整时回调。'],
          ['setStyleOverride(SliderStyleOverride)', '覆盖轨道、填充、thumb 和焦点样式。']
        ],
        usage: 'auto volume = std::make_shared<oneui::Slider>();\nvolume->setRange(0.0, 100.0);\nvolume->setStep(5.0);\nvolume->setValue(60.0);',
        notes: ['当前 C ABI 尚未暴露 Slider。']
      },
      {
        name: 'TextField',
        include: '#include <oneui/controls/text_field.h>',
        purpose: '单行文本输入控件，支持 placeholder、选择区、剪贴板、撤销重做、只读、密码模式和前后缀图标。',
        constructor: 'TextField(std::wstring placeholder = L"")',
        api: [
          ['setPlaceholder(std::wstring)', '设置占位提示。'],
          ['setText(std::wstring) / text()', '设置或读取文本。'],
          ['bindText(State<std::wstring>&)', '绑定文本状态。'],
          ['setCaretIndex(size_t) / caretIndex()', '设置或读取光标位置。'],
          ['setSelectionRange(size_t start, size_t end)', '设置选择范围。'],
          ['selectionStart() / selectionEnd() / hasSelection()', '读取选择区状态。'],
          ['selectedText() / selectAll() / clearSelection()', '读取、全选或清空选择区。'],
          ['copySelectionToClipboard() / cutSelectionToClipboard() / pasteFromClipboard()', '剪贴板编辑。'],
          ['undo() / redo()', '撤销和重做。'],
          ['setReadOnly(bool) / readOnly()', '设置或读取只读状态。'],
          ['setPasswordMode(bool) / passwordMode()', '启用密码显示。'],
          ['setPasswordMask(wchar_t) / passwordMask()', '设置密码掩码字符。'],
          ['setPrefixIcon(std::optional<IconSymbol>) / setSuffixIcon(...)', '设置前缀或后缀图标。'],
          ['setOnChanged(std::function<void(const std::wstring&)>)', '文本变化回调。'],
          ['setStyleOverride(TextFieldStyleOverride)', '覆盖 normal、hovered、disabled、readOnly、focusVisible 样式。']
        ],
        usage: 'auto name = std::make_shared<oneui::TextField>(L"请输入项目名称");\nname->setPrefixIcon(oneui::IconSymbol::Search);\nname->setOnChanged([](const std::wstring& value) { validate(value); });',
        notes: ['TextField 是单行输入。', '跨语言调用时文本参数使用 wchar_t*。']
      },
      {
        name: 'Select',
        include: '#include <oneui/controls/select.h>',
        purpose: '从少量选项中选择一项，类似下拉选择框。',
        constructor: 'Select()',
        api: [
          ['setItems(std::vector<std::wstring>)', '设置选项列表。'],
          ['setSelectedIndex(int) / selectedIndex()', '设置或读取当前索引。'],
          ['bindSelectedIndex(State<int>&)', '绑定选中索引。'],
          ['setOnChanged(std::function<void(int)>)', '选中项变化回调。'],
          ['setStyleOverride(SelectStyleOverride)', '覆盖 normal、hovered、pressed、disabled、selected、focusVisible 样式。']
        ],
        usage: 'auto mode = std::make_shared<oneui::Select>();\nmode->setItems({L"自动", L"手动", L"只读"});\nmode->setSelectedIndex(0);',
        notes: ['当前 popup 行为仍偏轻量，完整 Overlay 迁移需要继续演进。', 'C ABI 暂未暴露 Select。']
      },
      {
        name: 'RadioGroup',
        include: '#include <oneui/controls/radio_group.h>',
        purpose: '从互斥选项中选择一项，适合模式、范围、单选配置。',
        constructor: 'RadioGroup()',
        api: [
          ['setItems(std::vector<std::wstring>)', '设置选项文本。'],
          ['setSelectedIndex(int) / selectedIndex()', '设置或读取选中项。'],
          ['setOrientation(Orientation) / orientation()', '设置 Vertical 或 Horizontal。'],
          ['bindSelectedIndex(State<int>&)', '绑定选中索引。'],
          ['setOnChanged(std::function<void(int)>)', '变化回调。'],
          ['setStyleOverride(RadioGroupStyleOverride)', '覆盖普通、悬停、按下、禁用、选中和焦点样式。']
        ],
        usage: 'auto group = std::make_shared<oneui::RadioGroup>();\ngroup->setItems({L"低", L"中", L"高"});\ngroup->setOrientation(oneui::RadioGroup::Orientation::Horizontal);',
        notes: ['C ABI 通过 oneui_radio_group_* 暴露基础能力。']
      },
      {
        name: 'Tabs',
        include: '#include <oneui/controls/tabs.h>',
        purpose: '在多个同级视图之间切换，例如概览、日志、设置。',
        constructor: 'Tabs()',
        api: [
          ['setItems(std::vector<std::wstring>)', '设置标签项。'],
          ['setSelectedIndex(int) / selectedIndex()', '设置或读取当前标签。'],
          ['bindSelectedIndex(State<int>&)', '绑定选中索引。'],
          ['setOnChanged(std::function<void(int)>)', '切换回调。'],
          ['setStyleOverride(TabsStyleOverride)', '覆盖普通、悬停、按下、禁用、选中和焦点样式。']
        ],
        usage: 'auto tabs = std::make_shared<oneui::Tabs>();\ntabs->setItems({L"概览", L"连接", L"日志"});',
        notes: ['Tabs 本身只负责选择，不自动切换页面内容；业务层根据 selectedIndex 切换内容。']
      }
    ]
  },
  {
    title: '数据展示与产品控件',
    description: '这些组件用于列表、表格、导航、状态条、通知和窗口标题栏。',
    items: [
      {
        name: 'List',
        include: '#include <oneui/controls/list.h>',
        purpose: '展示可选择的项目列表，每项包含 title 和 detail。',
        constructor: 'List()',
        api: [
          ['setItems(std::vector<ListItem>)', '设置列表项，ListItem 包含 title、detail。'],
          ['setSelectedIndex(int) / selectedIndex()', '设置或读取选中行。'],
          ['bindSelectedIndex(State<int>&)', '绑定选中索引。'],
          ['setOnChanged(std::function<void(int)>)', '选中行变化回调。'],
          ['setStyleOverride(ListStyleOverride)', '覆盖行背景、文本、边框、焦点等样式。']
        ],
        usage: 'oneui::ListItem item{L"主机 A", L"192.168.1.10"};\nauto list = std::make_shared<oneui::List>();\nlist->setItems({item});',
        notes: ['适合中小型列表。大型虚拟列表还不是当前能力。']
      },
      {
        name: 'Table',
        include: '#include <oneui/controls/table.h>',
        purpose: '展示二维文本数据，适合简单报表、设备清单、连接列表。',
        constructor: 'Table()',
        api: [
          ['setColumns(std::vector<TableColumn>)', '设置列头和列宽。'],
          ['setRows(std::vector<std::vector<std::wstring>>)', '设置表格行数据。'],
          ['setStyleOverride(TableStyleOverride)', '覆盖表头、行、边框、文本等样式。'],
          ['clearStyleOverride()', '恢复默认样式。']
        ],
        usage: 'auto table = std::make_shared<oneui::Table>();\ntable->setColumns({{L"名称", 180}, {L"状态", 100}});\ntable->setRows({{L"节点 A", L"在线"}});',
        notes: ['当前不是完整 DataGrid，没有排序、编辑、冻结列和虚拟滚动。']
      },
      {
        name: 'NavItem',
        include: '#include <oneui/controls/nav_item.h>',
        purpose: '侧边栏导航项，包含图标、文本、选中状态和点击行为。',
        constructor: 'NavItem(std::wstring text, IconSymbol symbol, bool selected = false)',
        api: [
          ['setText(std::wstring)', '设置导航文本。'],
          ['setSymbol(IconSymbol)', '设置导航图标。'],
          ['setSelected(bool) / selected()', '设置或读取选中状态。'],
          ['setOnClick(std::function<void()>)', '点击回调。'],
          ['setStyleSheet(std::shared_ptr<StyleSheet>, StyleNode)', '应用 CSS-like 样式。']
        ],
        usage: 'auto home = std::make_shared<oneui::NavItem>(L"首页", oneui::IconSymbol::Home, true);',
        notes: ['适合与 AppShell、ProductShell、Stack 组合。']
      },
      {
        name: 'Tile',
        include: '#include <oneui/controls/tile.h>',
        purpose: '可点击的信息入口，包含标题、副标题、前导图标和尾随图标。',
        constructor: 'Tile(std::wstring title = L"", std::wstring subtitle = L"")',
        api: [
          ['setTitle(std::wstring)', '设置主标题。'],
          ['setSubtitle(std::wstring)', '设置副标题。'],
          ['setLeadingSymbol(IconSymbol) / clearLeadingSymbol()', '设置或清除左侧图标。'],
          ['setTrailingSymbol(IconSymbol) / clearTrailingSymbol()', '设置或清除右侧图标。'],
          ['setOnClick(std::function<void()>)', '点击回调。'],
          ['setStyleSheet(std::shared_ptr<StyleSheet>, StyleNode)', '应用 CSS-like 样式。']
        ],
        usage: 'auto tile = std::make_shared<oneui::Tile>(L"远程协助", L"连接到一台设备");\ntile->setLeadingSymbol(oneui::IconSymbol::RemoteAssist);',
        notes: ['当前未被 <oneui/oneui.h> 聚合，需要直接 include。', 'C ABI 已提供 oneui_tile_*。']
      },
      {
        name: 'StatusStrip',
        include: '#include <oneui/controls/status_strip.h>',
        purpose: '页面底部或面板底部的状态条，可带主/次操作。',
        constructor: 'StatusStrip(std::wstring title = L"", std::wstring message = L"")',
        api: [
          ['setTitle(std::wstring)', '设置状态标题。'],
          ['setMessage(std::wstring)', '设置状态说明。'],
          ['setPrimaryAction(std::wstring)', '设置主操作按钮文本。'],
          ['setSecondaryAction(std::wstring)', '设置次操作按钮文本。'],
          ['setOnPrimaryAction(std::function<void()>)', '主操作回调。'],
          ['setOnSecondaryAction(std::function<void()>)', '次操作回调。'],
          ['setStyleSheet(std::shared_ptr<StyleSheet>, StyleNode)', '应用 CSS-like 样式。']
        ],
        usage: 'auto strip = std::make_shared<oneui::StatusStrip>(L"已连接", L"延迟 24ms");\nstrip->setPrimaryAction(L"断开");',
        notes: ['当前未被 <oneui/oneui.h> 聚合。', 'C ABI 已提供 oneui_status_strip_*。']
      },
      {
        name: 'Toast',
        include: '#include <oneui/controls/toast.h>',
        purpose: '短暂通知卡片，可带图标、主/次操作和关闭按钮。',
        constructor: 'Toast(std::wstring title = L"", std::wstring message = L"")',
        api: [
          ['setTitle(std::wstring)', '设置通知标题。'],
          ['setMessage(std::wstring)', '设置通知正文。'],
          ['setPrimaryAction(std::wstring)', '设置主操作文本。'],
          ['setSecondaryAction(std::wstring)', '设置次操作文本。'],
          ['setIconSymbol(IconSymbol) / clearIconSymbol()', '设置或清除图标。'],
          ['setCloseVisible(bool)', '控制关闭按钮可见性。'],
          ['setOnPrimaryAction / setOnSecondaryAction / setOnClose', '设置操作和关闭回调。'],
          ['setStyleSheet(std::shared_ptr<StyleSheet>, StyleNode)', '应用 CSS-like 样式。']
        ],
        usage: 'auto toast = std::make_shared<oneui::Toast>(L"保存成功", L"配置已写入本机");\ntoast->setCloseVisible(true);',
        notes: ['当前未被 <oneui/oneui.h> 聚合。', '通常应放进 OverlayHost。']
      },
      {
        name: 'WindowTitleBar',
        include: '#include <oneui/controls/window_title_bar.h>',
        purpose: '自绘窗口标题栏，适合 borderless Win32 窗口。',
        constructor: 'WindowTitleBar(std::wstring title = L"")',
        api: [
          ['setTitle(std::wstring)', '设置标题。'],
          ['setIconSymbol(IconSymbol)', '设置窗口图标。'],
          ['setMaximized(bool)', '设置最大化视觉状态。'],
          ['setOnMinimize / setOnMaximize / setOnClose', '设置窗口控制按钮回调。'],
          ['setStyleSheet(std::shared_ptr<StyleSheet>, StyleNode)', '应用标题栏样式。']
        ],
        usage: 'auto titleBar = std::make_shared<oneui::WindowTitleBar>(L"OneUI 控制台");\ntitleBar->setOnClose([window] { window->close(); });',
        notes: ['通常配合 WindowOptions.borderless = true 使用。']
      }
    ]
  },
  {
    title: '表单、弹层与远程场景',
    description: '这些组件用于表单组织、验证提示、弹层挂载，以及远程桌面/远程控制相关能力。',
    items: [
      {
        name: 'FormField',
        include: '#include <oneui/controls/form_field.h>',
        purpose: '给输入控件加 label、helper、error、required、invalid 状态和可访问性关联。',
        constructor: 'FormField()',
        api: [
          ['setChild(std::shared_ptr<Widget>) / child()', '设置或读取内部输入控件。'],
          ['clearChild()', '移除子控件。'],
          ['setLabel / label / bindLabel', '设置、读取或绑定标签。'],
          ['setHelperText / helperText / bindHelperText', '设置、读取或绑定帮助文本。'],
          ['setErrorText / errorText / bindErrorText', '设置、读取或绑定错误文本。'],
          ['setRequired / required / bindRequired', '设置、读取或绑定必填状态。'],
          ['setInvalid / invalid / bindInvalid', '设置、读取或绑定错误状态。'],
          ['setStyleOverride(FormFieldStyleOverride)', '覆盖 label、helper、error、间距等样式。']
        ],
        usage: 'auto field = std::make_shared<oneui::FormField>();\nfield->setLabel(L"连接地址");\nfield->setRequired(true);\nfield->setChild(addressInput);',
        notes: ['FormField 不是表单提交器，只负责单项字段布局和语义。']
      },
      {
        name: 'ValidationMessage',
        include: '#include <oneui/controls/validation_message.h>',
        purpose: '单独显示帮助或错误信息。',
        constructor: 'ValidationMessage(std::wstring text = L"")',
        api: [
          ['setText(std::wstring) / text()', '设置或读取提示文本。'],
          ['bindText(State<std::wstring>&)', '绑定提示文本。'],
          ['setTone(ValidationMessageTone) / tone()', '设置 Helper 或 Error。'],
          ['setStyleOverride(ValidationMessageStyleOverride)', '覆盖颜色、图标、字体等。']
        ],
        usage: 'auto error = std::make_shared<oneui::ValidationMessage>(L"地址不能为空");\nerror->setTone(oneui::ValidationMessageTone::Error);',
        notes: ['FormField 内部也能显示 helper/error；独立组件适合复杂布局。']
      },
      {
        name: 'Popup',
        include: '#include <oneui/controls/popup.h>',
        purpose: '锚定到某个区域的弹层，支持首选位置、翻转、轻关闭、模态和外部指针策略。',
        constructor: 'Popup()',
        api: [
          ['setAnchor(std::shared_ptr<Widget>) / anchor()', '设置锚点控件。'],
          ['setContent(std::shared_ptr<Widget>) / content()', '设置弹层内容。'],
          ['setOpen(bool) / isOpen() / bindOpen(State<bool>&)', '控制打开状态。'],
          ['setPreferredPlacement(PopupPreferredPlacement)', '设置 BottomStart、TopEnd 等首选位置。'],
          ['setViewport(std::optional<Rect>) / clearViewport()', '设置可用视口。'],
          ['setAnchorRect(std::optional<Rect>) / clearAnchorRect()', '显式设置锚点矩形。'],
          ['setCloseOnOutsideClick(bool)', '外部点击时关闭。'],
          ['setInteractionMode(PopupInteractionMode)', '设置 Modeless、LightDismiss、Modal。'],
          ['overlayOptions(int layer = 0)', '生成 OverlayHost 可用的挂载选项。'],
          ['setOutsidePointerPolicy(PopupOutsidePointerPolicy)', '设置 PassThrough、Close 或 Block。'],
          ['setCloseOnEscape(bool)', 'Esc 关闭。'],
          ['resolvedContentRect()', '读取最终内容矩形。']
        ],
        usage: 'auto popup = std::make_shared<oneui::Popup>();\npopup->setAnchor(button);\npopup->setContent(menu);\npopup->setPreferredPlacement(oneui::PopupPreferredPlacement::BottomStart);\npopup->setOpen(true);',
        notes: ['PopupPlacement::resolve 可单独做几何计算测试。', '复杂弹层建议通过 OverlayHost 管理层级。']
      },
      {
        name: 'OverlayHost',
        include: '#include <oneui/layout/overlay_host.h>',
        purpose: '在普通内容之上挂载弹层、通知、菜单、模态层。',
        constructor: 'OverlayHost()',
        api: [
          ['setContent(std::shared_ptr<Widget>)', '设置底层主内容。'],
          ['addOverlay(child, int layer)', '按层级添加普通 overlay。'],
          ['addOverlay(child, OverlayOptions)', '使用 trapsFocus、blocksOutsidePointer 等选项添加 overlay。'],
          ['addAnchoredOverlay(child, options, Size, Insets, hAlign, vAlign)', '添加带锚定尺寸和对齐的 overlay。'],
          ['removeOverlay(std::shared_ptr<Widget>)', '移除指定 overlay。'],
          ['clearOverlays()', '清空所有 overlay。'],
          ['overlays()', '读取 overlay 列表。']
        ],
        usage: 'auto host = std::make_shared<oneui::OverlayHost>();\nhost->setContent(app);\nhost->addOverlay(toast, oneui::OverlayOptions::modeless(10));',
        notes: ['Modal overlay 可阻断外部指针并限制焦点。']
      },
      {
        name: 'RealtimeFrameView',
        include: '#include <oneui/controls/realtime_frame_view.h>',
        purpose: '显示远程画面最新帧，并按 ActualSize、Fit、Fill、Stretch 计算内容矩形。',
        constructor: 'RealtimeFrameView()',
        api: [
          ['submitFrame(VideoFrame)', '提交一帧 BGRA/RGBA/NV12 数据。'],
          ['setScaleMode(ScaleMode) / scaleMode()', '设置或读取缩放模式。'],
          ['contentRect()', '读取当前帧在控件中的实际显示矩形。'],
          ['latestFrame()', '读取最新帧快照。']
        ],
        usage: 'oneui::VideoFrame frame{};\nframe.width = width;\nframe.height = height;\nframe.format = oneui::PixelFormat::Bgra8888;\nview->submitFrame(frame);',
        notes: ['适合 MVP 远程画面显示。', '后续高性能视频路径仍需要专门优化。']
      },
      {
        name: 'RemoteInputRegion',
        include: '#include <oneui/controls/remote_input_region.h>',
        purpose: '把本地鼠标和键盘输入映射成远端坐标与原始按键事件。',
        constructor: 'RemoteInputRegion()',
        api: [
          ['setRemoteSize(Size) / remoteSize()', '设置远端画面尺寸。'],
          ['setScaleMode(RemoteInputScaleMode) / scaleMode()', '设置坐标映射缩放模式。'],
          ['contentRect()', '读取远端画面在本地控件中的内容矩形。'],
          ['setOnPointer(PointerCallback)', '设置指针事件回调，包含本地、内容、归一化、远端坐标。'],
          ['setOnRawKey(RawKeyCallback)', '设置原始键盘事件回调。'],
          ['dispatchPointer(RemotePointerEvent)', '手动派发指针事件。'],
          ['dispatchRawKey(RawKeyEvent)', '手动派发键盘事件。'],
          ['releaseAllInputs()', '释放所有已按下输入，适合连接断开或焦点丢失。']
        ],
        usage: 'region->setRemoteSize(oneui::Size{1920, 1080});\nregion->setOnPointer([](const oneui::RemotePointerEvent& e) {\n  sendPointer(e.remotePosition, e.button, e.pressed);\n});',
        notes: ['坐标映射必须和 RealtimeFrameView 使用同一种 scale mode。']
      }
    ]
  },
  {
    title: '桥接与计算型组件',
    description: '这些不是普通页面控件，更多用于把 OneUI 的视觉/布局计算复用到原生平台控件或产品壳实现里。',
    items: [
      {
        name: 'TextInputBridge',
        include: '#include <oneui/controls/text_input_bridge.h>',
        purpose: '计算自绘输入框与原生编辑器之间的位置、显示和预留区域。',
        constructor: '纯函数和结构体，无需构造控件',
        api: [
          ['TextInputBridgeConfig', '输入 frame、focused、hovered、disabled、readOnly、revealNativeEditor、leadingReservedWidth 等配置。'],
          ['TextInputBridgeLayout', '输出 frame、contentRect、editorRect、hiddenEditorRect、showNativeEditor。'],
          ['textInputBridgeState(config)', '根据状态得到桥接状态。'],
          ['computeTextInputBridgeLayout(config)', '计算编辑器矩形。']
        ],
        usage: 'auto layout = oneui::computeTextInputBridgeLayout(config);',
        notes: ['适合平台层或嵌入原生输入法场景。']
      },
      {
        name: 'ButtonBridge',
        include: '#include <oneui/controls/button_bridge.h>',
        purpose: '计算按钮桥接状态、布局和命中测试。',
        constructor: '纯函数和结构体，无需构造控件',
        api: [
          ['buttonBridgeState(config)', '根据 hovered、pressed、disabled 等得到按钮状态。'],
          ['computeButtonBridgeLayout(config)', '计算图文区域。'],
          ['hitTestButtonBridge(layout, point)', '命中测试。']
        ],
        usage: 'if (oneui::hitTestButtonBridge(layout, mouse)) { /* hover */ }',
        notes: ['用于平台桥接和一致视觉计算。']
      },
      {
        name: 'ProductShell Helpers',
        include: '#include <oneui/layout/product_shell.h>',
        purpose: '为产品级布局计算标准矩形，例如侧边栏、顶栏、仪表盘、表单行、状态条、窗口 chrome。',
        constructor: 'ProductShell 是控件；computeProduct* 是纯计算函数',
        api: [
          ['computeProductShellLayout(...)', '计算产品壳主体区域。'],
          ['computeProductSidebarLayout(...)', '计算侧边栏区域。'],
          ['computeProductTopBarLayout(...)', '计算顶栏区域。'],
          ['computeProductDashboardLayout(...)', '计算仪表盘卡片区域。'],
          ['computeProductFormRowLayout(...)', '计算表单行区域。'],
          ['computeProductAssistHomeLayout(...)', '计算远程协助首页区域。'],
          ['computeProductStatusStripLayout(...)', '计算状态条区域。'],
          ['computeProductWindowChromeLayout(...)', '计算窗口 chrome 区域。']
        ],
        usage: 'auto layout = oneui::computeProductDashboardLayout(metrics);',
        notes: ['这类函数适合写单元测试，避免产品壳布局退化成魔法数字。']
      },
      {
        name: 'Animation / StyleTransition',
        include: '#include <oneui/animation.h> 与 #include <oneui/style_transition.h>',
        purpose: '提供 FloatTransition、ColorTransition、StyleBoxTransition 和 easing 计算。',
        constructor: 'FloatTransition(), ColorTransition(), StyleBoxTransition()',
        api: [
          ['TransitionSpec{durationMs, easing}', '描述动画时长与 easing。'],
          ['applyEasing(EasingCurve, float)', '计算 easing 后的进度。'],
          ['interpolateFloat / interpolateColor', '插值工具。'],
          ['reset(value)', '立即重置当前值和目标值。'],
          ['animateTo(target, spec)', '启动过渡。'],
          ['tick(deltaMs)', '推进动画，返回是否仍在运行。'],
          ['value() / target() / running()', '读取当前值、目标值和运行状态。'],
          ['StyleBoxTransition::applyTo(StyleBox&)', '把过渡值写入样式盒。']
        ],
        usage: 'oneui::FloatTransition opacity;\nopacity.reset(0.0f);\nopacity.animateTo(1.0f, oneui::TransitionSpec{160, oneui::EasingCurve::EaseOutCubic});',
        notes: ['当前未被 <oneui/oneui.h> 聚合，需要直接 include。']
      }
    ]
  }
]

export const layoutComponents = [
  ['Stack', '一维布局。direction 控制 Row/Column，gap 控制间距，padding 控制内边距，align 控制 Start/Center/End/Stretch。最常用于表单、按钮组、侧边栏。'],
  ['Grid', '固定列数网格。setColumns、setGap、setColumnGap、setRowGap、setPadding、setAutoRows。适合仪表盘卡片和入口矩阵。不是浏览器 CSS Grid。'],
  ['Wrap', '自动换行布局。setGap、setRowGap、setPadding。适合标签、筛选项、快捷操作按钮。'],
  ['DockView', '五区布局。setTop、setRight、setBottom、setLeft、setCenter，外加 gap 和 padding。适合传统桌面应用外壳。'],
  ['SplitView', '水平或垂直分栏。setFirst、setSecond、setOrientation、setSplitRatio、setGap、setPadding。当前偏静态比例，不是完整可拖拽 splitter。'],
  ['ScrollView', '滚动容器。setContent、setContentWidth、setContentHeight、setWheelStep、setHorizontalScrollOffset、setScrollOffset，并提供最大滚动偏移读取。'],
  ['AppShell', '应用壳。setSidebar、setHeader、setContent、setFooter，可设置 sidebarWidth、headerHeight、footerHeight、gap、padding、sidebarVisible。'],
  ['TopBar', '顶部工具栏。setLeading、addAction、clearActions、setPadding、setGap、setLeadingWidth。当前未被 oneui.h 聚合。'],
  ['ProductShell', '产品壳控件。setSidebar、setTopbar、setContent、setStatus，并可设置宽高、gap、padding、sidebarVisible。']
]

export const styleRows = [
  ['StyleNode', '控件暴露给样式系统的节点信息，通常包含 tag 和 classes，例如 button.primary、input.search。'],
  ['StyleSheet::addRulesFromCss', '解析 CSS-like 文本。失败时返回错误信息，适合加载主题文件。'],
  ['StyleSheet::resolve', '按 selector、class、伪状态和优先级解析最终 StyleBox。'],
  ['StyleBox', '通用视觉属性容器，包含 background、color、border、radius、padding、gap、fontSize、shadow、opacity 等。'],
  ['Typed Override', '各控件把 StyleBox 转成 ButtonStyleOverride、TextFieldStyleOverride 等强类型样式。']
]

export const stylePseudoStates = [
  [':hover', '鼠标悬停。'],
  [':active', '按下或激活中。'],
  [':focus', '焦点可见。'],
  [':disabled', '禁用。'],
  [':selected / :checked', '选中状态；checked 会映射到 selected 语义。'],
  [':read-only / :readonly', '只读状态，主要用于 TextField。']
]

export const styleProperties = [
  ['background / background-color', '背景色，支持 #RRGGBB / #RRGGBBAA，部分场景会解析简化 linear-gradient 起止色。'],
  ['color', '前景色，用于文本、图标或选中标记。'],
  ['placeholder-color', 'TextField 占位文字颜色。'],
  ['caret-color', 'TextField 光标颜色。'],
  ['border-color / border-width', '边框颜色和宽度。'],
  ['border-radius', '圆角半径。'],
  ['padding', '内边距，支持类似 CSS 的 1、2、4 值写法。'],
  ['gap', '布局或复合控件内部间距。'],
  ['font-size', '字号。'],
  ['outline-color / outline-width / outline-offset', '焦点环样式。'],
  ['box-shadow', '阴影，支持 OneUI 简化模型。'],
  ['opacity', '透明度，通常会 clamp 到 0 到 1。'],
  ['content-background / content-radius / content-inset', '复杂控件内部内容盒样式。']
]

export const cApiGroups = [
  ['Window', 'oneui_window_create、oneui_window_destroy、oneui_window_show、oneui_window_run、oneui_window_close、oneui_window_request_close、oneui_window_minimize、oneui_window_toggle_maximize、oneui_window_set_borderless、oneui_window_post、oneui_window_request_animation_frame、oneui_window_set_title、oneui_window_native_handle、oneui_window_client_width、oneui_window_client_height、oneui_window_set_content、oneui_window_set_style_sheet。'],
  ['Widget', 'oneui_widget_destroy、oneui_widget_set_preferred_size、oneui_widget_set_disabled、oneui_widget_set_visible、oneui_widget_set_classes、oneui_widget_set_style_node、oneui_widget_apply_style_sheet。'],
  ['StyleSheet', 'oneui_style_sheet_create、oneui_style_sheet_destroy、oneui_style_sheet_set_custom_property、oneui_style_sheet_add_css、oneui_style_sheet_load_file。'],
  ['Layouts', 'oneui_stack_*、oneui_top_bar_*、oneui_app_shell_*、oneui_product_shell_*、oneui_overlay_host_*、oneui_panel_*'],
  ['Display', 'oneui_label_*、oneui_icon_*、oneui_badge_*、oneui_card_*、oneui_tile_*、oneui_status_strip_*、oneui_toast_*'],
  ['Input', 'oneui_button_*、oneui_icon_button_*、oneui_switch_*、oneui_radio_group_*、oneui_text_field_*、oneui_search_box_create'],
  ['Chrome / Nav', 'oneui_title_bar_*、oneui_nav_item_*'],
  ['未覆盖提醒', 'Select、Tabs、List、Table、Slider、Popup、RealtimeFrameView、RemoteInputRegion 等目前主要通过 C++ API 使用。']
]

export const bindingRows = [
  ['C', '直接 include oneui_c_api.h，链接 oneui.lib 或动态加载 oneui.dll。'],
  ['C++', '优先使用完整 C++ API；在插件边界、脚本边界或二进制边界可使用 C ABI。'],
  ['Rust', '使用 bindgen 或手写 extern "C"。注意 wchar_t*、回调生命周期和 UI 线程。'],
  ['Go', '可通过 cgo 或 syscall/windows 调用 DLL。复杂回调场景建议先封装一层 C。'],
  ['C#', '使用 DllImport/PInvoke 声明结构体和函数。字符串按 UTF-16 宽字符处理。'],
  ['Python', '使用 ctypes/cffi 加载 oneui.dll。必须保存 callback 对象引用，避免被 GC 回收。'],
  ['Node.js', '通过 ffi-napi、N-API addon 或自定义 native addon 包装 C ABI。'],
  ['Java/Kotlin', '通过 JNI/JNA 调用 C ABI。复杂 UI 生命周期建议封装成更小的 Java API。'],
  ['其他 FFI 语言', '只要能加载 DLL、按 C 调用约定传参，并处理宽字符和回调，就可以接入。']
]

export const cAbiRules = [
  ['字符串', '公开 C ABI 的文本多使用 const wchar_t*，在 Windows 上通常按 UTF-16 宽字符传递。'],
  ['生命周期', 'window、widget、style_sheet 都有 destroy 函数。谁创建，谁负责在合适时机释放。'],
  ['回调', '函数指针和 user_data 必须在控件可能触发事件期间保持有效。托管语言要防止回调被 GC。'],
  ['线程', 'UI 对象应在 UI 线程访问。后台线程更新 UI 时使用 oneui_window_post。'],
  ['消息循环', '常规程序调用 oneui_window_run。嵌入已有消息循环的宿主需要专门集成策略。'],
  ['DLL 查找', '运行时必须能找到 oneui.dll 及其依赖。开发环境可设置 PATH，发布时通常放在 exe 旁边。']
]

export const testRows = [
  ['oneui_control_behavior_tests', '已覆盖', '控件状态、样式覆盖、可访问性、Popup 几何等基础行为。'],
  ['oneui_overlay_host_behavior_tests', '已覆盖', 'Overlay 层级、焦点和外部指针边界。'],
  ['oneui_scroll_view_behavior_tests', '已覆盖', '滚动偏移、越界 clamp、水平滚动等。'],
  ['oneui_stack_behavior_tests', 'CMake 已列出', '本地旧 build 可能未重新 configure，需要重新生成后确认。'],
  ['oneui_panel_behavior_tests', 'CMake 已列出', '本地旧 build 可能未重新 configure，需要重新生成后确认。'],
  ['oneui_c_api_behavior_tests', 'CMake 已列出', '用于验证 C ABI 基础行为。'],
  ['oneui_monitor_behavior_tests', '旧构建失败', '旧构建中退出码 0xc0000135，疑似 DLL 查找问题。']
]

export const faqItems = [
  {
    label: 'OneUI 现在是否任何语言都能调用？',
    content: '准确说：任何能调用 Windows DLL 的语言，理论上都能通过 oneui_c_api.h 暴露的 C ABI 调用 OneUI 的一部分能力。但 C ABI 不是完整 C++ API 镜像，部分控件仍只能直接用 C++。'
  },
  {
    label: '为什么有些组件头文件没有出现在 <oneui/oneui.h>？',
    content: '这是当前审查发现的 API 聚合问题。组件实现和单独头文件已经存在，但聚合头还没更新。现在使用这些组件时直接 include 对应头文件即可，后续应在项目源码中修复聚合头。'
  },
  {
    label: 'StyleSheet 是完整 CSS 吗？',
    content: '不是。它是 CSS-like 子集，只覆盖 OneUI 当前需要的选择器、伪状态和视觉属性。布局仍由 Stack、Grid、AppShell 等布局控件完成。'
  },
  {
    label: 'Linux 和 macOS 当前能用吗？',
    content: '当前不能当成可用后端。仓库里有 skeleton 和规划痕迹，但真正可运行实现是 Windows Win32。'
  }
]
