#pragma once
#include "oneui/oneui_c_api.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct OneUiDockSession OneUiDockSession;
typedef struct OneUiDockDestination {
    unsigned long long host, pane;
    int position; /* 0 tab, 1 left, 2 right, 3 top, 4 bottom */
    int index;    /* -1 append */
} OneUiDockDestination;
typedef struct OneUiDockPaneLocation {
    unsigned long long host, group;
    int index, selected;
} OneUiDockPaneLocation;
/* Borrowed native window handles. Phase: 0 prepare, 1 commit, 2 rollback.
 * Nonzero accepts. Rollback cannot veto. All callbacks run on the UI thread. */
typedef int (*OneUiDockTransferCallback)(void *source, void *target, int phase, void *data);
typedef void (*OneUiDockIdsCallback)(const unsigned long long *, size_t count, void *);
/* Session result: 0 applied, 1 invalid pane, 2 invalid host, 3 invalid target,
 * 4 busy, 5 stale transaction, 6 resource rejected, 7 failure; -1 ABI exception.
 * Application must outlive its sessions. Registered external windows remain
 * caller-owned. Destroy sessions before destroying their Application. */
ONEUI_API OneUiDockSession *oneui_dock_session_create(OneUiApplication *);
ONEUI_API void oneui_dock_session_destroy(OneUiDockSession *);
ONEUI_API unsigned long long oneui_dock_session_attach_host(OneUiDockSession *, OneUiWindow *,
                                                            OneUiWidget *panel, void *owner,
                                                            OneUiDestroyCallback cleanup);
/* Data ownership always transfers, including on rejection. cleanup runs once
 * after the pane and its callbacks are no longer used by the session. */
ONEUI_API int oneui_dock_session_add_pane_utf8(OneUiDockSession *, unsigned long long pane,
                                               OneUiUtf8String title, OneUiWidget *content,
                                               float minimum_width, float minimum_height,
                                               OneUiDockDestination, OneUiDockTransferCallback,
                                               void *data, OneUiDestroyCallback cleanup);
ONEUI_API int oneui_dock_session_remove_pane(OneUiDockSession *, unsigned long long pane);
ONEUI_API int oneui_dock_session_move_pane(OneUiDockSession *, unsigned long long pane,
                                           OneUiDockDestination);
ONEUI_API int oneui_dock_session_select_pane(OneUiDockSession *, unsigned long long pane);
/* Retains a nonnull stylesheet. Refreshes existing/future hosts and original
 * pane controls; call again after modifying CSS. Uses the session result codes. */
ONEUI_API int oneui_dock_session_set_style_sheet(OneUiDockSession *,OneUiStyleSheet *);
ONEUI_API int oneui_dock_session_detach_pane_utf8(OneUiDockSession *, unsigned long long pane,
                                                  OneUiUtf8String title, int width, int height,
                                                  unsigned long long *new_host);
ONEUI_API unsigned long long oneui_dock_session_begin_drag(OneUiDockSession *,
                                                           unsigned long long pane);
ONEUI_API int oneui_dock_session_commit_drag(OneUiDockSession *, unsigned long long token,
                                             OneUiDockDestination);
ONEUI_API int oneui_dock_session_cancel_drag(OneUiDockSession *, unsigned long long token);
ONEUI_API int oneui_dock_session_native_drag_supported(OneUiDockSession*, unsigned long long host);
/* Synchronous borrowed UTF-8 diagnostics; empty means no reported limitation. */
ONEUI_API int oneui_dock_session_native_drag_limitation_utf8(OneUiDockSession*, unsigned long long host, OneUiUtf8TextCallback, void*);
ONEUI_API int oneui_dock_session_start_native_drag(OneUiDockSession*, unsigned long long pane);
ONEUI_API OneUiDockPaneLocation oneui_dock_session_location(OneUiDockSession *,
                                                            unsigned long long pane);
/* Arrays are borrowed only for this synchronous callback. host=0 lists all panes. */
ONEUI_API int oneui_dock_session_panes(OneUiDockSession *, unsigned long long host,
                                       OneUiDockIdsCallback, void *);
ONEUI_API int oneui_dock_session_hosts(OneUiDockSession *, OneUiDockIdsCallback, void *);
ONEUI_API int oneui_dock_session_set_on_changed(OneUiDockSession *, OneUiVoidCallback, void *,
                                                OneUiDestroyCallback);
#ifdef __cplusplus
}
#endif
