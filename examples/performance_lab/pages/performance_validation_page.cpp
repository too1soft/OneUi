#include "../lab_process.hpp"
#include "internal/unicode.h"
#include "page_factories.hpp"
#include <array>
#include <atomic>
#include <fstream>
#include <map>
#include <sstream>
#include <thread>

namespace lab {
namespace {
using namespace oneui;
using Report = std::map<std::string, std::string>;
Report readReport(const std::filesystem::path &path) {
    std::ifstream file(path);
    if (!file)
        throw std::runtime_error("Benchmark did not produce its completion report");
    Report result;
    for (std::string line; std::getline(file, line);) {
        auto equal = line.find('=');
        if (equal != std::string::npos)
            result.emplace(line.substr(0, equal), line.substr(equal + 1));
    }
    return result;
}
std::wstring pathArgument(const std::filesystem::path &path) {
#ifdef _WIN32
    return path.wstring();
#else
    return unicode::fromUtf8(path.u8string());
#endif
}
std::wstring describe(const Report &report, const wchar_t *name) {
    auto value = [&](const char *key) {
        auto found = report.find(key);
        return found == report.end() ? std::wstring(L"未采集") : unicode::fromUtf8(found->second);
    };
    std::wostringstream out;
    out << name << L" · 实际后端 " << value("backend") << L"\n设备：" << value("device")
        << L"\n进程 CPU（整机比例）：" << value("cpu_percent") << L"%" << L"\nCPU 绘制均值："
        << value("paint_mean_ms") << L" ms" << L"\n内容绘制 P95：" << value("content_p95_ms")
        << L" ms" << L"\n绘制回调频率：" << value("frame_callback_hz") << L" Hz" << L"\n物理尺寸："
        << value("physical_width") << L" × " << value("physical_height") << L"\n回退原因："
        << value("reason") << L"\n\n";
    return out.str();
}
class ValidationPage final : public ContentPage {
    struct Job {
        std::atomic<bool> cancelled{false}, active{true};
    };
    bool stability_;
    int scene_ = 0, load_ = 1, seconds_;
    std::shared_ptr<Job> job_;
    std::thread worker_;
    std::shared_ptr<Label> status_;
    std::shared_ptr<TextField> result_;
    std::shared_ptr<Button> start_;
    void stop() {
        if (job_)
            job_->cancelled = true;
        if (worker_.joinable())
            worker_.join();
        if (job_)
            job_->active = false;
        if (start_)
            start_->setDisabled(false);
    }
    void start() {
        stop();
        auto job = job_ = std::make_shared<Job>();
        auto dispatcher = Application::current()->dispatcher();
        auto progress = [job, dispatcher, label = std::weak_ptr<Label>(status_),
                         result = std::weak_ptr<TextField>(result_),
                         button = std::weak_ptr<Button>(start_)](
                            std::wstring status, std::wstring report, bool finished) {
            dispatcher([job, label, result, button, status = std::move(status),
                        report = std::move(report), finished] {
                if (job->cancelled.load())
                    return;
                if (auto widget = label.lock())
                    widget->setText(status);
                if (!report.empty())
                    if (auto widget = result.lock())
                        widget->setText(report);
                if (finished)
                    if (auto widget = button.lock())
                        widget->setDisabled(false);
            });
        };
        const std::array<const wchar_t *, 6> scenes{L"chart", L"particles", L"table",
                                                    L"idle",  L"effects",   L"effects-idle"};
        const std::wstring scene = scenes.at(scene_);
        const std::wstring load =
            std::array<const wchar_t *, 3>{L"light", L"medium", L"heavy"}.at(load_);
        const auto output = resultsDirectory() / (stability_ ? "stability" : "renderer-comparison");
        start_->setDisabled(true);
        status_->setText(L"即将打开独立测试窗口；切换页面或点击停止会取消测试。");
        worker_ = std::thread([job, progress, output, scene, load, seconds = seconds_,
                               stability = stability_] {
            try {
                std::vector<Report> reports;
                std::wstring summary;
                for (const auto mode : stability ? std::vector<std::wstring>{L"auto"}
                                                 : std::vector<std::wstring>{L"cpu", L"gpu"}) {
                    if (job->cancelled.load())
                        break;
                    const auto directory =
                        stability ? output : output / std::string(mode.begin(), mode.end());
                    std::filesystem::create_directories(directory);
                    const auto reportPath = directory / (stability ? "stability-complete.txt"
                                                                   : "renderer-performance.txt");
                    // Fixed result names; an interrupted child must not expose an older success.
                    std::filesystem::remove(reportPath);
                    std::vector<std::wstring> args{
                        L"--renderer",      mode,        L"--load",   load,
                        L"--width",         L"1320",     L"--height", L"900",
                        L"--particle-mode", L"combined", L"--output", pathArgument(directory)};
                    if (stability)
                        args.insert(args.end(), {L"--stability-seconds", std::to_wstring(seconds),
                                                 L"--stability-exercise"});
                    else
                        args.insert(args.end(), {L"--renderer-benchmark", scene,
                                                 L"--sample-seconds", std::to_wstring(seconds)});
                    progress(stability ? L"稳定性测试运行中：依次切换场景、缩放和暂停状态。"
                                       : (mode == L"cpu" ? L"正在运行 CPU 固定场景…"
                                                         : L"正在运行 GPU 固定场景…"),
                             summary, false);
                    auto result = runLabProcess(args, directory / "runtime.log", job->cancelled);
                    if (result.cancelled || job->cancelled.load())
                        break;
                    if (!result.started || result.exitCode != 0)
                        throw std::runtime_error(
                            result.error.empty() ? "Benchmark process failed; inspect runtime.log"
                                                 : result.error);
                    auto report = readReport(reportPath);
                    if (stability)
                        summary = L"已完成 " + unicode::fromUtf8(report.at("seconds")) +
                                  L" 秒；场景变化 " +
                                  unicode::fromUtf8(report.at("exercise_steps")) +
                                  L" 次。\n资源采样：stability.csv\n交互轨迹：interactions."
                                  L"csv\n\n短时完成不代表长期泄漏验证通过。";
                    else
                        summary += describe(report, mode == L"cpu" ? L"CPU 请求" : L"GPU 请求");
                    reports.push_back(std::move(report));
                }
                if (!job->cancelled.load()) {
                    bool comparable =
                        !stability && reports.size() == 2 && reports[0]["backend"] == "raster" &&
                        (reports[1]["backend"] == "opengl" || reports[1]["backend"] == "metal");
                    if (!stability)
                        for (auto field : {"physical_width", "physical_height", "dpi_scale",
                                           "scene", "particle_mode"})
                            comparable = comparable && reports[0][field] == reports[1][field];
                    summary += L"\n结果目录：" + pathArgument(output);
                    progress(
                        stability ? L"本轮稳定性场景已完成。"
                        : comparable
                            ? L"对照完成：尺寸、负载和场景一致。"
                            : L"本轮不能作为 CPU/GPU 对照：GPU 回退或几何条件不一致，请检查结果。",
                        summary, true);
                }
            } catch (const std::exception &error) {
                progress(L"测试未完成", unicode::fromUtf8(error.what()), true);
            }
            job->active = false;
        });
    }

  public:
    explicit ValidationPage(bool stability) : stability_(stability), seconds_(stability ? 30 : 5) {
        text(stability ? L"稳定性 · 固定场景与资源采样" : L"CPU / GPU · 相同条件独立运行", 18);
        auto hint = text(L"测试使用独立窗口和原有场景几何；当前导航与代码面板不参与测量。", 14);
        hint->setTextWrapping(true);
        if (!stability) {
            text(L"绘制回调 Hz、CPU 绘制耗时和屏幕呈现帧率是不同指标。当前接口未采集实际呈现帧率。",
                 13)
                ->setTextWrapping(true);
            auto scene = std::make_shared<Select>();
            scene->setItems({L"实时图表", L"粒子绘制", L"表格", L"空闲", L"材质动效", L"材质空闲"});
            scene->setOnChanged([this](int value) { scene_ = value; });
            add(scene);
        }
        auto load = std::make_shared<Select>();
        load->setItems({L"轻量负载", L"标准负载", L"高压负载"});
        load->setSelectedIndex(1);
        load->setOnChanged([this](int value) { load_ = value; });
        add(load);
        auto duration = text(L"采样时长：" + std::to_wstring(seconds_) + L" 秒");
        auto slider = std::make_shared<Slider>();
        slider->setRange(stability ? 20 : 1, 60);
        slider->setStep(1);
        slider->setValue(seconds_);
        slider->setOnChanged([this, duration](double value) {
            seconds_ = int(value);
            duration->setText(L"采样时长：" + std::to_wstring(seconds_) + L" 秒");
        });
        add(slider);
        status_ = text(L"尚未运行。");
        status_->setTextWrapping(true);
        start_ = action(L"开始测试", [this] { start(); });
        action(L"停止测试", [this] {
            stop();
            status_->setText(L"已停止；保留已完成的采样结果。");
        });
        result_ = std::make_shared<TextField>();
        result_->setMultiline(true);
        result_->setReadOnly(true);
        result_->setClipboard(std::make_shared<SystemClipboard>());
        add(result_, 420);

    }
    ~ValidationPage() override { stop(); }
    void deactivate() override {
        const bool running = job_ && job_->active.load();
        stop();
        if (running)
            status_->setText(L"离开页面时已停止测试，返回后可重新运行。");
    }
};
} // namespace
std::unique_ptr<DemoPage> createPerformanceValidationPage(bool stability) {
    return std::make_unique<ValidationPage>(stability);
}
} // namespace lab
