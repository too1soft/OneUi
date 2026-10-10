# WP3 语言无关 View 配对基准

Windows x64 / MSVC 14.50；capture RelWithDebInfo `/O2 /DNDEBUG` 原生库，Rust
`cargo --release`。同一个 exe、同一个 DLL，交替选择 IR 或手写路径；环境与二进制/IR
SHA-256 见 environment.json，原始时间样本及布局快照在 raw/。

## 口径

- 设置探针直接复用示例的 Settings VM 和同一 schema，手写参考为 bench/manual.rs。
  五个用例是 UI 输入、dispatcher、scalar、text、12 主题选择。不是用完整设置页的大量
  绘制稀释 adapter 成本。表格两条路径使用同一 Table VM，1,000 行，每行一个字符串。
- 软件后端 `ONEUI_ENABLE_GPU=0`，640×600，DPI/字体相同；CPU 实际重绘包含后端提交，
  不声称 GPU 或屏幕呈现延迟。本轮两张真实示例截图也使用软件后端。
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

冷加载 construction 和首帧数据保存在 raw/*-runs.csv。当前环境文件中的二进制 hash
对应本轮完整配对样本，不使用上轮或插桩构建的 hash。

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

## 列表性能修正

全表转换路径已替换为按稳定 key 缓存的 UTF-16 行及原生结构性 delta。
优化前后排他计时、ABI 44、回调语义、按钮描边诊断和回归证据见
[listperf.md](listperf.md)。results.md 与 raw/ 覆盖旧配对样本；旧结果保留在 Git 历史。
没有新增 bulk ABI，median/P95 的 +10% 门槛不变。

复现命令见 [示例 README](../../../examples/language_neutral_views/README.md)。
图像为真实原生帧：light-wide.png、dark-narrow.png；D8 不要求 iShell 像素同构。
