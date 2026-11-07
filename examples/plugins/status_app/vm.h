#pragma once
#include "oneui/reactive.h"
#include <functional>
#include <string>
struct StatusVM {
    oneui::State<std::wstring> label{L"Transfer status"};
    oneui::State<double> value{0.25};
    struct Action { std::function<void()> execute; } activate;
};
