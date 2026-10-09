#pragma once
#include "demo_page.hpp"
namespace lab::mobile {
std::unique_ptr<DemoPage> createPlotPage(bool particles);
std::string plotDiagnostics(const DemoPage *page);
} // namespace lab::mobile
