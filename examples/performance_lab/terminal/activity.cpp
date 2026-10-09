#include "activity.hpp"
#include <cmath>

namespace lab::terminal {
Activity::Activity(MonitorModel &monitor, FilesModel &files, oneui::ui::Mount &mount,
                   oneui::Application::Dispatcher dispatch)
    : monitor_(monitor), files_(files), dispatch_(std::move(dispatch)) {
    processes_->setColumns({{L"进程 / PID", 0},
                            {L"CPU", 84, oneui::TextAlign::Right},
                            {L"RSS", 70, oneui::TextAlign::Right}});
    processes_->setRowHeight(42);
    processes_->setHeaderHeight(30);
    processes_->setColumnDividersVisible(false);
    processes_->setRows({{L"node · 3124", L"19.0%", L"128M"},
                         {L"postgres · 920", L"8.4%", L"256M"},
                         {L"redis · 1102", L"2.1%", L"84M"},
                         {L"nginx · 1884", L"0.8%", L"32M"},
                         {L"containerd · 743", L"0.5%", L"46M"},
                         {L"sshd · 4310", L"0.2%", L"12M"},
                         {L"systemd · 1", L"0.1%", L"18M"},
                         {L"journald · 344", L"0.1%", L"22M"}});
    processes_->setOnChanged([this](int row) {
        if (row >= 0 && row < int(processes_->rows().size()))
            monitor_.processDetail.set(processes_->rows()[row][0] +
                                       L" · 用户 deploy · 本地模拟快照");
    });
    mount.registerNative("processes", processes_);
    monitor_.toggleSampling.setAction([this] { monitor_.sampling.set(!monitor_.sampling.get()); });
    subscriptions_.push_back(monitor_.sampling.subscribeScoped([this](bool on) {
        monitor_.samplingLabel.set(on ? L"暂停采样" : L"开始采样");
        monitor_.samplingStatus.set(on ? L"每秒更新 · 模拟" : L"已暂停");
        update();
    }));
    subscriptions_.push_back(files_.transferring.subscribeScoped([this](bool) { update(); }));
    subscriptions_.push_back(monitor_.monitorTab.subscribeScoped([this](const auto &key) {
        oneui::ui::Batch batch;
        monitor_.resourceVisible.set(key == L"resources");
        monitor_.processVisible.set(key == L"processes");
        monitor_.connectionVisible.set(key == L"connection");
    }));
    sample();
}
Activity::~Activity() {
    deactivate();
    processes_->setOnChanged({});
}
void Activity::sample() {
    ++sample_;
    auto make = [&](double base, double amplitude, double phase, oneui::Color color) {
        oneui::TimeSeriesChartSeries series;
        series.name = L"模拟";
        series.color = color;
        for (int i = 0; i < 90; ++i)
            series.values.push_back(base + amplitude * std::sin((i + sample_) * 0.13 + phase) +
                                    amplitude * 0.25 * std::sin(i * 0.7));
        return series;
    };
    auto cpu = make(25, 10, 0, {51, 153, 102}), memory = make(52, 3, 2, {111, 143, 216});
    auto network = make(38, 19, 1, {66, 162, 164}), upload = make(18, 6, 2, {174, 148, 93});
    oneui::ui::Batch batch;
    monitor_.cpu.set(std::to_wstring(int(cpu.values.back())) + L".8%");
    monitor_.memory.set(std::to_wstring(int(memory.values.back())) + L".1%");
    monitor_.network.set(std::to_wstring(int(network.values.back())) + L" MB/s");
    monitor_.memoryValue.set(float(memory.values.back() / 100));
    monitor_.cpuSeries.set({cpu});
    monitor_.memorySeries.set({memory});
    monitor_.networkSeries.set({network, upload});
}
void Activity::activate() {
    active_ = true;
    update();
}
void Activity::deactivate() {
    active_ = false;
    stop();
    monitor_.sampling.set(false);
    if (files_.transferring.get()) {
        files_.transferring.set(false);
        files_.transferLabel.set(L"离开页面，模拟传输已停止 · 未写入磁盘");
    }
}
void Activity::stop() {
    generation_.reset();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }
    wake_.notify_all();
    if (worker_.joinable())
        worker_.join();
}
void Activity::update() {
    if (!active_ || (!monitor_.sampling.get() && !files_.transferring.get())) {
        stop();
        return;
    }
    if (worker_.joinable())
        return;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = false;
    }
    generation_ = std::make_shared<int>(0);
    ticks_ = 0;
    auto weak = std::weak_ptr<int>(generation_);
    worker_ = std::thread([this, weak, dispatch = dispatch_] {
        std::unique_lock<std::mutex> lock(mutex_);
        while (
            !wake_.wait_for(lock, std::chrono::milliseconds(250), [this] { return stopping_; })) {
            lock.unlock();
            dispatch([this, weak] {
                if (!weak.expired())
                    tick();
            });
            lock.lock();
        }
    });
}
void Activity::tick() {
    if (!active_)
        return;
    if (++ticks_ % 4 == 0 && monitor_.sampling.get())
        sample();
    if (files_.transferring.get()) {
        float progress = std::min(1.f, files_.transferValue.get() + 0.0625f);
        files_.transferValue.set(progress);
        if (progress >= 1) {
            files_.transferring.set(false);
            files_.transferLabel.set(L"模拟传输完成 · 未写入磁盘");
        }
    }
}
} // namespace lab::terminal
