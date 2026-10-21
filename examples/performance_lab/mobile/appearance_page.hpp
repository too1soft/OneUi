#pragma once
#include "demo_page.hpp"
#include "oneui/mobile/appearance.h"

namespace lab::mobile {
std::unique_ptr<DemoPage> createAppearancePage(oneui::mobile::Appearance &appearance);
void refreshAppearancePage(DemoPage *page);
} // namespace lab::mobile
