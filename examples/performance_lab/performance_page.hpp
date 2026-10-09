#pragma once
#include "connection_workspace.hpp"
#include "lab_options.hpp"
#include "lab_widgets.hpp"
#include "renderer_benchmark.hpp"
#include "renderer_soak.hpp"

class Lab final : public LayoutView {
    Window &window;
    Options options;
    Model m;
    RendererBenchmark rendererBenchmark_;
    RendererSoak soak_;
    int exerciseStep_ = -1;
    std::ofstream exerciseLog_;
    Clock::time_point rendererLabelTime_{};
    std::atomic<bool> benchmarkPostPending_{false};
    int effectStep_ = -1;
    bool rendererSampling_ = false;
    bool desktopMenusEnabled_ = false, pageActive_ = true;
    bool autoStarted = false, snapshotDone = false, finishing = false;
    int result = 0;
    std::unique_ptr<connection_demo::Workspace> editor;
    bool editing = false, resumeAnimation = true;
    std::uint64_t editorPaints = 0;
    int stressStep_ = 0;
    Clock::time_point stressStart_{}, lastStressPaint_{};
    double stressCpuStart_ = 0;
    std::vector<double> stressPaints_, stressIntervals_;
    static double processCpuMs();
    std::shared_ptr<Panel> sidebar, signalCard, spectrumCard, particleCard, listCard;
    std::shared_ptr<ScrollView> workspace;
    std::shared_ptr<LayoutView> scene;
    std::shared_ptr<VirtualList> list;
    std::array<L, 5> metricNames, metricValues, metricUnits;
    std::array<B, 4> nav;
    std::array<B, 3> loads;
    B speed, pause, record;
    L title, subtitle, live, loadLabel, status, build, signalMeta, particleMeta, listMeta,
        listFooter, particleSpeed;
    std::shared_ptr<Rule> topRule, bottomRule;
    std::wstring message = L"就绪 · 切换负载、拖动窗口或滚动列表，观察帧间隔变化";
    void rebuildRows();
    void refreshDesktopMenus();
    void controls();
    void setLoad(int n);
    void setTab(int n);
    void toggle();
    void startRecord(double seconds = 10);
    void createSidebar();
    void createSignal();
    void createParticleAndList();

  public:
    void setPageActive(bool active);
    void enableDesktopMenus();
    void ensureEditor();
    void showEditor();
    void hideEditor();
    void closeEditor();
    void requestClose(bool forced);
    Lab(Window &w, Options o);
    void stressStep();
    void updateRendererLabel();
    void rendererBenchmarkTick(double elapsed);
    void beginRendererBenchmark();
    void stabilityTick(double elapsed);
    void beginStability();
    bool idleStarted_ = false; // Accessed only by the bounded benchmark worker.
    void begin();
    ~Lab();
    std::atomic<bool> diagnosticStop{false};
    std::thread diagnosticThread;
    std::uint64_t idleStart = 0;
    int exitCode() const;
    bool onMouseWheel(const MouseWheelEvent &e) override;
    bool key(const KeyEvent &e);
    // Apply widget properties before painting; paint only samples telemetry.
    void updateMetrics();
    bool tickAnimations(double now) override;
    void paint(Canvas &canvas) override;
};
