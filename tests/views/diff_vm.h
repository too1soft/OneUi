#pragma once
#include "oneui/ui_declarative.h"
struct DiffRow {
    std::wstring id;
    oneui::State<std::wstring> text;
    DiffRow(std::wstring key,std::wstring value):id(std::move(key)),text(std::move(value)){}
};
struct DiffVm {
    oneui::State<std::wstring> title{L"initial"};
    oneui::State<bool> enabled{true};
    oneui::State<int> choice{0};
    oneui::State<std::vector<std::wstring>> choices{{L"one",L"two"}};
    oneui::State<std::vector<std::shared_ptr<DiffRow>>> rows{{
        std::make_shared<DiffRow>(L"a",L"alpha"),std::make_shared<DiffRow>(L"b",L"beta")}};
    oneui::ui::VmCommand run;
    std::vector<std::wstring> trace;
    DiffVm() { run.setAction([this] { trace.push_back(title.get());title.set(L"invoked"); }); }
};
