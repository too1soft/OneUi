#include "oneui_c_api_dock_model_internal.h"
#include "oneui_c_api_error.h"

using namespace oneui;
using namespace oneui::capi;
int oneui_dock_model_minimum(OneUiDockModel *model, float gap, OneUiDockMinimumCallback policy,
                             void *data, OneUiDockSize *result) {
    try {
        if (!policy || !result)
            throw std::invalid_argument("Minimum policy and result are required");
        const auto state = dockModel(model).snapshot();
        if (!state.root)
            return int(DockError::MissingPane);
        auto minimum = dockMinimum(*state.root, gap, [&](DockId id) {
            auto value = policy(id, data);
            return Size{value.width, value.height};
        });
        *result = {minimum.width, minimum.height};
        return 0;
    } catch (...) {
        recordCurrentException(__func__);
        return -1;
    }
}
OneUiDockModel *oneui_dock_model_create(const OneUiDockSnapshot *source, int *error) {
    try {
        auto value = std::make_unique<OneUiDockModel>();
        const auto result =
            source ? value->model.restore(decodeDockSnapshot(*source)) : DockError::None;
        if (error)
            *error = int(result);
        return result == DockError::None ? value.release() : nullptr;
    } catch (...) {
        recordCurrentException(__func__);
        if (error)
            *error = -1;
        return nullptr;
    }
}
void oneui_dock_model_destroy(OneUiDockModel *model) { delete model; }
unsigned long long oneui_dock_model_sequence(OneUiDockModel *model) {
    try {
        return dockModel(model).sequence();
    } catch (...) {
        recordCurrentException(__func__);
        return 0;
    }
}
int oneui_dock_model_preserve_sequence(OneUiDockModel *model, unsigned long long sequence) {
    try {
        dockModel(model).preserveSequence(sequence);
        return 0;
    } catch (...) {
        recordCurrentException(__func__);
        return -1;
    }
}
int oneui_dock_model_snapshot(OneUiDockModel *model, OneUiDockSnapshotCallback callback,
                              void *data) {
    try {
        visitDockSnapshot(dockModel(model).snapshot(), callback, data);
        return 0;
    } catch (...) {
        recordCurrentException(__func__);
        return -1;
    }
}
int oneui_dock_model_apply(OneUiDockModel *model, int command, unsigned long long id,
                           unsigned long long target, OneUiRect bounds, float value,
                           const unsigned char *path, size_t pathLength) {
    try {
        if (pathLength > 64 || (pathLength && !path))
            throw std::invalid_argument("Invalid divider path");
        auto next = dockModel(model);
        DockError error = DockError::InvalidTarget;
        switch (command) {
        case 0:
            error = next.hide(id);
            break;
        case 1:
            error = next.show(id);
            break;
        case 2:
        case 3:
        case 4:
        case 5:
            error = next.dock(id, target, DockEdge(command - 2));
            break;
        case 6:
            error = next.floatPane(id, dockRect(bounds));
            break;
        case 7:
            error = next.redock(id);
            break;
        case 8:
            error = next.maximize(id);
            break;
        case 9:
            error = next.setPinned(id, value != 0);
            break;
        case 10:
            error = next.raise(id);
            break;
        case 11:
            error = next.setFloatBounds(id, dockRect(bounds));
            break;
        case 12:
            error = next.swap(id, target);
            break;
        case 13:
            next.balance();
            error = DockError::None;
            break;
        case 14: {
            std::vector<bool> bits;
            for (size_t i = 0; i < pathLength; ++i)
                bits.push_back(path[i] != 0);
            error = next.setRatio(bits, value);
            break;
        }
        default:
            break;
        }
        if (error == DockError::None) {
            const auto sequence = next.sequence();
            error = next.restore(next.snapshot());
            next.preserveSequence(sequence);
        }
        if (error == DockError::None)
            dockModel(model) = std::move(next);
        return int(error);
    } catch (...) {
        recordCurrentException(__func__);
        return -1;
    }
}
int oneui_dock_model_geometry(OneUiDockModel *model, OneUiRect bounds, float gap,
                              OneUiDockMinimumCallback minimum, void *minimumData,
                              OneUiDockGeometryCallback callback, void *data) {
    try {
        if (!minimum || !callback)
            throw std::invalid_argument("Geometry callbacks are required");
        const auto state = dockModel(model).snapshot();
        if (!state.root)
            return int(DockError::MissingPane);
        DockGeometry output;
        auto result = dockGeometry(
            *state.root, dockRect(bounds), gap,
            [&](DockId id) {
                auto size = minimum(id, minimumData);
                return Size{size.width, size.height};
            },
            output);
        if (result != DockError::None)
            return int(result);
        for (const auto &[id, rect] : output.panes)
            callback(0, id, 0, nullptr, 0, dockRect(rect), data);
        for (const auto &divider : output.dividers) {
            std::vector<unsigned char> path;
            for (bool side : divider.path)
                path.push_back(side);
            callback(1, 0, int(divider.axis), path.data(), path.size(), dockRect(divider.bounds),
                     data);
        }
        return 0;
    } catch (...) {
        recordCurrentException(__func__);
        return -1;
    }
}
int oneui_dock_clamp_floating(OneUiRect bounds, OneUiRect workspace, OneUiDockSize minimum,
                              OneUiRect *result) {
    try {
        if (!result)
            throw std::invalid_argument("Result is required");
        Rect output;
        auto error = clampDockFloating(dockRect(bounds), dockRect(workspace),
                                       {minimum.width, minimum.height}, output);
        if (error == DockError::None)
            *result = dockRect(output);
        return int(error);
    } catch (...) {
        recordCurrentException(__func__);
        return -1;
    }
}
int oneui_dock_resize_floating(OneUiRect bounds, float dx, float dy, int edge, OneUiRect workspace,
                               OneUiDockSize minimum, OneUiRect *result) {
    try {
        if (!result)
            throw std::invalid_argument("Result is required");
        Rect output;
        auto error =
            resizeDockFloating(dockRect(bounds), {dx, dy}, DockResizeEdge(edge),
                               dockRect(workspace), {minimum.width, minimum.height}, output);
        if (error == DockError::None)
            *result = dockRect(output);
        return int(error);
    } catch (...) {
        recordCurrentException(__func__);
        return -1;
    }
}
