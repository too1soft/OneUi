#include "plots.hpp"
#include "connection_workspace.hpp"
#include "renderer_benchmark.hpp"
#include "renderer_soak.hpp"
#include <iostream>
#include <oneui/layout/panel.h>
#include <shellapi.h>

// App layout uses public View/Panel APIs; input and scrolling remain native
// controls.
class LayoutView : public View {
public:
  std::function<void(Rect)> arrange;

protected:
  void layoutChildren() override {
    if (arrange)
      arrange(frame());
  }
};
class Rule final : public Widget {
  Color ink;

public:
  explicit Rule(Color c = LINE) : ink(c) {}
  void paint(Canvas &c) override { c.fillRect(frame(), ink); }
};
using L = std::shared_ptr<Label>;
using B = std::shared_ptr<Button>;
L label(View &parent, std::wstring text, float size = 12, Color ink = MUTED,
        int weight = 400) {
  auto p = std::make_shared<Label>(std::move(text));
  p->setFontSize(size);
  p->setColor(ink);
  p->setFontWeight(weight);
  parent.add(p);
  return p;
}
std::wstring number(double value, int precision = 1) {
  std::wostringstream o;
  o << std::fixed << std::setprecision(precision) << value;
  return o.str();
}
ButtonStyleOverride buttonStyle(bool active = false, bool nav = false) {
  ButtonStateStyleOverride base;
  base.background = active ? (nav ? color(0x2a3b34) : ACCENT)
                           : (nav ? color(0x131a1d) : color(0x222d31));
  base.foreground = active ? (nav ? ACCENT : BG) : TEXT;
  base.borderWidth = 0;
  base.radius = 5;
  base.fontSize = 13;
  base.fontWeight = 400;
  base.shadows = std::vector<ControlShadowStyle>{};
  ButtonStyleOverride s;
  s.normal = base;
  auto hover = base;
  hover.background = active ? color(0xd3f5a7) : color(0x304039);
  if (active)
    hover.foreground = BG;
  s.hovered = hover;
  auto down = base;
  down.background = color(0x66804e);
  down.foreground = TEXT;
  s.pressed = down;
  auto disabled = base;
  disabled.foreground = color(0x75847e);
  disabled.background = color(0x222a27);
  s.disabled = disabled;
  return s;
}
B button(View &parent, std::wstring text, std::function<void()> action) {
  auto b = std::make_shared<Button>(std::move(text));
  b->setStyleOverride(buttonStyle());
  b->setOnClick(std::move(action));
  parent.add(b);
  return b;
}
std::shared_ptr<Panel>
panel(View &parent, const std::shared_ptr<Widget> &content, Color bg = PANEL) {
  auto p = std::make_shared<Panel>();
  p->setBackground(bg);
  p->setBorderWidth(0);
  p->setRadius(8);
  p->setPadding(Insets{0});
  p->setContent(content);
  parent.add(p);
  return p;
}

struct Options {
  int load = 1, tab = 0, width = 1320, height = 900;
  double benchmark = 0;
  std::string renderer = "auto", rendererBenchmark;
  ParticleMode particleMode=DEFAULT_PARTICLE_MODE;
  double sampleSeconds = 5, stabilitySeconds=0;
  bool stabilityExercise=false;
  bool exitAfter = false, snapshotExit = false, traceInput = false;
  bool editor = false, warmEditor = false, editorLight = false, editorDark=false, editorInvalid = false;
  bool connections = false, details=false, gallery=false, compact=false;
  int galleryScene=0;
  bool templateEntry = false, dev = false;
  std::filesystem::path css = std::filesystem::path(__FILE__).parent_path()/"connections.css";
  int connectionStress = 0;
  std::wstring connectionQuery;
  double editorIdle = 0;float scale=1;
  std::filesystem::path output = std::filesystem::path(__FILE__).parent_path() /
                                 "results",
                        snapshot;
};
class Lab final : public LayoutView {
  Window &window;
  Options options;
  Model m;
  RendererBenchmark rendererBenchmark_;
  RendererSoak soak_;
  int exerciseStep_=-1;
  std::ofstream exerciseLog_;
  Clock::time_point rendererLabelTime_{};
  std::atomic<bool> benchmarkPostPending_{false};
  bool rendererSampling_ = false;
  bool autoStarted = false, snapshotDone = false, finishing = false;
  int result = 0;
  std::unique_ptr<connection_demo::Workspace> editor;
  bool editing=false, resumeAnimation=true;
  std::uint64_t editorPaints=0;
  int stressStep_=0;
  Clock::time_point stressStart_{},lastStressPaint_{};
  double stressCpuStart_=0;
  std::vector<double> stressPaints_,stressIntervals_;
  static double processCpuMs() {
    FILETIME created,exit,kernel,user;GetProcessTimes(GetCurrentProcess(),&created,&exit,&kernel,&user);
    return (Monitor::ticks(kernel)+Monitor::ticks(user))/10000.0;
  }
  std::shared_ptr<Panel> sidebar, signalCard, spectrumCard, particleCard,
      listCard;
  std::shared_ptr<ScrollView> workspace;
  std::shared_ptr<LayoutView> scene;
  std::shared_ptr<VirtualList> list;
  std::array<L, 5> metricNames, metricValues, metricUnits;
  std::array<B, 4> nav;
  std::array<B, 3> loads;
  B speed, pause, record;
  L title, subtitle, live, loadLabel, status, build, signalMeta, particleMeta,
      listMeta, listFooter, particleSpeed;
  std::shared_ptr<Rule> topRule, bottomRule;
  std::wstring message = L"就绪 · 切换负载、拖动窗口或滚动列表，观察帧间隔变化";
  void rebuildRows() {
    std::vector<VirtualListItem> items;
    items.reserve(m.rows());
    const std::array<const wchar_t *, 5> names{L"准备图元", L"更新采样缓冲",
                                               L"提交绘制指令", L"同步视图状态",
                                               L"合成帧输出"};
    for (int i = 0; i < m.rows(); ++i) {
      std::wostringstream titleText;
      titleText << std::setfill(L'0') << std::setw(6) << i + 1 << L"      "
                << names[i % 5];
      VirtualListItem item;
      item.title = titleText.str();
      item.trailing = i % 7 == 0 ? L"待处理" : L"完成";
      item.trailingColor = i % 7 == 0 ? ORANGE : TEAL;
      items.push_back(std::move(item));
    }
    list->setRichItems(std::move(items));
    m.selected = std::min(m.selected, m.rows() - 1);
    list->setSelectedIndex(m.selected);
  }
  void controls() {
    for (int i = 0; i < 3; ++i) {
      loads[i]->setStyleOverride(buttonStyle(i == m.load));
      loads[i]->setDisabled(m.telemetry.recording);
    }
    for (int i = 0; i < 4; ++i) {
      nav[i]->setStyleOverride(buttonStyle(i == m.tab, true));
      nav[i]->setDisabled(m.telemetry.recording);
    }
    speed->setText(L"速度 " + number(m.speed) + L"×");
    pause->setText(m.running ? L"暂停" : L"继续");
    speed->setDisabled(m.telemetry.recording);
    pause->setDisabled(m.telemetry.recording);
    record->setDisabled(m.telemetry.recording);
    live->setText(m.running ? L"●  实时运行" : L"●  已暂停");
    live->setColor(m.running ? ACCENT : MUTED);
    signalMeta->setText(L"模拟数据 · 3 × " + std::to_wstring(m.points()) +
                        L" 采样点");
    particleMeta->setText(std::to_wstring(m.particles()) + L" 个 / 实时运动");
    particleSpeed->setText(number(m.speed) + L"× 速度");
    listMeta->setText(std::to_wstring(m.rows()) + L" 行 · 虚拟滚动");
    std::wostringstream selected;
    selected << L"已选择 #" << std::setfill(L'0') << std::setw(6)
             << m.selected + 1 << L" · 仅绘制可见行";
    listFooter->setText(selected.str());
    const std::array<const wchar_t *, 4> titles{
        L"让流畅，看得见。", L"每一条曲线，都在流动。",
        L"让每个粒子，各自发光。", L"十万行，也能轻松浏览。"};
    title->setText(titles[m.tab]);
    signalCard->setVisible(m.tab == 0 || m.tab == 1);
    spectrumCard->setVisible(m.tab == 0 || m.tab == 1);
    particleCard->setVisible(m.tab == 0 || m.tab == 2);
    listCard->setVisible(m.tab == 0 || m.tab == 3);
    workspace->setContentHeight(m.tab == 0 ? 568.f
                                           : (m.tab == 1 ? 568.f : 623.f));
    invalidate();
  }
  void setLoad(int n) {
    if (m.telemetry.recording)
      return;
    m.load = n;
    rebuildRows();
    m.telemetry.reset();
    controls();
  }
  void setTab(int n) {
    if (m.telemetry.recording)
      return;
    m.tab = n;
    workspace->setScrollOffset(0);
    m.telemetry.reset();
    controls();
  }
  void toggle() {
    if (m.telemetry.recording)
      return;
    m.running = !m.running;
    m.telemetry.hasPrevious = false;
    controls();
    if (m.running)
      requestAnimationFrame();
  }
  void startRecord(double seconds = 10) {
    if (m.telemetry.recording)
      return;
    m.running = true;
    m.telemetry.record(seconds);
    controls();
    requestAnimationFrame();
  }
  void createSidebar() {
    auto content = std::make_shared<LayoutView>();
    sidebar = panel(*this, content, color(0x131a1d));
    sidebar->setRadius(0);
    auto brand = label(*content, L"FORM / LAB", 23, TEXT, 700);
    auto detail = label(*content, L"OneUI 原生性能实验台");
    auto mark = std::make_shared<Rule>(ACCENT);
    content->add(mark);
    const std::array<const wchar_t *, 4> texts{L"综合实验台", L"实时图表",
                                               L"粒子场", L"虚拟列表"};
    for (int i = 0; i < 4; ++i) {
      nav[i] = button(*content, L"   " + std::wstring(texts[i]),
                      [this, i] { setTab(i); });
      nav[i]->setContentAlign(TextAlign::Left);
    }
    auto edit=button(*content,L"   连接管理",[this]{if(!m.telemetry.recording)showEditor();});
    edit->setContentAlign(TextAlign::Left);
    auto divider = std::make_shared<Rule>();
    content->add(divider);
    std::array<L, 7> foot{label(*content, L"快捷操作"),
                          label(*content, L"Space    暂停 / 继续"),
                          label(*content, L"1 / 2 / 3    切换负载"),
                          label(*content, L"B    采样 10 秒"),
                          label(*content, L"R    重置统计"),
                          label(*content, L"C++ + OneUI", 11),
                          label(*content, L"Windows / Skia", 11)};
    content->arrange = [this, brand, detail, mark, divider, foot, edit](Rect r) {
      brand->setFrame({r.x + 33, r.y + 20, 148, 34});
      detail->setFrame({r.x + 20, r.y + 58, 165, 22});
      mark->setFrame({r.x + 20, r.y + 26, 4, 24});
      for (int i = 0; i < 4; ++i)
        nav[i]->setFrame({r.x + 20, r.y + 119 + i * 54.f, 149, 47});
      edit->setFrame({r.x+20,r.y+335,149,47});
      float y = r.y + r.height - 230;
      divider->setFrame({r.x + 20, y, 149, 1});
      for (int i = 0; i < 5; ++i)
        foot[i]->setFrame({r.x + 20, y + 19 + i * 28.f, 166, 20});
      foot[5]->setFrame({r.x + 20, r.y + r.height - 58, 166, 20});
      foot[6]->setFrame({r.x + 20, r.y + r.height - 40, 166, 20});
    };
  }
  void createSignal() {
    auto c = std::make_shared<LayoutView>();
    signalCard = panel(*scene, c);
    auto heading = label(*c, L"连续信号", 16, TEXT, 600);
    signalMeta = label(*c, L"");
    signalMeta->setAlign(TextAlign::Right);
    auto plot = std::make_shared<Plot>(m, PlotKind::Signals);
    c->add(plot);
    std::array<L, 5> ys;
    for (int i = 0; i < 5; ++i)
      ys[i] = label(*c, std::to_wstring(100 - i * 25), 10);
    std::array<L, 4> xs;
    const std::array<const wchar_t *, 4> ticks{L"−12 s", L"−8 s", L"−4 s",
                                               L"现在"};
    for (int i = 0; i < 4; ++i)
      xs[i] = label(*c, ticks[i]);
    xs[3]->setAlign(TextAlign::Right);
    std::array<L, 3> legend{label(*c, L"A / 波形", 11, ACCENT),
                            label(*c, L"B / 调制", 11, TEAL),
                            label(*c, L"C / 谐波", 11, ORANGE)};
    c->arrange = [this, heading, plot, ys, xs, legend](Rect r) {
      heading->setFrame({r.x + 20, r.y + 19, 150, 28});
      signalMeta->setFrame({r.x + 180, r.y + 22, r.width - 200, 22});
      float h = r.height - 132;
      plot->setFrame({r.x + 57, r.y + 62, r.width - 77, h});
      for (int i = 0; i < 5; ++i)
        ys[i]->setFrame({r.x + 20, r.y + 54 + h * i / 4, 30, 18});
      for (int i = 0; i < 4; ++i)
        xs[i]->setFrame(
            {r.x + 57 + (r.width - 127) * i / 3, r.y + r.height - 62, 50, 22});
      for (int i = 0; i < 3; ++i)
        legend[i]->setFrame(
            {r.x + r.width - 175 + i * 57.f, r.y + r.height - 40, 55, 22});
    };
    auto s = std::make_shared<LayoutView>();
    spectrumCard = panel(*scene, s);
    auto sh = label(*s, L"动态频谱", 16, TEXT, 600);
    auto sm = label(*s, L"模拟");
    sm->setAlign(TextAlign::Right);
    auto sp = std::make_shared<Plot>(m, PlotKind::Spectrum);
    s->add(sp);
    auto left = label(*s, L"20 Hz"), right = label(*s, L"20 kHz");
    right->setAlign(TextAlign::Right);
    s->arrange = [sh, sm, sp, left, right](Rect r) {
      sh->setFrame({r.x + 20, r.y + 19, 140, 28});
      sm->setFrame({r.x + r.width - 65, r.y + 22, 45, 22});
      sp->setFrame({r.x + 20, r.y + 62, r.width - 40, r.height - 112});
      left->setFrame({r.x + 20, r.y + r.height - 40, 75, 22});
      right->setFrame({r.x + r.width - 100, r.y + r.height - 40, 80, 22});
    };
  }
  void createParticleAndList() {
    auto c = std::make_shared<LayoutView>();
    particleCard = panel(*scene, c);
    auto heading = label(*c, L"流场粒子", 16, TEXT, 600);
    particleMeta = label(*c, L"");
    particleMeta->setAlign(TextAlign::Right);
    auto plot = std::make_shared<Plot>(m, PlotKind::Particles);
    c->add(plot);
    auto note = label(*c, L"连续轨道 · 独立图元绘制");
    particleSpeed = label(*c, L"");
    particleSpeed->setAlign(TextAlign::Right);
    c->arrange = [this, heading, plot, note](Rect r) {
      heading->setFrame({r.x + 20, r.y + 19, 150, 28});
      particleMeta->setFrame({r.x + 180, r.y + 22, r.width - 200, 22});
      plot->setFrame({r.x + 20, r.y + 62, r.width - 40, r.height - 110});
      note->setFrame({r.x + 20, r.y + r.height - 40, r.width - 150, 22});
      particleSpeed->setFrame(
          {r.x + r.width - 100, r.y + r.height - 40, 80, 22});
    };
    auto lc = std::make_shared<LayoutView>();
    listCard = panel(*scene, lc);
    auto lh = label(*lc, L"事件流", 16, TEXT, 600);
    listMeta = label(*lc, L"");
    listMeta->setAlign(TextAlign::Right);
    auto columns = label(*lc, L"序号               模拟任务", 10);
    listFooter = label(*lc, L"");
    list = std::make_shared<VirtualList>();
    list->setRowHeight(31);
    list->setWheelStep(31);
    ListStateStyleOverride base;
    base.background = PANEL;
    base.borderWidth = 0;
    base.radius = 0;
    base.titleColor = TEXT;
    base.detailColor = MUTED;
    base.separator = LINE;
    base.rowBackground = PANEL;
    base.rowRadius = 0;
    base.rowInset = Insets{0};
    base.textInset = 8;
    base.titleOffsetY = 5.5f;
    base.titleFontSize = 12;
    base.titleFontWeight = 600;
    base.scrollbarColor = MUTED;
    base.scrollbarWidth = 3;
    base.selectedRowBackground = color(0x2a3b34);
    base.selectedTitleColor = TEXT;
    base.selectedDetailColor = MUTED;
    ListStyleOverride style;
    style.normal = base;
    auto hover = base;
    hover.rowBackground = color(0x25322d);
    style.hovered = hover;
    style.selected = base;
    list->setStyleOverride(style);
    list->setOnChanged([this](int i) {
      m.selected = i;
      controls();
    });
    lc->add(list);
    lc->arrange = [this, lh, columns](Rect r) {
      lh->setFrame({r.x + 20, r.y + 19, 140, 28});
      listMeta->setFrame({r.x + 170, r.y + 22, r.width - 190, 22});
      columns->setFrame({r.x + 20, r.y + 61, r.width - 40, 22});
      list->setFrame({r.x + 20, r.y + 87, r.width - 40, r.height - 135});
      listFooter->setFrame({r.x + 20, r.y + r.height - 40, r.width - 40, 22});
    };
  }

public:
  void ensureEditor() {
    if(editor)return;
    editor=std::make_unique<connection_demo::Workspace>(window,[this]{hideEditor();},options.templateEntry,options.dev,options.css);
    add(editor->page.root.widget);editor->page.root.widget->setVisible(false);
  }
  void showEditor() {
    ensureEditor();resumeAnimation=m.running;m.running=false;editing=true;
    for(auto& child:children())child->setVisible(child==editor->page.root.widget || child==subtitle);
    window.setMinimumClientSize({640,560});invalidate();
  }
  void hideEditor() {
    editing=false;for(auto& child:children())child->setVisible(child!=editor->page.root.widget);
    window.setContentScale(1);window.setMinimumClientSize({1050,720});m.running=resumeAnimation;m.telemetry.reset();controls();
    // The old laboratory has a fixed minimum layout. Returning from its narrow
    // editor restores enough native client space without changing editor layout.
    auto hwnd=static_cast<HWND>(window.nativeHandle());RECT bounds{},client{};
    if(GetWindowRect(hwnd,&bounds) && GetClientRect(hwnd,&client)) {
      const int cw=std::max(int(client.right),int(std::ceil(1050*window.dpiScale()))),ch=std::max(int(client.bottom),int(std::ceil(720*window.dpiScale())));
      if(cw!=client.right || ch!=client.bottom)SetWindowPos(hwnd,nullptr,0,0,cw+bounds.right-bounds.left-client.right,ch+bounds.bottom-bounds.top-client.bottom,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
    }
    if(m.running)requestAnimationFrame();
  }
  void closeEditor(){if(editor)editor->close();}
  void requestClose(bool forced) {
    if(forced || !editing || !editor)window.close();
    else editor->flow.request(connection_demo::Connections::Destination::Close);
  }
  void setTextEnvironment(std::wstring family, float scale) override {
    View::setTextEnvironment(std::move(family), scale);
    for (auto &value : metricValues)
      if (value)
        value->setTextEnvironment(L"Consolas", scale);
  }
  Lab(Window &w, Options o) : window(w), options(std::move(o)) {
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
    const std::array<const wchar_t *, 5> names{
        L"UI 更新率", L"平均帧间隔", L"P95 帧间隔", L"进程 CPU", L"工作集内存"},
        units{L"FPS", L"ms", L"ms", L"%", L"MB"};
    for (int i = 0; i < 5; ++i) {
      metricNames[i] = label(*this, names[i]);
      metricValues[i] = label(*this, L"—", 27,
                              i == 0   ? ACCENT
                              : i == 3 ? TEAL
                                       : TEXT,
                              500);
      metricUnits[i] = label(*this, units[i], 11);
    }
    loadLabel = label(*this, L"绘制负载");
    for (int i = 0; i < 3; ++i)
      loads[i] = button(
          *this, std::array<const wchar_t *, 3>{L"轻量", L"标准", L"高压"}[i],
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
    auto method = label(
        *scene,
        L"性能指标为 UI paint 回调间隔，非 GPU 耗时；CPU 为本进程占整机比例。");
    scene->arrange = [this, frameTitle, frameMeta, framePlot, method](Rect r) {
      float y = 0;
      if (m.tab == 0) {
        signalCard->setFrame({r.x, r.y, r.width - 261, 210});
        spectrumCard->setFrame({r.x + r.width - 245, r.y, 245, 210});
        particleCard->setFrame({r.x, r.y + 226, (r.width - 16) / 2, 210});
        listCard->setFrame(
            {r.x + (r.width + 16) / 2, r.y + 226, (r.width - 16) / 2, 210});
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
      if(editing){subtitle->setFrame({r.x+20,r.y+7,r.width-40,20});editor->page.root.widget->setFrame({r.x,r.y+34,r.width,std::max(0.f,r.height-34)});return;}
      sidebar->setFrame({r.x, r.y, 190, r.height});
      float x = r.x + 214, w = r.width - 238;
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
    if(options.warmEditor)ensureEditor();
    if(options.editor){showEditor();if(options.details){editor->flow.selectedKey.set(L"1");editor->flow.view.execute();}else if(!options.connections && !options.gallery)editor->flow.open(L"1");editor->flow.query.set(options.connectionQuery);editor->vm.theme.set(options.editorLight || ((options.connections || options.details || options.gallery) && !options.editorDark)?0:1);editor->vm.density.set(options.compact?1:0);if(options.editorInvalid){editor->vm.host.set(L"https://bad/path");editor->vm.port.set(L"70000");editor->vm.attempted.set(true);}if(options.gallery){editor->flow.gallery.set(true);editor->mount.flush();editor->samples->scene.set(options.galleryScene);}editor->flush();}
  }
  void stressStep() {
    const int cycle=stressStep_/3, phase=stressStep_%3;
    if(phase==0) {
      if(cycle==50){stressStart_=Clock::now();stressCpuStart_=processCpuMs();stressPaints_.clear();stressIntervals_.clear();lastStressPaint_={};}
      if(cycle%50==0 || cycle==options.connectionStress) {
        PROCESS_MEMORY_COUNTERS_EX memory{};memory.cb=sizeof(memory);
        GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),sizeof(memory));
        const auto stats=editor->mount.diagnostics();
        std::filesystem::create_directories(options.output);
        std::ofstream report(options.output/"connections-stress.csv",cycle==0?std::ios::out:std::ios::app);
        if(cycle==0)report<<"cycle,working_set_mib,private_mib,styles,mount_subscriptions,owned_objects,paint_count\n";
        report<<cycle<<','<<memory.WorkingSetSize/1048576.0<<','<<memory.PrivateUsage/1048576.0<<','<<editor->mount.styles()->size()<<','<<stats.subscriptions<<','<<stats.ownedObjects<<','<<editorPaints<<'\n';
      }
      if(cycle==options.connectionStress){
        if(cycle>50) {
          const double elapsed=ms(Clock::now()-stressStart_),cpu=processCpuMs()-stressCpuStart_;
          auto paint=summarize(stressPaints_),interval=summarize(stressIntervals_);
          PROCESS_MEMORY_COUNTERS_EX memory{};memory.cb=sizeof(memory);GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),sizeof(memory));
          std::ofstream report(options.output/"entry-performance.txt");
          report<<"entry="<<(options.templateEntry?"template":"code")<<"\ncycles="<<cycle-50<<"\nelapsed_ms="<<elapsed<<"\ncpu_ms="<<cpu<<"\ncpu_percent="<<100*cpu/elapsed/std::max(1u,std::thread::hardware_concurrency())<<"\npaint_mean_ms="<<paint.mean<<"\npaint_p95_ms="<<paint.p95<<"\ninterval_mean_ms="<<interval.mean<<"\ninterval_p95_ms="<<interval.p95<<"\npaint_count="<<stressPaints_.size()<<"\nworking_set_mib="<<memory.WorkingSetSize/1048576.0<<"\nprivate_mib="<<memory.PrivateUsage/1048576.0<<'\n';
          std::ofstream raw(options.output/"entry-paint.csv");raw<<"paint_ms\n";for(auto value:stressPaints_)raw<<value<<'\n';
        }
        window.close();return;
      }
      editor->flow.query.set(cycle%2?L"节点":L"");editor->flow.filter.set(cycle%3);editor->flush();
      editor->flow.selectedKey.set(editor->flow.filtered.get().front().id);editor->flow.edit.execute();
    } else if(phase==1) {
      editor->vm.note.set(L"循环输入 "+std::to_wstring(cycle));editor->flow.request(connection_demo::Connections::Destination::List);
    } else editor->flow.discard.execute();
    editor->flush();++stressStep_;
    window.requestAnimationFrame([this](double){stressStep();});
  }
  void updateRendererLabel() {
    const auto now = Clock::now();
    if (ms(now-rendererLabelTime_) < 250) return;
    rendererLabelTime_ = now;
    const auto info = window.rendererInfo();
    const auto wide=[](const std::string& text){return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>{}.from_bytes(text);};
    const auto policy=options.renderer=="cpu"?L"CPU 软件":options.renderer=="gpu"?L"GPU 优先":L"自动";
    std::wstring text = std::wstring(L"启动选择：") + policy + L"  ·  实际：";
    if (info.backend==RenderBackend::OpenGL) text += L"OpenGL / Skia Ganesh  ·  " + wide(info.device);
    else if(info.backend==RenderBackend::Software) {
      text += L"CPU / Skia Raster";
      text += info.reason=="disabled-by-environment"?L"  ·  已指定软件渲染":L"  ·  GPU 回退：" + wide(info.reason);
    } else text += L"等待首次绘制";
    subtitle->setText(text);
    subtitle->setTooltip(text + L"\n切换模式需要重新启动；自动遵循 ONEUI_ENABLE_GPU 环境设置。");
  }
  void rendererBenchmarkTick(double elapsed) {
    if(diagnosticStop)return;
    if(elapsed>=3 && !rendererSampling_) {
      rendererBenchmark_.begin(window,m.meshDraws,m.meshFallbacks,m.meshVertices);rendererSampling_=true;
    }
    if(elapsed>=3+options.sampleSeconds) {
      try { rendererBenchmark_.finish(window,options.output,options.renderer,options.rendererBenchmark,
          particleModeName(m.particleMode),m.meshDraws,m.meshFallbacks,m.meshVertices,m.meshReason,m.meshLastFallback); }
      catch(const std::exception& error){std::cerr<<error.what()<<'\n';result=2;}
      diagnosticStop=true;window.close();return;
    }
    if(options.rendererBenchmark=="table") {
      auto table=std::dynamic_pointer_cast<Table>(editor->page.fields.at("$table"));
      // Same wall-time trajectory at 60 posted updates/s; native virtualized table.
      table->setScrollOffset(static_cast<float>(std::fmod(elapsed*360.0,8000.0)));
    }
  }
  void beginRendererBenchmark() {
    diagnosticThread=std::thread([this]{
      const auto start=Clock::now();auto next=start;
      while(!diagnosticStop) {
        const double elapsed=ms(Clock::now()-start)/1000.0;
        const bool idle=options.rendererBenchmark=="idle";
        const bool shouldPost=!idle || (elapsed>=3 && !idleStarted_) || elapsed>=3+options.sampleSeconds;
        if(shouldPost && !benchmarkPostPending_.exchange(true)) {
          if(elapsed>=3)idleStarted_=true;
          if(!window.post([this,elapsed]{benchmarkPostPending_=false;rendererBenchmarkTick(elapsed);}))break;
        }
        next+=std::chrono::microseconds(16667);
        if(next<Clock::now())next=Clock::now();
        std::this_thread::sleep_until(next);
      }
    });
  }
  void stabilityTick(double elapsed) {
    if(diagnosticStop)return;
    try {
      if(elapsed<3)return;
      const double sampled=elapsed-3;
      if(!soak_.active)soak_.begin(window,m,options.output);
      if(sampled-soak_.previousSeconds>=5 || sampled>=options.stabilitySeconds)
        soak_.sample(window,m,sampled);
      if(options.stabilityExercise) {
        const int step=int(sampled/2);
        if(step!=exerciseStep_) {
          exerciseStep_=step;
          switch(step%10) {
            case 0: setTab(2);break;
            case 1: toggle();break;
            case 2: toggle();break;
            case 3: setTab(1);break;
            case 4: setTab(3);break;
            case 5: showEditor();break;
            case 6: {
              window.setContentScale(1.5f);
              auto hwnd=static_cast<HWND>(window.nativeHandle());RECT outer{},client{};
              if(!GetWindowRect(hwnd,&outer) || !GetClientRect(hwnd,&client))throw std::runtime_error("Resize diagnostic failed");
              if(!SetWindowPos(hwnd,nullptr,0,0,640+outer.right-outer.left-client.right,
                  600+outer.bottom-outer.top-client.bottom,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE))throw std::runtime_error("Resize failed");
              break;
            }
            case 7: hideEditor();break;
            case 8: window.setContentScale(1.25f);break;
            case 9: window.setContentScale(1);setTab(2);break;
          }
          if(m.running != !(step%10==1 || step%10==5 || step%10==6) ||
             editing != (step%10==5 || step%10==6))throw std::runtime_error("Interaction state mismatch");
          if(!exerciseLog_.is_open()) {
            exerciseLog_.open(options.output/"interactions.csv");exerciseLog_.exceptions(std::ios::failbit|std::ios::badbit);
            exerciseLog_<<"step,seconds,tab,editing,running,width,height,mesh_draws,mesh_fallbacks\n";
          }
          exerciseLog_<<step<<','<<sampled<<','<<m.tab<<','<<editing<<','<<m.running<<','
            <<window.clientSize().width<<','<<window.clientSize().height<<','<<m.meshDraws<<','<<m.meshFallbacks<<'\n';
          exerciseLog_.flush();
        }
      }
      if(sampled>=options.stabilitySeconds) {
        std::ofstream done(options.output/"stability-complete.txt");done<<"seconds="<<sampled<<"\nexercise_steps="<<exerciseStep_+1<<'\n';
        diagnosticStop=true;window.close();
      }
    } catch(const std::exception& error) {
      std::cerr<<error.what()<<'\n';result=2;diagnosticStop=true;window.close();
    }
  }
  void beginStability() {
    diagnosticThread=std::thread([this]{
      const auto start=Clock::now();
      while(!diagnosticStop) {
        const double elapsed=ms(Clock::now()-start)/1000;
        if(!benchmarkPostPending_.exchange(true) && !window.post([this,elapsed]{
          benchmarkPostPending_=false;stabilityTick(elapsed);
        }))break;
        for(int i=0;i<5 && !diagnosticStop;++i)std::this_thread::sleep_for(std::chrono::milliseconds(50));
      }
    });
  }
  bool idleStarted_=false; // Accessed only by the bounded benchmark worker.
  void begin() {
    window.prepareLayoutSnapshot();updateRendererLabel();
    if(options.stabilitySeconds>0){requestAnimationFrame();beginStability();return;}
    if(!options.rendererBenchmark.empty()){if(!editing)requestAnimationFrame();beginRendererBenchmark();return;}
    if(options.warmEditor && !editing){showEditor();window.prepareLayoutSnapshot();hideEditor();window.prepareLayoutSnapshot();}
    requestAnimationFrame();
    if(options.connectionStress>0){window.requestAnimationFrame([this](double){stressStep();});return;}
    if(editing && (!options.snapshot.empty() || options.editorIdle>0)) {
      // Event-driven editor needs no render loop. Bounded diagnostic timers
      // return through the native UI queue and are joined before Lab is released.
      diagnosticThread=std::thread([this] {
        for(int i=0;i<20 && !diagnosticStop;++i)std::this_thread::sleep_for(std::chrono::milliseconds(50));
        if(diagnosticStop)return;
        window.post([this]{
          window.prepareLayoutSnapshot();
          if(!options.snapshot.empty()) {
            std::filesystem::create_directories(options.snapshot.parent_path());
            if(!window.captureFramePng(options.snapshot.wstring()))result=3;
            std::ofstream report(options.snapshot.string()+".layout.txt");report<<oneui::ui::formatLayoutIssues(oneui::ui::inspectLayout(editor->page.root.widget,*editor->mount.styles()));
            if(options.snapshotExit)window.close();
          }
          idleStart=editorPaints;
        });
        if(options.editorIdle<=0)return;
        const auto end=Clock::now()+std::chrono::duration<double>(options.editorIdle);
        while(!diagnosticStop && Clock::now()<end)std::this_thread::sleep_for(std::chrono::milliseconds(25));
        if(!diagnosticStop)window.post([this]{
          std::filesystem::create_directories(options.output);std::ofstream report(options.output/"editor-idle.txt");
          report<<"paint_count="<<editorPaints-idleStart<<"\ncpu_percent="<<m.telemetry.monitor.cpu<<"\nworking_set_mb="<<m.telemetry.monitor.memory<<"\ndpi_scale="<<window.dpiScale()<<"\nstyles="<<editor->mount.styles()->size()<<"\n";
          window.close();
        });
      });
    }
  }
  ~Lab(){diagnosticStop=true;if(diagnosticThread.joinable())diagnosticThread.join();closeEditor();}
  std::atomic<bool> diagnosticStop{false};std::thread diagnosticThread;std::uint64_t idleStart=0;
  int exitCode() const { return result; }
  bool onMouseWheel(const MouseWheelEvent &e) override {
    if(editing)return View::onMouseWheel(e);
    // The outer ScrollView consumes wheel events before its descendants.
    // Give the native virtual list first refusal inside its visible viewport.
    if (listCard->visible() && workspace->frame().contains(e.position) &&
        list->frame().contains(e.position) && list->onMouseWheel(e))
      return true;
    return View::onMouseWheel(e);
  }
  bool key(const KeyEvent &e) {
    if (options.traceInput)
      std::cout << "key=" << e.virtualKey << " down=" << e.pressed
                << " repeat=" << e.repeat << " ctrl=" << e.control
                << " alt=" << e.alt << std::endl;
    if(editing){
      if(e.key==Key::Escape && e.pressed) {
        auto* focused=oneui::ui::focusedField(editor->page.root.widget);
        if(auto* select=dynamic_cast<Select*>(focused);select && select->onKeyDown(e))return true;
        if(auto* input=dynamic_cast<TextField*>(focused);input && input->hasTextComposition())return false;
      }
      if(e.control && !e.alt && e.virtualKey=='S') {if(e.pressed && !e.repeat && editor->flow.showForm.get())editor->vm.save.execute();return true;}
      if(e.virtualKey==VK_ESCAPE && e.pressed) {if(editor->flow.gallery.get())editor->flow.gallery.set(false);else if(editor->flow.prompt.get())editor->flow.keep.execute();else if(editor->flow.form.get())editor->vm.back.execute();else if(editor->flow.showDetail.get())editor->flow.detailBack.execute();return true;}
      if(options.traceInput && e.control && e.shift && e.virtualKey==VK_F12 && e.pressed) {
        window.post([this]{
          std::filesystem::create_directories(options.output);window.prepareLayoutSnapshot();
          window.captureFramePng((options.output/"editor-interaction.png").wstring());
          auto utf8=[](const std::wstring& s){return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>{}.to_bytes(s);};
          std::ofstream report(options.output/"editor-interaction.txt");auto* focus=oneui::ui::focusedField(editor->page.root.widget);
          report<<"name="<<utf8(editor->vm.name.get())<<"\nport="<<utf8(editor->vm.port.get())<<"\nstatus="<<utf8(editor->vm.status.get())<<"\nfocused="<<(focus?utf8(focus->accessibleName()):"")<<"\nload="<<m.load<<"\nsampling="<<m.telemetry.recording<<"\ndpi_scale="<<window.dpiScale()<<"\n";
          report<<"records="<<editor->flow.records.get().size()<<"\nfiltered="<<editor->flow.filtered.get().size()<<"\nselected="<<utf8(editor->flow.selectedKey.get())<<"\nform="<<editor->flow.form.get()<<"\nprompt="<<editor->flow.prompt.get()<<"\nscroll="<<std::dynamic_pointer_cast<Table>(editor->page.fields.at("$table"))->scrollOffset()<<"\nstyles="<<editor->mount.styles()->size()<<"\n";
          report<<"entry="<<(options.templateEntry?"template":"code")<<"\nstyle_applies="<<editor->styles.applied<<"\nstyle_error="<<utf8(editor->flow.styleError.get())<<"\n";
          report<<"density="<<editor->vm.density.get()<<"\ntheme="<<editor->vm.theme.get()<<"\n";
          if(editor->samples)report<<"gallery_name="<<utf8(editor->samples->name.get())<<"\ngallery_scene="<<editor->samples->scene.get()<<"\ngallery_loading="<<editor->samples->loading.get()<<"\ngallery_error="<<utf8(editor->samples->error.get())<<"\n";
        });return true;
      }
      if(e.key==Key::Tab && e.pressed)editor->afterTab();return false;
    }
    if (e.control || e.alt)
      return false;
    const bool mapped = (e.virtualKey >= '1' && e.virtualKey <= '3') ||
                        e.virtualKey == VK_SPACE || e.virtualKey == 'B' ||
                        e.virtualKey == 'R' || e.virtualKey == 'P';
    if (!mapped)
      return false;
    // Chinese IMEs may replace keydown with VK_PROCESSKEY. Keyup retains
    // the physical key; consume ordinary keydown and activate once on release.
    if (e.pressed)
      return true;
    if (e.virtualKey >= '1' && e.virtualKey <= '3') {
      setLoad(int(e.virtualKey - '1'));
      return true;
    }
    if (e.virtualKey == VK_SPACE) {
      toggle();
      return true;
    }
    if (e.virtualKey == 'B') {
      startRecord();
      return true;
    }
    if (e.virtualKey == 'R') {
      if (!m.telemetry.recording) {
        m.telemetry.reset();
        message = L"统计已重置";
        status->setText(message);
        invalidate();
      }
      return true;
    }
    // Diagnostic capture is explicitly excluded from benchmark runs.
    if (e.virtualKey == 'P' && !m.telemetry.recording) {
      window.post([this] {
        std::filesystem::create_directories(options.output);
        window.captureFramePng((options.output / "interaction.png").wstring());
        std::ofstream state(options.output / "interaction.txt");
        state << "load=" << m.load << "\nview=" << m.tab
              << "\nrunning=" << m.running << "\nselected=" << m.selected
              << "\nscroll=" << list->scrollOffset() << "\nspeed=" << m.speed
              << "\nclient_width=" << window.clientSize().width
              << "\nclient_height=" << window.clientSize().height << '\n';
      });
      return true;
    }
    return false;
  }
  // Apply widget properties before painting; paint only samples telemetry.
  void updateMetrics() {
    if (options.benchmark > 0 && !autoStarted &&
        ms(Clock::now() - m.telemetry.started) > 3000) {
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
        auto path =
            m.telemetry.save(options.output, m.load, m.tab, m.particles(),
                             m.points(), m.speed, window.clientSize().width,
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
    status->setText(m.telemetry.recording
                        ? L"正在采样 · 剩余 " +
                              number(m.telemetry.remaining()) +
                              L" 秒 · 负载、视图与速度已锁定"
                        : message);
  }
  bool tickAnimations(double now) override {
    updateRendererLabel();
    if (!editing) updateMetrics();
    bool changed = View::tickAnimations(now);
    if (m.running) {
      invalidate();
      return true;
    }
    return changed;
  }
  void paint(Canvas &canvas) override {
    RendererBenchmark::PaintSample sample(rendererBenchmark_);
    if(editing){
      canvas.fillRect(frame(),BG);
      ++editorPaints;const auto begin=Clock::now();View::paint(canvas);
      if(options.connectionStress>50 && stressStep_>=150) {
        stressPaints_.push_back(ms(Clock::now()-begin));
        if(lastStressPaint_!=Clock::time_point{})stressIntervals_.push_back(ms(begin-lastStressPaint_));
        lastStressPaint_=begin;
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
};

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  try {
    int argc = 0;
    auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    Options o;
    bool selfTest = false;
    for (int i = 1; i < argc; ++i) {
      std::wstring arg = argv[i];
      auto next = [&]() -> std::wstring {
        if (i + 1 >= argc)
          throw std::runtime_error("Missing argument value");
        return argv[++i];
      };
      if (arg == L"--renderer") {
        const auto value=next();
        if(value!=L"auto" && value!=L"gpu" && value!=L"cpu")throw std::runtime_error("Renderer must be auto, gpu or cpu");
        o.renderer=std::string(value.begin(),value.end());
      } else if (arg == L"--particle-mode") {
        const auto value=next();
        if(value==L"reference")o.particleMode=ParticleMode::Reference;
        else if(value==L"precomputed")o.particleMode=ParticleMode::Precomputed;
        else if(value==L"batch")o.particleMode=ParticleMode::Batch;
        else if(value==L"combined")o.particleMode=ParticleMode::Combined;
        else if(value==L"mesh")o.particleMode=ParticleMode::Mesh;
        else throw std::runtime_error("Particle mode must be reference, precomputed, batch, combined or mesh");
      } else if (arg == L"--renderer-benchmark") {
        const auto value=next();
        if(value!=L"idle" && value!=L"table" && value!=L"chart" && value!=L"particles")throw std::runtime_error("Invalid renderer benchmark scene");
        o.rendererBenchmark=std::string(value.begin(),value.end());
      } else if(arg==L"--sample-seconds")o.sampleSeconds=std::stod(next());
      else if(arg==L"--stability-seconds")o.stabilitySeconds=std::stod(next());
      else if(arg==L"--stability-exercise")o.stabilityExercise=true;
      else if (arg == L"--load") {
        auto s = next();
        if (s != L"light" && s != L"medium" && s != L"heavy")
          throw std::runtime_error("Invalid load");
        o.load = s == L"light" ? 0 : s == L"heavy" ? 2 : 1;
      } else if (arg == L"--view") {
        auto s = next();
        if (s != L"overview" && s != L"chart" && s != L"particles" &&
            s != L"list" && s != L"editor" && s != L"connections" && s != L"details" && s != L"components")
          throw std::runtime_error("Invalid view");
        o.tab = s == L"chart"       ? 1
                : s == L"particles" ? 2
                : s == L"list"      ? 3
                                    : 0;
        o.connections=s==L"connections";o.details=s==L"details";o.gallery=s==L"components";o.editor=s==L"editor" || o.connections || o.details || o.gallery;
      } else if (arg == L"--benchmark-seconds") {
        o.benchmark = std::stod(next());
        if (!std::isfinite(o.benchmark) || o.benchmark <= 0 ||
            o.benchmark > 3600)
          throw std::runtime_error("Invalid duration");
      } else if (arg == L"--width")
        o.width = std::max(640, std::stoi(next()));
      else if (arg == L"--height")
        o.height = std::max(560, std::stoi(next()));
      else if (arg == L"--warm-editor")o.warmEditor=true;
      else if (arg == L"--editor-light")o.editorLight=true;
      else if (arg == L"--editor-dark")o.editorDark=true;
      else if (arg == L"--editor-invalid")o.editorInvalid=true;
      else if (arg == L"--connection-query")o.connectionQuery=next();
      else if (arg == L"--entry") {auto value=next();if(value!=L"code" && value!=L"template")throw std::runtime_error("Invalid entry");o.templateEntry=value==L"template";}
      else if (arg == L"--dev")o.dev=true;
      else if (arg == L"--compact")o.compact=true;
      else if (arg == L"--component-scene"){o.galleryScene=std::stoi(next());if(o.galleryScene<0 || o.galleryScene>2)throw std::runtime_error("Component scene must be 0, 1 or 2");}
      else if (arg == L"--css")o.css=std::filesystem::absolute(next());
      else if (arg == L"--connections-stress")o.connectionStress=std::stoi(next());
      else if (arg == L"--scale")o.scale=std::stof(next());
      else if (arg == L"--editor-idle-seconds")o.editorIdle=std::stod(next());
      else if (arg == L"--exit-after-benchmark")
        o.exitAfter = true;
      else if (arg == L"--snapshot")
        o.snapshot = std::filesystem::absolute(next());
      else if (arg == L"--snapshot-exit")
        o.snapshotExit = true;
      else if (arg == L"--output")
        o.output = std::filesystem::absolute(next());
      else if (arg == L"--trace-input")
        o.traceInput = true;
      else if (arg == L"--self-test")
        selfTest = true;
      else
        throw std::runtime_error("Unknown argument");
    }
    LocalFree(argv);
    if(!std::isfinite(o.scale) || o.scale<1 || o.scale>2 || !std::isfinite(o.editorIdle) || o.editorIdle<0 || o.editorIdle>60)throw std::runtime_error("Invalid diagnostic options");
    if(o.connectionStress<0 || o.connectionStress>3000 || (o.connectionStress && (!o.connections || !o.snapshot.empty() || o.editorIdle>0 || !o.connectionQuery.empty())))throw std::runtime_error("Use --view connections for standalone stress diagnostics");
    if(!o.editor){o.width=std::max(1050,o.width);o.height=std::max(720,o.height);}
    if(o.editor && o.benchmark>0)throw std::runtime_error("Use --editor-idle-seconds for the event-driven editor");
    if (selfTest) {
      std::vector<double> values(94, 10);
      values.insert(values.end(), 6, 50);
      auto s = summarize(values);
      bool ok = std::abs(s.mean - 12.4) < 1e-9 && s.p95 == 50 && s.p99 == 50 &&
                s.max == 50 && summarize({}).fps == 0;
      std::cout << (ok ? "Telemetry tests passed\n"
                       : "Telemetry tests failed\n");
      return ok ? 0 : 1;
    }
    if (o.benchmark > 0 && !o.snapshot.empty())
      throw std::runtime_error("Capture and benchmark must run separately");
    if(!std::isfinite(o.sampleSeconds) || o.sampleSeconds<1 || o.sampleSeconds>60)throw std::runtime_error("Sample seconds must be 1..60");
    if(!o.rendererBenchmark.empty()) {
      if(o.benchmark>0 || o.dev || !o.snapshot.empty() || o.editorIdle>0 || o.connectionStress>0)throw std::runtime_error("Renderer benchmark must run standalone with dev and capture disabled");
      o.editor=o.connections=o.rendererBenchmark=="idle" || o.rendererBenchmark=="table";
      o.details=o.gallery=false;o.tab=o.rendererBenchmark=="chart"?1:o.rendererBenchmark=="particles"?2:0;
    }
    if(!std::isfinite(o.stabilitySeconds) || o.stabilitySeconds<0 || o.stabilitySeconds>3600 ||
       (o.stabilityExercise && o.stabilitySeconds<20))throw std::runtime_error("Invalid stability duration");
    if(o.stabilitySeconds>0) {
      if(o.benchmark>0 || !o.rendererBenchmark.empty() || o.dev || !o.snapshot.empty() || o.editorIdle>0 || o.connectionStress>0)
        throw std::runtime_error("Stability diagnostics must run standalone");
      o.editor=o.connections=o.details=o.gallery=false;o.tab=2;
    }
    // Auto inherits the process environment. Explicit modes override it before initialization.
    if(o.renderer!="auto" && !SetEnvironmentVariableW(L"ONEUI_ENABLE_GPU",o.renderer=="cpu"?L"0":L"1"))throw std::runtime_error("Unable to select renderer");
    WindowOptions wo;
    wo.title = std::wstring(L"FORM / LAB — OneUI 性能实验台 · ")+(o.templateEntry?L".one":L"C++")+(o.dev?L" · 样式预览":L"");
    wo.width = o.width;
    wo.height = o.height;
    auto window = Window::create(wo);
    window->setDefaultFontFamily(L"Microsoft YaHei UI");
    window->setMinimumClientSize(o.editor?Size{640,560}:Size{1050,720});window->setContentScale(o.scale);
    auto lab = std::make_shared<Lab>(*window, o);
    window->setContent(lab);
    window->setRawKeyHandler([lab](const KeyEvent &e) { return lab->key(e); });
    window->setOnCloseRequested([lab](bool forced){lab->requestClose(forced);});
    window->initialize();
    window->centerOnActiveMonitor();
    window->show();
    window->activate();
    lab->begin();
    int code = window->run();
    lab->diagnosticStop=true;lab->closeEditor();window->setRawKeyHandler({});window->setOnCloseRequested({});
    return lab->exitCode() ? lab->exitCode() : code;
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }
}
