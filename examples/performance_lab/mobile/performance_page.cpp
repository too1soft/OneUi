#include "performance_page.hpp"
#include "plots.hpp"

namespace lab::mobile {
namespace {
class PlotSurface final : public View {
    double previous_ = 0;
    bool active_ = false;
    std::shared_ptr<Plot> plot_;

  public:
    Model model{false}; // No hidden desktop process-monitor worker on mobile.
    unsigned ticks = 0;
    void refresh() { invalidate(); }
    explicit PlotSurface(bool particles) {
        model.running = false;
        plot_ = std::make_shared<Plot>(model, particles ? PlotKind::Particles : PlotKind::Signals);
        add(plot_);
    }
    void active(bool value) {
        active_ = value;
        previous_ = 0;
        if (active_ && model.running)
            requestAnimationFrame();
    }
    void running(bool value) {
        model.running = value;
        previous_ = 0;
        if (value && active_)
            requestAnimationFrame();
        invalidate();
    }
    bool tickAnimations(double now) override {
        const bool children = View::tickAnimations(now);
        if (!active_ || !model.running) {
            previous_ = 0;
            return children;
        }
        if (previous_ > 0)
            model.time += float(std::clamp(now - previous_, 0., 100.) / 1000.);
        previous_ = now;
        ++ticks;
        return true;
    }
    void paint(Canvas &canvas) override {
        canvas.fillRect(frame(), PANEL);
        View::paint(canvas);
    }

  private:
    void layoutChildren() override { plot_->setFrame(frame()); }
};
class PlotPage final : public ContentPage {
    std::shared_ptr<PlotSurface> scene_;
    std::shared_ptr<Label> feedback_;
    std::shared_ptr<Button> run_;
    bool particles_;
    void feedback() {
        auto &m = scene_->model;
        std::wstring message = m.running ? L"运行中" : L"已暂停";
        message += L" · " + std::to_wstring(particles_ ? m.particles() : m.points());
        message += particles_ ? L" 个粒子" : L" 点 / 曲线";
        feedback_->setText(message);
        run_->setText(m.running ? L"暂停" : L"开始运行");
    }

  public:
    explicit PlotPage(bool particles) : particles_(particles) {
        auto note = text(L"复用 PC 场景和负载；移动视口结果不直接与桌面比较。", 14);
        note->setTextWrapping(true);
        body_->setFlex(note, {0, 0, 54, 54});
        scene_ = std::make_shared<PlotSurface>(particles);
        add(scene_, 260);
        feedback_ = text(L"已暂停");
        run_ = action(L"开始运行", [this] {
            scene_->running(!scene_->model.running);
            feedback();
        });
        body_->setFlex(run_, {0, 0, 48, 48});
        auto load = action(L"切换负载：轻量 / 标准 / 高压", [this] {
            scene_->model.load = (scene_->model.load + 1) % 3;
            scene_->refresh();
            feedback();
        });
        body_->setFlex(load, {0, 0, 48, 48});
        if (particles) {
            auto mode = action(L"绘制路径：mesh", [] {});
      mode->setOnClick([this, weak = std::weak_ptr<Button>(mode)] {
                auto &m = scene_->model;
                m.particleMode = ParticleMode((int(m.particleMode) + 1) % 5);
                const std::string name = particleModeName(m.particleMode);
        if (auto button = weak.lock())
          button->setText(L"绘制路径：" + std::wstring(name.begin(), name.end()));
                scene_->refresh();
            });
            body_->setFlex(mode, {0, 0, 48, 48});
            auto fallback =
                text(L"Android 尚未启用 mesh 实验路径；选中 mesh 时明确回退到批量绘制。");
            fallback->setTextWrapping(true);
            body_->setFlex(fallback, {0, 0, 66, 66});
        }
        auto limits = text(
            L"离页、导航遮挡或后台时停止。回调次数不等于实际呈现 FPS；GPU 耗时与功耗尚未采集。");
        limits->setTextWrapping(true);
        body_->setFlex(limits, {0, 0, 72, 72});
        feedback();
    }
    void activate() override { scene_->active(true); }
    void deactivate() override { scene_->active(false); }
    std::string diagnostics() const {
        const auto &m = scene_->model;
        std::ostringstream out;
        out << " ticks=" << scene_->ticks << " running=" << m.running << " load=" << m.load
            << " plotTime=" << m.time << " meshFallbacks=" << m.meshFallbacks
            << " meshReason=" << m.meshReason;
        return out.str();
    }
};
} // namespace
std::unique_ptr<DemoPage> createPlotPage(bool particles) {
    return std::make_unique<PlotPage>(particles);
}
std::string plotDiagnostics(const DemoPage *page) {
    if (auto plot = dynamic_cast<const PlotPage *>(page))
        return plot->diagnostics();
    return {};
}
} // namespace lab::mobile
