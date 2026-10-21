#pragma once
#include "demo_page.hpp"
namespace lab::mobile {
std::unique_ptr<DemoPage> createTextPage();
void textPageMetrics(DemoPage *, float fontScale, float bodySize);
std::string textPageDiagnostics(DemoPage *);
} // namespace lab::mobile
