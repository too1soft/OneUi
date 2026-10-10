# WP3 语言无关 View 配对基准

Windows x64 / MSVC 14.50；capture RelWithDebInfo `/O2 /DNDEBUG` 原生库，Rust
`cargo --release`。同一个 exe、同一个 DLL，交替选择 IR 或手写路径；环境与二进制/IR
SHA-256 见 environment.json，原始时间样本及布局快照在 raw/。

## 口径

- 设置探针直接复用示例的 Settings VM 和同一 schema，手写参考为 bench/manual.rs。
  五个用例是 UI 输入、dispatcher、scalar、text、12 主题选择。不是用完整设置页的大量
  绘制稀释 adapter 成本。表格两条路径使用同一 Table VM，1,000 行，每行一个字符串。
- 软件后端 `ONEUI_ENABLE_GPU=0`，640×600，DPI/字体相同；CPU 实际重绘包含后端提交，
  不声称 GPU 或屏幕呈现延迟。真实示例截图另用 OpenGL+Skia Ganesh。
- 关闭 watcher/追踪/应用动效。关闭动效通过测试专用 Windows/MSVC 入口调用已有
  `oneui::setMotionEnabled(false)` 导出；未增加 C ABI，没有 C++ VM 胶水。
  未关闭动效的初次试跑已作废，并由最终完整数据覆盖。
- t0 在 UI 线程状态修改/dispatcher 投递前，t1 在携带更新的首次 WM_PAINT 处理返回后。
  定时器不参与热路径；显式 ready 标记排除事件处理中、更新尚未完成的嵌套 paint。
  输入焦点通过原生鼠标事件建立，WM_CHAR/Backspace 必须实际改变 VM，否则断言失败。
- 每用例每路径预热 50 次，记录 1,000 样本，5 个交替顺序配对轮次。批量增删每批 10 项
  交替，INSERT/REMOVE 各有独立 1,000 样本；MOVE 每批移动 10 项。原始总计 90,000 样本。
- 每轮分别求 median 和 nearest-rank P95，再取五轮配对百分比的 median。
  不将所有样本混在一起替代配对；门槛仍是两个统计量均 ≤+10%。

**结果表见 [results.md](results.md)，逐轮统计见 [summary.json](summary.json)。**
冷加载 construction_ns（包括窗口/VM/控件构建）和 cold_paint_ns（从开始显示到首帧）
单列在 `*-runs.csv`，没有混进热更新。

本次 construction / 首帧 median：设置 IR 3.313 / 4.487 ms，手写 1.473 / 4.108 ms；
表格 IR 47.425 / 5.745 ms，手写 2.224 / 5.561 ms。它们是各运行的独立初始化指标，
没有套用热更新 +10% 门槛。计时后仅修复了 IME commit 后 reload 失败的缓存恢复并新增
重载断言；该路径在本基准中关闭且不执行。环境文件的 SHA-256 保留实际采样二进制，
不将后来重链的 DLL hash 冒充采样 hash。

## 工作等价与生命周期证据

check_workload.py 比较初始与预热后实际原生布局快照：设置页两边均 43 个节点，表格均
一个原生 Table；比较树、几何、字号/字重、style、禁用/选中/显示、文本和全部表格数据。
16 对快照通过（每个用例初始/预热后）。表格在插入时 1,010 行、删除后 1,000 行；
Native Widget 数不随数据行数增长。框架类名和诊断 source metadata 不参与视觉比较。

另一次非计时构建启用 `bench-diagnostics`，每组 50 次预热+5 次更新；
`callback-counters.txt` 保存 VM FFI/get/set/list/notify 计数（包括构建和关闭）。计时构建
未启用该 feature。设置页连接峰值 13；表格 UPDATE/MOVE 为 2,002，插入时 2,022，
全部关闭后为 0。所有正式运行的根 VM `Rc::strong_count` 在关闭后均为 1（调用者）。
这些是实测连接/租约及关闭后强引用，不冒充所有内部 Rc 的连续精确峰值。

非计时 500 ms 空闲观察：UI 输入两条路径均 0 帧；设置页其余四用例均 1 帧；表格均
0 帧。IR 相对手写没有额外空闲帧。这里仍含同样的原生光标/焦点维护，没有把它伪装成
产品关闭后的零活动。正式 paint 总数也在 runs.csv，UI 输入可能产生两次 paint。

VM 回调计数不等于全部原生 setter/所有 C ABI 的总数：手写路径的 VM callback 计数为
0，但仍通过普通控件 C ABI。当前没有修改冻结 C ABI 增设 native setter 计数入口，
因此“所有 native setter/FFI、全部强引用精确峰值”仍是第 9 节证据缺口，不能据此宣称
完整计数预算已验证。

## 列表超预算的边界

列表四个用例必须逐项看最终结果表，不因设置用例通过而略去。当前实现的属性依赖避免了
重新读取所有 VM 值，计数也支持这一点；但 `src/views/record_properties.cpp` 仍遍历
整个列表，复制/UTF-8→UTF-16 转换和重建 ui::TableRow，然后交给既有 KeyedTable。
单行 UPDATE 在原生控件层虽只 updateRow，前面的全列表适配并未消失；批量结构变化也
经过完整记录整理。这里是代码路径分析，没有声称做过采样 profiler 的归因。

推荐下一步在现有协议内缓存类型化行、按稳定 key 应用 delta、只转换 dirty 字段，先
降低全表整理成本；D7 的 bulk ABI 扩展仍需另行评审，本次没有擅自添加。

复现命令见 [示例 README](../../../examples/language_neutral_views/README.md)。
图像为真实原生帧：light-wide.png、dark-narrow.png；D8 不要求 iShell 像素同构。
