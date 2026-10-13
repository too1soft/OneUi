# P14 TimeSeriesChart 模板合同

状态：待 review；不代表蓝图确认或用户验收。

资源页使用现有原生 TimeSeriesChart。新增声明属性只暴露旧页面已使用的范围、网格、轴标签、采样位置、填充、平滑、端点及 plot insets；结构化序列允许 valid=false 缺测，不把缺测绘制成零。inspection 事件传原生样本索引和 pinned 状态。表格 P15 复用已有 Table 合同，没有新增表格能力。

注册表追加属性 ID，series 使用兼容内置记录的结构投影；提升 Chart COMP 版本，重载恢复默认值。编译器正反例及实际 IR→adapter→Chart 测试覆盖 range/grid、缺测、hover 事件、RecordingCanvas 网格笔宽和记录 lease 配平。

在原 build/capture 中经 vcvars64.bat 加载 MSVC，执行 ninja -t clean + ninja；本批因 header/.inc 依赖和已有 inline COMDAT 风险共 clean 三次，无 reconfigure、无旁路 build。最后定向 ctest 10/10 PASS。初次测试错误要求 area=false 时仍有 fillRect，已修为真实水平网格线断言并再次全 clean。

完整检查中的 SDK 全量 ctest 82/82 PASS（29秒），native436和库684亦通过；应用56组合中图表全部通过，P15性能窄窗口夹具修复另见应用报告。外部 windows-compat hash audit 仍按现有脚本排除。

消费方：iShell MonitorChart.partial / MonitorOverview / MonitorResources，资源和进程真实运行截图由 iShell P14/P15 报告固定引用。

MyStudio project -l9ttZtpJcpAUs6fUrkp，task 1jLP9gjI2RfejKPoLpXX，execution rO8iqWR3LOJ8wP-0hrNJ。独立本地 session c2aa9f36-1fae-4ddc-8d91-b86898f34945，agent /root/p14_p15。蓝图未确认，无 verified 声明。
