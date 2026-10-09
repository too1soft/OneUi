#pragma once
#include <oneui/controls/terminal_view.h>
#include <oneui/ui_declarative.h>

namespace lab::terminal {
struct FilesModel {
    oneui::State<std::wstring> fileQuery, path{L"/srv/workspace"};
    oneui::State<std::wstring> fileSummary, transferLabel{L"传输队列为空"};
    oneui::State<bool> transferring{false};
    oneui::State<float> transferValue{0};
    oneui::ui::VmCommand up, newFolder, removeFile, download, upload;
};
struct MonitorModel {
    oneui::State<std::wstring> monitorTab{L"resources"}, processDetail{L"选择进程查看详情"};
    oneui::State<std::wstring> cpu, memory, network;
    oneui::State<std::wstring> samplingLabel{L"开始采样"}, samplingStatus{L"已暂停"};
    oneui::State<bool> sampling{false}, resourceVisible{true}, processVisible{false},
        connectionVisible{false};
    oneui::State<float> memoryValue{0.36f}, diskValue{0.82f};
    oneui::State<std::vector<oneui::ui::TabItem>> monitorTabs{
        {{L"resources", L"资源"}, {L"processes", L"进程"}, {L"connection", L"连接"}}};
    oneui::State<std::vector<oneui::TimeSeriesChartSeries>> cpuSeries, memorySeries, networkSeries;
    oneui::ui::VmCommand toggleSampling;
};
struct SessionsModel {
    oneui::State<bool> first{true}, second{false}, third{false}, fourth{false}, empty{false};
    oneui::State<bool> pane0{false}, pane1{false}, pane2{false}, pane3{false};
    oneui::State<float> paneRatio0{0.5f}, paneRatio1{0.5f}, paneRatio2{0.5f}, paneRatio3{0.5f};
    oneui::State<std::wstring> selected{L"edge"}, connectionQuery, terminalQuery;
    oneui::State<std::wstring> searchResult{L"搜索当前可见内容"}, sessionInfo;
    oneui::State<int> quickConnection{0};
    oneui::State<std::vector<std::wstring>> connectionNames{
        {L"edge-gateway", L"build-node", L"postgres-lab", L"cache-lab"}};
    oneui::State<std::vector<oneui::ui::TabItem>> tabs{
        {{L"edge", L"edge-gateway"}, {L"build", L"build-node"}}};
    std::function<void(std::wstring)> closeSession;
    oneui::ui::VmCommand clear, send, newSession, searchNext, toggleSplit;
};
// Flat template names are a compatibility facade; controllers receive only their own model.
struct WorkbenchModel : FilesModel, MonitorModel, SessionsModel {
    oneui::State<bool> mainVisible{true}, terminalVisible{true}, commandVisible{false},
        searchVisible{false};
    oneui::State<bool> filesCollapsed{false}, filesExpanded{true}, sidebarVisible{false};
    oneui::State<bool> monitorVisible{true}, filesVisible{true}, narrow{false}, wide{true};
    oneui::State<float> fileRatio{0.54f}, monitorRatio{0.70f};
    oneui::State<std::wstring> compactTab{L"terminal"}, fileCollapseIcon{L"down"};
    oneui::State<std::vector<oneui::ui::TabItem>> compactTabs{{{L"terminal", L"终端"},
                                                               {L"files", L"模拟文件"},
                                                               {L"monitor", L"监控"},
                                                               {L"logs", L"日志"}}};
    oneui::State<bool> logVisible{false}, workspaceVisible{true};
    oneui::ui::VmCommand restore, toggleSidebar, toggleMonitor, toggleFiles;
    oneui::ui::VmCommand showTerminal, showFiles, showMonitor, showLogs, toggleCommand,
        toggleSearch;
    oneui::ui::VmCommand collapseFiles, showStatus, showProcesses, appendLog, clearLog;
};
} // namespace lab::terminal
