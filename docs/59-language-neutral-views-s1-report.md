# WP3 S1：契约、编译器与能力缺口

2026-10-10；基线 f393c5e；分支 `codex/language-neutral-views`。design-2 已落实 R1–R7、D1–D8。本交付只覆盖 S1，不包含 View runtime、C ABI 导出、Rust/C++ IR adapter，也不开始 S2。

## 1. 可运行的工具入口

```powershell
build/capture/oneui-viewc.exe --emit ir --input tests/viewc/Basic.one --schema tests/viewc/Basic.oneschema --name Basic --output build/capture/Basic.oneview
build/capture/oneui-viewc.exe --verify build/capture/Basic.oneview
build/capture/oneui-viewc.exe --roundtrip build/capture/Basic.oneview --output build/capture/Basic.roundtrip.oneview
```

`--emit cpp-legacy` 保留原 `.g.h` 生成器；省略 emit 仍保持旧行为，现有 CMake 调用不破坏。`--emit rust` 和统一 C++ adapter 分别在 S4/S5 实施，当前明确诊断拒绝，不能把 legacy 输出当作 IR 执行。

`.oneschema` 支持注释、分号、相对 import、model/key、r/rw、四种标量、嵌套非递归 list、命令参数、when/busy；ID 按模型/属性/命令名排序生成。import 的同一规范文件只读取一次，菱形共享不会重复声明；不同文件的重复模型报错。未用的独立模型不输出，但仍校验；根可达的未用属性/命令全部参与 SHA-256。

内置记录自动提供：`oneui.v1.StringItem`、`NumberItem`、`TabItem`、`Row`、`Column`、`Series`，保留命名空间，用户不可覆盖。COMP 固定注册清单与现有 authoring schema 的组件、属性和类型在工具启动时核对；新增 authoring 属性不能悄悄得到新的排序 ID。

编译器实现 18 段、绑定类型/可写性、事件签名、条件、稳定 key 列表、嵌套词法 scope、import、命名插槽和 fallback、scoped CSS、静态 NativeHost、ref/view-id、属性行列与导入来源链。字符串仅 UTF-8，二进制完全按 little-endian 字节写入，不序列化 native struct。writer 发布前必须经过独立 reader；reader 重算哈希，并检查范围、溢出、引用、类型、记录所有权、调用/节点/模型环和资源上限。

编译器为各调用点生成模板特化；reader 允许合法的共享模板，在每个调用上下文重新校验词法深度。NativeHost 必须是静态且唯一的实例。展开后的静态骨架同样限制为 100,000 节点，防止小型调用 DAG 造成指数展开。

## 2. 黄金字节与诊断验收

最小输入：

```xml
<template view-model="Basic"><Text :text="title"/></template>
```

```text
model Basic { r title: utf8 }
```

黄金文件为 `tests/viewc/Basic.oneview.hex`，**1360 字节**。`golden_reference.py` 用独立的显式记录和 Python hashlib 构造规范字节，不读生产 writer、reader 或组件注册文件；测试同时比较冻结 hex、参考编码器和 C++ 编译器三者。

```text
0000  4f 4e 45 56 49 45 57 00 01 00 00 00 80 00 00 00
0010  50 05 00 00 00 00 00 00 80 00 00 00 00 00 00 00
0020  12 00 00 00 00 00 00 00 01 00 00 00 00 00 00 00
```

含义：ONEVIEW NUL；格式 1.0；128 字节头；总长 0x550；目录从 0x80 开始；18 段；SHA-256 算法 1；根 template 索引 0。

完整 schema SHA-256：

```text
d2754964dcade1430145b9853b71649a187a74104564701be9c3e85aceb55fcf
```

新增三组测试共 22 个 unittest 方法（包含表驱动子例），另把原 declarative compiler 测试接入主 CTest。覆盖所有段的非空样例与 reader→writer 字节不变、声明顺序/绝对路径不影响输出、每类契约变化改变哈希、UTF-8/类型/事件/key/import/slot/CSS/行列诊断、截断/重叠/整数范围/版本/未知必需段/可选扩展/伪造哈希/模型和引用校验。原编译器的正反例及 6 个正向、12 个反向 C++ 类型/结构/生命周期诊断保留。

测试临时目录由 `tests/workspace_fixture.py` 在固定 scratch 根内建立并在 finally 清理。没有改系统或已有目录的 ACL。原因是本机 Python 3.14 的 mkdtemp(0700) 创建仅 creator 可访问的目录，限制令牌无法再进入；新的非机密测试目录直接继承工作树权限。

实际构建、完整 CTest 结果及环境限制见第 5 节；不要把测试定义数量当成全部通过。

## 3. D8 能力缺口清单

依据当前 `include/oneui/ui_component_schema.h:7–55` 的 authoring 注册表，而不是仅按原生类是否存在判断 `.one` 能力。以下归 WP4；S1 没有新增任何 UI 组件或用 NativeHost 代替普通设置 UI。

| 编号 | 需求/依据 | 当前能力与缺口 | S6 的行为/数据流表达 | WP4 补齐及验收 |
|---|---|---|---|---|
| G1 | 字体族选择、筛选、字体预览；iShell `native-shell/src/settings_basic.rs:288–363` | `fonts.h:9–20` 有系统字体枚举服务；没有通用 authoring FontPicker/带预览的选择组件 | 异步 VM 提供 StringItem 列表，Select + Input 编辑 family，状态/错误属性显示保存与应用结果；S6 模拟数据，不读写用户配置 | 通用字体选择组件；可搜索/键盘导航/无障碍、选中稳定、枚举失败/空集合、长字体名与预览；枚举/持久化仍由服务与 VM 负责 |
| G2 | 背景图片/媒体选择、清除及预览；`settings_appearance.rs:141–297` | 有原生 ImageView 和文件服务，但没有声明式媒体选择/预览组合契约；不应把路径导入/文件清理塞进 IR | Input 路径 + Button 选择/清除命令 + Text 状态；VM 模拟选择成功/取消/非法路径、异步结果与 clear；不要求图片像素预览 | 通用文件/媒体选择与预览组件；异步取消、错误、所有权、清除、不支持格式；业务文件迁移/管理仍在 iShell |
| G3 | 背景强度、内容透明度和字体/界面比例；`settings_appearance.rs:53–88`、`settings_basic.rs:366–467` | 原生 `controls/slider.h:17–39` 完整；**Slider 尚未进入 `.one` 注册表** | 当前 `.one` 用 Input 编辑数字文本，VM 解析/校验/应用，或 Select 预设值，保留自定义值及错误数据流 | 注册通用 Slider 的 range/step/value、双向绑定及 Begin/Commit/Cancel 语义；不能因原生类存在就声称 WP3 已支持 |
| G4 | 12 主题画廊/已选状态；`settings_basic.rs:514–715` | 原生 `controls/card.h:13–23` 存在；**Card 不在 authoring 注册表**。已有 Surface/Column/Button、v-for、v-if | 用 v-for + Surface/Column/Button；命令更新选中模型，用条件标记显示选择。行为同构，不复制每个缩略预览像素 | Card/InteractiveSurface 的 authoring 契约与可访问激活状态若产品需要再补齐；不为 S6 新造主题专用控件 |
| G5 | 主题卡逐项预览色、动态外观 | 当前 class/大部分布局属性静态，不能绑定任意 style 对象，也没有列表项独立主题 token 契约 | 展示名称、分组、选中状态与命令效果；使用现有 variant/tone/条件节点，接受 D8 的非像素同构 | 若需要逐项 palette preview，评审通用局部主题/颜色属性能力、作用域和缓存；不能为预览开放任意 CSS/指针通道 |
| G6 | 语言、标题栏、密度、关闭行为、启动项、背景填充/位置 | Switch、Select、Tabs(segmented)、Input、Status 可表达现有数据流；本项无通用组件阻塞 | enum↔索引/稳定 key 转换在 VM；列表和配置错误通过 typed schema；动态区域用已有条件/列表 | 仅在 WP4 审查发现真实交互缺口时补齐，不把 iShell 业务枚举移入 OneUI |

WP4 的 PaneGrid/DockWorkspace/CommandBar/容器查询与 `(key path,name)` 动态 host 仍按任务书独立设计，本清单不把它们算作 S1 已实现。D8 允许 S6 行为替代，不要求先完成上述组件再验收 WP3。

## 4. 边界与需要 Claude 裁决的差异

1. **导入组件调用点的普通属性**：旧 emitter 把 `class/grow/...` 直接应用到导入模板返回的根；已批准 IR 的 CALL 节点 component=NONE，PROP 又要求组件属性 ID，无法无歧义承载这条旧捷径。S1 明确报 `CALL_ATTRIBUTES`，没有静默忽略。推荐要求源模板显式 wrapper，或在 S2 前单独规定“对实际子根应用属性”的 CALL overlay 契约；推荐前者使组件边界清晰。需要确认迁移策略，不能宣称这部分已经与 legacy 树一致。
2. **Slider/Card 的 D8 表述**：它们是现有原生控件，尚非现有 `.one` 组件。推荐按 G3/G4 处理：WP3 使用已注册 Input/Select/Surface/Button，WP4 补注册与交互契约。本次没有擅自扩展组件实现。
3. **验收环境**：标准 Ninja 在此限制令牌环境下无法完成子进程退出等待，连 echo 探针都可复现。需在正常开发终端重跑已配置 capture 的 `ninja` 和 `ctest -j 8`，才能补齐标准构建证据；不能把手工编译说成 Ninja 成功。无需改 ABI 或业务代码解决此问题。

其他 design-1 待决事项已经全部由 R/D 裁决关闭。外部动态 Slots 不支持；静态模板插槽支持。运行时刷新/销毁/列表租约/IME/热重载/性能属于 S2–S6，S1 不宣称已验证它们。

## 5. 实际验证与文件清单

此节在本轮验证结束后填写最终结果。无公共 include 类布局变更，无需 clean；原仓库及 iShell 文件始终只读。所有临时构建与测试证据复用 `build/capture`。

- `docs/59-language-neutral-views-s1-report.md`?
- `tests/viewc/Basic.one`?
- `tests/viewc/Basic.oneschema`?
- `tests/viewc/Basic.oneview.hex`?
- `tests/viewc/golden_reference.py`?
- `tests/viewc/support.py`?
- `tests/viewc/test_binary.py`?
- `tests/viewc/test_diagnostics.py`?
- `tests/viewc/test_schema.py`?
- `tests/workspace_fixture.py`?
- `tools/viewc/ast.h`?
- `tools/viewc/binary.cpp`?
- `tools/viewc/binary.h`?
- `tools/viewc/compiler.cpp`?
- `tools/viewc/compiler.h`?
- `tools/viewc/component_contract.cpp`?
- `tools/viewc/component_contract.h`?
- `tools/viewc/component_registry.inc`?
- `tools/viewc/ir.h`?
- `tools/viewc/ir_graph.cpp`?
- `tools/viewc/ir_reader.cpp`?
- `tools/viewc/ir_validate.cpp`?
- `tools/viewc/ir_writer.cpp`?
- `tools/viewc/legacy.h`?
- `tools/viewc/lower.cpp`?
- `tools/viewc/lower_attributes.cpp`?
- `tools/viewc/schema.cpp`?
- `tools/viewc/schema.h`?
- `tools/viewc/schema_parser.cpp`?
- `tools/viewc/validate.cpp`?

## 6. ???????

??????? 8 ???????? `.ilk` ????????????? **609 MiB** ????????????????????????????? build ?? **19.86 GiB**?E: ??? **61.37 GiB**????? 22 GiB ???????????????????`build/artifact-state.json` ????

??? 24 ??????????????? creator-only ACL ????/???**????**?????????????/???????? 8 ??????????? 16 ?????? build-space ???Python ???? PermissionError?????? 8 ??????????????????????????????????????????????????????????? fixture ??????????????????????????????????

| ???? | ?? | ?????/???? |
|---|---|---|
| `E:/project/byname/oneui-views/build/capture/viewc-tests/viewc-9qim1tkb` | ???????? | ACL ??????????? fixture ??? |
| `E:/project/byname/oneui-views/build/capture/viewc-tests/viewc-_ztlfb0p` | ???????? | ACL ??????????? fixture ??? |
| `E:/project/byname/oneui-views/build/capture/viewc-tests/viewc-akgnxd9z` | ???????? | ACL ??????????? fixture ??? |
| `E:/project/byname/oneui-views/build/capture/viewc-tests/viewc-fr3kdbgg` | ???????? | ACL ??????????? fixture ??? |
| `E:/project/byname/oneui-views/build/capture/viewc-tests/viewc-n80amp14` | ???????? | ACL ??????????? fixture ??? |
| `E:/project/byname/oneui-views/build/capture/viewc-tests/viewc-nrqse2kj` | ???????? | ACL ??????????? fixture ??? |
| `E:/project/byname/oneui-views/build/capture/viewc-tests/viewc-rt5_3vb7` | ???????? | ACL ??????????? fixture ??? |
| `E:/project/byname/oneui-views/build/capture/viewc-tests/viewc-vpiz1tj5` | ???????? | ACL ??????????? fixture ??? |
| `C:/Users/Administrator/AppData/Local/Temp/tmp0zs9n4ut` | ???????? | ACL ??????????? fixture ??? |
| `C:/Users/Administrator/AppData/Local/Temp/tmp2xxbtxup` | ???????? | ACL ??????????? fixture ??? |
| `C:/Users/Administrator/AppData/Local/Temp/tmp3eqchuch` | ???????? | ACL ??????????? fixture ??? |
| `C:/Users/Administrator/AppData/Local/Temp/tmp9510j384` | ???????? | ACL ??????????? fixture ??? |
| `C:/Users/Administrator/AppData/Local/Temp/tmp9d5r0h4x` | ???????? | ACL ??????????? fixture ??? |
| `C:/Users/Administrator/AppData/Local/Temp/tmp_w6yg580` | ???????? | ACL ??????????? fixture ??? |
| `C:/Users/Administrator/AppData/Local/Temp/tmpazzv83db` | ???????? | ACL ??????????? fixture ??? |
| `C:/Users/Administrator/AppData/Local/Temp/tmpbcoxzqqc` | ???????? | ACL ??????????? fixture ??? |
| `C:/Users/Administrator/AppData/Local/Temp/tmpc7doitpp` | ???????? | ACL ??????????? fixture ??? |
| `C:/Users/Administrator/AppData/Local/Temp/tmpd39h5q3c` | ???????? | ACL ??????????? fixture ??? |
| `C:/Users/Administrator/AppData/Local/Temp/tmpdgo37424` | ???????? | ACL ??????????? fixture ??? |
| `C:/Users/Administrator/AppData/Local/Temp/tmpel3vsqp0` | ???????? | ACL ??????????? fixture ??? |
| `C:/Users/Administrator/AppData/Local/Temp/tmpg88kqbfh` | ???????? | ACL ??????????? fixture ??? |
| `C:/Users/Administrator/AppData/Local/Temp/tmpmhrvlftl` | ???????? | ACL ??????????? fixture ??? |
| `C:/Users/Administrator/AppData/Local/Temp/tmpni6u6mk3` | ???????? | ACL ??????????? fixture ??? |
| `C:/Users/Administrator/AppData/Local/Temp/tmppoldlpm_` | ???????? | ACL ??????????? fixture ??? |

Git ???????? `E:/project/byname/oneui/.git/worktrees/oneui-views/index.lock: Permission denied`???????????????? `WP-3-design2-commit-msg.txt`?`WP-3-S1-commit-msg.txt`??????????? Co-Authored-By?????????????????????

MyStudio ????????????????**???**??S1 ????????? Claude ??????????????? S2?
