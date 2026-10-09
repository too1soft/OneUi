#pragma once
#include "oneui/oneui_c_api.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct OneUiDockModel OneUiDockModel;
typedef struct OneUiDockSize {
    float width, height;
} OneUiDockSize;
/* Tree references are indexes into nodes; -1 denotes an absent root.
 * kind=0 leaf (nonzero pane), kind=1 split; axis=0 horizontal, 1 vertical. */
typedef struct OneUiDockTreeNode {
    int kind;
    unsigned long long pane;
    int axis;
    float ratio;
    int first, second;
} OneUiDockTreeNode;
typedef struct OneUiDockFloating {
    unsigned long long pane;
    OneUiRect bounds;
    int pinned;
    unsigned long long order;
} OneUiDockFloating;
typedef struct OneUiDockRestore {
    unsigned long long pane;
    int before_root, after_root;
    unsigned long long target;
    int edge;
    float ratio;
    int has_floating;
    OneUiDockFloating floating;
} OneUiDockRestore;
typedef struct OneUiDockSnapshot {
    const OneUiDockTreeNode *nodes;
    size_t node_count;
    int root;
    const OneUiDockFloating *floating;
    size_t floating_count;
    const OneUiDockRestore *hidden;
    size_t hidden_count;
    unsigned long long maximized;
} OneUiDockSnapshot;
/* Model errors: 0 success, 1 invalid geometry, 2 duplicate ID, 3 missing pane,
 * 4 invalid target, -1 invalid ABI argument/runtime error (see last_error).
 * Models contain no UI objects. Callers must serialize access to each model. */
ONEUI_API OneUiDockModel *oneui_dock_model_create(const OneUiDockSnapshot *, int *error);
ONEUI_API void oneui_dock_model_destroy(OneUiDockModel *);
ONEUI_API unsigned long long oneui_dock_model_sequence(OneUiDockModel *);
ONEUI_API int oneui_dock_model_preserve_sequence(OneUiDockModel *, unsigned long long sequence);
typedef void (*OneUiDockSnapshotCallback)(const OneUiDockSnapshot *, void *);
/* Borrowed snapshot arrays remain valid only during this synchronous callback. */
ONEUI_API int oneui_dock_model_snapshot(OneUiDockModel *, OneUiDockSnapshotCallback, void *);
/* Commands: 0 hide, 1 show, 2..5 dock left/right/top/bottom, 6 float, 7 redock,
 * 8 toggle maximize, 9 pin(value!=0), 10 raise, 11 floating bounds,
 * 12 swap(id,target), 13 balance, 14 split ratio(value,path).
 * Failure never mutates the model. */
ONEUI_API int oneui_dock_model_apply(OneUiDockModel *, int command, unsigned long long id,
                                     unsigned long long target, OneUiRect bounds, float value,
                                     const unsigned char *path, size_t path_length);
typedef OneUiDockSize (*OneUiDockMinimumCallback)(unsigned long long id, void *);
ONEUI_API int oneui_dock_model_minimum(OneUiDockModel *, float gap, OneUiDockMinimumCallback,
                                       void *, OneUiDockSize *result);
/* kind=0 pane bounds (id valid), kind=1 divider (axis/path valid). */
typedef void (*OneUiDockGeometryCallback)(int kind, unsigned long long id, int axis,
                                          const unsigned char *path, size_t path_length,
                                          OneUiRect bounds, void *);
ONEUI_API int oneui_dock_model_geometry(OneUiDockModel *, OneUiRect bounds, float gap,
                                        OneUiDockMinimumCallback, void *minimum_data,
                                        OneUiDockGeometryCallback, void *geometry_data);
ONEUI_API int oneui_dock_clamp_floating(OneUiRect, OneUiRect workspace, OneUiDockSize minimum,
                                        OneUiRect *result);
ONEUI_API int oneui_dock_resize_floating(OneUiRect, float dx, float dy, int edge,
                                         OneUiRect workspace, OneUiDockSize minimum,
                                         OneUiRect *result);
typedef struct OneUiDockHitTarget {
    unsigned long long pane;
    OneUiRect bounds;
    int floating;
} OneUiDockHitTarget;
typedef struct OneUiDockHit {
    unsigned long long target;
    OneUiRect bounds, preview;
    int placement;
} OneUiDockHit;
/* mode: 0 pane swap, 1 strict guide, 2 nearest guide, 3 tab merge.
 * placement: 0 no direction/tab merge, 1 left, 2 right, 3 top, 4 bottom, 5 swap.
 * Returns 1 hit, 0 no hit, -1 invalid ABI argument/exception. Nonfinite points
 * and invalid rectangles never hit. Caller orders the borrowed target array.
 * Floating targets are excluded only in guide modes; source is always excluded. */
ONEUI_API int oneui_dock_hit_test(unsigned long long source,float x,float y,
    const OneUiDockHitTarget *,size_t count,int mode,OneUiDockHit *result);
#ifdef __cplusplus
}
#endif
