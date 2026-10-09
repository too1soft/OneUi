#include "performance_page.hpp"
#include <iostream>

void Lab::rebuildRows() {
    std::vector<VirtualListItem> items;
    items.reserve(m.rows());
    const std::array<const wchar_t *, 5> names{L"准备图元", L"更新采样缓冲", L"提交绘制指令",
                                               L"同步视图状态", L"合成帧输出"};
    for (int i = 0; i < m.rows(); ++i) {
        std::wostringstream titleText;
        titleText << std::setfill(L'0') << std::setw(6) << i + 1 << L"      " << names[i % 5];
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

void Lab::createSidebar() {
    auto content = std::make_shared<LayoutView>();
    sidebar = panel(*this, content, color(0x131a1d));
    sidebar->setRadius(0);
    auto brand = label(*content, L"FORM / LAB", 23, TEXT, 700);
    auto detail = label(*content, L"OneUI 原生性能实验台");
    auto mark = std::make_shared<Rule>(ACCENT);
    content->add(mark);
    const std::array<const wchar_t *, 4> texts{L"综合实验台", L"实时图表", L"粒子场", L"虚拟列表"};
    for (int i = 0; i < 4; ++i) {
        nav[i] = button(*content, L"   " + std::wstring(texts[i]), [this, i] { setTab(i); });
        nav[i]->setContentAlign(TextAlign::Left);
    }
    auto edit = button(*content, L"   连接管理", [this] {
        if (!m.telemetry.recording)
            showEditor();
    });
    edit->setContentAlign(TextAlign::Left);
    auto divider = std::make_shared<Rule>();
    content->add(divider);
    std::array<L, 7> foot{label(*content, L"快捷操作"),
                          label(*content, L"Space    暂停 / 继续"),
                          label(*content, L"1 / 2 / 3    切换负载"),
                          label(*content, L"B    采样 10 秒"),
                          label(*content, L"R    重置统计"),
                          label(*content, L"C++ + OneUI", 11),
                          label(*content,
#ifdef __APPLE__
                                L"macOS / Skia",
#else
                                L"Windows / Skia",
#endif
                                11)};
    content->arrange = [this, brand, detail, mark, divider, foot, edit](Rect r) {
        brand->setFrame({r.x + 33, r.y + 20, 148, 34});
        detail->setFrame({r.x + 20, r.y + 58, 165, 22});
        mark->setFrame({r.x + 20, r.y + 26, 4, 24});
        for (int i = 0; i < 4; ++i)
            nav[i]->setFrame({r.x + 20, r.y + 119 + i * 54.f, 149, 47});
        edit->setFrame({r.x + 20, r.y + 335, 149, 47});
        float y = r.y + r.height - 230;
        divider->setFrame({r.x + 20, y, 149, 1});
        for (int i = 0; i < 5; ++i)
            foot[i]->setFrame({r.x + 20, y + 19 + i * 28.f, 166, 20});
        foot[5]->setFrame({r.x + 20, r.y + r.height - 58, 166, 20});
        foot[6]->setFrame({r.x + 20, r.y + r.height - 40, 166, 20});
    };
}

void Lab::createSignal() {
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
    const std::array<const wchar_t *, 4> ticks{L"−12 s", L"−8 s", L"−4 s", L"现在"};
    for (int i = 0; i < 4; ++i)
        xs[i] = label(*c, ticks[i]);
    xs[3]->setAlign(TextAlign::Right);
    std::array<L, 3> legend{label(*c, L"A / 波形", 11, ACCENT), label(*c, L"B / 调制", 11, TEAL),
                            label(*c, L"C / 谐波", 11, ORANGE)};
    c->arrange = [this, heading, plot, ys, xs, legend](Rect r) {
        heading->setFrame({r.x + 20, r.y + 19, 150, 28});
        signalMeta->setFrame({r.x + 180, r.y + 22, r.width - 200, 22});
        float h = r.height - 132;
        plot->setFrame({r.x + 57, r.y + 62, r.width - 77, h});
        for (int i = 0; i < 5; ++i)
            ys[i]->setFrame({r.x + 20, r.y + 54 + h * i / 4, 30, 18});
        for (int i = 0; i < 4; ++i)
            xs[i]->setFrame({r.x + 57 + (r.width - 127) * i / 3, r.y + r.height - 62, 50, 22});
        for (int i = 0; i < 3; ++i)
            legend[i]->setFrame({r.x + r.width - 175 + i * 57.f, r.y + r.height - 40, 55, 22});
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

void Lab::createParticleAndList() {
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
        particleSpeed->setFrame({r.x + r.width - 100, r.y + r.height - 40, 80, 22});
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
