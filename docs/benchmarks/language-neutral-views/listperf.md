# WP3 列表性能修正

基线：Claude 已提交并复核的 `b9525c1`。C ABI **44** 保持冻结，没有 bulk ABI，
没有改动 Rust VM、手写参考页、负载或统计门槛。历史 S4/S6 报告中的 ABI 45 保留为历史记录。

## 先测量，再修改

`wpr -status` 在本沙箱返回 `0x80070514`（缺少特权），未获得 ETW 采样。
因此使用本次要求允许的分段计时插桩，不把它描述为采样 profiler。
`src/internal/view_profile.h` 在 `ONEUI_VIEW_PROFILE` 定义下使用 MSVC steady_clock/QPC；
嵌套阶段从父阶段扣除整个子阶段，记录**排他耗时**。正常构建宏完全为空。
每个最外层操作结束才写一次聚合记录，没有逐行文件 I/O。

采集使用 capture 的 RelWithDebInfo `/O2 /DNDEBUG` 编译命令重放并重链 DLL，
同一个 release Rust 基准，表格每用例预热 50 次、200 样本、1 轮。
`profile-before.csv` 在任何列表算法修改前采集；`profile-after.csv` 在优化后采集。
首轮记录器因后台线程截断文件而作废，修正延迟打开和用例标签读取后完整覆盖，未混入证据。
`summarize_profile.py` 去掉初始化 flush/frame 和预热；INSERT/REMOVE 各 200 次，分段表按交替组汇总。
正式验收仍独立采用无插桩的 5×1,000 配对样本。

| 排他阶段（ms，均值） | UPDATE 前→后 | INSERT/REMOVE 前→后 | MOVE 前→后 |
|---|---:|---:|---:|
| list delta / 租约缓存整理 | 0.6234→0.0210 | 0.6691→0.0485 | 0.6357→0.0335 |
| VM 外部读取回调（含借用值复制） | 0.0081→0.0034 | 0.0126→0.0066 | 0.0060→0.0018 |
| UTF-8→UTF-16 转换 | 17.5197→0.0003 | 19.8809→0.0009 | 17.9909→0.0001 |
| TableRow 构造/记录适配（扣除上列子阶段） | 0.5509→0.0082 | 0.7414→0.0135 | 0.6334→0.0068 |
| KeyedTable / 原生更新 | 0.1569→0.0014 | 0.4483→0.0519 | 0.4209→0.0845 |
| flush 其余调度/提交 | 0.0114→0.0098 | 0.0286→0.0159 | 0.0129→0.0100 |
| 布局（原生根 setFrame） | 0.000105→0.000043 | 0.000173→0.000051 | 0.000118→0.000051 |
| 绘制/后端提交（扣除布局） | 1.7932→1.7558 | 2.0855→1.9554 | 2.0419→1.9152 |

布局栏测的是固定尺寸 Table 的根布局入口；本控件的可见行几何计算在 paint 内，属于绘制栏。
这些排他均值不能与正式端到端 median/P95 相加比较。插桩证据确认主因是每次提交对全表
重复执行 `ui::wide`，其次是旧 delta 路径重建全表映射/查找及行记录整理，不是 VM FFI 本身。
优化后 MOVE 仍有列标题适配的少量转换，未谎称整条路径绝对零转换。

第一轮最终二进制复测曾出现 `themes12` 的 P95 +14.0%（列表四项均已通过），没有采用
之前通过的结果代替它。随后补齐两处共用热路径：v-for 通过列表来源/提交 serial 判断
结构未变化，只更新脏子节点；已严格校验的 UTF-8 使用现有 `unicode::fromUtf8`，与普通
控件 C ABI 一致，避免 `wstring_convert` 每次构造 locale facet。同一转换器也用于缓存行
的首次/脏字段适配。原有外部值 UTF-8 校验不变，补充 CJK、非 BMP 与非法序列拒绝断言。
表中 after 数据在上述列表/转换算法完成后重新采集。随后差分测试抓到并修复了
`class` 必须走 UTF-8 token 入口的问题；它不涉及表格负载的更新热路径。
正式 raw/ 在全部修复和回归完成后，以完整五轮覆盖，
没有改 VM、缩减通知、调整用例、抽取表现最好的轮次或放宽 P95 门槛。

## 实现与协议核对

- `src/views/list_delta.cpp` 独立处理精确 delta；先验证 revision/范围/key 序列，再获取
  最终快照中新出现的 key。已有 Scope 直接移动/复用，移除项按提交时机释放。
  RESET 继续由 `lists.cpp` 校验整份快照，同 key 新 VM 仍按替换处理。
- `src/views/table_records.cpp` 按稳定 key 缓存 `ui::TableRow`、UTF-16 和子 Scope 身份。
  属性版本独立于 get 缓存，避免两个 Table 消费同一 VM 时，先消费的一方清除 dirty 后
  第二方漏更新。只读取/转换脏行和脏字段；原生写入和类型化缓存一起进入提交队列。
  INSERT 后又 REMOVE 的同批暂态 key 没有最终 VM，合并到最终快照，不虚构租约。
- `Table::insertRows/eraseRows/moveRows` 是内部 C++ 控件接口；普通 UPDATE 使用已有
  `updateRow`。SelectionModel 的索引映射保留选择、独立键盘焦点和 Shift 锚点，
  Table 保留像素 scrollOffset（超出新范围时夹取）。结构变更取消失效拖动目标并清除
  itemDragIds，沿用 setRows 的拖动取消语义。没有 C ABI 导出或版本变更。
- KeyedTable 从 `ui_declarative.h` 分离到 `ui_keyed_table.h`；原有 update 保留用于 RESET
  和旧 C++ State 快照路径。原文件减少 50 行。新增手写源文件均小于 500 行。
  Table/SelectionModel/KeyedTable 的数据成员、虚表与布局没有变化，仅增加非虚方法。
- 每个**被刷新列表属性**每次提交只读一次 `list_info`；两个 Table 共享同一属性也只读一次。
  release 精确 delta 不对已有 key 调用 `list_key/list_acquire`。debug 的身份审计仍有显式
  全量探针；RESET 按冻结协议必须重新核对 VM 身份，不能省略。
- 基准 UPDATE 的脏字段是 Row.cells（整个嵌套列表属性），所以这一个嵌套列表需要 RESET
  身份核对。这不等于重新取得根列表的 1,000 行。非计时计数：初始 2,001 次 acquisition，
  55 次 UPDATE 后 2,056 次；MOVE 55 次后仍是 2,001 次。根列表精确 UPDATE/MOVE
  的零额外 key/acquire 另有 C++ 断言。

2,002 个 connect = 根 1 + Column 1 + Row 1,000 + StringItem 1,000。
记录没有子 Widget，但仍可以单独通知 `Row.cells` 或 `StringItem.value`，协议并未声明它们
只能通过父列表 UPDATE 刷新。因此不能擅自去掉这些连接。连接只在首次获取租约时建立，
精确 MOVE/UPDATE 不重建；插入 10 行峰值 2,022，全部关闭后 0。本次保留这种通知能力，
测试直接修改单元格并通知子 Scope，也验证两个 Table 都刷新。若将来提供明确的“只接受父列表
通知”的记录契约，才可安全省略这些连接；本次性能已经不依赖该改变。

热重载在复用原生 Table 时丢弃候选 Table 的未发布类型化缓存，并重建依赖观察关系。
回归覆盖 50 次重载后的 UPDATE、同 key VM 替换、失败读取后重试及连接/租约平衡。

## 按钮描边结论

这是**示例 CSS 不完整**，不是 IR 默认样式不同，更不是全部按钮获得焦点。
原生 `Button` 默认 variant 为 Primary；只改背景而不改 border 时，默认蓝色 border 保留。
示例使用自定义样式替换，未继承默认声明式主题里的 `border-width:0`。

`cpp_diff_tests.cpp::buttonBorderDifferential` 对冻结的 legacy 生成树和 IR 树应用原示例规则，
断言两边 `focusVisible()==false`、只有一个普通边框 stroke、颜色均为 Primary。
补全中性 border 后再次断言两边颜色一致。示例 CSS 现显式设置中性/主按钮边框和 accent
焦点 outline，保留键盘可访问的真实焦点反馈。最终 light-wide/dark-narrow 截图覆盖旧截图。

真实窗口复查还暴露了两条路径共用的 Win32 挂载问题：构树期间子控件已经具有父节点的
动画转发回调，但根尚未连接窗口，样式过渡的请求会丢失；仅延迟截图不能使其继续。
`WindowWin32::setContent` 现在在连接调度器后安排一次内容动画帧，恢复未完成的过渡。
`win32_window_loop_tests::testPreMountStyleAnimationResumes` 先构树并启动颜色过渡，再挂载
真实窗口；修改前确实失败，修改后通过。没有关闭产品动画或隐藏真实焦点描边。
截图入口以非阻塞 RAF 等待 350ms，并附实际布局/焦点 JSON；两张最终截图已目视核对。

## 验证与复现

- 未重新配置 capture，未改 CMakeCache，也未新建配置目录。
  直接 MSVC 重放受影响 `/O2` 命令，构建并重链 DLL、静态库和 **61 个可执行目标**。
  新 Table 行操作属于 core，新的记录/delta 适配属于 views，没有给无关测试补平台依赖。
- 全量 CTest **66/71**：既有 windows runtime audit 哈希失败 1 项；沙箱环境失败 4 项
  （ICU/Win7 recipe 的 Git sh Win32 error 5、notification `0x80070005`、托盘不可用）。
  首次未加载 vcvars 的运行额外缺 MSVC optional 头文件；修正环境后已完整重跑覆盖日志，
  不把该次错误归为产品失败或沙箱限制。
- 8 个原列表场景 + 11 个类型化记录/增量/重载场景，以及独立焦点/锚点和无效范围断言；
  C++ 控件树/样式/事件/输入/IME/列表差分、50 次保留和 50 次移除重载均通过。
  另有上述真实窗口挂载动画回归；Win32 大文件只增加必要唤醒和可关闭的计时接入，
  未扩展独立业务职责，属于本次小修复的明确规模例外。
- Rust View 9/9（包含 panic/reentry/drop/compile-fail），示例 3/3；
  ABI 1,229 布局断言、union 四标签 roundtrip、616 编译检查签名通过。
  本次未修改 Rust 绑定实现，未把上轮正常终端 108/108 冒充本轮重跑。

实际本地重放脚本是 `build/wp3-list-build.py`，在 vcvars64 环境执行：

```powershell
python -X utf8 -B build/wp3-list-build.py --profile  # 仅插桩采集
python -X utf8 -B build/wp3-list-build.py --tests    # 正常构建，全部链接
ctest --test-dir build/capture -j 8 --output-on-failure --output-log build/listperf-ctest.log
```

正式性能结果、原始样本和采样二进制 hash 见本目录 results.md、raw/、environment.json。
最终完整五轮 **9/9 通过**：列表 UPDATE 的 median/P95 为 +4.4%/+6.4%，
INSERT 为 -3.5%/-4.2%，REMOVE 为 -7.3%/-9.4%，MOVE 为 -5.4%/-6.6%；
五个设置用例全部通过，主题选择为 +2.9%/+7.1%。无需 bulk ABI。
偏差是逐轮配对百分比的中位数，不是结果表两个独立聚合值的直接商。
`summarize_profile.py` 只汇总插桩证据；正式 `summarize.py` 的 9 用例、5 轮、1,000 样本断言
和 +10% 门槛保持不变。构建、测试均结束后才进行正式采样。

MyStudio 已读取 workspace/project/blueprint 上下文（技能版本 5），未调用需审批写工具，
任务和交接记录**未同步**。代码图工具被审批策略阻止，采用本地源码检索。
