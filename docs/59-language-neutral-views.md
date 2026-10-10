# 与语言无关的声明式视图：WP3 设计稿

状态：**待 Claude 评审，仅设计，未实现**。版本：design-1，2026-10-10。

本稿以 OneUI `2aa15e9be3fab90316bfa6c5447ee099e177aa55`、工作树 `E:/project/byname/oneui-views`、分支 `codex/language-neutral-views` 为依据。iShell 只读调查时 HEAD 为 `b4509dee67aca9fe74c39dbaefd8670b9079c89c`；其另一路 WP2b 可能继续移动文件，以下行号指本次读到的版本。

任务依据：`E:/project/tauri/ishellpro-arch/docs/codex/工程整治与界面模板化-任务书.md` 第 0、2、3 节，第 5 节 WP3/WP4，第 9 节设计评审门禁。本稿的“必须”均为**推荐协议内部约束**，不是已获批准的实现决策；第 12 节列出需 Claude 裁决的事项。评审通过后才进入实现。

## 1. 现状调查与可复用边界

### 1.1 viewc：解析、检查和生成

| 证据（仓库相对路径:行号） | 当前行为 | WP3 处理 |
|---|---|---|
| `tools/viewc/main.cpp:10–49` | 手写 XML 子集 Parser；节点/属性记录行号；支持引号属性、注释、自闭合节点；不是浏览器 XML/HTML 解析器 | 复用语法，抽出 AST 和精确 span；不增加 JS |
| `tools/viewc/main.cpp:61–82` | import 按文件递归加载、检测循环；一个 template 一个根；view-model 可写 C++ 限定类型名；scoped CSS 在加载时检查 | 保留 import/单根/scoped；新增语言无关 schema 输入，view-model 改为契约名 |
| `tools/viewc/main.cpp:84–90` | 表达式仅 true/false 或点分成员；转换为 `vm.x` / `item->x`；生成 `#line` | 绑定解析为 schema 路径和整数 ID；不保留语言表达式 |
| `tools/viewc/main.cpp:99–177` | 验证 v-for/key、if/else、组件子节点个数、Workspace/DockPanel 区域、FormGrid 等；import 调 `build_子组件`，slot 生成 C++ lambda | 同一语义检查前端输出 typed IR；保留当前合成 Column 和区域排序语义 |
| `tools/viewc/main.cpp:180–237` | ref、事件、v-model、bindTyped、静态值验证；拒绝未知属性/事件、非法数值和图标 | 静态诊断复用；VM 成员存在性和类型必须前移至 viewc |
| `include/oneui/ui_component_schema.h:6–54,96–128` | 元数据是组件名、属性名、model 类型；类型含 Text/Boolean/Integer/Number/Strings/Columns/Rows/TabItems/Series；没有完整 VM schema 或命令签名表 | 扩充统一组件注册表，不能把 C++ enum 的顺序值直接作为磁盘 ID |
| `tools/viewc/main.cpp:242–268` | 输出 `.g.h`，含 `build_X(VM&, Mount&, Slots)`、`styles_X()`、`sources_X()` 和 depfile；vm-header 是 C++ 头 | 新增 `.oneview` 和语言适配器；旧 build 名称可暂留为 IR 包装入口 |
| `include/oneui/ui_declarative.h:514–540` | `bindTyped` 的 static_assert、`modelTyped` 重载承担 VM 类型检查 | **目前不是 viewc 独立完成全部类型检查**；去掉 C++ 后必须有显式类型契约 |

运行时已保留控件树：`Mount::watch/flush` 在 `include/oneui/ui_declarative.h:240–257` 合并待应用值；`condition` 在 `:553–556` 切可见性；`repeat` 在 `:559–577` 使用 shared item、子 Mount 和 `reconcileChildren`，拒绝空/重复 key 及同 key 更换对象。`click` 在 `:541–545` 自动由 canExecute/running 控制 disabled，不能在迁移时丢掉该语义。`KeyedTable::update` 在 `:47–73` 同 key 更新单行，结构变化则重设行并恢复选择/滚动；它并非完整结构增量数据源。

### 1.2 七份声明式文档

| 文档依据 | 可直接复用 | 需要替换或明确的冲突 |
|---|---|---|
| `docs/34-declarative-authoring-proposal.md:11–29,49,67` | MVVM、同一控件树、统一通知调度、无 JS、稳定身份/自动清理 | “编译为创建和绑定代码”改为编译 IR；不是另建第二状态运行时 |
| `docs/35-declarative-authoring-v1.md:53–61,97,107–111,136–141` | 属性/事件限制、保留式条件、稳定 key、import/slot、CSS 严格消费检查、绘前合并、输入状态保留 | C++ State/wstring/vector 不可跨 ABI；现有类型检查依赖 C++；结构热更新原本不支持 |
| `docs/36-declarative-page-patterns.md:3–12,30–43,51–57` | SettingsPage/ListPage/DetailPage、ActionBar 归位、无障碍、源码定位、焦点滚动工具 | 诊断须认识 IR span；L4 不因取得 ref 而获准手工搭普通控件 |
| `docs/40-declarative-stage-validation.md:24–55` | 入口一致性、生命周期、CSS 回滚与状态保留测试方法 | 历史 C++ paint 接近数据不是 Rust 属性到重绘的 ±10% 证明；不得挪用 |
| `docs/56-declarative-terminal-workspace.md:23–41,61,77–79` | Tabs 稳定 ID、SplitView、图表、NativeHost 主题 opt-in | @close 有字符串参数，不能把所有事件当无参命令；vector 复合类型须编码；旧 Rust 是原生内容接入 |
| `docs/57-workspace-layout-components.md:21–55,73–79,92` | 语义区域、DockPanel 收起保留、主题和布局组件 | DockPanel 不等于 WP4 DockWorkspace；旧 VM 宽度断点不等于容器查询；不把 WP4 能力算已实现 |
| `docs/58-declarative-workspace-stage.md:7–11,20–34` | 回归覆盖与阶段边界 | “直接生成 Rust 不在本版承诺”由本次 WP3 扩展；历史测试数量不是当前运行证据 |

### 1.3 C ABI 与 Rust 生命周期

- 当前真实 ABI 为 **43**：`include/oneui/oneui_c_api.h:544`、`bindings/rust/oneui-sys/src/lib.rs:14`。`docs/c-abi-integration.md:4–10` 仍写 40，是文档滞后；其 `:53–55` 还说明开发期同版本出现过新增导出。因此既检查版本，也检查匹配 SDK 的符号清单，不能只看文档版本号。
- ABI 使用 opaque `OneUiWindow/OneUiWidget`（头文件 `:13–17`）；实际 widget handle 包 `shared_ptr<Widget>`（`src/capi/oneui_c_api_widget_internal.h:18–24`）。`oneui_widget_destroy` 当前只 delete handle（`src/capi/oneui_c_api.cpp:1847–1849`）；树持有 widget 时原生对象可以仍存活，**这不延长外语回调所有者的寿命**。
- 回调一般为函数指针 + user_data；UTF-8 用指针/长度，不依赖 wchar_t。错误诊断为线程局部借用文本，不随成功调用自动清除（头文件 `:19–36`）。新协议使用明确返回码，同时保留该诊断通道。
- `oneui_window_post_owned`（头文件 `:703–711`、实现 `src/capi/oneui_c_api.cpp:1551–1568`）在窗口 UI 线程执行；未能执行走 cleanup，参数不合法也立即 cleanup。不能在返回 0 后再次释放同一 payload。调用者仍须保证传入 window handle 有效，post 并不让裸句柄自动线程安全。
- Rust 原始绑定手写 `repr(C)` opaque/POD、整数枚举别名和 extern C（`oneui-sys/src/lib.rs:1–43,370–491`），不是自动反射；`oneui/src/application.rs:82–89` 严格比较 ABI。`scripts/check-abi-sync.cmake:31–33` 检查版本同步，`supported-symbols.txt` 维护导出集合。
- Rust `UiDispatcher` 持 `Arc<WindowState>`；窗口裸句柄访问与销毁同步（`oneui/src/lib.rs:537–545,700–744`）。`dispatch` 要求 Send，`dispatch_local` 限 UI 线程且接收非 Send 闭包（`:995–1056`）。任务通过 Box 转 raw，在执行或取消路径回收。
- Rust Button 用 Box token + `Rc<RefCell<FnMut>>`；清回调后才释放 token（`:3779–3812`）。进入回调前 clone Rc，允许业务在回调里销毁包装；嵌套命令用 try_borrow_mut 拒绝重入（`:2073–2098`）。`run_callback_guarded` 捕获 panic（`:288–305`）。新 VM 适配器必须覆盖**所有**回调的 panic/借用保护，不能仅套 get/set。
- `Widget::drop` 使跨线程状态失效再销毁 C handle（`:1803–1816`），Window drop 清回调再销毁（`:10918–10928`）。原生保留树与 Rust 生命周期不能混为一谈。

### 1.4 iShell 只读现状与验收背景

以下路径以 `E:/project/tauri/ishellpro-arch/` 为根：

| 文件:行号 | 实际用途 |
|---|---|
| `native-shell/one-templates/TerminalWorkspace.one:1–10` | Column + sessions/selector/dock 三个 NativeHost；只有 selector 的 `:visible="compact"` 是动态绑定 |
| `native-shell/one-templates/TerminalPane.one:1–9` | header/content 两个 NativeHost，静态布局 |
| `native-shell/one-templates/MonitorPanel.one:1–11` | header/tabs/body/footer 四个 NativeHost，静态布局 |
| `native-shell/one-templates/vm.h:1–5` | 唯一 VM 字段为 `State<bool> compact` |
| `native-shell/one-templates/bridge.cpp:12–21,29–73` | C++ Layout 持 VM/Mount/root，retain Rust 原生 widget、registerNative、调 build_X；刷新 compact 时手动 flush；析构先从 Panel 拆 root |
| `native-shell/one-templates/CMakeLists.txt:17–30` | 三模板生成 C++ 并链接独立 bridge DLL；记录 OneUI DLL 哈希 |
| `native-shell/Cargo.toml:10`、`native-shell/build.rs:168–195` | one-template 特性门控；校验 bridge 与 Rust 使用同一 OneUI DLL并链接/复制 bridge |
| `native-shell/src/settings_basic.rs:20–65,99–198` | 基础设置仍手工构造，含主题画廊、语言、标题栏风格、密度、关闭动作；即时更新设置 |
| `native-shell/src/product_shell/settings_appearance.rs:19–28,31–138` | 保留自定义百分比、背景文件/强度/内容透明度/填充/位置；不能以最小示例替代产品完整迁移 |

三个模板只是大块手写 UI 的壳。后续 WP5/WP7 不能照搬 header/tabs/body 等大 NativeHost 来绕过普通控件禁止规则；只有终端画面、编辑器等白名单内容留在 NativeHost。

已读 `docs/codex/reports/WP-0-review.md`、`WP-1-review.md`、`WP-2-review.md`：三者通过；WP1 的真实改动重编指标在 WP2 达成；门控夹具在对应 WP5 批次迁移，既有缺陷也随批次处理；像素比较须固定不确定来源，或比较旧版自身波动；WP2b 另行拆 protocol 文件。本次不改变这些安排。当前 OneUI 58/59 通过及 windows runtime audit 哈希既有失败是 Claude 提供的基线，本次文档任务未重跑 CTest。

## 2. 目标管线与类型契约

```text
.one + scoped CSS + .one.schema.json
        │ 解析 / import 展开 / 组件和 VM 类型校验
        ▼
      typed IR ───────► .oneview（唯一可执行的视图描述）
        ├────────────► Rust trait、ID、命令 enum、VTable adapter
        └────────────► C++ VM adapter + build_X 薄包装
                              │ 同一个 C 协议
                              ▼
                  View runtime → 现有组件 / Mount 调度 / 控件树
                              ▼
                         Yoga / 渲染 / 平台
```

IR 没有语言源码、函数地址或可执行字节码。L4 只提供数据与命令，复杂计算在 VM；L5 不依赖 OneUI。WP4 注册新组件/语义事件，不能把 PaneGrid 的布局算法塞进 IR 或 VM。IR runtime 与现有 Mount 共用组件工厂、属性应用器和绘前队列，不引入每帧遍历的虚拟 DOM。

### 2.1 schema 来源（待决 D1）

推荐显式、语言无关 JSON sidecar：`SettingsBasic.one.schema.json`，由 `--schema` 指定；根 `.one` 的 view-model 是 schema 名称。import 默认继承调用者的契约；导入文件可声明同名契约或要求显式匹配的契约，不能以 C++ 类型名推断类型。不能从使用位置猜测可写性、命令参数、未使用字段或列表项结构。v1 不做 C++/Rust 源码反射。

```json
{
  "schema_version": 1,
  "root": "SettingsBasic",
  "models": [
    {"name":"SettingsBasic", "properties":[
      {"id":1,"name":"language","type":"i64","access":"rw"},
      {"id":2,"name":"compact","type":"bool","access":"rw"},
      {"id":3,"name":"status","type":"utf8","access":"r"},
      {"id":4,"name":"themes","type":{"list":"ThemeItem"},"access":"r"}
    ], "commands":[
      {"id":1,"name":"reset","args":[],"result":"void"}
    ]},
    {"name":"ThemeItem", "key":"id", "properties":[
      {"id":1,"name":"id","type":"utf8","access":"r"},
      {"id":2,"name":"title","type":"utf8","access":"r"},
      {"id":3,"name":"selected","type":"bool","access":"r"}
    ], "commands":[
      {"id":1,"name":"select","args":[],"result":"void"}
    ]}
  ]
}
```

ID 是显式非零 u32，属性和命令分开命名空间，各 model 内唯一；废弃 ID 不复用。模型 ID 按 UTF-8 名称字节序从 1 编号，仅在本份 schema 内使用；增删模型可能重排 ID，哈希必变化。属性名可为 `settings.language` 这种静态点分路径，整体对应一个 ID；运行期不查字符串、不沿指针反射。列表变量仅改变词法 VM 上下文，`item.title` 对应 ThemeItem 的 title；外层变量用 lexical depth 定位。非法/模糊捕获编译失败。

属性标量为 bool/i64/f64/utf8，集合统一 list<model>。没有隐式 nullable，void 只供命令结果；需要“可选值”用显式 hasX + X。v-model 必须 rw 且类型精确匹配。i64→原生 int、f64→float 在边界检查范围/有限性，禁止静默截断整数。类型检查由编译器完成，运行时仍验证恶意或错误的 VM 返回。

v1 list 属性只允许 r；替换集合由 VM 自己完成并通知 RESET，不接受来源不明的 list 输入值。`true/false` 用在条件时进入 COND 常量，用在单向布尔属性时降为静态 PROP，不能用于 v-model 或命令。property 和 key 名解析后的 ID 进入 IR，原表达式只留源码映射。

命令参数必须与组件事件签名完全匹配；v1 click/activate/delete 为无参、Tabs.close 为 utf8 key。新事件由注册表声明参数序列；不允许 IR 随意调用 VM 任意函数。命令同步返回协议状态，没有跨 ABI 的 Future；异步结果通过 VM 属性和 notify 送回。

每个命令可在 schema 指定 `can_execute`、`running` 两个只读 bool 属性 ID（缺省分别 true/false），计入哈希。runtime 每次调用前检查缓存状态；Button.disabled 为显式 disabled OR !can_execute OR running。这保留 VmCommand 行为，C++ adapter 不再重复挂 click 订阅。`invoke` 执行期间同一 VM scope 的另一条可变回调也不能嵌套。

### 2.2 复合属性映射

为现有注册表的所有 ValueType 提供公开内置 model，不能把 std::vector 或 JSON 字符串传进 C：

| 现有类型 | list item 的字段（固定 ID 从左到右为 1…N） | 应用规则 |
|---|---|---|
| Strings | `id:utf8, value:utf8` | Select 按顺序提取 value；生成 adapter 为旧 vector 提供确定位置 key；它不冒充可重排业务列表 |
| TabItems | `id:utf8, title:utf8` | key=id，沿用 KeyedTabs 选择语义 |
| Rows | `id:utf8, cells:list<StringItem>` | key=id；cells 为上述 StringItem；保留按 ID 选择 |
| Columns | `id:utf8, header:utf8, width:f64, alignment:i64, action:bool, kind:i64` | width≥0；alignment=0/1/2 左中右，kind=0/1 text/percent，非法拒绝 |
| Series | `id:utf8, name:utf8, rgba:i64, values:list<NumberItem>` | rgba 为 0…0xffffffff 的 RRGGBBAA；NumberItem=`id:utf8,value:f64`；值有限 |

内置记录名使用 `oneui.v1.*`，同样进入完整 schema、生成绑定及哈希，不隐藏在 runtime C++ 类型里。现有 TableColumn/TimeSeriesChartSeries 字段依据 `include/oneui/controls/table.h:20–26`、`include/oneui/controls/time_series_chart.h:15–21`。只承诺当前声明式可表达的数据；rich TableCell、任意对象、惰性虚拟数据源另有组件接口，不靠无类型指针偷渡。C++ 旧集合适配可以缓存快照子对象；重复列表仍要求稳定业务身份。

## 3. `.oneview` 二进制格式 v1.0

### 3.1 公共编码规则和文件头

所有整数 little-endian；浮点是 IEEE-754 binary64；禁止写入本机 struct、size_t、enum、指针、wchar_t。offset 是文件起点起算的 u64；段内索引为 u32，空索引用 `0xffffffff`（NONE），字符串空值合法索引 0。所有段起点 8 字节对齐，padding 必须为 0。数组范围以 checked arithmetic 验证；禁止段重叠、越界、环形组件/模型图；不允许压缩、网络 import 或执行任意代码。

头部固定前缀 128 字节：

| offset | 字节 | 字段及 v1 值 |
|---:|---:|---|
| 0 | 8 | magic：`4f 4e 45 56 49 45 57 00`，即 ONEVIEW + NUL |
| 8 | 2 | major=1 |
| 10 | 2 | minor=0 |
| 12 | 4 | header_bytes=128 |
| 16 | 8 | file_bytes，必须等于输入 len |
| 24 | 8 | directory_offset=128 |
| 32 | 4 | directory_count |
| 36 | 4 | flags，v1 必须 0 |
| 40 | 4 | schema_hash_algorithm=1（SHA-256） |
| 44 | 4 | root_template（TMPL 索引） |
| 48 | 32 | 完整 VM schema 的 SHA-256 |
| 80 | 32 | COMP 规范内容的 SHA-256（使用到的组件契约） |
| 112 | 4 | 最低 view protocol version=1 |
| 116 | 4 | root_model_id，与 SCHM 一致 |
| 120 | 8 | reserved=0 |

目录项 32 字节：`kind:u32@0, flags:u32@4, offset:u64@8, length:u64@16, count:u32@24, stride:u32@28`。flags bit0=required，其余 v1=0。kind 使用下面的固定数字，FourCC 仅供人读。每种 kind 只能出现一次；未用的核心段仍写 count=0。固定记录段 length=count*stride；STRS/SCHM/COMP 是变长段，stride=0。文件尾不允许未声明数据。

默认资源上限（可在未来提供受控加载选项，不能由文件自身抬高）：文件 64 MiB、字符串总字节 16 MiB、节点 100,000、单字符串 1 MiB、import/树/model 深度 64、每实例活列表项 100,000。超过返回 LIMIT；这不是“可信模板可绕过内存检查”的开关。

### 3.2 各段精确记录

表中字段按顺序紧密排列；u32 简写 U，u64 简写 Q，u16 简写 H。固定记录均无隐含 padding。引用其他段的 start/count 表示该段连续记录范围，不是字节 offset。

| kind / 段 | stride | 字节布局（顺序即偏移） |
|---|---:|---|
| 1 STRS | 0 | `offsets:Q[count+1]` 后接 UTF-8 blob；offset 相对 blob，offset[0]=0，末项=blob 字节数；索引 0 必为空串，不存 NUL 终止符 |
| 2 SCHM | 0 | 第 3.3 节的 canonical schema 原始字节；count=model 数 |
| 3 COMP | 0 | 第 3.3 节的组件需求规范字节；count=组件数 |
| 4 TMPL | 32 | `name_sid U, root_node U, model_id U, scope_id U, slot_start U, slot_count U, span U, flags U` |
| 5 NODE | 48 | `opcode H, flags H, component_index U, parent U, edge_start U, edge_count U, static_start U, static_count U, ref_sid U, scope_id U, aux U, span U, stable_id_sid U` |
| 6 EDGE | 8 | `parent_node U, child_node U`，同一 parent 按源顺序连续；树校验同时核对 NODE.parent |
| 7 VALU | 24 | `tag U, flags U, payload0 Q, payload1 Q`；静态 bool=1/i64=2/f64=3/utf8=4，flags=0；bool payload0 为 0/1，i64 为补码，f64 为位模式，utf8 payload0 为 SID，未使用 payload 必须 0 |
| 8 PROP | 16 | `node U, component_property_id U, value_index U, span U` |
| 9 BIND | 32 | `node U, component_property_id U, vm_property_id U, depth H, mode H, model_id U, flags U, span U, reserved U`；mode=1 单向/2 双向，flags/reserved=0 |
| 10 EVNT | 32 | `node U, component_event_id U, command_id U, depth H, flags H, model_id U, arg_start U, arg_count U, span U` |
| 11 EARG | 16 | `source_kind U, source_index U, type_tag U, span U`；kind=1 事件参数位置/2 VALU 索引，严格对应命令签名，不支持任意表达式 |
| 12 COND | 24 | `node U, property_or_value U, model_id U, depth H, mode H, span U, reserved U`；mode bit0=inverse、bit1=常量（指向 bool VALU），其他位 0；否则引用 bool property |
| 13 REPT | 32 | `node U, list_property_id U, parent_model U, depth H, flags H, item_model U, body_template U, key_property_id U, span U` |
| 14 CALL | 24 | `node U, template_index U, slot_start U, slot_count U, span U, reserved U`；v1 共享调用点 VM |
| 15 SLOT | 32 | `owner_index U, name_sid U, body_template U, flags U, capture_depth U, capture_model U, span U, reserved U` |
| 16 CSS | 24 | `scope_id U, original_text_sid U, lowered_text_sid U, source_file_sid U, span U, flags U` |
| 17 SMAP | 24 | `file_sid U, start_line U, start_column U, end_line U, end_column U, import_parent_span U` |
| 18 HOST | 16 | `node U, host_name_sid U, flags U, span U`，flags bit0=themed，其他 0 |

NODE opcode：1=COMPONENT，2=REPEAT，3=CALL，4=SLOT。普通节点 component_index 引 COMP；REPEAT/CALL/SLOT 取 NONE，aux 分别指 REPT/CALL/SLOT。COMPONENT.aux 是 HOST 索引（仅 NativeHost）或 NONE。无其他 opcode。flags=0，parent 是同模板内父 node，无父为 NONE。static_start/count 只指 PROP；BIND/EVNT/COND 等段按 node、源码序排序，加载一次建立反向索引。一个 node 至多一个 COND 和一个 ref，不允许 v-if+v-for 同节点。

TMPL 包括根、导入组件、repeat body 和 slot body；每个 template 的树互不共享 NODE。调用图必须无环。所有模板均可达，实例化图（不是裸 NODE 索引）确定控件唯一性。NODE.stable_id_sid 为显式 ref/新增 `view-id`，或编译器按相对文件+结构路径生成的 ID；显式 ID 模板内唯一，用于诊断/热重载，不是 VM schema 身份。

词法 depth=0 是当前 scope，1 是直接词法父 scope，以此类推；import CALL 不新建 VM 层，REPEAT 的 body 新建 item scope、父为发起 repeat 的 scope；slot body 在调用者捕获 scope 下实例化，fallback 在被调用模板当前 scope。TMPL.model_id 必须等于其实际 scope 的 schema model；BIND/EVNT/COND/REPT 中 model_id 必须等于沿 depth 找到的模型，否则 BAD_IR。这同时约束嵌套列表引用外层 item 和 slot 的捕获，不能靠运行时猜测。

SLOT.flags bit0=0 时为模板的 outlet 声明，owner_index 是 TMPL；body_template 是 fallback 或 NONE。bit0=1 时为 CALL 的供给，owner_index 是 CALL；body_template 是调用者内容，捕获调用者 VM lexical context。TMPL.slot_start/count 只指声明，CALL.slot_start/count 只指供给；NODE opcode SLOT 的 aux 指 outlet 声明。同名供给按原顺序合成一个 body，不重复消费同一个 widget。slot 运行时保留旧编译器的 Column 包装；未知供给名、重复 outlet 名编译时报错。该收紧与旧行为的差异纳入迁移诊断，不静默丢内容。

scope_id 引用 CSS 中出现的非零 scope 编号（同 scope 可多条 CSS），没有样式仍分配该模板的稳定 scope。生成类为 `one-s-` + SHA-256(规范相对路径、模板名) 前 16 hex；编译时检测截断碰撞，碰撞即报错。lowered_text 为经现有 `syntax::css` 消费校验和 scope 转换后的 UTF-8；加载仍验证支持的 CSS，不直接信任 blob。主题变量仍来自外部 :root，不新增浏览器继承语义。

SMAP 行列从 1 开始，列以 UTF-8 字节计，end 为开区间；file 是构建根相对 `/` 路径，不写绝对路径。源码 map 可选（flags.required=0），删除时所有 span 置 NONE，错误降级为 section/index/node ID；正式包推荐保留，性能测试也保留。其他 17 段 required=1。source span 必須覆盖属性/事件/key/CSS，不只标签。

### 3.3 哈希规范：输入、排序、影响范围

**schema 哈希用于契约一致性，不是模板文件校验或安全签名。** 修改 CSS、静态文字、节点结构不会改变 VM schema 哈希；修改属性/命令/列表项契约一定改变。loader 重新计算 SCHM/COMP 哈希，不信任头部声明；VM vtable 的 32 字节 schema_hash 必须与 SCHM 完全相等，所有 child vtable 也携带同一完整 schema 哈希和各自 model_id。

SCHM canonical 格式（不依赖 JSON key 顺序或空白，不引用 STRS）：

1. 前缀 8 字节 ASCII `OVSCHM1` + NUL，随后 `root_model_id:U, model_count:U`。
2. 模型按 UTF-8 名称字节序：`model_id:U, name:S, key_property_id:U, property_count:U, command_count:U`；S=`byte_len:U + UTF-8 原字节`，无 padding。名称限 ASCII 标识符/点分标识符，大小写敏感，不做 Unicode 归一化。
3. 属性按显式 ID 升序：`id:U, name:S, tag:U, access:U, item_model_id:U`。tag=1 bool/2 i64/3 f64/4 utf8/5 list；access=1 r/3 rw；非 list 的 item_model_id=0；key 必为只读 utf8。
4. 命令按 ID 升序：`id:U, name:S, result_tag:U(0=void), can_execute_id:U(0=缺省), running_id:U(0=缺省), argc:U`；再按参数顺序写 `name:S, tag:U, item_model_id:U`。v1 参数只允许四种标量，item_model_id=0；事件复杂记录扩展时需协议/组件契约新版本。
5. 所有声明模型均纳入，包括当前模板未引用但根契约可达的模型；不可达模型编译时报错。递归 list schema v1 拒绝；嵌套非递归 list 支持。model 名、key 字段、读写性、参数顺序/名/类型、命令门控关系、增删字段、显式 ID 变化均改变哈希。注释、JSON 排版和声明顺序不改变哈希。

COMP 前缀同样 8 字节 `OVCOMP1` + NUL，随后 count:U。按组件名 UTF-8 序写 `name:S, semantic_version:U, property_count:U, event_count:U`，属性按固定注册 ID 排序写 `id:U,name:S,tag:U,flags:U,record_model_name:S`，flags bit0 static/bit1 bind/bit2 model；事件写 `id:U,name:S,argc:U` 后接各参数 `name:S,tag:U`。record_model_name 仅 list 使用。末尾写 `child_rule:S, model_property_id:U`；child_rule 是注册表固定版本的规范结构规则串，不是执行脚本。固定值域（如 selectedIndex≥-1）、默认行为改变时必须提升 semantic_version。只纳入本 IR 使用组件的**完整**契约；普通新组件加入 SDK 不使旧 IR 失效。运行时逐项比较注册表，禁止仅比较一个全 SDK 哈希而拒绝所有旧文件。

注册属性/事件 ID 各自从 1 起、组件内唯一。S1 将当前每个组件的完整属性名（含通用属性）和事件名按 ASCII 序生成初始 ID 清单后作为版本化注册数据保存；此后追加非零 ID、废弃不复用，不再每次排序重编号。kind、组件索引、property ID、VM property ID 是不同命名空间，loader 必须分别查验。COMP 的顺序定义 component_index，不暴露 native Kind enum。

两份规范使用 SHA-256 全 32 字节，无截断、无 salt，生成 Rust/C++ 常量来自同一结果。S1 必须提交独立黄金字节/哈希用例，并证明调换 JSON 排序保持哈希、每类契约变化改变哈希；本稿不捏造尚未生成的样例 hash。

### 3.4 前向与后向兼容

- major 不同拒绝。相同 major 下，reader 支持旧 minor；新 minor 只有纯可选元信息才可被旧 reader 忽略。未知 required 段/record opcode/标志、已知段 stride 不匹配、未知 hash 算法、view protocol 超出支持范围均拒绝，绝不“尽量画一部分”。
- 新 minor 若需要改变已知记录语义，使用新 required 段及 capability，或升 major；不得加宽旧 record 后让旧 loader 错读。header_bytes≥128 且未知头尾全零才可忽略；非零扩展需 required capability 段，否则拒绝。
- VM schema 严格相等，不做隐式“兼容子集”。增加未使用字段也需重新生成 adapter/IR；UI/CSS-only 热重载不改 schema。需要兼容旧 VM 时由 viewc 为旧契约重新产 IR，不由 runtime 猜测。
- C ABI 43→**下一可用版本（基线预计 44）**，合并前核对主线是否已占用。IR 1.0、VTable protocol 1、组件 semantic_version 与 SDK ABI 分别演进。头/库/Rust/符号清单同批升级，老 ABI 43 SDK 不加载新 view 功能。

## 4. C 协议完整草案

推荐保留任务书的 `OneUiWidget*` 返回形状，view 是带 ViewControl 的根 widget handle；不把 VM 指针作为 widget。新增 `OneUiViewMount` 管理挂载目的地/窗口调度，`OneUiViewNotifier` 是可失效的 scope 通知令牌。下面拟放进 `include/oneui/oneui_view_c_api.h`，主 C 头包含它，ABI 检查器随之扩展到该头。所有函数采用既有 C 默认调用约定（Windows x86 为 cdecl），不使用编译器 enum/bool 作为 POD 字段。

```c
#ifndef ONEUI_VIEW_C_API_H
#define ONEUI_VIEW_C_API_H
#include "oneui/oneui_c_api.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define ONEUI_VIEW_PROTOCOL_VERSION 1u
#define ONEUI_VIEW_ALL_PROPERTIES 0u
typedef int32_t OneUiViewResult;
#define ONEUI_VIEW_OK                 0
#define ONEUI_VIEW_INVALID_ARGUMENT  -1
#define ONEUI_VIEW_WRONG_THREAD      -2
#define ONEUI_VIEW_BAD_IR            -3
#define ONEUI_VIEW_VERSION           -4
#define ONEUI_VIEW_SCHEMA_MISMATCH   -5
#define ONEUI_VIEW_UNKNOWN_ID        -6
#define ONEUI_VIEW_TYPE              -7
#define ONEUI_VIEW_READ_ONLY         -8
#define ONEUI_VIEW_CLOSED            -9
#define ONEUI_VIEW_REENTRANT        -10
#define ONEUI_VIEW_BAD_KEY          -11
#define ONEUI_VIEW_STALE_CHANGE     -12
#define ONEUI_VIEW_FOREIGN_FAILURE  -13
#define ONEUI_VIEW_OUT_OF_MEMORY    -14
#define ONEUI_VIEW_LIMIT            -15
#define ONEUI_VIEW_NOT_FOUND        -16
#define ONEUI_VIEW_BUSY             -17

typedef struct OneUiViewMount OneUiViewMount;
typedef struct OneUiViewNotifier OneUiViewNotifier;
typedef struct OneUiViewModelVTable OneUiViewModelVTable;
typedef void* OneUiViewList; /* language-owned opaque list lease */

#define ONEUI_VALUE_BOOL  1u
#define ONEUI_VALUE_I64   2u
#define ONEUI_VALUE_F64   3u
#define ONEUI_VALUE_UTF8  4u
#define ONEUI_VALUE_LIST  5u
#define ONEUI_VALUE_LEASED 1u
typedef struct OneUiValue {
    uint32_t tag;
    uint32_t flags;
    union {
        uint32_t boolean; /* exactly 0 or 1 */
        int64_t integer;
        double number;
        struct { const char* data; size_t length; } utf8;
        OneUiViewList list;
    } as;
    void* lease; /* only interpreted by producer.release_value */
} OneUiValue;

typedef struct OneUiViewModelRef {
    const OneUiViewModelVTable* vtable;
    void* vm;
} OneUiViewModelRef;

typedef struct OneUiViewListInfo {
    uint64_t revision;
    uint64_t length;
} OneUiViewListInfo;

#define ONEUI_LIST_RESET  0u
#define ONEUI_LIST_INSERT 1u
#define ONEUI_LIST_REMOVE 2u
#define ONEUI_LIST_MOVE   3u
#define ONEUI_LIST_UPDATE 4u
typedef struct OneUiViewListChange {
    uint32_t struct_size;
    uint32_t kind;
    uint64_t before_revision;
    uint64_t after_revision;
    uint64_t index;
    uint64_t count;
    uint64_t to_index; /* MOVE: index after removing the source range */
    const OneUiUtf8String* keys;
    size_t key_count;
    const uint32_t* properties; /* UPDATE: item property IDs; empty=all */
    size_t property_count;
} OneUiViewListChange;

struct OneUiViewModelVTable {
    uint32_t struct_size;
    uint32_t protocol_version;
    uint8_t schema_hash[32];
    uint32_t model_id;
    uint32_t flags; /* v1 zero */
    void (*retain)(void* vm);
    void (*release)(void* vm);
    OneUiViewResult (*connect)(void* vm, OneUiViewNotifier* notifier);
    void (*disconnect)(void* vm, OneUiViewNotifier* notifier);
    OneUiViewResult (*get)(void* vm, uint32_t property_id, OneUiValue* out);
    void (*release_value)(void* vm, OneUiValue* value);
    OneUiViewResult (*set)(void* vm, uint32_t property_id,
                         const OneUiValue* value);
    OneUiViewResult (*invoke)(void* vm, uint32_t command_id,
                            const OneUiValue* args, size_t argc);
    void (*list_retain)(void* vm, OneUiViewList list);
    void (*list_release)(void* vm, OneUiViewList list);
    OneUiViewResult (*list_info)(void* vm, OneUiViewList list,
                               OneUiViewListInfo* out);
    OneUiViewResult (*list_key)(void* vm, OneUiViewList list,
                              uint64_t index, OneUiValue* out_utf8);
    OneUiViewResult (*list_acquire)(void* vm, OneUiViewList list,
                                  uint64_t index, OneUiViewModelRef* out);
};

typedef struct OneUiViewError {
    uint32_t struct_size;
    OneUiViewResult result;
    uint64_t sequence;
    uint32_t section_kind;
    uint32_t record_index;
    uint32_t source_map_index;
    uint32_t reserved;
    char message[512]; /* owned inline UTF-8, NUL terminated */
} OneUiViewError;

ONEUI_API uint32_t oneui_view_protocol_version(void);
/* Borrow window/destination during call; see lifetime rules below. */
ONEUI_API OneUiViewMount* oneui_view_mount_create(
    OneUiWindow* window, OneUiWidget* destination);
ONEUI_API void oneui_view_mount_destroy(OneUiViewMount* mount);
/* density: 0 comfortable, 1 compact. Replaces external theme/CSS atomically. */
ONEUI_API OneUiViewResult oneui_view_mount_set_styles(
    OneUiViewMount* mount, OneUiUtf8String css, uint32_t density);

ONEUI_API OneUiWidget* oneui_view_instantiate(
    OneUiViewMount* mount, const uint8_t* ir_bytes, size_t len,
    const OneUiViewModelVTable* vtable, void* vm);
/* Pointer-returning functions expose the precise failure here, same thread. */
ONEUI_API OneUiViewResult oneui_view_last_result(void);
ONEUI_API OneUiViewResult oneui_view_notify(
    OneUiWidget* view, uint32_t property_id);
ONEUI_API OneUiViewResult oneui_view_notify_list(
    OneUiWidget* view, uint32_t property_id,
    const OneUiViewListChange* change);
ONEUI_API OneUiViewResult oneui_view_flush(OneUiWidget* view);
ONEUI_API OneUiViewResult oneui_view_disconnect(OneUiWidget* view);
/* Consumes the owned view handle; NULL permitted; UI thread only. */
ONEUI_API void oneui_view_destroy(OneUiWidget* view);
ONEUI_API OneUiViewResult oneui_view_get_error(
    OneUiWidget* view, OneUiViewError* out);

/* Returned notifier owns one reference. Null on failure. */
ONEUI_API OneUiViewNotifier* oneui_view_notifier(OneUiWidget* view);
ONEUI_API void oneui_view_notifier_retain(OneUiViewNotifier* notifier);
ONEUI_API void oneui_view_notifier_release(OneUiViewNotifier* notifier);
ONEUI_API OneUiViewResult oneui_view_notifier_notify(
    OneUiViewNotifier* notifier, uint32_t property_id);
ONEUI_API OneUiViewResult oneui_view_notifier_notify_list(
    OneUiViewNotifier* notifier, uint32_t property_id,
    const OneUiViewListChange* change);

/* Borrowed host handle. Do not call oneui_widget_destroy on it. */
ONEUI_API OneUiWidget* oneui_view_find_host(
    OneUiWidget* view, OneUiUtf8String name);
/* Owner lease retains the foreign widget's callback owner. */
ONEUI_API OneUiViewResult oneui_view_host_attach(
    OneUiWidget* host, OneUiWidget* content, void* owner,
    OneUiVoidCallback retain_owner, OneUiDestroyCallback release_owner);
ONEUI_API OneUiViewResult oneui_view_host_detach(OneUiWidget* host);
ONEUI_API OneUiViewResult oneui_view_reload(
    OneUiWidget* view, const uint8_t* ir_bytes, size_t len);

#ifdef __cplusplus
}
#endif
#endif
```

### 4.1 句柄、挂载和失败约定

`mount_create(window, destination)` 要求 UI 线程。destination=NULL 表示该 window 的空内容槽；非空必须是同窗口的空 Panel，runtime 只保留原生目的节点及窗口失效控制块，不持有调用方裸 C handle。每 mount 只容纳一个 active view；重复 instantiate 返回 BUSY。mount 关闭使 view 断开并从目的地拆除，但**不消费应用持有的 view handle**；该句柄之后仍可 get_error/destroy，其他操作 CLOSED。window 关闭先关闭其 mounts，不能等到已经销毁的 window handle 上再取消投递。

mount 初始使用现有浅色 Comfortable 声明式主题。mount_set_styles 复制并严格校验外部 theme/CSS，以“外部 CSS + 当前 IR scoped CSS”的顺序构造完整新样式表，成功才替换，density 同步原生几何；空 css 表示空外部规则，不暗中补另一主题。错误保持旧表，回调内调用 REENTRANT。应用入口可切全局主题/密度，L4 不取得逐控件样式权限。safe Rust 对应 `ViewMount::set_styles(&self, css: &str, density: Density) -> Result<(), ViewError>`；watcher 和用户主题切换共用此入口。

instantiate 先完整解析 IR、匹配组件/VM 契约，再建立未发布树、读取初值、连接 scope；全部成功后一次挂入目的地并请求布局。失败返回 NULL，目的地保持原样，平衡已获取的所有租约。输入 IR 在调用中复制为 runtime-owned 不可变 buffer；返回后调用方可释放。vtable 复制已知前缀到控制块，**其函数所在动态库仍必须活到所有 release 返回**。只要用户提供的 VM 指针/vtable 本身有效，schema 不匹配应在任何 VM 回调前返回。

`struct_size` 小于协议 v1 完整前缀返回 VERSION；较大只接受未知尾部为零。不能把 size 当成任意混用 ABI 的许可。必选回调 retain/release/connect/disconnect/get/release_value 非空；schema 有 rw、命令、列表时相应 set/invoke/全部 list 回调非空；无该能力可 NULL。未知 flags、ID、model_id 均明确拒绝。

`oneui_view_notify(view, 0)` 表示根 scope 全部属性脏；指定 ID 仅标记该属性，不提供新值，不修改 VM。list 属性用 notify 会按 RESET 处理；notify_list 必须指向 list 属性。notify 返回 OK 只保证排队，不能当作已绘制。notify 的子 scope 等价入口是 notifier_notify；列表项不用可变 index 来标识通知目标。

`oneui_view_last_result` 是线程局部最近一次 pointer-returning view API 的状态，成功写 OK，失败写具体负值；失败同时设置既有 `oneui_last_error()` 文本。status-returning API 以返回值为准，也更新错误文本；其成功不清除既有 last_error。`get_error` 拷贝实例最近一次异步错误及递增 sequence；无错误时 result=OK、sequence=0。错误信息含 section/record/span，禁止自动打印 VM 字符串值、凭证或文档内容。入参非法映射既有 InvalidArgument，OOM 映射 OutOfMemory，其他映射 OperationFailed；细分以 OneUiViewResult 为准。

本协议不能验证释放后的任意裸地址；销毁后再传 view 指针仍是调用方错误。跨生命周期只能持 notifier；它在 ViewControl 失效后返回 CLOSED，不能复活 VM。

### 4.2 OneUiValue 的内存与转换

- ABI 中 OneUiValue 使用平台 C 默认对齐，不等同磁盘 VALU；x86/x64/ARM 必须做 sizeof/align/offsetof 对照。flags 只有 0（调用中借用输入）或 LEASED（VM 返回）；所有未用 union 字节和 lease 初始为零。
- set/invoke 输入 flags=0，字符串/参数数组仅在回调内有效，VM 要持久保存必须复制。utf8 允许 length=0/data=NULL；非空必须有效 UTF-8，v1 拒绝内含 NUL。key 除此之外必须非空。runtime 不调用 release_value 释放输入。
- get/list_key 成功输出 flags=LEASED。其字符串/list 对象有效期延续至 runtime 调用该 VM 的 release_value，**不是仅到 get 返回**。VM 可把借用稳定缓冲或独立分配封装成 lease；runtime 先验证/复制字符串或取得列表租约，再恰好调用一次 release_value，即便后来类型校验失败也要释放。scalar 也走一次 no-op release_value，避免按标签遗漏资源。
- get/list_key 失败须保持 out 为零、未转移租约；适配器自行清理中间分配。release_value 必须不抛异常、不通知、不进入事件循环，返回后 runtime 不再读 value。runtime 绝不跨语言 free。
- get 返回 list 时，先 `list_retain(vm,list)`，再 release_value；缓存换成另一 list 或 scope 关闭时 list_release。每份持有独立配对，且 list_release 之前 vm 本身仍被租约保活。list 本身是稳定可查询的集合对象，不把 vector 地址/元素地址当句柄；结构版本由 list_info 描述。
- `list_acquire` 成功转移**一个**子 VM 租约（已 retain），runtime 不再额外 retain；out.vtable 必须符合该列表 item_model 及完整 schema hash。失败无租约。最终 `out.vtable->release(out.vm)` 配对。get 值借用、列表租约、子 VM 租约三者分开，不能靠一个含糊的 free 回调处理。

### 4.3 VTable 每个成员的语义

| 成员 | 调用时机与约束 |
|---|---|
| retain / release | 根 instantiate 接受后 retain 一次，最终 release 一次；child 使用 acquire 返回的租约。纯引用计数/销毁，不得失败或触发业务通知；所有 VM 释放在 UI 线程 |
| connect | 控件未发布时传入当前 scope 的借用 notifier；适配器若保存需 retain。用于 C++ 订阅/Rust 记录通知上下文，不读取/修改控件。允许标脏但不立即刷新。返回非 OK 导致创建回滚 |
| disconnect | 对每次**已进入**的 connect 恰好调用一次，包括 connect 失败；notifier 已标记 CLOSED。解除订阅、清 adapter 保存的 notifier；不得再调用业务方法 |
| get | 无副作用读取一个已声明属性；不轮询未脏属性。不能在其中 notify、set、invoke、flush、reload 或 pump 消息。返回 ID/类型/资源错误时保持旧属性值 |
| release_value | 对成功 get/list_key 的输出租约清理；不能依赖 View 仍 active |
| set | 只接受 schema rw 属性，通常由 v-model 输入触发；先更新 VM，再返回状态。OK 后 runtime 自动标该属性脏，以重新读取被规范化的实际值；派生/其他字段仍由 VM notify。拒绝时下轮恢复原 VM 值，错误显示由 VM 状态负责 |
| invoke | 只调注册事件对应的命令，检查 argc/类型和门控；不返回业务数据；业务可修改 VM 并 notify，异步命令立即返回，结果随后投递 |
| list_retain / list_release | 建立/解除 list 查询对象租约；不改变集合内容；不得调用 list 读取时已释放的 parent VM |
| list_info | 返回 length 和单调 revision；同一次提交的读取期间不能改变；revision 从 1 开始，不允许回绕，达到上限需新 list 对象 + RESET |
| list_key | 读取当前 index 的非空 UTF-8 稳定 key；以 OneUiValue 租约返回，不用 index 字符串代替业务身份 |
| list_acquire | 按当前 index 获取对应 child VM。对连续存在的 key，必须返回相同 vm/vtable 身份；成功即转移 +1 租约，移除后可被释放 |

没有 polling、subscribe 函数或 VM 主动回调 raw widget 的要求。多视图共享同一 VM 时 connect/disconnect 按 notifier 区分；VM adapter 为每个连接管理订阅，notify 广播到需要的连接，不能只保存“最后一个 view”。

### 4.4 NativeHost

instantiate 创建空 Host 占位，布局可存在，空时 naturalSize=0。`find_host(view,name)` 只查根实例域的静态 host 名，返回由 ViewControl 持有的借用 OneUiWidget；不存在 NOT_FOUND，已关闭 CLOSED。全展开后重名编译失败；v1 禁止 repeat 内 NativeHost（动态 pane host 能力留给 WP4 组件的独立契约，待决 D6）。不要把通用 ref 暴露成 L4 构建普通 UI 的后门。

host_attach 要求 content 未被挂载、owner 和 retain/release 均非空，先 retain owner，成功后原生树持 content；失败回退 retain。Rust owner 包含实际 `TerminalView` 等包装及回调分配。它仍拥有 content C handle，框架只保留原生对象。host_detach 先断开树的事件/invalidator/焦点和输入路由，再释放原生持有，最后 release_owner。每 host 至多一个 content，不隐式偷取或替换别人的 parent。

与 `include/oneui/ui_native_host.h:12–27` 的“构造即有内容”不同，空 host 是本方案明确扩展；measure/layout/theme 必须处理空状态。host_attach 在 invoke 内返回 REENTRANT，业务使用 dispatch_local 在回调后挂载。host handle 对 view 有效，但结构 reload 改掉该 host 后旧借用不能继续使用；safe Rust HostRef 绑定当前 View 的 generation，过期调用返回 CLOSED。删除已附着 host 的 reload 见第 8 节，不静默释放应用会话。

## 5. 线程、调度、重入和销毁

### 5.1 刷新事务

所有 VM、list、retain/release/connect/disconnect/host-owner 回调在所属 UI 线程。除 notifier_retain/release 可跨线程（它们只管理独立控制块，不释放 VM）外，本稿所有 view API 都限 UI 线程；查询 protocol_version/last_result 是无 UI 的例外。错误线程返回 WRONG_THREAD 且不触发 VM。void 销毁接口在错误线程记录错误且不释放，绑定必须预防这种使用。

后台工作只产生 owned 数据，通过既有 dispatcher 投递到 UI。在 UI 闭包内校验弱 scope 存活、修改 VM，再 notify。禁止从后台先改 UI 所有的 VM、随后仅投递 notify；这会让读取竞态。C 使用有效 window 的 post_owned 和保留 notifier；Rust 后台只发送结果到 UI mailbox，不能捕获 `Rc<Vm>` 进 Send 闭包，也不能把不 Send VM 包装成 unsafe Send。

```text
UI 输入 → set/invoke（callback depth +1）→ VM 状态更新 / notify 仅入队
       → 外层回调返回（depth=0）→ 合并 dirty(scope, property)
       → 下一个绘前提交：get 脏值 → 类型验证/差异比较 → 应用属性和树变更
       → Yoga（仅需要时）→ dirty-region paint → 已有后端提交
```

每 scope/property 每提交至多 get 一次；同属性多通知只保留 dirty 位。初次实例化读取所有被引用字段/门控。没有变化就不调用 setter、不额外 paint；设置 Input 相同文本保持光标/选区/IME。双向回写用“正在应用”抑制器，程序 setter 不能重新产生用户 set；但 VM 自己修正值后下一次读取必须应用。

`invoke` 里 notify 立即返回 OK，flush 不能在 callback depth>0 执行（REENTRANT）；状态在下一绘前提交，不承诺 invoke 返回时已经重绘。外部代码可在 UI 空闲时 `oneui_view_flush` 同步提交数据/树，用于测试和需要之后定位焦点的业务；flush 不运行消息循环、不等 GPU、不代表布局/绘制完成。

### 5.2 重入矩阵

| 当前回调 | notify | set/invoke 的嵌套 UI 事件 | flush/reload/attach | disconnect/destroy |
|---|---|---|---|---|
| get/list_info/list_key/list_acquire | REENTRANT，不入队 | REENTRANT | REENTRANT | 标 Closing，读完返回前不释放当前 VM |
| set/invoke | 允许入队 | 同 scope 拒绝；不自动重放点击 | REENTRANT | 立即封门，释放延后至外层回调退出 |
| connect | 允许入队 | 未发布树，无事件 | REENTRANT | 标 Closing，创建回滚 |
| disconnect/release/release_value/list_release | CLOSED 或 REENTRANT | 禁止 | 禁止 | 对已 Closing 幂等封门，不再进入用户回调 |

特别地，**在 get 里调用 notify 返回 REENTRANT，不能安排下一帧偷偷重试**，否则 getter 可能制造无限刷新。读 phase 标志是 view runtime 事务级的，跨 scope 也不能借 get 修改另一 scope。开发诊断记下这一错误；当前 get 若仍返回有效值可用于当前提交，但整体 flush 返回首个错误。嵌套 modal 消息循环对当前 scope 的可变事件拒绝；其他实例也不能插入正在进行的读/提交事务。没有“持 Rust RefCell 可变借用时重进 FFI”的路径。

### 5.3 销毁顺序与回调内自毁

ViewControl 状态为 Building → Active → Closing → Closed，generation 单调变化。树上所有事件桥、脏任务、dispatcher continuation 只持 weak control + generation，不捕获裸 vm/view。进入回调前临时持强 control/VM 租约并增加 active depth。

1. UI 上 disconnect 先将 control 置 Closing，使所有 notifier 立即 CLOSED，禁止新业务回调；清 dirty 和待执行事件。
2. 从目的地拆除 view；停止/断开树的输入、动画、焦点、invalidator 和事件桥。保留本次正在执行的回调栈，不提前释放它的 token。
3. active depth>0 时返回 OK（表示封门完成，**不表示 VM 已释放**），将剩余清理安排在该回调最外层 epilogue；无需消息循环继续运行。depth=0 则立即继续。
4. 对 child scopes 从叶到根 disconnect，清语言订阅；释放临时 value、子 VM 租约、list 租约，再根 VM release。parent VM 租约必须覆盖 list_release；已 connect 的失败创建同样走此清理。
5. NativeHost 内容先拆除再释放外语 owner；清样式/source 注册、控件和 control。仍由其他原生树引用的控件也只能是已脱离事件桥的对象。
6. oneui_view_destroy 最后消费根 C handle。建议同时让已有 oneui_widget_destroy 检测 view control 并调用同一关闭路径，防止按一般 Widget drop 释放时遗留 VM。这需要修改公共类布局时按任务书 clean 全量重编，当前设计阶段不执行。

正常外部 disconnect 返回后，不再有 get/set/invoke/list 查询/新 connect；只可能存在回调内自毁所约定的当前回调和其 cleanup。该回调返回后 release 是最后一个 VM 回调；release 返回后 native 不再持 VM 指针。调用方不能在 disconnect 返回后自行 free VM，因为回调内自毁可能尚未完成；**统一由租约 release 决定回收**。

根 vm 参数是调用方的借用引用；运行时经 retain 持有自己的引用，不取得调用方原有所有权。纯 C 可用引用计数 Box，Rust 用 adapter 的 Rc。只有遵守配对释放的适配器才是安全绑定；不接受“用户保证裸 vm 活够久”作为 safe Rust 的底层契约。

## 6. 稳定列表与增量算法

### 6.1 变更通知定义

集合修改必须先完成，再发 notify_list。每次 mutation（包括 UPDATE）使 revision 严格递增；before/after 表示本次变化两端，after>before。runtime 同步深拷贝 change 的 keys/properties，不保留调用方数组。notify 阶段不调用 VM，因此可在 invoke 内安全使用。

| kind | index/count/to_index、keys | 语义 |
|---|---|---|
| RESET | index/count/to_index=0；keys/properties 为空；before 可为 0，after 为新版本 | 从最新 list_info/key 建快照并按 key reconcile；不是销毁所有节点 |
| INSERT | index 为插入前边界，count>0；keys 长度=count，是新项顺序；to_index=0 | 新 key 不得存在；最终从 list_acquire 获取新增项 |
| REMOVE | index/count 为移除前范围；keys 必须等于该旧范围；to_index=0 | 从缓存按旧 key 拆除，**不回调已不存在 index 的 child** |
| MOVE | index/count 为旧区间，to_index 是移走区间之后的目标边界；keys 为源区间；properties 为空 | 例如 `[A,B,C,D]` 的 index=1,count=2,to=2 → `[A,D,B,C]`；to=1 为 no-op，不换 VM/控件 |
| UPDATE | index/count 为当前范围；keys 对应该范围；properties 为 item 字段 ID（空表示全部），to_index=0 | key 与 item 身份不变，只标这些 child 属性脏，不重建条目 |

所有增量的 key_count=count，properties 仅 UPDATE 可非空。单条 index/count 上界在 flush 按版本序列校验，直接可判的参数错误在 notify 时拒绝。root notify(list_id) 相当于未给精确 delta 的 RESET；list 对象换新必须 RESET，不能偷偷发送 INSERT。业务不能遗漏通知；调试模式可额外全量核对，release 不逐帧扫列表。

单 list 在一次提交前最多缓存 4,096 条 delta 或 1 MiB key/字段数据，超过后折叠为一次 RESET dirty，释放已复制 delta，不丢最终状态；单次 change 超过文件/字符串/项数限制直接 LIMIT。折叠后同帧 REMOVE+INSERT 的身份历史必须保留为有界的 retired-key 集合；若集合也达到上限，则明确返回 LIMIT 并要求调用方先提交，不允许因折叠而错误复用已退休 scope。

### 6.2 提交算法与失败原子性

1. 首次 get(list) 取得 list 租约，list_info 得 `(revision,n)`，按 index 读取 key，构建 `ordered_keys` 和 `key→RowControl`。只对新 key acquire 子 VM、connect notifier、构建 body；重复/空 key 在发布前 BAD_KEY。
2. 每帧对该 list 的已复制 delta 以 arrival 顺序检查 revision 链，从上次 applied revision 起在**暂存的 key 序列**上执行 INSERT/REMOVE/MOVE/UPDATE。keys 同时校验旧范围和新增唯一性；积累 dirty 子字段、移除集合和新增集合。不立即改真实树。
3. 最后调用 list_info。若 revision/length 与 delta 链终点一致，则只对最终新增 key 的最终 index 做 list_key 校验和 acquire，复用仍存活 key 的 RowControl。已存在 child 的 UPDATE 读取其属性；不每次重新 acquire 全部子 VM。
4. revision 链断裂、VM 已进一步变化、普通 notify 或出现 RESET 时，放弃 delta 暂存，重新读取**当前**完整 key 快照，按 key reconcile。记录 STALE_CHANGE 诊断，成功恢复后 flush 返回 OK；它是可观察的恢复事件而非错误数据静默应用。非法/重复 key、acquire 失败则保持旧树，flush 返回对应错误，等待新的 notify 修正，不无限安排重试帧。
5. 全部读取、类型检查、组件创建、容量预留成功后一次 commit：移除旧 scope、调整父 children/flex、连接新节点并应用 dirty 值。commit 阶段不再调可失败的业务 get/acquire；结构暂存失败不得破坏可见旧树。现有 `reconcileChildren` 若不能提供所需异常保证，S2 必须补一个预分配的 no-throw commit 边界并测试 OOM。
6. 移除 scope 立即失效 notifier，再断事件、释放 child；MOVE 不 connect/disconnect、不改焦点链。先保留 key 的 selection/focus/scroll anchor，再恢复仍存在 key；删除焦点项由组件默认策略移交。业务选择 ID 不随 index 移位。

连续存在的 key 必须是同一 child 对象。RESET 可以换 list 容器但不能以同 key 换 VM 身份；需要换对象要发 REMOVE+INSERT，通知链中即使同一帧重新插入相同 key也记为新的 generation，原 scope 必须退休。对 RESET 没有可靠移除历史，若 acquire 校验发现同 key 不同 VM则 BAD_KEY，不猜测替换。UPDATE 禁止修改 key 属性；key 变化是 REMOVE+INSERT。

RESET 路径对保留 key 也逐项 acquire 一次以验证 vm/vtable 身份，匹配后立即 release 这次额外租约，不重复 connect；新 key 的租约转给新 RowControl。精确 delta 路径的保留项不重新 acquire，身份稳定是 provider 的责任。验证所用 vtable 身份是原始 provider 的稳定表身份（以及协议/hash/model），不是 runtime 复制前缀的内部地址。

复杂度：普通属性更新 O(该属性依赖数)；结构暂存/数组 children 重排可为 O(n+k)，新增/删除的**控件和 VM 工作量**为 O(k)，不是承诺所有 MOVE 都 O(1)。v-for 当前是实体控件列表，不承诺百万条虚拟化；DataTable/VirtualList 使用它们的数据适配能力。表格结构改动可沿用一次 setRows，字段 UPDATE 用 updateRow；不能将此称为底层零复制插入。

集合快照与 get/list 回调在同一 UI 事务中读取，期间禁止消息循环和 VM mutation；异步更新已排队也必须等该事务结束。嵌套 list 递归使用相同 scope/租约规则，深度受限。WP3 所需 child 重排以现有 reconcileChildren 为基础，WP4 的公共 Stack insert/remove 不作为 WP3 阻塞依赖；二者后续共用同一内部树操作。

## 7. C++ 与 Rust 的统一入口

### 7.1 C++ 迁移：同一 IR，适配现有 State

推荐正式交付时只有一个 runtime。`--emit cpp` 生成：IR byte array、schema hash/ID、State/Computed/VmCommand 的 C VTable adapter、`build_X(vm,ui)` 兼容包装、styles/sources 元信息；**不再生成逐节点 ui.make/bind 的执行代码**。adapter 的 get 读取 State/Computed，set 写 State，invoke 调命令；connect 保存作用域订阅，将旧 State 的变化转成 notifier dirty，disconnect 清订阅。legacy float/int/wstring 在 adapter 里显式转换。列表 adapter 为 stable shared item 保持租约；不得把 C++ shared_ptr 的对象布局传到 C。

`build_X` 的返回 Element 仍可供旧作者 API 使用，但其底层来自 oneui_view_instantiate。SDK 内部为 `ui::Mount` 提供 C mount 的互操作入口：分享该 Mount 的组件服务、StyleRegistry 和 schedule，子 view control 由 Mount 持有，返回根 native widget 包成 Element。普通 C/Rust 消费者看不到这些 C++ 类型。已有 `build_X(VM&, Mount&)` 的 vm 必须比 Mount 活得久的约束保留；推荐新增共享所有权 C++ 接入采用 `shared_ptr<VM>`。兼容 adapter 自身 retain/release，但不能凭空延长借用 VM 的寿命；它必须在 Mount teardown 前断订阅，提供文档和生命周期测试。

旧 Slots 函数对象无法序列化。`.one` import/slot 全部编译进 IR；外部 C++ `Slots` 回调入口属于兼容问题（待决 D3）：推荐正式新入口只接受模板插槽，旧动态 Slots 调用在迁移期明确编译错误并给出替代模板，不能把 native 创建 lambda 藏进语言无关 IR。确需外语提供画面只走 NativeHost，不提供“动态普通控件插槽”绕过分层。

分两步切换，且旧执行路径仅作短期测试 oracle：

1. S1–S4 保留旧 emitter，冻结为 `--emit cpp-legacy`，同时新 emitter 输出 IR；全部同源 fixtures 在旧 C++ 路径与 IR C++ 路径运行。对照组件逻辑树、实际原生子树、默认值、事件 trace、输入回写、列表身份、焦点/IME/scroll、样式命令；只比较 JSON 形状不够。
2. S5 差分通过后切换 CMake 默认，移除旧逐节点 emitter 和生产开关；永久保留固定黄金语义用例及 C++/Rust 驱动同 IR 的一致性测试。无需长期维护两套 emitter；单次历史差分不能代替未来回归。

同一 `.one` 的 CPP/Rust emit 必须引用完全相同 SHA-256 的 `.oneview`。CSS lowering 和默认控件属性来自共享注册/组件服务，不能在 Rust 生成器再次实现。C++ 手写 Compose 可以继续存在作为 SDK 入口，但不作为 iShell L4 手工页面构造的豁免。

### 7.2 viewc 命令与 Rust 产物

拟定命令（均为未来接口）：

```text
oneui-viewc --input SettingsBasic.one --schema SettingsBasic.one.schema.json \
  --name SettingsBasic --emit ir --output SettingsBasic.oneview --depfile SettingsBasic.d
oneui-viewc --input SettingsBasic.one --schema SettingsBasic.one.schema.json \
  --name SettingsBasic --emit rust --output settings_basic.g.rs --ir-output SettingsBasic.oneview
```

rust emit 产生类型安全的 VM trait、属性 ID newtype/常量、命令 enum、child trait、只读/可写区分、统一 IR/schema 常量引用和 adapter 描述。不生成 Button/Stack 构造代码，不生成 C++ 文件，不把 `.one` 转成 Rust 的另一套 UI。编译器可独立使用预编译 host 工具，Cargo build.rs 只运行 viewc/跟踪 depfile/包含产物，不因改模板重建整个 OneUI；交叉编译工具使用 host target，IR 平台无关。

safe Rust 的确切公共签名建议为：

```rust
pub struct View { /* erased native control owner; !Send + !Sync */ }
pub struct ViewMount { /* window lifetime token; !Send + !Sync */ }
pub struct ViewSpec<Vm> { /* private fields: IR + generated typed adapter */ }

impl View {
    pub fn instantiate<Vm: 'static>(
        mount: &ViewMount,
        spec: ViewSpec<Vm>,
        vm: std::rc::Rc<std::cell::RefCell<Vm>>,
    ) -> Result<Self, ViewError>;
    pub fn notifier(&self) -> ViewNotifier; // UI-only, weak control semantics
    pub fn flush(&self) -> Result<(), ViewError>;
    pub fn reload(&mut self, ir: &[u8]) -> Result<(), ViewError>;
    pub fn find_host(&self, name: &str) -> Result<HostRef<'_>, ViewError>;
    pub fn disconnect(&mut self) -> Result<(), ViewError>;
}
```

spec 参数解决一个 VM 实现多个页面 trait 时的选择/blanket impl 冲突。`ViewSpec<Vm>` 只能由公共安全的生成辅助宏构造：宏接收经类型检查的字段/命令函数和同源常量，内部 FFI 由 oneui crate 实现；手写原始适配器入口标 unsafe，不让应用伪造安全 descriptor。返回 View 不暴露可以独立销毁的 owning Widget；root 挂载由 mount 管理。

下列是**设计样例，非本次已生成/编译结果**。对应第 2.1 节 schema，展示基础页的语言/密度、12 项主题列表、状态和恢复默认操作。它与 iShell 设置结构同类，但不是完整基础页的像素复刻；S6 还需按后面的验收矩阵补全。

```xml
<template view-model="SettingsBasic">
  <SettingsPage title="设置 → 基础">
    <Section title="界面">
      <FormRow label="语言"><Select :items="languageOptions" v-model="language" /></FormRow>
      <FormRow label="紧凑密度"><Switch v-model="compact" /></FormRow>
    </Section>
    <Section title="主题">
      <Column v-for="theme in themes" :key="theme.id">
        <Button :text="theme.title" @click="theme.select" />
        <Text v-if="theme.selected" text="当前主题" />
      </Column>
    </Section>
    <ActionBar><Status :text="status" /><Button @click="reset">恢复默认</Button></ActionBar>
  </SettingsPage>
</template>
```

为使这个模板闭合，在第 2.1 节根模型中增加 `id=5,name=languageOptions,type=list<oneui.v1.StringItem>,access=r` 及 StringItem 内置模型声明。这是契约变化，必须重算 hash；生成器不能仅依靠 template 推导第 5 项而令 VM 漏实现。

拟生成的 Rust API 示意（`define_view_adapter!` 是计划在绑定内提供的生成辅助宏；宏展开负责完整 trampoline，不是让应用填写 TODO）：

```rust
// settings_basic.g.rs -- generated; do not edit
use oneui::view::{PropertyId, StableList, StringItem, ViewSpec, VmContext, VmResult};

pub const LANGUAGE: PropertyId = PropertyId::new(1);
pub const COMPACT: PropertyId = PropertyId::new(2);
pub const STATUS: PropertyId = PropertyId::new(3);
pub const THEMES: PropertyId = PropertyId::new(4);
pub const LANGUAGE_OPTIONS: PropertyId = PropertyId::new(5);
pub const IR: &[u8] = include_bytes!(concat!(env!("OUT_DIR"), "/SettingsBasic.oneview"));
// 文件由同次 viewc 生成，内容是完整的 [u8; 32] Rust 表达式。
pub const SCHEMA_HASH: [u8; 32] = include!(concat!(env!("OUT_DIR"), "/SettingsBasic.schema_hash.rs"));

#[derive(Clone, Copy, Debug)]
pub enum SettingsBasicCommand { Reset }
#[derive(Clone, Copy, Debug)]
pub enum ThemeItemCommand { Select }

pub trait ThemeItemVm: 'static {
    fn id(&self) -> &str;
    fn title(&self) -> &str;
    fn selected(&self) -> bool;
    fn invoke(&mut self, command: ThemeItemCommand, cx: &VmContext) -> VmResult;
}
pub trait SettingsBasicVm: 'static {
    type Theme: ThemeItemVm;
    fn language(&self) -> i64;
    fn set_language(&mut self, value: i64, cx: &VmContext) -> VmResult;
    fn compact(&self) -> bool;
    fn set_compact(&mut self, value: bool, cx: &VmContext) -> VmResult;
    fn status(&self) -> &str;
    fn themes(&self) -> &StableList<Self::Theme>;
    fn language_options(&self) -> &StableList<StringItem>;
    fn invoke(&mut self, command: SettingsBasicCommand, cx: &VmContext) -> VmResult;
}

oneui::view::define_view_adapter! {
    pub fn spec<V: SettingsBasicVm>() -> ViewSpec<V>;
    ir = IR; schema = SCHEMA_HASH; model = "SettingsBasic";
    read_write LANGUAGE: i64 => (V::language, V::set_language);
    read_write COMPACT: bool => (V::compact, V::set_compact);
    read_only STATUS: utf8 => V::status;
    list THEMES: V::Theme => V::themes;
    list LANGUAGE_OPTIONS: StringItem => V::language_options;
    command 1: SettingsBasicCommand::Reset => V::invoke;
    child ThemeItem: ThemeItemVm {
        key 1 => id;
        read_only 2: utf8 => title;
        read_only 3: bool => selected;
        command 1: ThemeItemCommand::Select => invoke;
    }
}
```

生成宏从同份 schema descriptor 取得 model 编号、内置模型和命令参数信息；不得按 Rust trait 名自行计算第二个 schema 哈希。若命令有参数，如 Tabs.close，则生成 `enum Commands { Close { key: String } }`，解析 argc/类型后才进入业务。字串 getters 的 &str 在退出 RefCell 借用前复制到 adapter 的可复用值租约缓冲，直到 release_value 才可重用；不能把 Ref<'_,Vm> 或 VM 内存借用带出外语回调。

应用入口示意（所有 Widget 构造均在 runtime；VmImpl 按生成 trait 实现状态/业务）：

```rust
let vm = Rc::new(RefCell::new(VmImpl::from_settings(snapshot)));
let view = View::instantiate::<VmImpl>(&mount, settings_basic::spec::<VmImpl>(), vm.clone())?;
// UI 线程上的外部状态结果处理；get/set/invoke 回调内不要再 borrow_mut 同一 vm。
vm.borrow_mut().status = "设置已同步".to_owned();
view.notifier().notify(settings_basic::STATUS)?;
// 窗口或页面 owner 持有 view；drop(view) 断开并释放适配器的 Rc 租约。
```

适配器 Box/Rc 持 VM、各连接弱通知令牌、可复用 UTF-8 lease、list/child adapters。每个 extern C trampoline 先持本地 Rc 再进入业务，try_borrow/try_borrow_mut 失败返回 REENTRANT；catch_unwind 转 FOREIGN_FAILURE，不跨 FFI unwind。retain/release/connect/disconnect 的 panic 同样隔离；cleanup panic 必须保证控制块不再使用该 VM，记录错误，不能带着失效指针继续运行。编译为 panic=abort 时进程 abort 不能被 catch，绑定文档不声称可恢复。

`View::drop` 在 UI 上失效其共享状态，调用 view_destroy，C teardown 的 release 释放 adapter 持有的 Rc；应用手里的 Rc 可继续保存业务状态。回调内 drop 由第 5.3 节的 active lease 延迟回收；不得 drop View 后再手工 Box::from_raw 第二次释放。mount/window 可以先关闭而 View 后 drop，后者只收尾失效 handle。`HostRef` 借用 + generation 检查，NativeContentOwner 强持外语控件包装，View drop 时先拆 host 再释放 owner。

`StableList<T>` 是绑定层的 UI 线程 keyed 容器，不是业务集合框架：维护 key→Rc<RefCell<T>>、顺序、revision，提供 insert/remove/move/update 的变更记录；同 key 换 Rc 被拒绝。业务可以写自己的 list provider，但安全 adapter 要求它满足同一稳定身份接口。子 VM 的 VmContext 来自它自己的 notifier，更新 title 不必把整份 themes 标 RESET。

## 8. 开发热重载与 CSS

开发 watcher 沿用 200ms 稳定观察策略；文件读取/编译在后台，owned IR bytes 经 dispatcher 送回 UI。开发工具用临时文件写完后 rename 发布；runtime 仍须防截断/版本不匹配。release 默认不启 watcher；reload API 也进行完整校验，不能只在 debug 检查。

`oneui_view_reload` 仅在 callback depth=0、无提交事务时执行；否则 BUSY，调用方稍后通过 dispatcher 再试，不在当前回调中递归重载。步骤：

1. 解析新 IR、比对完整 VM schema hash/组件需求、scoped CSS；失败旧树/样式/订阅均保持。schema 不同返回 SCHEMA_MISMATCH，VM 状态转换需要应用重启或显式另建新 VM，不做隐式迁移。
2. 在未发布的计划中按 `(template instance path, stable_id, component contract, list key path, lexical capture)` 匹配旧节点。匹配的节点保留原生对象；新增节点离屏建立，删除节点只记录。所有必要 get/新 child acquire/connect 完成后才进入 commit。
3. 比较新旧静态属性集合：删除的属性恢复组件默认值；绑定/事件先准备新边表，不能只加不删。样式原子换表；一个字段从绑定变静态按新定义覆盖，写入前检查类型/值域。
4. commit 不调用业务代码，一次交换树关系、依赖索引/事件 generation 和 stylesheet；旧 generation 的事件任务丢弃。保留 scope 的 notifier **不失效**，VM 身份未变；新增 scope connect，移除 scope disconnect。事务期间已排队 notify 在提交后读当前 VM，不因 reload 丢状态。
5. 兼容的 Input/Scroll/Tabs 等保持焦点、选区、滚动/IME；删除或换类型的节点不承诺控件局部状态保留，焦点按现有规则回退。若会移除/替换正在 IME composition 的节点，返回 BUSY，等 composition 结束再重载，不强制提交用户输入。

VM 是状态真源，reload 从不重置或反序列化覆盖 VM。结构插入使自动生成结构路径 ID 变化时，相关控件可能重建，故需要保留编辑状态的节点推荐显式 view-id；源码行号不作为身份。

**NativeHost 的保守事务边界（待决 D4）**：已附着内容的 host 必须以相同名字、component 和实例路径保留；若新 IR 删除/改名/跨 scope 移动该 host，返回 BUSY 并保留旧页，要求应用显式 detach 后重试。允许 host 外围布局节点变化，commit 才重新接入同一 host；不能在预构建时把其 content 同时挂两棵树。这避免热重载无声销毁终端/编辑器会话。

CSS-only 更新无需重建 IR 树，继续走现有 StyleRegistry 原子 replace、错误回滚、删除声明恢复默认；本稿不另造 CSS 运行时。独立 CSS 热更新也受同一事务边界保护。实现不得以整页销毁重建冒充全部状态保留。

## 9. 性能预算与验收方法

±10% 是 WP3 的**验收目标，当前没有测量结果或性能保证**。可实现的依据是热路径不解释 AST/字符串、不反射查找、不重建未变控件；IR 解析和 UTF-8 名称解析只在加载时做，运行时为 property ID→依赖边、脏位合并、一次 get 和相同原生 setter。Rust 手写路径也经过 C ABI、同一控件/Yoga/渲染，差异主要应限于通知和类型适配。

建议额外调度/适配成本预算（必须用 S6 数据证明，不以预算冒充实测）：

| 阶段 | 目标 |
|---|---|
| notify、dirty 合并、边查找 | 总额外时延≤手写基线 2%；热态 scalar notify 不分配 |
| get、类型验证、UTF-8 缓冲复用 | ≤基线 3%；每个脏值一次读取，同值不 setter |
| adapter 属性应用/额外调度 | ≤基线 3%；最多一个待处理 frame 请求 |
| 余量 | 2%，处理统计噪声/必要生命周期校验，不用于掩盖不同工作量 |

比较口径：

1. 同一 release 测试 exe/DLL、同 VM 初值、CSS/字体/DPI/主题/窗口尺寸、相同输入序列；关闭 watcher、追踪、采样动画。手写 Rust 参考页属于 OneUI 测试夹具，不进入 iShell 产品 L4。
2. t0 为 UI 线程状态修改前；t1 为携带该 update sequence 的首次实际重绘结束/后端提交完成。异步调度等待包含在 t1−t0，不能只测 notify 函数或 get。若能获取 presentation 完成另报，不把 CPU paint 等同屏幕呈现；已提供平台 frame marker 的口径两路径保持一致。
3. 固定 UI event→VM mutation、dispatcher→UI、单 scalar、文本、12 主题选择、list 1,000 项中单项 UPDATE/批量 INSERT/REMOVE/MOVE 六类用例。确定时钟/光标/采样/等待条件，不能靠固定“睡几帧”。
4. 预热 50 次，每组 1,000 次，至少 5 个交替顺序配对轮次，保存原始样本/环境/二进制及 IR hash；报告 median、P95、空闲额外帧、FFI/get/set 次数、控件/租约峰值及稳定后数量。冷加载/首帧单列，不混入热更新。
5. 以配对轮次聚合 median 和 P95 的 `(IR−manual)/manual` 落在 [-10%,+10%] 为门槛；显著快于 -10% 也先核对工作是否等价再由 Claude 确认，不私自放宽验收。极短基线接近计时分辨率时先增加样本/成批测量并保留端到端结果，不能改用更慢页面稀释开销（待决 D5）。

列表批量变化和图表数据复制可能达不到标量预算，须逐类报告，不能只挑 paint 主导的大页面宣称全部通过。若超过预算，先减少重复 get/转换/申请、预编译 setter 分发表；不删除 schema 校验或关闭生命周期保护。不要在本阶段承诺零拷贝集合；必要的 bulk value 扩展需单独评审协议。

## 10. Go / C# 绑定要点

两者共享 `.oneview`、schema/组件验证、C vtable；不得重新实现控件工厂或解释 `.one`。应提供与 Rust 相同的 typed schema generator、UI scope、错误值、列表租约和显式 Close/Dispose。v1 只交文档契约，不把新增完整 Go/C# 包列作本次实现交付。

- Go：不要让 C 长期保存含 Go 指针的 VM/字符串地址。用 `runtime/cgo.Handle` 保活 Go adapter，C 分配的小 context 存 uintptr_t handle，再把 context 指针作为 vm；release 在 UI 上 Delete handle 并释放 C context。跨回调存活的值租约可复制到 C 分配缓冲，release_value 配对释放；callback shim 属于 OneUI Go 绑定包，应用仓库无需 C++。显式 Close 经 UI dispatcher 完成；finalizer 只能兜底安排关闭，不能直接调用 UI API。handle 的有效期/删除规则参见 [Go runtime/cgo.Handle 官方说明](https://pkg.go.dev/runtime/cgo#Handle)。
- C#：用 GCHandle 保活 adapter，静态 Cdecl trampoline 或被强引用的 delegate 与 vtable 同寿命；不能只保存临时 delegate 地址。UTF-8 明确编码并保持租约缓冲直到 release_value；长度映射 nuint，i64 映射 long，bool 用 uint 的 0/1。用 IDisposable 在 UI 线程关闭，SafeHandle/finalizer 的线程不能直接释放 UI VM，应先安排 UI 关闭后才回收纯句柄资源。布局/指针/回调寿命按 [Microsoft 原生互操作实践](https://learn.microsoft.com/en-us/dotnet/standard/native-interop/best-practices) 核查，异常转换为 FOREIGN_FAILURE。
- 两种绑定都必须测试 callback 内关闭、窗口先关、队列取消、重复 key、失效 notifier、UTF-8 非 ASCII、x86/x64 struct offset。保活与 pinning 不是一回事；本协议不依赖 GC 地址长期固定。Go goroutine/C# Task 的后台结果只通过 UI 投递修改 VM。

## 11. 实现步骤、文件边界与验收产物

每步独立提交并可编译；仅在 Claude 通过本稿后执行。正式交付的最终门槛是 S1–S6 全部通过，阶段性“完成 IR”不能替代 Rust 示例/性能和现有声明式回归。预计文件名可随代码职责微调，不允许把全部 runtime 写进一个 view.cpp。

| 步骤 | 交付与预计文件 | 可独立验收的测试 |
|---|---|---|
| **S1 契约与编译器** | `tools/viewc/{ast,parser,schema,validate,ir_writer,emit_rust,emit_cpp}.{h,cpp}` 按实际职责拆分；`main.cpp` 只 CLI；`include/oneui/view/{ir_format,component_contract}.h`；共享注册表；JSON schema 格式和黄金 `.oneview` | 旧 parser 正反例；独立 VM 未知字段/类型/可写性/事件参数/key/import/slot/CSS 诊断；规范 hash、跨路径可重复字节、每段 roundtrip、损坏文件/溢出/未知必需段/版本/循环测试；独立 golden bytes 避免 writer/reader 同错 |
| **S2 核心 runtime** | `src/view/{ir_reader,component_adapter,instance,binding,scope,keyed_list,style_scope}.cpp` 及私有头；从 `ui_declarative.h` 抽组件工厂/属性应用共享服务；不新增布局引擎 | 以 C++ mock VTable 驱动属性/双向绑定/命令门控、if/else 保留、嵌套列表、slot 捕获；keyed 增删改移/同 key 换对象拒绝、选择焦点、失败事务/OOM、无效 notify、无空闲额外帧 |
| **S3 C ABI 与生命周期** | `include/oneui/oneui_view_c_api.h`、主 C 头接入；`src/capi/oneui_c_api_view.cpp` + `oneui_c_api_view_lifetime.cpp`；widget/window 控制块接入；`tests/view_c_api_tests.cpp` | 真 C 消费者、不匹配 schema 零 VM 调用、每个租约的成功/失败配对、get 内 notify、invoke 内 notify/self-destroy、关闭窗口取消、错误线程、list 删除后 child 通知、host owner 生命周期；ABI POD/导出/版本检查 |
| **S4 Rust 绑定** | `oneui-sys/src/ffi/view.rs` 与类型/符号清单；`oneui/src/view/{mod,model,value,list,notifier,host}.rs`；生成宏/辅助描述；`oneui/tests/view_*.rs` | trait 类型/命令参数 compile-fail；!Send/!Sync；panic/重入/lease/drop 次数；双视图共享 VM；异步结果在 View drop 后被拒；真正 Rust VM 无 C++ 胶水创建页面、child command 路由 |
| **S5 C++ 统一与开发重载** | `include/oneui/view/cpp_adapter.h`；`cmake/OneUIView.cmake`；旧示例 schema 与新生成入口；`src/view/reload.cpp`、开发 watcher 接入；移除 cpp-legacy emitter | 旧/IR C++ 的控件树及绘制/交互差分，再测 IR C++/Rust；原有 declarative runtime/compiler 全回归；热 reload 增删节点/属性删除/插槽捕获/出错回滚/IME BUSY/host BUSY；50 次重载无 lease/订阅增长 |
| **S6 设置示例与性能交付** | `examples/language_neutral_views/` 下 `.one`/CSS/schema、Rust VM、测试专用手写参考；`tests/view_performance*`；固定 `docs/benchmarks/language-neutral-views/` 报告 | 基础设置示例 12 主题、语言/标题栏/密度/关闭动作/启动项、字体/背景及状态/错误、异步结果；两主题/宽窄窗口、输入及列表动作、真实截图；release ±10% 测量与所有现有声明式/ABI/Rust 测试；59 项基线既有审计失败单列 |

S6 示例是本地模拟设置模型，禁止真实写用户配置或网络；操作行为/模板能力与产品基础页同构。标题栏风格/背景/字体和主题画廊等若现有注册组件表达不足，必须在 S1 能力清单中逐项暴露并由 Claude 确定补齐边界，不能用普通 UI NativeHost 偷渡，也不能只交上面四属性示例宣称完整同构。iShell 真实 CSS/持久化/12 主题像素迁移仍属 WP5-B1，示例不是产品验收替代品。

预计还需同步：`CMakeLists.txt` 的 viewc/runtime/tests 接入、`scripts/check-abi-sync.{cmake,ps1}`、`bindings/rust/oneui-sys/supported-symbols.txt`、C ABI/Rust 版本、`docs/c-abi-integration.md`、docs/35/58 的入口说明。全部列为后续，不在本次修改。

模块约束：新手写源码目标≤500 行，超过 500 审查职责，超过 1,000 按真实边界拆分；不要借生成文件名规避。当前 `ui_declarative.h` 为 581 行，把样式/组件/绑定/列表放在同一头，S2 抽出工厂和属性应用器是必要的共享，不复制第二份；旧 API 留薄转发。Rust `bindings/rust/oneui/src/lib.rs` 为 10,944 行，同时有 dispatcher、生命周期和众多控件；S4 需迁出直接相关的既有 dispatcher/任务回收职责到独立模块，保留公开 re-export，验证 post_owned 取消/窗口关闭，不把整个大 lib 复制成 common。`src/capi/oneui_c_api.cpp` 为 6,503 行，S3 新 view 导出必须在独立领域模块，涉及的 widget destroy/生命周期入口迁入现有 widget 领域边界；不继续往总文件追加 runtime。以上是含注释/空行的物理行数，只读基线，尚未拆分。完整控件模块治理是后续专项，WP3 不顺带重写无关控件。

构建与产物：实施前读取 build-artifact-hygiene，复用本工作树 `build/capture`；release 性能使用 Claude 确认的固定输出位置，不能另按日期建副本。更改 include 类布局先 clean 再全量编译；现有主仓库 third_party/skia、out/text-assets、Yoga 仅只读输入，绝不 clean 或改动主仓库工作区。每步记录实际命令/结果/失败，不把预定拆分批次声称为已验证提交。报告/截图用固定目录替换并按项目合计预算清理。

## 12. 风险与待决问题（请 Claude 裁决）

| ID | 需要决定的问题 | 推荐与理由；未同意时的影响 |
|---|---|---|
| **D1** | schema 来源和严格匹配范围 | 采用显式 `.one.schema.json`、ID 固定、完整契约 SHA-256（包括未用但可达字段）；避免从 C++ 或模板推断导致 Rust/Go 类型分叉。代价是加字段也需重生成全契约绑定。若要内嵌 schema 语法，应在 S1 前统一输入方式 |
| **D2** | view 根继续用 OneUiWidget*，还是独立 OneUiView* | 本稿遵循任务书返回 Widget，要求 widget_destroy 接入 ViewControl；优点是既有树接口兼容。独立 View 句柄更容易防误销毁，但改变任务书签名。请确认 Widget 方案和 mount/window 主动关闭语义，不能等绑定写完才换所有权 |
| **D3** | C++ 旧 VM/header/Slots 的迁移程度 | 采用单 runtime，短期 legacy emitter 只作差分 oracle，S5 删除；借用 VM 的 build_X 保留既有寿命要求，推荐新代码共享所有权；外部动态 Slots 显式迁移为模板/NativeHost。若必须无损保留动态 Slots，需另定受控 SDK 作者接口，不能混入中立 IR |
| **D4** | 热重载对 NativeHost/IME 的边界 | 保留同名 attached host；删除/移动该 host 或替换 composition 节点时 BUSY、整次回滚。推荐优先保证会话/输入不丢；不要承诺跨类型无条件保留全部 widget 状态 |
| **D5** | ±10% 的具体测量与低于 -10% 的处理 | 采用 UI mutation→对应重绘提交完成、配对 median/P95，按第 9 节固定场景；性能明显更快先核对等价性，由 Claude 确认是否按“不退化”接受。CPU paint 历史数据不能代替这一门槛 |
| **D6** | WP3 与 WP4 的动态 host/复杂事件接口 | v1 静态 host 名唯一、repeat 内 host 拒绝；PaneGrid 动态窗格/host 由 WP4 组件拥有并提出 typed 语义事件，必要时扩协议。请确认是否 WP3 就要 keyed host 查询；若要，须设计 `(scope/key path,name)` API 和内容租约，不以全局名字拼接凑合 |
| **D7** | 当前复合集合先用通用 list，是否立即增加 bulk ABI | 推荐先统一可验证语义和每次提交缓存；S6 专门测 Series/Rows，若因逐项回调超预算再评审 bulk。不要同时发两套未经测量的数据协议，也不保证通用 list 的图表吞吐已足够 |
| **D8** | “基础设置同构示例”的首期控件补齐范围 | 推荐覆盖实际行为矩阵，现有 Select/Switch/模板主题列表先验证协议；缺失的主题卡/字体/背景控件列入 OneUI 注册组件补齐，不回退普通 NativeHost。完整像素/产品 CSS 在 WP5。请确认 S6 同构粒度及这些通用组件是否计入 WP3 |

其他已识别风险：C ABI 默认对齐必须跨目标实测；恶意 IR 必须先限额校验再分配；reconcile/热 reload 的 no-throw commit 需要真实故障注入；旧 C++ 字符串/浮点与新 utf8/f64 的边界可能造成布局差异；语言 VM 在 connect/disconnect 自行修改业务状态会破坏回滚，因此协议禁止。以上均列入对应 S1–S6 测试，不以“异常很少”略过。

## 13. 本次设计交付与验证记录

- 本次设计交付仅新增本文件；未改源码、include 布局、iShell、主仓库工作区或构建输入，未构建、未运行 CTest、未测量性能、未生成截图。文中的头文件/Rust 是拟议接口，不能当作已编译通过的 SDK。
- 已只读核对任务书、WP0–WP2 验收、七份声明式文档、viewc/运行时/C ABI/Rust/iShell 三个模板；文档内列明对应路径与行号。知识图谱只读 `list_projects` 因当前审批策略拒绝，不再调用该 MCP，改用本地只读检索。
- 文档检查：JSON schema 示例可解析，代码围栏成对，引用的本地文件/行号范围已核对，固定 IR 记录尺寸已按字段复核；`git diff --check` 用于提交前空白检查。没有把这些文档检查称作 C/Rust 编译或 runtime 测试。
- MyStudio workspace/project/blueprint 上下文已读；本机技能和服务 agent kit 均为 5，无更新。按本任务及 WP0 验收的约束，未调用写入工具，**本次任务/交接/文档未同步 MyStudio**，以 Git 文档为交付。
- 本次不产生可再生构建/测试临时产物，不清理其他会话正在使用的既有构建目录；保留项仅本设计文档。后续实现必须按第 11 节独立记录真实证据。
- Git 暂存实际被 `E:/project/byname/oneui/.git/worktrees/oneui-views/index.lock` 权限拒绝，未提交。按 Claude 指示另留根目录 `WP-3-design-commit-msg.txt`（UTF-8 无 BOM，含 Co-Authored-By），供代提交本设计文档；该提交信息文件不是源码或测试产物。
- 当前待执行事项：Claude 评审 D1–D8、确定推荐协议是否接受；通过后才实施 S1。没有把设计方案或历史测试计作 WP3 功能通过。
