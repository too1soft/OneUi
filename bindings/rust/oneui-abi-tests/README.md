# C / Rust ABI 检查

该开发工具把 `include/oneui/oneui_c_api.h` 与当前 `oneui-sys/src` 比较，加载实际
OneUI 库验证 ABI 版本。普通应用不依赖该 crate，workspace 默认成员也不包含它。

在 OneUI 根目录执行（复用 `bindings/rust/target`）：

```powershell
python scripts/test-rust-abi.py --negative --report out/rust-abi-checks.json
```

Windows 默认使用 `build/msvc-bundled-static-release`；可用 `--oneui-lib-dir`
或 `ONEUI_LIB_DIR` 指定已有库目录。其他平台需提供匹配目标的库和 C 编译器。
脚本保留 `CARGO_TARGET_DIR` 的显式设置。也可以配置库路径后直接执行：

```text
cargo run --manifest-path bindings/rust/Cargo.toml -p oneui-abi-tests --locked
```

检查内容：

- `ctest` 比较大小、对齐、字段偏移、字段类型、常量及按值传递的往返结果。
- 从展开后的真实 Rust AST 生成无类型转换的 C 函数指针检查，编译警告视为错误；
  所有 `supported-symbols.txt` 中的函数必须有检查，具名和内联回调也比较签名。
- 运行中的检查函数通过有类型的 volatile 指针引用全部符号，确保它们实际参与链接，
  不因无调用的辅助函数被链接器清除而漏掉缺失导出；不会调用这些控件接口。
- 不完整的窗口／控件句柄仅以指针传递，跳过不适用的结构体布局检查。
- C enum 的符号属性由实现决定；Rust 使用整数别名容纳未知值。保留大小、对齐和
  往返检查，不强制 enum 的有符号性。不要改为具有无效判别值风险的 Rust enum。
- Windows 上 C 的 DLL 入口和 Rust 的导入跳板可能有不同地址，所以跳过函数地址
  相等检查；类型检查、链接和数据布局检查仍执行。

`--negative` 在现有 target 内临时复制 FFI 源码，分别改变返回类型、值／指针参数、
具名回调参数和同大小字段顺序。前 3 个必须在签名编译时被拒绝，最后一个必须在
运行时布局检查中被拒绝。每个 fixture 自动删除，最后恢复真实源码生成的检查程序。
JSON 报告覆盖写入，仅保留紧凑诊断；产品源文件不会被改动。

依赖与限制：

- 工具固定 `ctest = 0.5.1`，最低 Rust 1.88，另需目标 C 编译器和匹配的 OneUI 库。
  `ctest` 在自己的 rustc 子进程中启用宏展开所需的 bootstrap，不更改系统工具链。
- 本次实测平台是 Windows x64 / MSVC；跨平台声明仍需对应平台实际编译和运行。
- 保留原有 `check-abi-sync.ps1/.cmake` 作为轻量版本／符号检查。本工具补充类型及
  布局保障，不替代控件生命周期、编码、异常边界和真实 UI 回归。
- 暂不把手写 FFI 全量迁移到 bindgen。它需要额外的 libclang，并需审阅生成类型和
  safe wrapper 的兼容成本。先以独立检查覆盖维护风险，再分批评估生成策略。

工具依据：[ctest 文档](https://docs.rs/ctest/0.5.1/ctest/)、
[bindgen 工具链要求](https://rust-lang.github.io/rust-bindgen/requirements.html)。
