#pragma once
#include "oneui/dock_model_c.h"
#include "oneui/layout/dock_model.h"
#include <stdexcept>

struct OneUiDockModel {
    oneui::DockWorkspace model;
};
namespace oneui::capi {
inline Rect dockRect(OneUiRect r) { return {r.x, r.y, r.width, r.height}; }
inline OneUiRect dockRect(Rect r) { return {r.x, r.y, r.width, r.height}; }
DockSnapshot decodeDockSnapshot(const OneUiDockSnapshot &);
void visitDockSnapshot(const DockSnapshot &, OneUiDockSnapshotCallback, void *);
inline DockWorkspace &dockModel(OneUiDockModel *model) {
    if (!model)
        throw std::invalid_argument("Dock model is required");
    return model->model;
}
} // namespace oneui::capi
