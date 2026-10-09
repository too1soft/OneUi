#include "performance_page.hpp"
#include <iostream>

void Lab::setPageActive(bool active) {
    pageActive_ = active;
    m.telemetry.monitor.setActive(active);
    m.telemetry.hasPrevious = false;
    if (editor) {
        if (active && editing)
            editor->resume();
        else
            editor->suspend();
    }
    if (active && m.running)
        requestAnimationFrame();
}

void Lab::ensureEditor() {
    if (editor)
        return;
    editor = std::make_unique<connection_demo::Workspace>(
        window, [this] { hideEditor(); }, options.templateEntry, options.dev, options.css);
    add(editor->page.root.widget);
    editor->page.root.widget->setVisible(false);
    editor->suspend();
}

void Lab::showEditor() {
    ensureEditor();
    editor->resume();
    resumeAnimation = m.running;
    m.running = false;
    editing = true;
    for (auto &child : children())
        child->setVisible(child == editor->page.root.widget || child == subtitle);
    window.setMinimumClientSize({640, 560});
    refreshDesktopMenus();
    invalidate();
}

void Lab::hideEditor() {
    editor->suspend();
    editing = false;
    for (auto &child : children())
        child->setVisible(child != editor->page.root.widget);
    window.setContentScale(1);
    window.setMinimumClientSize({1050, 720});
    m.running = resumeAnimation;
    m.telemetry.reset();
    controls();
    // The old laboratory has a fixed minimum layout. Returning from its narrow
    // editor restores enough native client space without changing editor layout.
#ifdef _WIN32
    auto hwnd = static_cast<HWND>(window.nativeHandle());
    RECT bounds{}, client{};
    if (GetWindowRect(hwnd, &bounds) && GetClientRect(hwnd, &client)) {
        const int cw = std::max(int(client.right), int(std::ceil(1050 * window.dpiScale()))),
                  ch = std::max(int(client.bottom), int(std::ceil(720 * window.dpiScale())));
        if (cw != client.right || ch != client.bottom)
            SetWindowPos(hwnd, nullptr, 0, 0, cw + bounds.right - bounds.left - client.right,
                         ch + bounds.bottom - bounds.top - client.bottom,
                         SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
#elif defined(__APPLE__)
    const auto size = window.clientSize();
    labSetClientSize(window.nativeHandle(), std::max(1050.f, size.width),
                     std::max(720.f, size.height));
#else
    const auto size = window.clientSize();
    if (!window.setClientSize({std::max(1050.f, size.width), std::max(720.f, size.height)}))
        throw std::runtime_error("Restore laboratory client size failed");
#endif
    if (m.running)
        requestAnimationFrame();
}

void Lab::closeEditor() {
    if (editor)
        editor->close();
}

void Lab::requestClose(bool forced) {
    if (forced || !editing || !editor)
        window.close();
    else
        editor->flow.request(connection_demo::Connections::Destination::Close);
}

Lab::Lab(Window &w, Options o) : window(w), options(std::move(o)) {
    m.load = options.load;
    m.particleMode = options.particleMode;
    m.tab = options.tab;
    createSidebar();
    title = label(*this, L"", 26, TEXT, 700);
    subtitle = label(*this, L"在真实窗口中，感受绘制、交互与动画的节奏。");
    live = label(*this, L"", 12);
    live->setAlign(TextAlign::Right);
    topRule = std::make_shared<Rule>();
    bottomRule = std::make_shared<Rule>();
    add(topRule);
    add(bottomRule);
    const std::array<const wchar_t *, 5> names{L"UI 更新率", L"平均帧间隔", L"P95 帧间隔",
                                               L"进程 CPU", L"工作集内存"},
        units{L"FPS", L"ms", L"ms", L"%", L"MB"};
    for (int i = 0; i < 5; ++i) {
        metricNames[i] = label(*this, names[i]);
        metricValues[i] = label(*this, L"—", 27, i == 0 ? ACCENT : i == 3 ? TEAL : TEXT, 500);
        // A persistent override survives inherited-font propagation during paint.
        // Replacing the inherited environment here on every frame dirtied even
        // hidden metrics and kept otherwise idle pages repainting indefinitely.
        metricValues[i]->setFontFamily(L"Consolas");
        metricUnits[i] = label(*this, units[i], 11);
    }
    loadLabel = label(*this, L"绘制负载");
    for (int i = 0; i < 3; ++i)
        loads[i] = button(*this, std::array<const wchar_t *, 3>{L"轻量", L"标准", L"高压"}[i],
                          [this, i] { setLoad(i); });
    speed = button(*this, L"", [this] {
        if (m.telemetry.recording)
            return;
        m.speed = m.speed == 1 ? .5f : m.speed == .5f ? 2 : 1;
        controls();
    });
    pause = button(*this, L"暂停", [this] { toggle(); });
    record = button(*this, L"采样 10 秒", [this] { startRecord(); });
    record->setStyleOverride(buttonStyle(true));
    status = label(*this, message, 11);
    build = label(*this, L"RELEASE BUILD", 10);
    build->setAlign(TextAlign::Right);
    scene = std::make_shared<LayoutView>();
    workspace = std::make_shared<ScrollView>();
    workspace->setContent(scene);
    workspace->setChromeVisible(false);
    workspace->setWheelStep(48);
    add(workspace);
    createSignal();
    createParticleAndList();
    auto frameTitle = label(*scene, L"帧节奏", 16, TEXT, 600);
    auto frameMeta = label(*scene, L"最近 240 帧 · 参考线 16.7 / 33.3 ms");
    frameMeta->setAlign(TextAlign::Right);
    auto framePlot = std::make_shared<Plot>(m, PlotKind::Frames);
    scene->add(framePlot);
    auto method =
        label(*scene, L"性能指标为 UI paint 回调间隔，非 GPU 耗时；CPU 为本进程占整机比例。");
    scene->arrange = [this, frameTitle, frameMeta, framePlot, method](Rect r) {
        float y = 0;
        if (m.tab == 0) {
            signalCard->setFrame({r.x, r.y, r.width - 261, 210});
            spectrumCard->setFrame({r.x + r.width - 245, r.y, 245, 210});
            particleCard->setFrame({r.x, r.y + 226, (r.width - 16) / 2, 210});
            listCard->setFrame({r.x + (r.width + 16) / 2, r.y + 226, (r.width - 16) / 2, 210});
            y = 452;
        } else if (m.tab == 1) {
            signalCard->setFrame({r.x, r.y, r.width - 261, 430});
            spectrumCard->setFrame({r.x + r.width - 245, r.y, 245, 430});
            y = 446;
        } else {
            auto p = m.tab == 2 ? particleCard : listCard;
            p->setFrame({r.x, r.y, r.width, 485});
            y = 501;
        }
        frameTitle->setFrame({r.x, r.y + y, 180, 28});
        frameMeta->setFrame({r.x + r.width - 380, r.y + y + 3, 380, 22});
        framePlot->setFrame({r.x, r.y + y + 45, r.width, 34});
        method->setFrame({r.x, r.y + y + 94, r.width, 22});
    };
    arrange = [this](Rect r) {
        if (editing) {
            subtitle->setFrame({r.x + 20, r.y + 7, r.width - 40, 20});
            editor->page.root.widget->setFrame(
                {r.x, r.y + 34, r.width, std::max(0.f, r.height - 34)});
            return;
        }
        sidebar->setVisible(!options.embedded);
        sidebar->setFrame({r.x, r.y, 190, r.height});
        float x = r.x + (options.embedded ? 24 : 214), w = r.width - (options.embedded ? 48 : 238);
        title->setFrame({x, r.y + 24, w - 120, 43});
        subtitle->setFrame({x, r.y + 68, w, 22});
        live->setFrame({x + w - 120, r.y + 42, 120, 24});
        topRule->setFrame({x, r.y + 105, w, 1});
        bottomRule->setFrame({x, r.y + 207, w, 1});
        for (int i = 0; i < 5; ++i) {
            float mx = x + w * i / 5;
            metricNames[i]->setFrame({mx, r.y + 120, w / 5, 22});
            metricValues[i]->setFrame({mx, r.y + 148, 125, 38});
            metricUnits[i]->setFrame({mx + 85, r.y + 171, 50, 19});
        }
        loadLabel->setFrame({x, r.y + 231, 56, 22});
        for (int i = 0; i < 3; ++i)
            loads[i]->setFrame({x + 56 + i * 66.f, r.y + 224, 58, 37});
        speed->setFrame({x + w - 258, r.y + 224, 90, 37});
        pause->setFrame({x + w - 160, r.y + 224, 58, 37});
        record->setFrame({x + w - 94, r.y + 224, 94, 37});
        workspace->setFrame({x, r.y + 277, w, std::max(200.f, r.height - 329)});
        status->setFrame({x, r.y + r.height - 45, w - 140, 24});
        build->setFrame({x + w - 130, r.y + r.height - 45, 130, 24});
    };
    rebuildRows();
    controls();
    if (options.warmEditor)
        ensureEditor();
    if (options.editor) {
        showEditor();
        if (options.details) {
            editor->flow.selectedKey.set(L"1");
            editor->flow.view.execute();
        } else if (!options.connections && !options.gallery)
            editor->flow.open(L"1");
        editor->flow.query.set(options.connectionQuery);
        editor->vm.theme.set(options.editorLight ||
                                     ((options.connections || options.details || options.gallery) &&
                                      !options.editorDark)
                                 ? 0
                                 : 1);
        editor->vm.density.set(options.compact ? 1 : 0);
        if (options.editorInvalid) {
            editor->vm.host.set(L"https://bad/path");
            editor->vm.port.set(L"70000");
            editor->vm.attempted.set(true);
        }
        if (options.gallery) {
            editor->flow.gallery.set(true);
            editor->mount.flush();
            editor->samples->scene.set(options.galleryScene);
            if (options.layoutSettings)
                editor->samples->workspaceMode.set(1);
            if (options.layoutDetail) {
                editor->samples->selected.set(L"1");
                editor->samples->detailOpen.set(true);
            }
            if (options.softPreset)
                editor->samples->visualPreset.set(1);
        }
        editor->flush();
    }
}

Lab::~Lab() {
    diagnosticStop = true;
    if (diagnosticThread.joinable())
        diagnosticThread.join();
    closeEditor();
}

int Lab::exitCode() const { return result; }

bool Lab::tickAnimations(double now) {
    if (!pageActive_)
        return false;
    updateRendererLabel();
    if (!editing)
        updateMetrics();
    bool changed = View::tickAnimations(now);
    if (m.running) {
        invalidate();
        return true;
    }
    return changed;
}

void Lab::paint(Canvas &canvas) {
    RendererBenchmark::PaintSample sample(rendererBenchmark_);
    if (editing) {
        canvas.fillRect(frame(), BG);
        ++editorPaints;
        const auto begin = Clock::now();
        View::paint(canvas);
        if (options.connectionStress > 50 && stressStep_ >= 150) {
            stressPaints_.push_back(ms(Clock::now() - begin));
            if (lastStressPaint_ != Clock::time_point{})
                stressIntervals_.push_back(ms(begin - lastStressPaint_));
            lastStressPaint_ = begin;
        }
        return;
    }
    double dt = m.telemetry.frame(m.running);
    if (m.running)
        m.time += float(std::min(dt / 1000., .1)) * m.speed;
    canvas.fillRect(frame(), BG);
    View::paint(canvas);
    if (!options.snapshot.empty() && !snapshotDone &&
        ms(Clock::now() - m.telemetry.started) > 2200) {
        snapshotDone = true;
        window.post([this] {
            std::filesystem::create_directories(options.snapshot.parent_path());
            if (!window.captureFramePng(options.snapshot.wstring())) {
                result = 3;
                std::cerr << "Frame capture failed\n";
            }
            if (options.snapshotExit)
                window.close();
        });
    }
}
