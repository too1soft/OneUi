#include "performance_page.hpp"
#include <iostream>

void Lab::controls() {
    refreshDesktopMenus();
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
    signalMeta->setText(L"模拟数据 · 3 × " + std::to_wstring(m.points()) + L" 采样点");
    particleMeta->setText(std::to_wstring(m.particles()) + L" 个 / 实时运动");
    particleSpeed->setText(number(m.speed) + L"× 速度");
    listMeta->setText(std::to_wstring(m.rows()) + L" 行 · 虚拟滚动");
    std::wostringstream selected;
    selected << L"已选择 #" << std::setfill(L'0') << std::setw(6) << m.selected + 1
             << L" · 仅绘制可见行";
    listFooter->setText(selected.str());
    const std::array<const wchar_t *, 4> titles{L"让流畅，看得见。", L"每一条曲线，都在流动。",
                                                L"让每个粒子，各自发光。",
                                                L"十万行，也能轻松浏览。"};
    title->setText(titles[m.tab]);
    signalCard->setVisible(m.tab == 0 || m.tab == 1);
    spectrumCard->setVisible(m.tab == 0 || m.tab == 1);
    particleCard->setVisible(m.tab == 0 || m.tab == 2);
    listCard->setVisible(m.tab == 0 || m.tab == 3);
    workspace->setContentHeight(m.tab == 0 ? 568.f : (m.tab == 1 ? 568.f : 623.f));
    invalidate();
}

void Lab::setLoad(int n) {
    if (m.telemetry.recording)
        return;
    m.load = n;
    rebuildRows();
    m.telemetry.reset();
    controls();
}

void Lab::setTab(int n) {
    if (m.telemetry.recording)
        return;
    m.tab = n;
    workspace->setScrollOffset(0);
    m.telemetry.reset();
    controls();
}

void Lab::toggle() {
    if (m.telemetry.recording)
        return;
    m.running = !m.running;
    m.telemetry.hasPrevious = false;
    controls();
    if (m.running)
        requestAnimationFrame();
}

void Lab::startRecord(double seconds) {
    if (m.telemetry.recording)
        return;
    m.running = true;
    m.telemetry.record(seconds);
    controls();
    requestAnimationFrame();
}

bool Lab::key(const KeyEvent &e) {
    if (options.traceInput)
        std::cout << "key=" << e.virtualKey << " down=" << e.pressed << " repeat=" << e.repeat
                  << " ctrl=" << e.control << " alt=" << e.alt << std::endl;
    if (editing) {
        if (e.key == Key::Escape && e.pressed) {
            auto *focused = oneui::ui::focusedField(editor->page.root.widget);
            if (auto *select = dynamic_cast<Select *>(focused); select && select->onKeyDown(e))
                return true;
            if (auto *input = dynamic_cast<TextField *>(focused);
                input && input->hasTextComposition())
                return false;
        }
        if (e.control && !e.alt && e.virtualKey == 'S') {
            if (e.pressed && !e.repeat && editor->flow.showForm.get())
                editor->vm.save.execute();
            return true;
        }
        if (e.key == Key::Escape && e.pressed) {
            if (editor->flow.gallery.get())
                editor->flow.gallery.set(false);
            else if (editor->flow.prompt.get())
                editor->flow.keep.execute();
            else if (editor->flow.form.get())
                editor->vm.back.execute();
            else if (editor->flow.showDetail.get())
                editor->flow.detailBack.execute();
            return true;
        }
        if (options.traceInput && e.control && e.shift && e.virtualKey == 0x7B && e.pressed) {
            window.post([this] {
                std::filesystem::create_directories(options.output);
                window.prepareLayoutSnapshot();
                window.captureFramePng((options.output / "editor-interaction.png").wstring());
                auto utf8 = [](const std::wstring &s) {
                    return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>{}.to_bytes(s);
                };
                std::ofstream report(options.output / "editor-interaction.txt");
                auto *focus = oneui::ui::focusedField(editor->page.root.widget);
                report << "name=" << utf8(editor->vm.name.get())
                       << "\nport=" << utf8(editor->vm.port.get())
                       << "\nstatus=" << utf8(editor->vm.status.get())
                       << "\nfocused=" << (focus ? utf8(focus->accessibleName()) : "")
                       << "\nload=" << m.load << "\nsampling=" << m.telemetry.recording
                       << "\ndpi_scale=" << window.dpiScale() << "\n";
                report << "records=" << editor->flow.records.get().size()
                       << "\nfiltered=" << editor->flow.filtered.get().size()
                       << "\nselected=" << utf8(editor->flow.selectedKey.get())
                       << "\nform=" << editor->flow.form.get()
                       << "\nprompt=" << editor->flow.prompt.get() << "\nscroll="
                       << std::dynamic_pointer_cast<Table>(editor->page.fields.at("$table"))
                              ->scrollOffset()
                       << "\nstyles=" << editor->mount.styles()->size() << "\n";
                report << "entry=" << (options.templateEntry ? "template" : "code")
                       << "\nstyle_applies=" << editor->styles.applied
                       << "\nstyle_error=" << utf8(editor->flow.styleError.get()) << "\n";
                report << "density=" << editor->vm.density.get()
                       << "\ntheme=" << editor->vm.theme.get() << "\n";
                if (editor->samples)
                    report << "gallery_name=" << utf8(editor->samples->name.get())
                           << "\ngallery_scene=" << editor->samples->scene.get()
                           << "\ngallery_loading=" << editor->samples->loading.get()
                           << "\ngallery_error=" << utf8(editor->samples->error.get()) << "\n";
            });
            return true;
        }
        if (e.key == Key::Tab && e.pressed)
            editor->afterTab();
        return false;
    }
    if (e.control || e.alt || e.meta())
        return false;
    const bool mapped = (e.virtualKey >= '1' && e.virtualKey <= '3') || e.key == Key::Space ||
                        e.virtualKey == 'B' || e.virtualKey == 'R' || e.virtualKey == 'P';
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
    if (e.key == Key::Space) {
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
            state << "load=" << m.load << "\nview=" << m.tab << "\nrunning=" << m.running
                  << "\nselected=" << m.selected << "\nscroll=" << list->scrollOffset()
                  << "\nspeed=" << m.speed << "\nclient_width=" << window.clientSize().width
                  << "\nclient_height=" << window.clientSize().height << '\n';
        });
        return true;
    }
    return false;
}

bool Lab::onMouseWheel(const MouseWheelEvent &e) {
    if (editing)
        return View::onMouseWheel(e);
    // The outer ScrollView consumes wheel events before its descendants.
    // Give the native virtual list first refusal inside its visible viewport.
    if (listCard->visible() && workspace->frame().contains(e.position) &&
        list->frame().contains(e.position) && list->onMouseWheel(e))
        return true;
    return View::onMouseWheel(e);
}

void Lab::refreshDesktopMenus() {
    if (!desktopMenusEnabled_)
        return;
    NativeMenuItem scenes{0, L"场景"}, loadsMenu{0, L"负载"}, run{0, L"运行"};
    scenes.enabled = loadsMenu.enabled = !editing && !m.telemetry.recording;
    const wchar_t *names[] = {L"总览", L"图表", L"粒子", L"列表"};
    for (int i = 0; i < 4; ++i) {
        NativeMenuItem item{100 + i, names[i]};
        item.check = MenuCheck::Radio;
        item.radioGroup = 1;
        item.checked = m.tab == i;
        item.shortcut = {std::to_string(i + 1), KeyModifierPrimary};
        scenes.children.push_back(item);
    }
    const wchar_t *levels[] = {L"轻量", L"标准", L"重度"};
    for (int i = 0; i < 3; ++i) {
        NativeMenuItem item{110 + i, levels[i]};
        item.check = MenuCheck::Radio;
        item.radioGroup = 1;
        item.checked = m.load == i;
        loadsMenu.children.push_back(item);
    }
    NativeMenuItem running{120, L"实时运行", !editing && !m.telemetry.recording};
    running.check = MenuCheck::Check;
    running.checked = m.running;
    running.shortcut = {"p", KeyModifierPrimary};
    run.children = {running,
                    {121, L"记录性能", !editing && !m.telemetry.recording},
                    {0, {}, true, true},
                    {130, L"显示窗口"},
                    {131, L"退出实验台"}};
    window.setMenuBar({scenes, loadsMenu, run});
    window.setTrayMenu({{130, L"显示窗口"},
                        scenes,
                        loadsMenu,
                        running,
                        {0, {}, true, true},
                        {131, L"退出实验台"}});
}

void Lab::enableDesktopMenus() {
    desktopMenusEnabled_ = true;
    auto callback = [this, weak = lifetimeToken()](int id) {
        if (weak.expired())
            return;
        if (id >= 100 && id < 104) {
            if (!editing)
                setTab(id - 100);
        } else if (id >= 110 && id < 113) {
            if (!editing)
                setLoad(id - 110);
        } else if (id == 120) {
            if (!editing)
                toggle();
        } else if (id == 121) {
            if (!editing)
                startRecord();
        } else if (id == 130)
            window.activate();
        else if (id == 131)
            requestClose(true);
    };
    window.setOnMenuAction(callback);
    window.setOnTrayAction(callback);
    refreshDesktopMenus();
    window.setTrayTooltip(L"OneUI 性能实验台");
    window.setTrayEnabled(true);
}
