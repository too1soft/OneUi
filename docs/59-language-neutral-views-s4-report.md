# WP3 S4：Rust 绑定

基线 `8bd0c2f`；2026-10-10。本页记录 S4 的交付和验证；最终联合结果见 S6 报告，检查点由 Claude 验收。

## 交付

- `bindings/rust/oneui-sys/src/ffi/view.rs`：完整 ABI 44 View POD、union、11 项 vtable 回调及 20 个导出。删除 pending 符号豁免。
- `bindings/rust/oneui/src/view/`：类型化 ViewSpec/ModelSpec、View::instantiate、ViewMount、ViewNotifier、HostRef、StableList、define_view_adapter。UI 所有者全部 !Send + !Sync；UTF-8 在退出 VM 借用前复制到 scratch。
- Rc 控制块管理根 VM/子 VM 租约。子 adapter 按属性、VM 身份和 model ID 缓存弱引用，RESET 保留同一 VM；同 key 换 VM 可替换。get/set/invoke/connect/disconnect/release 均隔离 panic；RefCell 重入返回 REENTRANT。
- `dispatcher.rs` 承接 WindowState、dispatch/dispatch_local、animation frame 和任务回收。lib.rs 保留 re-export，净减少 230 行（增加 4、删除 234）。无关控件未重写；tabs.rs 私有导入改为 super，修复主线程 harness 把 lib.rs 作为子模块时的编译。
- `tools/viewc/emit_rust.*`：trait、属性常量、类型化命令 enum、列表 item trait、模型 descriptor、同源 IR/hash、define_view_adapter 调用。Rust 产物没有逐控件创建代码。
- `oneui-view-tests` 独立主线程集成测试；C ABI 匿名 union 的额外 C/Rust 布局和值往返检查；生成器诊断测试。
- build_space 测试改用既有 temporary_workspace + addCleanup，避免 Python 3.14 Windows creator-only ACL 使测试清理失败。位置仍为 build/capture/viewc-tests。

## 与设计稿的差异

S4 本身未修改 C ABI：签名、结构布局及所有权契约保持 44。后续 S5 新增原设计延期的 reload 导出并升为 45，理由在 S5 报告单列；当前联合工作区以 ABI 45 为准。

View::notifier() 返回 VmResult<ViewNotifier>，原设计示意直接返回对象；这样能够表达 CLOSED 和分配失败。生成器使用 ModelSpec 的安全函数指针描述加宏构造 ViewSpec，替代示意宏的字段 DSL，类型约束相同。

VmContext::same_connections 仅比较连接身份，用于 VM 按传入 connect/disconnect context 管理多个 View，不暴露裸指针。StableList::reset 允许同 key 换 Rc，与 R4 一致。命令 enum 为 Rust 持有的值，UTF-8 参数不会跨回调借用。

## 实测

| 验证 | 结果 |
|---|---|
| 直接 MSVC /O2 编译 viewc | 通过（S4 14 个对象；加入 S5 emitter 后 15 个） |
| cargo check -p oneui | 通过 |
| Rust View 集成 | 7/7 场景通过 |
| compile-fail | 5/5：错误属性类型、只读 setter、错误命令参数、View Send、Notifier Sync；另有 1 个正向夹具通过 |
| 私有 adapter 回归 | 5/5：借用/UTF-8 scratch、panic/清零、重入、租约仅释放一次、稳定列表 delta |
| dispatcher + View 定向 | 11/11，通过主线程 harness |
| Rust 全部主线程场景逐项独立进程运行 | 107/108；唯一失败为托盘环境不支持 set_tray_enabled(true) |
| ABI | 1,229 项通过；615 个编译器检查签名通过；union 布局/四标签往返通过 |
| Rust emitter 诊断 | 4/4：只读/命令、点号名称冲突、命令 variant 冲突、关键字 |

最终联合复核：View 集成增加 S5 reload/watcher 后为 9/9；ABI 仍为 1,229 项布局检查，
函数签名增加 reload 后为 616；Rust 主线程逐项重跑仍为 107/108。build_space 修复后
全量 capture CTest 为 63/68，剩余 audit 和 4 项沙箱失败详见 S6。新增可选
view-diagnostics feature 仅统计 VM 回调/连接，默认关闭，不改变 C 协议。

集成覆盖：两个 View 共享 Rc VM、窗口先关后 drop、关闭后 notifier 返回 CLOSED、get/connect 失败回滚、disconnect/drop panic 后计数恰为 1、Host 保留完整 Rust callback owner、列表同 key/VM RESET 连续 50 次及同 key 新 VM 替换。panic 测试会打印 hook，但退出码为 0，unwind 未跨 C。

整组主线程测试最初得到 54/108，是托盘失败污染共享 Mutex 后连带失败。随后逐项进程隔离得到 107/108。原始结果在 build/s4-rust-regression.json。

capture 已配置的 68 项 CTest（vcvars 环境）得到 62/68。既有 audit 单列：windows-compat runtime/rust/src/canonical.rs 哈希不符。其余为 build_space 临时目录 ACL、ICU/Win7 fixture 的 Git sh CreateFileMapping 权限、通知 HRESULT 0x80070005、桌面托盘不可用。build_space 已修正，最终全量结果需更新。

未重新配置 capture，因此新增 Rust emitter CTest 尚不在这份 68 项注册表中；直接执行 4 项已通过。不把旧静态测试程序的运行冒充全部修改后的 clean 构建。

## 构建和产物

在 vcvars64.bat 环境运行固定脚本 build/wp3-build-viewc.py，读取 ninja -t compdb -x，保留 /O2，去掉 showIncludes、PDB/增量链接，将工具写入 build/codex-views。该目录不是 CMake 配置目录。没有重新配置或编辑 capture cache。

Cargo 环境：ONEUI_LIB_DIR=build/capture、ONEUI_VIEWC=build/codex-views/oneui-viewc.exe、CARGO_TARGET_DIR=build/cargo-target，均为本工作树绝对路径；CARGO_INCREMENTAL=0。命令：cargo test --offline --manifest-path bindings/rust/Cargo.toml -p oneui-view-tests --test views；cargo run --offline --manifest-path bindings/rust/Cargo.toml -p oneui-abi-tests。常规测试关闭 debug 信息，S6 单独 cargo --release。

用户已删除 58 个 capture 顶层 .ilk，不再列为受阻。WP3 至 S6 获批项目合计 30 GiB；最近审计 20,641,649,088 bytes（19.22 GiB），E 盘余 66,612,101,120 bytes，无超限。原仓库 build 为用户指定只读输入，未修改。

8 个既有 workspace ACL fixture 仍无法读取；旧 wp3-obj / menu probe 和系统 Temp 残留受阻清单沿用 S3 报告第 7 节。未换工具绕过已拒绝的删除。新 View/compiler 夹具使用固定根、上下文退出或 Drop/addCleanup 清理。

MyStudio 只读上下文已加载；写入未同步（用户禁止审批型工具）。图工具要求审批，未继续调用，使用本地检索。
