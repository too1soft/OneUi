#include "oneui/layout/dock_model.h"
#include "oneui/oneui_c_api.h"
#include "oneui_c_api_error.h"
#include <stdexcept>

int oneui_dock_hit_test(unsigned long long source, float x, float y,
                        const OneUiDockHitTarget *targets, size_t count, int mode,
                        OneUiDockHit *result) {
    try {
        if (!result || (!targets && count) || count > 100000 || mode < 0 || mode > 3)
            throw std::invalid_argument("Invalid dock hit-test arguments");
        *result = {};
        std::vector<oneui::DockHitTarget> candidates;
        candidates.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            const auto &t = targets[i];
            const auto r = t.bounds;
            candidates.push_back({t.pane, {r.x, r.y, r.width, r.height}, t.floating != 0});
        }
        auto hit = oneui::dockHitTest(source, {x, y}, candidates, oneui::DockHitMode(mode));
        if (!hit)
            return 0;
        auto rect = [](oneui::Rect r) { return OneUiRect{r.x, r.y, r.width, r.height}; };
        *result = {hit->target, rect(hit->bounds), rect(hit->preview), int(hit->placement)};
        return 1;
    } catch (...) {
        oneui::capi::recordCurrentException(__func__);
        return -1;
    }
}
