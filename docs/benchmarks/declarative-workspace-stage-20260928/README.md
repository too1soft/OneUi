# 声明式工作区提交前回归 · 2026-09-28

- `sdk-tests.txt`：重建受影响目标，23/23 CTest 通过。
- `performance-lab-tests.txt`：`examples/performance_lab/build.ps1 -Test` 完整执行成功；含代码／模板、热更新、页面组合、渲染、动效与诊断。
- `rust-tests.txt`：使用当前终端工作区的 OneUI DLL／导入库执行 `cargo test --manifest-path bindings/rust/Cargo.toml --workspace --locked`；100 项绑定和13项布局通过。预期 panic 测试会打印被捕获的异常，最终失败数为0。
- 终端工作区四组原生测试及哈希见 [工作区验收记录](../terminal-workspace-v4-20260928/result.json)。

SDK 执行方式：通过 VS x64 vcvars 环境构建 `examples/declarative/build` 中的对应测试目标，再用 CTest 执行。所选23项名称及环境路径保留在日志中。此前的标题栏无障碍断言已改用公开 presentation API，并在最终日志验证通过。

不将此结果解释为全仓库／跨平台验证，也不作为新的 CPU 或内存对比数据。
