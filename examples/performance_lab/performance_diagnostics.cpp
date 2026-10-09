#include "performance_page.hpp"
#include <iostream>

double Lab::processCpuMs() { return labProcessCpuMs(); }

void Lab::stressStep() {
    const int cycle = stressStep_ / 3, phase = stressStep_ % 3;
    if (phase == 0) {
        if (cycle == 50) {
            stressStart_ = Clock::now();
            stressCpuStart_ = processCpuMs();
            stressPaints_.clear();
            stressIntervals_.clear();
            lastStressPaint_ = {};
        }
        if (cycle % 50 == 0 || cycle == options.connectionStress) {
            const auto memory = labProcessMemory();
            const auto stats = editor->mount.diagnostics();
            std::filesystem::create_directories(options.output);
            std::ofstream report(options.output / "connections-stress.csv",
                                 cycle == 0 ? std::ios::out : std::ios::app);
            if (cycle == 0)
                report << "cycle,working_set_mib,private_mib,styles,mount_subscriptions,owned_"
                          "objects,paint_count\n";
            report << cycle << ',' << memory.WorkingSetSize / 1048576.0 << ','
                   << memory.PrivateUsage / 1048576.0 << ',' << editor->mount.styles()->size()
                   << ',' << stats.subscriptions << ',' << stats.ownedObjects << ',' << editorPaints
                   << '\n';
        }
        if (cycle == options.connectionStress) {
            if (cycle > 50) {
                const double elapsed = ms(Clock::now() - stressStart_),
                             cpu = processCpuMs() - stressCpuStart_;
                auto paint = summarize(stressPaints_), interval = summarize(stressIntervals_);
                const auto memory = labProcessMemory();
                std::ofstream report(options.output / "entry-performance.txt");
                report << "entry=" << (options.templateEntry ? "template" : "code")
                       << "\ncycles=" << cycle - 50 << "\nelapsed_ms=" << elapsed
                       << "\ncpu_ms=" << cpu << "\ncpu_percent="
                       << 100 * cpu / elapsed / std::max(1u, std::thread::hardware_concurrency())
                       << "\npaint_mean_ms=" << paint.mean << "\npaint_p95_ms=" << paint.p95
                       << "\ninterval_mean_ms=" << interval.mean
                       << "\ninterval_p95_ms=" << interval.p95
                       << "\npaint_count=" << stressPaints_.size()
                       << "\nworking_set_mib=" << memory.WorkingSetSize / 1048576.0
                       << "\nprivate_mib=" << memory.PrivateUsage / 1048576.0 << '\n';
                std::ofstream raw(options.output / "entry-paint.csv");
                raw << "paint_ms\n";
                for (auto value : stressPaints_)
                    raw << value << '\n';
            }
            window.close();
            return;
        }
        editor->flow.query.set(cycle % 2 ? L"节点" : L"");
        editor->flow.filter.set(cycle % 3);
        editor->flush();
        editor->flow.selectedKey.set(editor->flow.filtered.get().front().id);
        editor->flow.edit.execute();
    } else if (phase == 1) {
        editor->vm.note.set(L"循环输入 " + std::to_wstring(cycle));
        editor->flow.request(connection_demo::Connections::Destination::List);
    } else
        editor->flow.discard.execute();
    editor->flush();
    ++stressStep_;
    window.requestAnimationFrame([this](double) { stressStep(); });
}

void Lab::updateRendererLabel() {
    const auto now = Clock::now();
    if (ms(now - rendererLabelTime_) < 250)
        return;
    rendererLabelTime_ = now;
    const auto info = window.rendererInfo();
    const auto wide = [](const std::string &text) {
        return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>{}.from_bytes(text);
    };
    const auto policy = options.renderer == "cpu"   ? L"CPU 软件"
                        : options.renderer == "gpu" ? L"GPU 优先"
                                                    : L"自动";
    std::wstring text = std::wstring(L"启动选择：") + policy + L"  ·  实际：";
    if (info.backend == RenderBackend::OpenGL)
        text += L"OpenGL / Skia Ganesh  ·  " + wide(info.device);
    else if (info.backend == RenderBackend::Metal)
        text += L"Metal / Skia Ganesh  ·  " + wide(info.device);
    else if (info.backend == RenderBackend::Software) {
        text += L"CPU / Skia Raster";
        text += info.reason == "disabled-by-environment" ? L"  ·  已指定软件渲染"
                                                         : L"  ·  GPU 回退：" + wide(info.reason);
    } else
        text += L"等待首次绘制";
    subtitle->setText(text);
    subtitle->setTooltip(text + L"\n切换模式需要重新启动；自动遵循 ONEUI_ENABLE_GPU 环境设置。");
}

void Lab::rendererBenchmarkTick(double elapsed) {
    if (diagnosticStop)
        return;
    if (elapsed >= 3 && !rendererSampling_) {
        rendererBenchmark_.begin(window, m.meshDraws, m.meshFallbacks, m.meshVertices);
        rendererSampling_ = true;
    }
    if (elapsed >= 3 + options.sampleSeconds) {
        try {
            rendererBenchmark_.finish(window, options.output, options.renderer,
                                      options.rendererBenchmark, particleModeName(m.particleMode),
                                      m.meshDraws, m.meshFallbacks, m.meshVertices, m.meshReason,
                                      m.meshLastFallback);
        } catch (const std::exception &error) {
            std::cerr << error.what() << '\n';
            result = 2;
        }
        diagnosticStop = true;
        window.close();
        return;
    }
    if (options.rendererBenchmark == "effects" ||
        (options.rendererBenchmark == "effects-idle" && elapsed < 2.1)) {
        const int step = static_cast<int>(elapsed * 2);
        if (step != effectStep_) {
            effectStep_ = step;
            editor->samples->expanded.set(step % 2 == 0);
            editor->samples->motion.set((step / 8) % 2);
            editor->mount.flush();
            auto button = std::dynamic_pointer_cast<Button>(editor->mount.find("effectsLift"));
            MouseEvent event;
            const auto r = button->frame();
            event.position = step % 2 == 0 ? Point{r.x + 5, r.y + 5} : Point{-1, -1};
            button->onMouseMove(event);
        }
    }
    if (options.rendererBenchmark == "table") {
        auto table = std::dynamic_pointer_cast<Table>(editor->page.fields.at("$table"));
        // Same wall-time trajectory at 60 posted updates/s; native virtualized table.
        table->setScrollOffset(static_cast<float>(std::fmod(elapsed * 360.0, 8000.0)));
    }
}

void Lab::beginRendererBenchmark() {
    diagnosticThread = std::thread([this] {
        const auto start = Clock::now();
        auto next = start;
        while (!diagnosticStop) {
            const double elapsed = ms(Clock::now() - start) / 1000.0;
            const bool idle =
                options.rendererBenchmark == "idle" || options.rendererBenchmark == "effects-idle";
            const bool shouldPost =
                !idle || (options.rendererBenchmark == "effects-idle" && elapsed < 2.1) ||
                (elapsed >= 3 && !idleStarted_) || elapsed >= 3 + options.sampleSeconds;
            if (shouldPost && !benchmarkPostPending_.exchange(true)) {
                if (elapsed >= 3)
                    idleStarted_ = true;
                if (!window.post([this, elapsed] {
                        benchmarkPostPending_ = false;
                        rendererBenchmarkTick(elapsed);
                    }))
                    break;
            }
            next += std::chrono::microseconds(16667);
            if (next < Clock::now())
                next = Clock::now();
            std::this_thread::sleep_until(next);
        }
    });
}

void Lab::stabilityTick(double elapsed) {
    if (diagnosticStop)
        return;
    try {
        if (elapsed < 3)
            return;
        const double sampled = elapsed - 3;
        if (!soak_.active)
            soak_.begin(window, m, options.output);
        if (sampled - soak_.previousSeconds >= 5 || sampled >= options.stabilitySeconds)
            soak_.sample(window, m, sampled);
        if (options.stabilityExercise) {
            const int step = int(sampled / 2);
            if (step != exerciseStep_) {
                exerciseStep_ = step;
                switch (step % 10) {
                case 0:
                    setTab(2);
                    break;
                case 1:
                    toggle();
                    break;
                case 2:
                    toggle();
                    break;
                case 3:
                    setTab(1);
                    break;
                case 4:
                    setTab(3);
                    break;
                case 5:
                    showEditor();
                    break;
                case 6: {
                    window.setContentScale(1.5f);
#ifdef _WIN32
                    auto hwnd = static_cast<HWND>(window.nativeHandle());
                    RECT outer{}, client{};
                    if (!GetWindowRect(hwnd, &outer) || !GetClientRect(hwnd, &client))
                        throw std::runtime_error("Resize diagnostic failed");
                    if (!SetWindowPos(hwnd, nullptr, 0, 0,
                                      640 + outer.right - outer.left - client.right,
                                      600 + outer.bottom - outer.top - client.bottom,
                                      SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE))
                        throw std::runtime_error("Resize failed");
#elif defined(__APPLE__)
                    labSetClientSize(window.nativeHandle(), 640, 600);
#else
                    if (!window.setClientSize({640, 600}))
                        throw std::runtime_error("Resize unsupported");
#endif
                    break;
                }
                case 7:
                    hideEditor();
                    break;
                case 8:
                    window.setContentScale(1.25f);
                    break;
                case 9:
                    window.setContentScale(1);
                    setTab(2);
                    break;
                }
                if (m.running != !(step % 10 == 1 || step % 10 == 5 || step % 10 == 6) ||
                    editing != (step % 10 == 5 || step % 10 == 6))
                    throw std::runtime_error("Interaction state mismatch");
                if (!exerciseLog_.is_open()) {
                    exerciseLog_.open(options.output / "interactions.csv");
                    exerciseLog_.exceptions(std::ios::failbit | std::ios::badbit);
                    exerciseLog_ << "step,seconds,tab,editing,running,width,height,mesh_draws,mesh_"
                                    "fallbacks\n";
                }
                exerciseLog_ << step << ',' << sampled << ',' << m.tab << ',' << editing << ','
                             << m.running << ',' << window.clientSize().width << ','
                             << window.clientSize().height << ',' << m.meshDraws << ','
                             << m.meshFallbacks << '\n';
                exerciseLog_.flush();
            }
        }
        if (sampled >= options.stabilitySeconds) {
            std::ofstream done(options.output / "stability-complete.txt");
            done << "seconds=" << sampled << "\nexercise_steps=" << exerciseStep_ + 1 << '\n';
            diagnosticStop = true;
            window.close();
        }
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        result = 2;
        diagnosticStop = true;
        window.close();
    }
}

void Lab::beginStability() {
    diagnosticThread = std::thread([this] {
        const auto start = Clock::now();
        while (!diagnosticStop) {
            const double elapsed = ms(Clock::now() - start) / 1000;
            if (!benchmarkPostPending_.exchange(true) && !window.post([this, elapsed] {
                    benchmarkPostPending_ = false;
                    stabilityTick(elapsed);
                }))
                break;
            for (int i = 0; i < 5 && !diagnosticStop; ++i)
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    });
}

void Lab::begin() {
    window.prepareLayoutSnapshot();
    updateRendererLabel();
    if (options.stabilitySeconds > 0) {
        requestAnimationFrame();
        beginStability();
        return;
    }
    if (!options.rendererBenchmark.empty()) {
        if (!editing)
            requestAnimationFrame();
        beginRendererBenchmark();
        return;
    }
    if (options.warmEditor && !editing) {
        showEditor();
        window.prepareLayoutSnapshot();
        hideEditor();
        window.prepareLayoutSnapshot();
    }
    requestAnimationFrame();
    if (options.connectionStress > 0) {
        window.requestAnimationFrame([this](double) { stressStep(); });
        return;
    }
    if (editing && (!options.snapshot.empty() || options.editorIdle > 0)) {
        // Event-driven editor needs no render loop. Bounded diagnostic timers
        // return through the native UI queue and are joined before Lab is released.
        diagnosticThread = std::thread([this] {
            for (int i = 0; i < 20 && !diagnosticStop; ++i)
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            if (diagnosticStop)
                return;
            window.post([this] {
                window.prepareLayoutSnapshot();
                if (!options.snapshot.empty()) {
                    std::filesystem::create_directories(options.snapshot.parent_path());
                    if (!window.captureFramePng(options.snapshot.wstring()))
                        result = 3;
                    std::ofstream report(options.snapshot.string() + ".layout.txt");
                    report << oneui::ui::formatLayoutIssues(oneui::ui::inspectLayout(
                        editor->page.root.widget, *editor->mount.styles()));
                    if (options.snapshotExit)
                        window.close();
                }
                idleStart = editorPaints;
            });
            if (options.editorIdle <= 0)
                return;
            const auto end = Clock::now() + std::chrono::duration<double>(options.editorIdle);
            while (!diagnosticStop && Clock::now() < end)
                std::this_thread::sleep_for(std::chrono::milliseconds(25));
            if (!diagnosticStop)
                window.post([this] {
                    std::filesystem::create_directories(options.output);
                    std::ofstream report(options.output / "editor-idle.txt");
                    report << "paint_count=" << editorPaints - idleStart
                           << "\ncpu_percent=" << m.telemetry.monitor.cpu
                           << "\nworking_set_mb=" << m.telemetry.monitor.memory
                           << "\ndpi_scale=" << window.dpiScale()
                           << "\nstyles=" << editor->mount.styles()->size() << "\n";
                    window.close();
                });
        });
    }
}

void Lab::updateMetrics() {
    if (options.benchmark > 0 && !autoStarted && ms(Clock::now() - m.telemetry.started) > 3000) {
        autoStarted = true;
        startRecord(options.benchmark);
    }
    auto stats = m.telemetry.current();
    metricValues[0]->setText(number(stats.fps));
    metricValues[1]->setText(number(stats.mean, 2));
    metricValues[2]->setText(number(stats.p95, 2));
    metricValues[3]->setText(number(m.telemetry.monitor.cpu));
    metricValues[4]->setText(number(m.telemetry.monitor.memory, 0));
    if (m.telemetry.finished() && !finishing) {
        finishing = true;
        try {
            auto path = m.telemetry.save(options.output, m.load, m.tab, m.particles(), m.points(),
                                         m.speed, window.clientSize().width,
                                         window.clientSize().height, window.dpiScale());
            message = L"采样已保存：" + path.filename().wstring();
            std::cout << "Saved " << path.string() << std::endl;
        } catch (const std::exception &e) {
            result = 2;
            message = L"导出失败，请检查输出目录权限";
            std::cerr << e.what() << std::endl;
        }
        m.telemetry.recording = false;
        controls();
        finishing = false;
        if (options.exitAfter) {
            m.running = false;
            window.post([this] { window.close(); });
        }
    }
    status->setText(m.telemetry.recording ? L"正在采样 · 剩余 " + number(m.telemetry.remaining()) +
                                                L" 秒 · 负载、视图与速度已锁定"
                                          : message);
}
