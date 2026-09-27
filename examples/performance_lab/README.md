# OneUI 性能实验台

同一个 Windows 原生应用里体验图表、粒子、虚拟列表、连接管理和默认组件。所有连接与保存均为本地模拟，不访问服务器。初次使用先按[仓库 README](../../README.md#运行-demowindows)准备依赖。

在仓库根目录执行：

```powershell
.\examples\performance_lab\run.ps1 -Build -Test
.\examples\performance_lab\run.ps1 -Components -Dev
.\examples\performance_lab\run.ps1 -Connections -Entry template -Dev
.\examples\performance_lab\run.ps1 -Editor -Entry code -Compact
.\examples\performance_lab\run.ps1 -Load heavy
```

每条运行命令打开一个窗口；对比性能前关闭其他实验台窗口。普通启动不监听文件，`-Dev` 才开启样式热更新。`-Entry code|template` 选择连接列表和编辑页的写法，组件展示页统一使用 `Gallery.one`。`-Compact` 使用紧凑密度。

## 看什么、改哪里

| 体验 | 操作 | 源码 |
|---|---|---|
| 综合性能 | 切换场景、负载、暂停，观察 CPU／内存／回调间隔 | [main.cpp](main.cpp)、[plots.hpp](plots.hpp) |
| 默认外观与自适应 | `-Components`；切主题、密度；缩到 640px；切表单／表格／反馈 | [views/Gallery.one](views/Gallery.one) |
| 完整列表与编辑流程 | `-Connections`；搜索、筛选、双击编辑、新建、删除、未保存确认 | [connection_workspace.hpp](connection_workspace.hpp) |
| C++ 表单与异步保存 | `-Editor -Entry code`；校验错误、保存、失败重试 | [connection_editor.hpp](connection_editor.hpp) |
| 模板里的同一业务 | `-Connections -Entry template` | [views/Connections.one](views/Connections.one)、[views/Editor.one](views/Editor.one) |
| 修改外观即时预览 | `-Dev` 后修改外部 CSS 或 `.one` 的 style 块 | [connections.css](connections.css) |

页面使用 SettingsPage/ListPage、FormGrid、FormRow 与 Toolbar。FormGrid 按约 320 逻辑像素列基准自动分栏／换行，字段标签上置；不用逐个 `setFrame`。它不是完整浏览器 Grid，不支持跨列。窄窗口与长中文交由布局和文字测量处理。

模板结构、事件和 C++ 逻辑变化要重新构建；CSS 错误保留上一次有效样式。组件页的加载外观是模拟状态，完整异步保存请到连接编辑页体验。展示页第一次打开时才创建，之后保留。

## 测量与截图

```powershell
# 关闭所有 oneui-performance-lab / gpui-performance-lab 进程后运行
.\examples\performance_lab\compare-entries.ps1 -Rounds 3 -Cycles 200
.\examples\performance_lab\capture-gallery.ps1
```

输出在忽略提交的 `results/` 下。每轮 50 次预热，随后 150 次列表搜索／编辑／取消；代码与模板交替先运行，使用同一 exe/DLL、窗口和数据。测量脚本拒绝与其他实验台同跑。保存的二进制哈希可帮助确认测的是哪个构建。

CPU 百分比是整机逻辑处理器归一化后的进程 CPU；paint 是 CPU 侧控件绘制遍历，不是 GPU 执行时间或显示帧率。综合图表页持续动画，不能用连接页空闲结果代表它。详细[测量结果与边界](../../docs/40-declarative-stage-validation.md)。

截图来自 OneUI 原生离屏客户端捕获，可复现宽／窄、浅／深和紧凑密度。它不证明物理显示器 125%／150% DPI 或系统 IME 候选窗口已通过验收。
