# WP4 S1–S2 实施报告：ABI 44 冻结检查点

日期：2026-10-10。基线：`7257eb4`，分支：`codex/workspace-components`。
状态：实现已交付，提交 Claude 复核；停在 S2，不进入 S3。普通 Cargo 回归仍有一项环境相关失败待正常终端复核，不能标为全部验收通过。

## 1. 交付边界

先将 Q1–Q10、导航发布边界、独立示例位置写入 [design-2](60-workspace-interaction-components.md)。本轮只实施 S1/S2：扁平几何、透明结构集合、动态 render scope、NativeHost 内容租约及 ABI/Rust 接入。未修改 iShell、原 OneUI 工作树或 `examples/performance_lab/`，未写 Git 索引、未提交。

`PaneGrid/Pane/DockWorkspace/DockItem` 本轮注册的是**结构及身份骨架**，用于独立验收 scope/host；尚无 S3 的分屏布局动画、S4 的拖动与导航、S6 的停靠行为。结构骨架不得当作已完成的产品组件。`tests/workspace/Tree.oneschema` 冻结扁平 nodes/groups/panes 与请求签名；`.one` 此时验证稳定内容池，完整 nodes 绑定在 S3 接入。新实验台 exe 在 S8 的 `examples/workspace_interactions/` 实施。

| 部分 | 主要文件与职责 |
|---|---|
| S1 几何 | `include/oneui/layout/workspace_tree.h`；`src/core/workspace_tree_validate.cpp`、`workspace_tree_layout.cpp`：迭代验证、缓存最小尺寸、平铺结果 |
| S1 共享规则 | `src/internal/split_geometry.h`；既有 `dock_geometry.cpp`、`dock_hit_test.cpp`、`split_view.cpp` 调用共享尺寸分配及边缘规则 |
| S1 契约/测试 | `tests/workspace/Tree.oneschema`、`Tree.one`、`tree_tests.cpp`；编译契约加入 workspace 编译器测试 |
| S2 格式/编译器 | `src/views/format/ir*`、`validate.cpp`、分领域 `component_registry_*.inc`；`tools/viewc/lower*.cpp`：WSC1、children-kind、动态 host 白名单、稳定调用路径 |
| S2 运行时 | `src/views/repeat.cpp`、`workspace_scope.cpp`、`workspace_children.cpp`；tree/notifications/lifetime/reload 接入 |
| S2 C ABI | `include/oneui/oneui_view_c_api.h`；`src/capi/oneui_c_api_view_scope.cpp`、host/internal/reload/flush 接入 |
| S2 Rust | `oneui-sys/src/ffi/view.rs`、`supported-symbols.txt`、safe `oneui/src/view/scope.rs`；普通测试入口接入 `oneui-view-tests/tests/workspace.rs` |
| 构建/证据 | `cmake/OneUI{Sources,Tests,ViewRuntime,ViewTests}.cmake`；[原始测试结果](benchmarks/workspace-interactions/s1-s2-tests.json)、[最终 ABI diff](benchmarks/workspace-interactions/abi44.diff)、[产物清单](benchmarks/workspace-interactions/artifacts.md) |

S1/S2 同时修改组件契约、编译器、构建注册和设计文档；透明集合必须与 scope runtime 一起才能形成可运行检查点。因此按评审第三节的交叉文件例外合并提交，信息在根目录 `WP-4-S1-S2-commit-msg.txt`，包含完整文件清单及 Co-Authored-By。

## 2. S1 的可验收语义

- `DockTreeIndex` 使用显式遍历栈和索引，不递归几何树；nodes/groups/panes 各自最多 100000 项，没有 64 层几何限制。空 group root 合法，孤立 node、重复 pane、跨 group 多父、环、悬挂引用均拒绝。pane 池允许保留暂未挂入几何树的内容。
- ratio 为有限开区间 `(0,1)`；最小尺寸、gap、viewport 都检查有限性及非负。几何深度 1/64/65/1000 已测。模板词法深度仍遵循 WP3 的 64 层限制，两者不同。
- 先构造候选索引/输出，再一次替换；验证失败保留旧索引与旧输出。最小尺寸不足时增大内容 extent 供后续滚动；最大化仅改变输出，不改持久 ratio。
- 共享规则保留旧行为差异：Dock 在不足时按 ratio 分配；SplitView 按双方 minimum 比例分配。边缘同距仍按 L/R/T/B；旧中心 `.28/.72` 严格边界不变。
- core 使用无空串、无 NUL 的不透明 key；语言边界继续由 schema/ABI 验证 UTF-8。没有改变旧 `DockNode` 的布局或 ABI。

## 3. S2 的 scope 与租约

路径是有序 `(instance_id,key)` 数组。业务 key 不分割；中文和 `/` 原样比较。实例名由显式 repeat id 与 `call-id/` 前缀组成，原始 repeat/call id 不允许 `/`，从编译器到 IR reader 双重拒绝歧义。嵌套 row 形成下一段，而非把几何 left/right 编进路径。供给槽内的 workspace 集合同样要求调用点身份。

每个渲染 row 单独 acquire/connect；同 VM 在同 View 的两个集合、或两个 View 中均有独立 notifier。数据读取 scope 仍使用 WP3 的缓存，不能用它替代 render identity。MOVE 复用 row/native host/content；REMOVE 关闭旧 scope，同 key 重插产生新身份。UPDATE 仅刷新指定属性，RESET 刷新保留 row 的全部属性。RESET 是兼容入口，正常结构操作仍用 INSERT/MOVE/REMOVE delta。

透明 REPT 的 Instance 只承担语义集合；其空包装 Widget 不挂到 native 树。Pane/DockItem 直接成为集合父节点的 children。候选 children、scope/host map 在发布前准备，失败时旧 scene 与内容仍有效；到 commit 才替换关系、拆除被删内容并关闭旧 render scope。

内容仍使用原 `host_attach/detach` 的 Widget 共享引用与 foreign owner 租约，无 host-move 导出。500 次 MOVE 后 host 地址、content 地址、owner retain/release 不变。显式 detach→attach 可将同一个 content 对象移到其他 host，测试证明未重建；S3 的几何变更将保持原 host，无需这一往返。

HostRef 的安全由 C 层提供：每次操作检查 View 所属及当前 host 身份。失效句柄保留小墓碑但清除 Widget，防止分配器复用地址造成旧 Rust 引用命中新对象。每 View 最多发行 100000 个 host 身份，超限 `LIMIT`；旧句柄仅在 View 寿命内可调用，销毁 View 后一律不能使用。内容释放发生在 runtime commit，墓碑不会继续持有终端内容。显式 C flush/reload 或下一次 lookup 会清掉已退休 host 的 native 引用。

## 4. 最终 ABI 变化

**ABI 仍为 44，View protocol 仍为 1，VTable/OneUiValue/旧 DockNode 布局不变。新增 C 函数只有一个。** 完整公共头文件差异见 [abi44.diff](benchmarks/workspace-interactions/abi44.diff)。

```c
typedef struct OneUiViewScopeSegment {
    OneUiUtf8String instance_id;
    OneUiUtf8String key;
} OneUiViewScopeSegment;

ONEUI_API OneUiViewResult oneui_view_resolve_scope(
    OneUiView *view, const OneUiViewScopeSegment *segments,
    size_t segment_count, OneUiViewNotifier **out_scope);
```

`find_host` 原有 scope 参数现在支持非 NULL，只接受本 View 的 root 或 workspace render notifier；普通 data-only notifier 返回 INVALID_ARGUMENT，不能因空 path 而别名到 root host。`resolve_scope` 成功返回 owned notifier；0 段为 root；失败清空输出。UI 线程、回调外调用，最多 64 段、总 UTF-8 长度 1 MiB。无匹配 `NOT_FOUND`，移除/关闭 `CLOSED`，非法字符串/跨 View `INVALID_ARGUMENT`，错误线程 `WRONG_THREAD`，回调重入 `REENTRANT`，超预算 `LIMIT`。输入只借到返回。Rust 新增 `ScopeSegment`、`View::resolve_scope`、`View::find_host_in`。

没有 Stack C 导出、Rust Stack 增删方法、host-move 或 VTable 扩展。Stack 内部增删仍归 S5。WSC1 是 **IR 能力门禁，不是 C ABI 升号**：kind=19、required、16 字节记录；本轮仅 bits 0/1，容器查询 bit 2 留待 S5，不提前接受。普通 WP3 IR 不写该段，黄金文件字节不变；实际运行旧 WP3 reader 验证新 fixture，得到 VERSION 拒绝。

## 5. 测试结果与覆盖

最终固定批次共 20 个命令：19 个符合预期，普通 Cargo 命令失败。详见原始 JSON；旧 reader 的 exit 1 是明确期待的 VERSION 负例。没有 OS 级输入自动化，没有性能达标声明。

| 测试 | 结果 |
|---|---|
| 新 workspace compiler | 9/9：必需能力、白名单、重复 host、非法直接子项、缺失/歧义 id、导入/供给槽、伪造 CALL、保留事件位、普通 repeat、扁平 schema |
| WP3 compiler | binary 11、diagnostics 6、overlay 4、schema 8、Rust codegen 4，全部通过；包含独立黄金 IR |
| S1 tree | 通过：深度四档、几何与旧 Dock 对照、ratio/NaN、cycle/dangling/multiple-parent、minimum/滚动、最大化、存储顺序不改变几何 |
| S2 native scope/host | 通过：下表全部场景 |
| WP3 native | runtime 9、typed record/delta/reload 11、list 8、C ABI lifetime 11；reload 50 retained + 50 remove/restore；C++ differential；dock gesture，均通过 |
| C11 / ABI | C11 编译/运行通过；C/Rust 符号清单同步通过；ctest ABI 1237 个布局检查、617 个函数签名通过 |
| Rust View integration | 10 个场景通过，包括新增动态 scope/content owner、get/connect/disconnect/drop panic containment、50 次 reload；测试中的预期 panic 输出不等于失败 |
| 普通 `cargo test --offline --manifest-path bindings/rust/Cargo.toml` | 完成普通入口编译；现有 main-thread suite 为 54 passed / 54 failed。首个失败是 `binding_tests.rs:920` 的 `window.set_tray_enabled(true)`，后续窗口测试因共享锁 PoisonError 失败。未改测试断言、未跳过此项冒充通过 |

| 动态 scope / 内容租约 | 证据 |
|---|---|
| Pane 与 DockItem 集合、透明 native children | Workspace + WorkspaceMirror，直接 children 无 REPT Column |
| 同 VM 两集合、同 VM 两 View | notifier/NativeHost 地址不同，关闭其中一个不关闭另一个 |
| root、中文及 slash key、嵌套路径、导入限定名 | resolve/find 成功且互不混淆 |
| 稳定性 | 500 次真实 list MOVE；host/content 身份、owner retain=1/release=0；子 notifier 属性通知及 RESET 更新 |
| 热重载 | 同 scope/name 复用；已 attach host 改名 BUSY，旧 scene/租约保留 |
| 失败事务 | INSERT 的 connect 返回失败；connect 抛 `std::bad_alloc`；C 边界拒绝并保持已发布内容，新增 VM 租约平衡，随后 RESET 可恢复。此为特定边界故障注入，未宣称所有 malloc 点都已覆盖 |
| 移除与重插 | 旧 notifier/host CLOSED，lookup NOT_FOUND；同 key 重插新身份；嵌套 scope 一并关闭；native content 脱离后 owner 才释放 |
| 租约转移/拒绝 | 未 detach 的跨 host attach BUSY，不额外 retain；显式 detach/attach 保持同 content 对象，最终 owner 恰好释放 |
| 非法调用 | 跨 View、data-only notifier、错误线程、回调重入、空 key、非法 UTF-8、超深度、关闭后调用；失败输出置空 |
| Rust | 独立动态 row connection，50 次 MOVE，删除后 HostRef/notifier CLOSED，完整 Rc owner drop 一次，row connections 回到 0 |

S3/S4 的真正分屏、交换、跨 tab、最大化运动等 A7 交互组合未在本轮实现，因此没有用 MOVE 冒充这些端到端场景。16 ms、WarmFrame、最终 resize 一次与 NVIDIA 新进程 trace 留在已批准的后续检查点。

## 6. 构建复核与文件规模

复用 `build/capture` 的 RelWithDebInfo/Ninja 编译数据库，直接 MSVC `/O2` 重放受影响 views、C API、core 对象，重链 DLL/static 与相关测试；新对象放固定 `build/codex-views`。没有重配置 CMakeCache，没有新增构建配置、没有 shader/资源加载排查。旧公开类的成员布局未修改；新 `DockTreeIndex` 单独实现。内部 Runtime/Image/OneUiView 布局变更的消费者与测试均重编。

Claude 正常终端可按既有 capture 构建流程复核新增 target：`oneui_workspace_tree_tests`、`oneui_workspace_scope_tests`（要求 static），以及 CTest 项 `oneui_viewc_workspace_tests`；本次不能声称 Ninja/CTest 全量执行成功。普通 Cargo 复核需设置 `ONEUI_LIB_DIR=build/capture`、`ONEUI_VIEWC=build/codex-views/oneui-viewc.exe`、固定 `CARGO_TARGET_DIR=build/cargo-target`，并把 capture 加到 PATH；**不依赖 `--test` 参数才能编译**。

按基线与当前物理行数统计，包含注释空行；新 C++ 文件经 clang-format，新 Rust 文件经 rustfmt。

| 文件/职责 | 前 → 后 | 说明 |
|---|---:|---|
| `src/views/format/component_registry.inc` | 607 → 4 | 入口只组合三个领域，仍只有一个 registry；controls 204、layout 317、workspace 186 行；旧属性 ID 不变 |
| `src/views/tree.cpp` | 305 → 253 | 迁出 repeat 生命周期与列表协调至 `repeat.cpp` 133 行；另拆 workspace scope 77、children 34 行 |
| `src/core/dock_geometry.cpp` | 125 → 111 | 纯分配/矩形规则迁至 `split_geometry.h` 44 行 |
| `src/core/split_view.cpp` | 418 → 413 | 共用纯规则，不改旧手势语义 |
| `src/core/dock_hit_test.cpp` | 94 → 93 | 共用最近边及稳定 tie 规则 |
| `src/core/terminal_view.cpp` | 1901 → 1901 | 本轮未触及；按评审在 S3 拆 viewport 职责 |
| `src/core/tabs.cpp` | 1098 → 1098 | 本轮未触及；S4 拆 drag 职责 |
| `src/core/style_sheet.cpp` | 1246 → 1246 | 本轮未触及；S5 拆 CSS 规则职责 |

本轮新增/修改的手写实现文件均 ≤500 行。没有把 S3/S4/S5 的未实施治理报告为已拆分。

## 7. 待 Claude 复核及后续

1. **ABI 冻结**：推荐接受当前一个导出及 scope 字符串/所有权语义；main 合并时版本号占用仍由 Claude 处理。
2. **失效 host 句柄预算**：推荐接受每 View 100000 个累计身份和墓碑策略。它保证安全 Rust 引用不会遇到地址复用，避免新增释放句柄的 ABI；到上限明确 LIMIT，不静默泄漏或重用旧身份。如需无限长寿命 View 的句柄回收协议，应在此次冻结时另裁决，不能暗改 borrowed 语义。
3. **普通 Cargo 托盘失败**：推荐正常桌面终端复核。当前证据只能定位到现有托盘启用断言，不能断言一定由沙箱引起；若仍复现，应独立修复该既有用例/平台能力判断，不能削弱 WP4 的 scope 测试来绕过。
4. **产物**：完整只读统计及不可清理项见产物清单；原仓库混合目录及其他会话产物受“不碰原仓库”约束。自动审批拒绝过旧 ILK 清理，没有换工具重试，也没有用其他任务的 35 GiB 例外覆盖本轮 30 GiB。

MyStudio 本轮已读取 workspace/project/blueprint context，本地 kit v5 与服务器一致；项目按真实 Git remote 匹配。WP4 task/execution/handoff **未同步**，未调用需审批的写工具、未通过 HTTP 绕过。图 MCP 在本会话已确认受审批策略阻挡，后续源码检索回退到本地只读。没有将本地报告当作服务器写入凭据。

到此停止，等待 S2 ABI 检查点评审。
