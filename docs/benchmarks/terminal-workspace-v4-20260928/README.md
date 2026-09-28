# 工作区 v4 布局组件验证 · 2026-09-28

Windows x64 / Release / Skia，当前工作树构建；未提交版本。`result.json` 保存四组原生回归结果和二进制 SHA256。原生回归覆盖新增收起、分屏、搜索、多选恢复、监控区域切换，以及此前已有的输入、所有权和生命周期检查。`declarative-tests.txt` 保存组件与模板编译器测试结果（2/2 通过）。

运行：`./examples/terminal_workbench/build.ps1 -Test`。增量回归：`./examples/terminal_workbench/check.ps1`。默认截图 `--capture <path>`；其他场景 `--dark --scene split|files|processes|collapsed --capture <path>`，文件窄屏额外加 `--narrow`。

截图在 `docs/images/terminal-workspace-v4/`，全部来自本轮原生客户端 `captureFramePng`；没有生成式图片或网页截图混入发布示例。PNG 同目录 `provenance.json` 记录来源。参考网页只在 GPUI_demo 的本地审阅目录中读取，没有复制到 OneUI SDK。

空闲回归在模拟采样暂停时检测重绘数为 0；这不是 CPU/内存基准，不声称相对之前性能提升。尚未验收真实 IME 候选窗口、多显示器 DPI、真实网络与 PTY；构建日志中原有数值转换及 Win32 返回路径警告不属于本轮修复范围。
