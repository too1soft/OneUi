# P16 监控模板所需原生合同

状态：review，未代替用户验收。仅 codex/terminal-tools，不合并。

Scroll 暴露既有 scrollbar-inset；VirtualList 暴露 trailing-width/gap 和行 trailing_rgba，保留监控连接状态颜色及紧凑列宽。SegmentedControl 消费已有 Tabs text-inset 样式，窄屏“监听/UDP”完整显示。C API element_metrics 返回 VirtualList 真实 scroll_y/max_scroll_y，供列表恢复与真实滚轮验证。没有扩展图表产品能力。

VirtualList 视口扩大后在绘制与命中测试前夹紧旧滚动偏移，避免显示/点击错行。该旧文件 969→972 行，属于原有滚动行为的小缺陷修复例外；未新增职责，未借本卡重构旧渲染和输入模块。后续拆分边界为富行绘制与输入命中，需保留当前原生行为测试。

新增实际验证：零 inset 配七像素滚动槽；原生富行 trailing 颜色/宽度/间距；扩大视口后原生点击首行、绘制滚动回零；199px SegmentedControl 的 UDP 文本可用宽度；C API 原生 VirtualList 100 行的滚动 180/max5800。runtime 与 C API 定向 2/2 PASS；已在原 capture 全 clean 重建 4 次，避免 inline/header/.inc 依赖漏跟踪使用旧 COMDAT。最终第3次完整 check 的 SDK ctest 82/82 PASS（原 capture，28s）；应用 no-run 成功，首轮88组合81通过，受影响12组合修正夹具后12/12通过，最终两张真实图目视通过。两次早期完整check均SDK82通过，但应用模板编译分别因未消费的text-inset、content-background别名失败；均已修复，未将未执行项记为通过。

负责人验收：在应用中查看连接列表深浅主题，筛选与滚动后双击详情；改变面板宽度确认文字完整、状态颜色正确、选择和滚动恢复；不以自动测试代替人工验收。

关联应用报告：D:/ishell-par/tools/ishell_oneui/docs/codex/reports/P16.md。MyStudio 蓝图范围未确认，任务仅待 review，无 done/verified。

MyStudio 关联 SDK task 3wf6g6IAB1kwcbSOjKzL、execution TEm1iE-0XqeygGWfgT-q；独立本地工作 session f2dc5d49-9a81-4b43-afbb-a889ff9e7dc2。两图文档 VVWybl04PYeKamGA3qOw v1 / mIkHc1mcjaR0ZKkFUfKj v1 在应用项目。
