# WP3 S2：核心运行时检查点

2026-10-10，分支 `codex/language-neutral-views`，基线 design-2 bdaa390 / S1 daf38b4。按 Claude 授权完成 S2，并继续 S3；S3 后停止。原仓库与 iShell 文件均未修改。

## 1. 交付内容

- `src/views/format/` 接收原 `tools/viewc` 的二进制契约、注册表、schema、IR reader/writer/validator；编译器和运行时共用，避免两份校验器漂移。
- `src/views/` 按值转换、作用域生命周期、树、通知、列表、记录属性、事件拆分。共享原生组件工厂及属性应用；保留式条件、双向绑定、命令门控、列表、导入、静态插槽、作用域 CSS、静态 NativeHost 均有执行路径。
- `ui_declarative.h` 的工厂/属性方法移到两个领域源文件；`View::reconcileChildren` 拆到 `src/core/view_reconcile.cpp`，新增 prepare/commit 子节点事务。本次分层修正为 Mount 增加 Clipboard 服务成员，已先 clean capture；既有 Widget/View 实例布局及虚函数槽不变。
- `OneUiViewModelVTable` 等协议类型头随 S2 核心契约准备；C 导出、主头包含、ABI 44 及 C 生命周期桥接归 S3。
- S1 报告第 6 节按 UTF-8 无 BOM 重写，关闭 CALL 待决项。viewc 临时 fixture 固定在 `build/capture/viewc-tests`，使用继承 ACL 的 mkdir + contextmanager finally，unittest addCleanup；故障清理也有测试。

## 2. CALL 裁决落实

PROP/BIND/EVNT 末尾追加 `owner_kind:u32`，stride 为 20/36/36。0 是组件 NODE，1 是 CALL NODE；沿调用链取得实际根组件的属性/事件 ID。IR 仍为未发布的 1.0。

支持静态值、调用者作用域绑定、事件及 v-model；覆盖根同名属性来源，class 按根→内层调用→外层调用合并。未知属性定位调用点。没有额外原生包装层；有 overlay 时要求唯一组件根。NativeHost 名称仍属于模板定义，不允许 CALL 改名。S5 差分必须覆盖这些语义；本阶段只验证新 IR 路径与旧工厂复用，未冒充 S5 完整差分。

## 3. 生命周期与列表边界

属性反向依赖使更新跳过干净分支。getter 的借用 UTF-8 在下一次 VM 调用前复制；notify 合并、flush 禁止读回调重入。MOVE/UPDATE 不重新获取已有子 VM；RESET 按 key+VM/vtable 身份复用或替换。

列表更新先准备新快照、新行及 native children plan，再发布；失败保留旧结构与 VM，释放本轮新租约。过期 delta 退化 RESET 并返回诊断，已成功恢复的快照仍有效。专门覆盖“恢复引入新 VM，错误返回不能误释放它”。该强保证针对列表结构；普通标量 setter 不提供整页值的事务回滚。

## 4. 测试与结果

| 测试 | 数量 / 结果 |
|---|---|
| schema / diagnostics / binary Python | 8 / 6 / 11 个方法，全部通过 |
| CALL overlay / cleanup Python | 4 个方法，全部通过 |
| 核心 runtime | 9 个场景，通过：绑定更新、条件/门控、schema 拒绝、失败 connect 回收、读回调重入、invoke 销毁、CALL、嵌套列表/插槽作用域 |
| 列表 | 8 个场景，通过：增删改移、stale 恢复、重复 key、子作用域通知、VM 构建失败重试、提交后诊断、新 children 准备故障 |
| 旧 core 行为回归 | component / stack / authoring / control 四个程序，用修改后的 core 重新链接，全部通过 |
| 旧 C++ emitter | legacy compiler suite 通过 |
| 完整 CTest | 68 项，62 通过、6 失败；5 项已知沙箱限制及 1 项既有 audit。详细见 S3 报告 |

首次直接编译验证被 S3 验收否决，不能证明 core 的独立链接。本次 clean 后按重新生成的完整配置直接 MSVC 重建：26 个原失败程序全部链接/运行通过，全配置 DLL/静态库/gallery 也全部链接；实际范围及与标准 Ninja 的区别见 S3 报告。

黄金样例：

```xml
<template view-model="Basic"><Text :text="title"/></template>
```

```text
model Basic { r title: utf8 }
```

```text
oneui-viewc --emit ir --input tests/viewc/Basic.one --schema tests/viewc/Basic.oneschema --name Basic --output build/capture/Basic.oneview
1368 bytes
SHA-256 af8aac29bf1ca98444393ccf4a764bba845dad38621463644717053ff290fe5f
0000: 4f 4e 45 56 49 45 57 00 01 00 00 00 80 00 00 00
0010: 58 05 00 00 00 00 00 00 80 00 00 00 00 00 00 00
0020: 12 00 00 00 00 00 00 00 01 00 00 00 00 00 00 00
0030: d2 75 49 64 dc ad e1 43 01 45 b9 85 3b 71 64 9a
```

黄金文件由独立 reference encoder 生成，production writer 与其逐字节比较；不是把 writer 输出直接当作期望值。

调试构建在精确 delta 路径额外获取并核对存活 key 的 VM/vtable 身份和最终 key 序列，失败记录 STALE_CHANGE 并 RESET；NDEBUG 构建跳过这项额外扫描。新增独立探针验证错误身份及租约平衡。两种编译分支均运行通过，列表测试显式区分调试探针开销与发布路径的 acquisition 预算。

## 5. 后续边界

S1 的 G1–G6 通用能力缺口仍适用（见 S1 报告），没有扩大 WP4 或添加业务控件。Rust 生成器/安全绑定是 S4；统一 C++、新旧树差分和热重载是 S5；性能、焦点/IME 交互验收与 iShell 行为同构是 S6。S2/S3 没有证明 ±10% 性能预算。

本轮新增手写 C++ 文件最大 324 行，协议头 232 行；既有超长模块仅做集成点修改。组件注册表是从 S1 移动的声明式数据清单（607 行），不是新增超长手写逻辑。`ui_declarative.h` 拆出约 266 行，core View 的 reconciliation 同时移到独立文件。

文件清单、可复现直接编译命令和产物清理见 S3 报告及 `WP-3-S2-commit-msg.txt`。无新增需要产品或架构裁决的阻塞问题；正常终端全量 Ninja/CTest 仍由 Claude 复核。

## 6. 文件清单

- `CMakeLists.txt`
- `cmake/OneUIDesktop.cmake`
- `cmake/OneUISources.cmake`（本阶段 hunk）
- `cmake/OneUITests.cmake`（本阶段 hunk）
- `cmake/OneUIView.cmake`
- `cmake/OneUIViewRuntime.cmake`
- `cmake/OneUIViewTests.cmake`（本阶段 hunk）
- `docs/59-language-neutral-views-s1-report.md`
- `docs/59-language-neutral-views-s2-report.md`
- `docs/59-language-neutral-views.md`
- `include/oneui/oneui_view_c_api.h`
- `include/oneui/ui_declarative.h`
- `include/oneui/ui_native_host.h`
- `include/oneui/view.h`
- `src/core/view.cpp`
- `src/core/view_reconcile.cpp`
- `src/platform/shared/declarative_mount.cpp`
- `src/views/component_factory.cpp`
- `src/views/component_properties.cpp`
- `src/views/events.cpp`
- `src/views/format/ast.h`
- `src/views/format/binary.cpp`
- `src/views/format/binary.h`
- `src/views/format/component_contract.cpp`
- `src/views/format/component_contract.h`
- `src/views/format/component_registry.inc`
- `src/views/format/ir.h`
- `src/views/format/ir_graph.cpp`
- `src/views/format/ir_reader.cpp`
- `src/views/format/ir_validate.cpp`
- `src/views/format/ir_writer.cpp`
- `src/views/format/schema.cpp`
- `src/views/format/schema.h`
- `src/views/format/validate.cpp`
- `src/views/format/validation.h`
- `src/views/lifetime.cpp`
- `src/views/lists.cpp`
- `src/views/notifications.cpp`
- `src/views/record_properties.cpp`
- `src/views/runtime.h`
- `src/views/tree.cpp`
- `src/views/value.cpp`
- `tests/viewc/Basic.oneview.hex`
- `tests/viewc/golden_reference.py`
- `tests/viewc/support.py`
- `tests/viewc/test_diagnostics.py`
- `tests/viewc/test_overlay.py`
- `tests/views/Advanced.one`
- `tests/views/Imported.one`
- `tests/views/Overlay.one`
- `tests/views/Runtime.one`
- `tests/views/Runtime.oneschema`
- `tests/views/Slotted.one`
- `tests/views/fixture.h`
- `tests/views/list_tests.cpp`
- `tests/views/runtime_tests.cpp`
- `tests/workspace_fixture.py`
- `tools/viewc/ast.h`
- `tools/viewc/binary.cpp`
- `tools/viewc/binary.h`
- `tools/viewc/component_contract.cpp`
- `tools/viewc/component_contract.h`
- `tools/viewc/component_registry.inc`
- `tools/viewc/ir.h`
- `tools/viewc/ir_graph.cpp`
- `tools/viewc/ir_reader.cpp`
- `tools/viewc/ir_validate.cpp`
- `tools/viewc/ir_writer.cpp`
- `tools/viewc/lower.cpp`
- `tools/viewc/lower_attributes.cpp`
- `tools/viewc/schema.cpp`
- `tools/viewc/schema.h`
- `tools/viewc/validate.cpp`

共享 CMake 的具体切分说明见对应提交信息文件；src/views/format 包含从 tools/viewc 移动的文件。

## 7. S3 分层复核后的 S2 修正

运行时属于独立 oneui_views OBJECT，不再把任何 src/views 对象加入 oneui_core。普通核心测试没有 views/窗口/C API 对象；runtime/list 两项只增加 views。共享工厂只接受 Clipboard 接口，宿主适配实现旧 Mount 的默认系统服务。Window 的存活与调度由 C ABI weak 控制块适配，不包含在运行时头中。

原 reconcileChildren 恢复菜单需要的 eager reparent 语义；新事务 API 独立使用，继续验证失败时保留旧树。增加 MemoryClipboard 粘贴与 legacy repeat 服务继承测试，runtime 现为 9 个场景。没有扩展 S4。
