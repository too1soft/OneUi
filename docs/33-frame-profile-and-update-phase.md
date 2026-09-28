# 绘制归因与更新阶段

日期：2026-09-26。本阶段增加 Windows 渲染阶段追踪，并修复综合示例在 paint 中更新指标引起的重绘反馈。没有减少原有粒子、曲线点数，也没有通过吞掉框架失效请求来掩盖问题。

## 追踪方式

运行应用前设置 `ONEUI_RENDER_TRACE=1`。`ONEUI_RENDER_TRACE_FILE` 可指定绝对日志路径，未指定时写 stderr。日志新增：

| 字段 | 范围 |
| --- | --- |
| backend | 当前窗口实际使用 opengl 或 raster |
| avg_layout | View 布局及 Yoga 安排坐标，ms/paint |
| avg_text_layout | Layout::make，包括缓存查找和未命中构建 |
| avg_fill | Canvas::fillRect 调用 |
| avg_path | Canvas::strokePath 调用 |
| avg_submit | GPU 表面复制、flushAndSubmit、SwapBuffers 调用时间 |
| paint_invalidations | 绘制过程中到达窗口的失效请求数 |

原有 avg_content、avg_paint、avg_blit 与文字绘制计时保留。GPU 初始化日志记录 GL renderer/version，以及已建立上下文后的关键失败阶段。backend 代表实际路径，不是应用是否曾经请求 GPU。

`src/internal/frame_profile.h` 为内部、线程局部、RAII 计时工具。只有启用 session 时读取时钟；同类别嵌套只计外层，各类别可能重叠。因此不能将布局、文字排版等时间再加到包含它们的内容绘制时间上。提交是 CPU 侧调用耗时，并非 GPU 时间戳。

## 应用应在绘制前更新状态

原 Demo 的 paint 调用 Label::setText，Label 的 invalidate 会再请求绘制。若每次 paint 又改变 FPS/帧间隔文字，就会形成大量短间隔局部绘制；虚高的 UI 回调次数不能代表屏幕帧数。

组件 Demo 将指标和倒计时的更新移到 tickAnimations，然后统一 invalidate。修改、测量和绘制的顺序变为：

```text
动画回调 → 更新指标文字 → 合并失效区域 → 布局和绘制 → 提交
```

遥测仍在绘制入口采样；展示的指标使用此前采样结果，至多延迟一次动画更新。默认仍为约 10 Hz 文本刷新。暂停时的重置提示在对应操作中立即更新。开始/结束采样仍会改变控件状态，它们的一次性失效不计入稳定动画段。

`--metrics-in-paint` 可恢复旧路径，配合 `--metrics-every-frame` 复现压力场景。SDK 不限制合法 paint 回调中的失效，也未实施全局失效丢弃。

## 同构建实测

完整报告为相邻工作区 `GPUI_demo/component-demo/PROFILING.md`；脚本 `GPUI_demo/profile-components.ps1` 使用同一 EXE/DLL、同一渲染后端，交替执行四种模式。日志与哈希可追溯。

本机当前明确使用 RTX 5080 OpenGL。两轮测试的标准负载逐帧指标模式，CPU 占整机比例约 3.718% → 1.958%，相对下降 47.3%。默认 10 Hz 模式收益很小；高压负载的 CPU 基本持平。修复后的所有稳定日志桶中，paint 内失效请求为零，局部重绘为零。

默认高压负载每次 paint 平均约 8.429 ms，其中填充图元 3.751 ms、提交/等待 2.598 ms、布局 0.023 ms、文字排版 0.058 ms。当前证据将后续优化重点指向大量图元的调用与提交，而非布局。追踪逐粒子调用有开销，上述不是关闭追踪时的绝对性能承诺。

保留旧构建的日志显示非零软件 blit，当前构建则为 OpenGL；不能跨这两个渲染路径将差异归功于本次修复。两轮测试也不足以证明小幅 P95 变化有统计意义。

## 验证

- 7 组 C++ 测试通过：frame_profile、control、stack、authoring、yoga、component、win32_window_loop。
- ABI 清单检查通过，本阶段未添加公共 C ABI。
- 16 次应用实测完成，汇总脚本会拒绝混合后端或修复路径稳定段存在 paint 失效的结果。
- 原生帧捕获成功并检查界面；本轮 UI 自动化未能激活窗口，未将快捷键人工式回归计为通过。

下一阶段可用相同追踪验证粒子绘制批处理，但需要保持重叠顺序、颜色、抗锯齿与剪裁效果一致，并独立观察输入响应和显示呈现时间。
