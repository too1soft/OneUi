#pragma once
#include <oneui/ui_declarative.h>
struct WorkbenchVm {
    oneui::State<bool> mainVisible{true},terminalVisible{true},commandVisible{false},searchVisible{false},filesCollapsed{false},filesExpanded{true};
    oneui::State<bool> pane0{false},pane1{false},pane2{false},pane3{false},resourceVisible{true},processVisible{false},connectionVisible{false},transferring{false};
    oneui::State<float> paneRatio0{0.5f},paneRatio1{0.5f},paneRatio2{0.5f},paneRatio3{0.5f},memoryValue{0.36f},diskValue{0.82f},transferValue{0};
    oneui::State<std::wstring> compactTab{L"terminal"},monitorTab{L"resources"},terminalQuery,searchResult{L"搜索当前可见内容"},fileCollapseIcon{L"down"},transferLabel{L"传输队列为空"},processDetail{L"选择进程查看详情"};
    oneui::State<std::vector<oneui::ui::TabItem>> compactTabs{{{L"terminal",L"终端"},{L"files",L"SFTP"},{L"monitor",L"监控"}}};
    oneui::State<std::vector<oneui::ui::TabItem>> monitorTabs{{{L"resources",L"资源"},{L"processes",L"进程"},{L"connection",L"连接"}}};
    oneui::ui::VmCommand showTerminal,showFiles,showMonitor,toggleCommand,toggleSearch,searchNext,toggleSplit,collapseFiles,newSession,download,upload,showStatus,showProcesses;
    oneui::State<bool> first{true},second{false},third{false},fourth{false},empty{false};
    oneui::State<bool> sidebarVisible{true},monitorVisible{true},filesVisible{true},narrow{false},wide{true};
    oneui::State<std::wstring> selected{L"edge"},connectionQuery,fileQuery,path{L"/srv/workspace"};
    oneui::State<std::wstring> feedback{L"本地演示 · 文件与监控均为模拟数据"},sessionInfo{L"edge-gateway  /  192.0.2.10"};
    oneui::State<std::wstring> cpu{L"24.8%"},memory{L"52.1%"},network{L"1.28 MB/s"},samplingLabel{L"开始采样"},samplingStatus{L"已暂停"};
    oneui::State<std::wstring> fileSummary{L"18 个项目"},themeLabel{L"深色"};
    oneui::State<float> fileRatio{0.54f},monitorRatio{0.70f};
    oneui::State<int> quickConnection{0};
    oneui::State<std::vector<std::wstring>> connectionNames{{L"edge-gateway",L"build-node",L"postgres-lab",L"cache-lab"}};
    oneui::State<std::vector<oneui::ui::TabItem>> tabs{{{L"edge",L"edge-gateway"},{L"build",L"build-node"}}};
    oneui::State<std::vector<oneui::TimeSeriesChartSeries>> cpuSeries,memorySeries,networkSeries;
    std::function<void(std::wstring)> closeSession;
    oneui::ui::VmCommand restore,clear,theme,toggleSidebar,toggleMonitor,toggleFiles,toggleSampling,send,up,newFolder,removeFile;
};
