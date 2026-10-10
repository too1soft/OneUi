# 语言无关的设置页

`.one` + `.oneschema` + 纯 Rust VM；没有 C++ 页面胶水。示例只模拟当前进程的设置，
不会改写用户配置、启动项或文件。12 项主题用 v-for；语言、标题栏、密度、关闭行为、
启动项、字体/背景、状态、错误及异步保存均经同一 VM/通知协议。

Windows x64，先用开发者终端构建 OneUI `build/capture`（包含 viewc）。在本仓库根目录：

```powershell
$env:CARGO_TARGET_DIR = "$PWD/build/cargo-target"
$env:ONEUI_LIB_DIR = "$PWD/build/capture"
$env:ONEUI_VIEWC = "$PWD/build/capture/oneui-viewc.exe"
cargo run --release --manifest-path examples/language_neutral_views/Cargo.toml
cargo test --manifest-path examples/language_neutral_views/Cargo.toml
```

`--smoke` 验证异步保存和窗口退出；`--dark --narrow` 打开暗色窄窗口；
`--capture path.png` 保存真实原生帧后退出。模拟失败按钮使下一次保存显示错误，
再次保存恢复；后台请求只传持有值，过期 generation 的结果丢弃。

开发重载先编译 IR，再运行 `--watch-ir`：

```powershell
& $env:ONEUI_VIEWC --emit ir --input examples/language_neutral_views/SettingsBasic.one --schema examples/language_neutral_views/SettingsBasic.oneschema --output build/capture/SettingsBasic.oneview
cargo run --release --manifest-path examples/language_neutral_views/Cargo.toml -- --watch-ir "$PWD/build/capture/SettingsBasic.oneview"
```

修改模板后再次执行 viewc，watcher 在写入稳定 200 ms 后替换 IR。schema 改变需重建程序；
VM 状态保留。这里 watcher 不自动启动编译器。

性能程序同一个 exe 用 `--bench 输出目录`，默认每用例 50 次预热、1,000 样本、5 轮
交替配对；`ONEUI_ENABLE_GPU=0` 固定软件后端。必须关闭其他测试/构建后采样。
比较方为 bench/manual.rs 的手写 Rust 参考及同一 Settings VM，列表另用同一 Table VM。
`bench-diagnostics` feature 仅用于另一次不计时的回调/空闲帧检查，正式计时禁止启用。
基准通过 Windows/MSVC 的已有 C++ 动效导出关闭动画，未新增 SDK C ABI，也不进入 VM。

```powershell
cargo build --release --manifest-path examples/language_neutral_views/Cargo.toml
$env:ONEUI_ENABLE_GPU = '0'
& build/cargo-target/release/language-neutral-views.exe --bench docs/benchmarks/language-neutral-views/raw
python examples/language_neutral_views/bench/check_workload.py docs/benchmarks/language-neutral-views/raw
python examples/language_neutral_views/bench/summarize.py docs/benchmarks/language-neutral-views/raw
```

性能口径和未达标项见 [基准报告](../../docs/benchmarks/language-neutral-views/README.md)。
