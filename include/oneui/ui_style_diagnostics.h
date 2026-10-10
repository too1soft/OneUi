#pragma once
#include "oneui/widget.h"
#include "oneui/style_sheet.h"
#include <memory>
#include <vector>

namespace oneui::ui {
// Capture-only metadata. Weak ownership must not retain a retired view mount.
struct StyleDiagnostic {
    std::weak_ptr<Widget> widget;
    StyleNode node;
    StyleBox box;
};
ONEUI_API void rememberStyleDiagnostic(const std::shared_ptr<Widget>&, const StyleNode&, const StyleBox&);
ONEUI_API std::vector<StyleDiagnostic> styleDiagnostics();
}
