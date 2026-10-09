#include "oneui/layout/dock_model.h"
#include "oneui/oneui_c_api.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace oneui;
namespace {
void require(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
bool same(Rect a, OneUiRect b) {
    return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
}
void knownTargets() {
    const std::vector<DockHitTarget> targets{{1, {100, 100, 400, 300}, false},
                                             {2, {700, 100, 400, 300}, true}};
    auto center = dockHitTest(0, {300, 250}, targets, DockHitMode::PaneSwap);
    require(center && center->target == 1 && center->placement == DockHitPlacement::Swap,
            "Inner swap zone missing");
    auto left = dockHitTest(0, {101, 250}, targets, DockHitMode::PaneSwap);
    require(left && left->placement == DockHitPlacement::Left && left->preview.width == 200,
            "Normalized left edge wrong");
    require(!dockHitTest(1, {300, 250}, targets, DockHitMode::PaneSwap), "Source hit itself");
    auto guide = dockHitTest(0, {300, 250}, targets, DockHitMode::GuideStrict);
    require(guide && guide->placement == DockHitPlacement::None,
            "Strict center must identify target without activating a drop");
    guide = dockHitTest(0, {250, 250}, targets, DockHitMode::GuideStrict);
    require(guide && guide->placement == DockHitPlacement::Left, "Central direction guide missing");
    require(!dockHitTest(0, {701, 200}, targets, DockHitMode::GuideNearest),
            "Guide included a floating target");
    auto tab = dockHitTest(0, {210, 250}, targets, DockHitMode::TabMerge);
    require(tab && tab->placement == DockHitPlacement::None, "Native tab merge center changed");
    tab = dockHitTest(0, {170, 250}, targets, DockHitMode::TabMerge);
    require(tab && tab->placement == DockHitPlacement::Left, "Native tab edge missing");
    const auto nan = std::numeric_limits<float>::quiet_NaN();
    const auto inf = std::numeric_limits<float>::infinity();
    require(!dockHitTest(0, {nan, 250}, targets, DockHitMode::PaneSwap), "NaN point hit");
    require(!dockHitTest(0, {inf, 250}, targets, DockHitMode::GuideNearest), "Infinite point hit");
    require(!dockHitTest(0, {0, 0}, {{1, {0, 0, inf, 100}}}, DockHitMode::PaneSwap),
            "Invalid rectangle hit");
    require(!dockHitTest(0, {0, 0}, {{1, {0, 0, 0, 100}}}, DockHitMode::GuideNearest),
            "Zero-width rectangle hit");
}
void cParity() {
    const OneUiDockHitTarget c[]{{11, {0, 0, 400, 300}, 0}, {12, {450, 0, 400, 300}, 1}};
    const std::vector<DockHitTarget> cpp{{11, {0, 0, 400, 300}, false},
                                         {12, {450, 0, 400, 300}, true}};
    for (int mode = 0; mode < 4; ++mode)
        for (int y = -10; y <= 310; y += 20)
            for (int x = -10; x <= 870; x += 20) {
                auto expected = dockHitTest(0, {float(x), float(y)}, cpp, DockHitMode(mode));
                OneUiDockHit actual{};
                int status = oneui_dock_hit_test(0, float(x), float(y), c, 2, mode, &actual);
                require(status == (expected ? 1 : 0), "C/C++ hit status differs");
                if (expected)
                    require(actual.target == expected->target &&
                                actual.placement == int(expected->placement) &&
                                same(expected->bounds, actual.bounds) &&
                                same(expected->preview, actual.preview),
                            "C/C++ hit geometry differs");
            }
    OneUiDockHit value{};
    require(oneui_dock_hit_test(0, 0, 0, nullptr, 1, 0, &value) == -1,
            "Invalid C target array accepted");
    require(oneui_dock_hit_test(0, 0, 0, c, 2, 99, &value) == -1, "Invalid C mode accepted");
    require(oneui_dock_hit_test(0, 0, 0, c, 2, 0, nullptr) == -1, "Null C output accepted");
    require(oneui_dock_hit_test(0, 0, 0, nullptr, 0, 0, &value) == 0, "Empty C targets hit");
}
} // namespace
int main() {
    try {
        knownTargets();
        cParity();
        std::cout << "PASS: shared dock hit geometry, strict guide exclusion, native tab zones, "
                     "invalid geometry and C ABI parity.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
