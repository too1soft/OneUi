# WP3 S6：设置示例与最终检查点

本次交付 S4–S6 联合候选，等待 Claude 验收；**不能宣称全部性能/事务目标已通过**。
S4、S5 分项见相邻报告。没有开始 WP4，也未修改原 OneUI 或 iShell 工作树。

## 交付内容

- S4：Rust sys POD/vtable/导出、safe View/ViewMount/ViewNotifier/HostRef/StableList，
  Rust emitter、类型/生命周期测试、dispatcher/任务回收模块拆分。
- S5：C++ 同源 IR adapter、11 个 schema 迁移、CALL 差分及删除 legacy emitter、
  核心 reload、ABI 45 单独导出、Rust 开发 watcher、composition/容器引用同步。
- S6：`examples/language_neutral_views/` 的 SettingsBasic.one/.oneschema、theme.css、
  纯 Rust Settings/Theme VM、异步 worker/result、状态/错误、验证测试、原生截图及基准。
  小型手写 Rust 对照使用同一 Settings VM，Table 对照也使用同一 VM 和变更序列。

主题 12 项 v-for；语言、标题栏风格、密度、关闭动作、启动项、字体、缩放、背景路径/
强度、不透明度、恢复默认、模拟失败、异步保存及过期结果丢弃均已接线。D8 不要求像素
同构；NumericSlider/主题预览/背景资源选择等仍按 S1 缺口，以现有通用控件和模拟结果
表达行为，不添加 NativeHost 普通 UI 逃生口，不真实写入用户设置。

完整文件清单放在合并提交信息 `WP-3-S4-S6-commit-msg.txt`。各步骤共享 emitter、
CMake、Rust 原始/安全接口和 ABI；采用用户允许的合并提交，不提供代码块拆分方案。

## 验证结果

| 检查 | 实测 |
|---|---|
| viewc 直接 /O2 编译 | 15 个对象，成功 |
| 原生修改对象/DLL | 38 个对象及新增文件，链接成功；static archive 重建成功 |
| 原有 C++ 示例回归 | 6/6 程序通过 |
| C++ 新旧 IR 差分 | 1/1 程序通过 |
| terminal_workbench / performance_lab main.cpp | 2/2 编译通过（未另计应用运行） |
| 原生 runtime / list | 9/9、8/8 场景通过 |
| 原生 reload | 50 原位 + 50 删除/恢复循环；Host/IME/错误回滚通过 |
| Rust View 集成 | 9/9 场景通过 |
| Rust compile-fail | 5/5 负例通过，另有 1 个正例通过 |
| Rust 主线程测试，逐项独立进程 | 107/108；托盘场景失败 |
| 私有 adapter + dispatcher 定向 | 5/5、合计 11/11（包含于主线程测试，不重复相加） |
| ABI | 1,229 布局检查、616 签名、四类型 union 往返通过 |
| Rust emitter | 4/4；其余 schema/binary/diagnostics/overlay/compiler 均在 CTest 通过 |
| Rust 设置 VM | 3/3：默认/验证、失败重试/取消/过期结果、主题身份 |
| 真实示例 | --smoke 通过；浅色宽屏、暗色窄屏 PNG 已查看 |
| capture 已注册全量 CTest | **63/68**，29.44 秒 |

CTest 失败分列，不能合并成“全部既有失败”：

1. **既有** `oneui_windows_runtime_audit_tests`：外部 windows-compat 文件 hash 不符。
2. `oneui_icu_patch_recipe_tests`、`oneui_win7_patch_recipe_tests`：Git sh
   CreateFileMapping Win32 error 5，沙箱映射权限限制。
3. `oneui_win32_notification_activation_tests`：HRESULT 0x80070005。
4. `oneui_backend_contract_tests`：当前 Windows 桌面托盘不可用；Rust 107/108 的失败相同。

首次联合 CTest 还包含两个测试二进制仍按 ABI 44 编译的问题；已重新编译
c_api_behavior/interaction_c_api 与 C header smoke，再跑得到上述 63/68。
这两个失败已修复，不列为沙箱失败。最终日志 `build/s6-ctest.log`；Rust 原始场景
结果 `build/s4-rust-regression.json`；重载/差分日志 `build/s5-*.log`。

capture 未重新配置，新增 rust emitter/reload/cpp_diff 三个 CTest 入口通过直接执行
验证，尚未加入本地 68 项注册表。Claude 正常终端 clean + 全量 ninja + ctest 是新
配置的最终复核；本报告不把重用未修改对象的直接链接声称为完成 clean 全量构建。

## 示例运行

在 vcvars64.bat 开发者终端设置本工作树路径后运行：

```powershell
$env:CARGO_TARGET_DIR = 'E:/project/byname/oneui-views/build/cargo-target'
$env:ONEUI_LIB_DIR = 'E:/project/byname/oneui-views/build/capture'
$env:ONEUI_VIEWC = 'E:/project/byname/oneui-views/build/capture/oneui-viewc.exe'
cargo run --release --manifest-path examples/language_neutral_views/Cargo.toml
```

追加 `-- --smoke` 自动验证保存/退出；`-- --dark --narrow` 检查窄屏。
示例 README 提供 IR 重载、测试和性能复现命令。本次直接工具位于
build/codex-views/oneui-viewc.exe；已同步 capture 下的工具供旧注册表运行。

## 性能与证据边界

最终表、逐轮统计、90,000 原始样本、冷加载/首帧、hash/环境、初始/预热后树快照见
[配对基准](benchmarks/language-neutral-views/README.md) 和
[结果表](benchmarks/language-neutral-views/results.md)。每用例保留 median/P95 两项
偏差，没有放宽 ±10% 口径。软件后端的 CPU paint/提交结果不能推广为 GPU presentation。

设置五项均通过；列表四项未达标。配对 median 偏差分别为 UPDATE +1,128.6%、
INSERT +823.5%、REMOVE +986.8%、MOVE +907.8%，P95 也全部超过 +10%。

非计时诊断记录 VM FFI/get/set/list/notify、连接峰值/释放、空闲帧；正式计时关闭诊断。
IR/手写空闲帧相同，根 VM 关闭后强引用均为 1。所有原生 setter/所有 C ABI、全部
内部 Rc 连续精确峰值尚未完整采集，已在基准报告明确区分，不能冒充完整第 9 节计数。

## 遗留问题与推荐

1. 列表 UPDATE/INSERT/REMOVE/MOVE 的完整记录遍历/转换仍超预算。推荐先在现有
   协议内做 typed row 缓存及 delta 适配，再讨论 D7 bulk ABI；本次未擅自扩大协议。
2. ABI 45 新增 reload 的冻结例外须审阅，已有 44 签名/POD 不变，原因见 S5。
3. 发布阶段系统性 bad_alloc 的全值回滚未证明。推荐增加原生属性提交的强异常接口和
   分配失败注入后再声称完整事务；当前已测解析/schema/Host/读及准备阶段回滚。
4. C++ Mount 保留 CSS watcher，结构 reload 当前通过独立 C View/Rust View；若要求
   旧 build_X/Mount 对象也直接结构重载，需补明确的 root republish/所有权入口。
5. 全部 native setter/FFI 和内部 Rc 峰值计数仍需非 ABI 的内部诊断接入；不要增加冻结
   ABI 的临时计数函数。是否将这些计数与 OOM 作为最终通过的前置条件，由 Claude 裁决。

## 模块、编码和产物

新增手写源文件全部不超过 500 行；Rust lib.rs 从 10,944 行减少到 10,714 行，
迁出 234 行 dispatcher/任务回收、增加 4 行接入。遗留大文件只做版本常量/相关测试
必要修改，无关控件按用户要求未重写。UI declarative 仍小于 500 行。
新增源文件最大为 305 行。UTF-8 无 BOM；最终对 tracked diff 及新增文本同时检查
替代字符和连续问号，并修正产物清单生成时一处中文锚点编码问题。

没有重新配置 capture，没有独立 CMake 配置目录；固定输出为 capture、codex-views、
cargo-target。编译使用 capture compdb 的真实 MSVC 参数，去掉增量链接/PDB 写入冲突，
保留 `/O2`。这些输出需保留到 Claude 最终复核；最终产物审计及清理见
[产物清单](benchmarks/language-neutral-views/artifacts.md)。批准预算为项目跨盘 30 GiB。

MyStudio 写入**未同步**，遵守“不调用审批工具”；图工具需要审批，已回退本地检索。
旧清理的自动审批拒绝没有换工具绕过，没有因此停工。
