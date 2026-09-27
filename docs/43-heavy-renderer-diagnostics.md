# 高压粒子与渲染缓存诊断

本轮为高压负载补齐测量。没有改动粒子算法、绘制路径、GPU 缓存上限或动画调度，也没有实施粒子批量渲染。

后续已完成预计算与保序批量入口的独立实验，见[粒子绘制优化](44-particle-drawing-optimization.md)。本页保留优化前基线与当时结论。

## 自己复现

在仓库根目录执行，先关闭其他性能实验台窗口：

```powershell
.\examples\performance_lab\build.ps1 -Test
.\examples\performance_lab\compare-renderers.ps1 -Load heavy -Rounds 3 -Seconds 5
```

输出位于 `examples/performance_lab/results/renderers-时间/`。查看 `summary.csv`：`paint_mean_ms` 是一次绘制的 CPU 侧总耗时，`content_mean_ms` 和 `submit_mean_ms` 是其中的内容与提交部分；不要相加，也不要将其当作 GPU 执行时间。表格、图表、粒子与空闲分别运行在独立进程中。

高压档为 10,000 个粒子、每条曲线 4,000 个点，以及综合实验台的 100,000 行虚拟列表数据。基准中的 idle/table 仍使用 1,000 条连接的页面；高压档可能同时影响应用内其他已初始化数据，内存不是一个独立粒子控件的大小。

## 新的只读缓存接口

在窗口所属 UI 线程调用：

```cpp
const auto memory = window->rendererMemoryInfo();
if (memory.gpuCacheAvailable) {
    // Skia 管理的 GPU 资源字节数；不是进程工作集或整张显卡显存。
    const auto bytes = memory.gpuCacheBytes;
}
```

该查询不创建 surface、不触发绘制、不清理缓存，也不设置缓存预算。正常窗口状态展示仍使用 `rendererInfo()`，不会隐式查询缓存。实验台只在采样结束后查询一次，CPU 时间和绘制区间已经结束。

| C++ 字段 | CSV 字段 | 含义 |
|---|---|---|
| cpuCachesAvailable | cpu_caches_available | 当前后端是否提供 Skia CPU 缓存统计 |
| cpuFontCacheBytes | skia_cpu_font_cache_mib | 进程共享的 Skia 字体缓存；不包含所有 OneUI 文字对象 |
| cpuResourceCacheBytes | skia_cpu_resource_cache_mib | 进程共享的 Skia CPU 资源缓存；不等于全部堆内存 |
| gpuCacheAvailable | gpu_cache_available | 当前窗口是否有可查询的 Ganesh GPU 上下文 |
| gpuCacheBytes | skia_gpu_cache_mib | Ganesh 管理的 GPU 资源缓存，包括正在使用的资源 |
| gpuPurgeableBytes | skia_gpu_purgeable_mib | 上述缓存中当前可清理的部分 |
| gpuCacheLimitBytes | skia_gpu_cache_limit_mib | 缓存预算上限，**不是已经分配的内存** |
| gpuResourceCount | skia_gpu_resource_count | 上述 GPU 缓存中的资源个数 |
| retainedSurfaceBytes | retained_surface_estimate_mib | 保留绘制 surface 的宽×高×4 估算，不含驱动对齐和附属缓冲 |

C++ 字节字段使用字节，CSV 使用 MiB（1,048,576 字节）。不可用标志为 false 时，数值零不代表测到了零占用。GPU surface 可能已包含在 GPU 缓存统计中，不能重复相加；GPU 缓存、进程工作集和私有提交属于不同口径，不能相减后把差额认定为驱动开销。

Win32 提供上述统计；其他后端默认报告不可用。新增 C++ 虚函数后应一起重建应用与 DLL；C ABI 版本与 Rust C 绑定没有变化。

## 测量环境和范围

2026-09-27，Windows 10 Pro 19045，Ryzen 9 9950X3D（32 逻辑处理器），RTX 5080 / NVIDIA 581.80，MSVC Release x64，1320×900、100% DPI。每次预热 3 秒、采样 5 秒，三轮交替 CPU/GPU 顺序；关闭 watcher 与逐图元追踪，测量期间不编译。

基线使用 `e5b6b79` 的干净构建；诊断组在相同源码基础上加入本文的缓存查询。两组分别保存二进制 SHA256。计数是完成的绘制次数，不是显示器 FPS；图表／粒子使用原生动画调度，并非强行固定相同帧数。进程 CPU 按整机 32 逻辑处理器归一化，约 3.125% 可对应一个逻辑处理器持续繁忙。

## 三轮结果

下表为三轮平均，每个格子的顺序都是 CPU / GPU。基线与诊断组各 24 次采样全部完成，实际后端符合请求，未出现采样中切换或 DPI 变化。

| 场景 | 基线每次绘制 ms | 加入诊断后每次绘制 ms | 诊断组工作集 MiB | 诊断组私有提交 MiB |
|---|---:|---:|---:|---:|
| 空闲连接页 | 0 / 0 | 0 / 0 | 69.26 / 97.95 | 49.73 / 141.03 |
| 表格滚动 | 7.27 / 0.80 | 7.30 / 0.79 | 71.36 / 118.81 | 52.05 / 161.35 |
| 图表 | 11.61 / 3.18 | 11.64 / 3.20 | 67.91 / 124.20 | 50.11 / 173.17 |
| 10,000 粒子 | 31.35 / 5.39 | 31.46 / 5.33 | 62.42 / 132.48 | 44.32 / 184.69 |

两组粒子每次绘制均相差约 5.8–5.9 倍。诊断组 GPU 内容绘制 3.89ms（总绘制约 73%）、提交 1.24ms（约 23%）；CPU 内容绘制 30.72ms（约 98%）。这些是整页内容计时，不能仅凭比例认定全是粒子位置计算，也不能把提交耗时等同显卡执行时间。

诊断组粒子每轮平均完成 CPU 159 / GPU 900 次绘制，进程 CPU 为 3.12% / 3.37%。GPU 完成了更多绘制，因此这个 CPU 百分比不代表同帧率下 GPU 路径更耗 CPU。表格两条路径均为每轮 300 次绘制。两组所有空闲采样都没有额外绘制；空闲的 0ms 是没有样本，CPU 0% 受进程计时分辨率限制。

两组结果接近，但三轮短测试不能证明微小变化有统计意义，更不能当作一次性能提升。工作集和私有提交均为采样末尾值，不是峰值。

## 内存能解释到哪一步

以下仅为诊断组 GPU 模式的三轮平均：

| 场景 | Skia GPU 缓存 MiB | 其中可清理 MiB | GPU 资源个数 | Skia CPU 字体缓存 MiB |
|---|---:|---:|---:|---:|
| 空闲连接页 | 8.70 | 0.13 | 7.00 | 0.054 |
| 表格滚动 | 8.73 | 0.13 | 9.33 | 0.054 |
| 图表 | 11.87 | 2.51 | 25.00 | 0.105 |
| 10,000 粒子 | 15.99 | 7.38 | 211.00 | 0.094 |

所有 GPU 样本的缓存预算是 256MiB；实际缓存远未占满。Skia CPU 资源缓存统计为零，不代表应用没有分配图像、文字或其他内存。保留 surface 估算为 GPU 4.53MiB、CPU 8.125MiB：CPU 路径按现有增长和对齐策略预留容量，GPU 路径按当前窗口尺寸分配。

因此，不能把 GPU 模式较高的进程内存全部归因于“256MiB 缓存”，也没有证据说明仅下调预算就能省掉进程工作集差额。当前统计不覆盖驱动内部、所有宿主堆分配或完整显存驻留；还不能精确分摊这些项。7.38MiB 是当时可清理的 Skia 资源量，不是清理后进程工作集必然减少的数量。

## 原始记录与验证

- [基线汇总](benchmarks/heavy-renderers-20260927/baseline/summary.csv) · [环境和二进制哈希](benchmarks/heavy-renderers-20260927/baseline/environment.json)
- [缓存诊断组汇总](benchmarks/heavy-renderers-20260927/diagnostics/summary.csv) · [环境和二进制哈希](benchmarks/heavy-renderers-20260927/diagnostics/environment.json)

两个目录均保留逐轮 `renderer-performance.txt` 和 `content-paint.csv`。后者是根控件内容绘制样本，与 Window 的 content 计时范围略有不同。

上述数字只来自隔离工作区的构建。合回保留其他本地改动的原工作区后，也通过 `build.ps1 -Test` 和产品边界扫描（0 项）；该兼容性构建没有被混入本页性能数据。

Release `build.ps1 -Test` 通过编辑页、两种入口各 300 次连接流程、30 个作者入口场景和 50 次 CSS 热更新、36 个组件场景以及渲染诊断回归。补充只读测试确认：首次创建 surface 前查询不初始化后端，创建后查询不增加绘制／切换计数，软件路径不伪报 GPU 缓存。真实 GPU 数据通过上述 12 次诊断采样验证可用标志、资源数和缓存范围；其他平台未运行本轮硬件验收。

## 下一项优化如何验证

粒子控件每帧计算位置并调用 10,000 次带圆角的 `Canvas::fillRect`，底层走 Skia 抗锯齿 `drawRRect`。这是应用层调用数，**不等于 10,000 次 GPU draw call**，Skia 可能在内部合批。当前计时无法进一步区分位置计算、绘图命令记录和驱动等待的贡献。

下一步应对粒子路径做独立 A/B 实验，分别验证常量预计算与批量绘制；保留绘制顺序、颜色、抗锯齿、动画速度与相同数据，检查像素差异，再比较每次内容绘制／提交成本、完成次数和内存。未获得收益和等价性证据前，不改默认渲染路径，也不把减少工作量当成优化。

本页仅对比 OneUI 两条渲染路径，不是新一轮 GPUI 对比。短时末尾内存快照不证明泄漏或长期稳定性，亦不能替代低配机器和其他驱动验证。
