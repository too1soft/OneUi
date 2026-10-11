#ifndef ONEUI_VIEW_C_API_H
#define ONEUI_VIEW_C_API_H
#include "oneui/oneui_c_api.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define ONEUI_VIEW_PROTOCOL_VERSION 1u
#define ONEUI_VIEW_ALL_PROPERTIES 0u
/* Except protocol_version and notifier retain/release, all operations and VM
 * callbacks run on
 * the window UI thread. Errors return a negative code and set
 * oneui_last_error; successful calls
 * do not clear diagnostics. Pointer outputs
 * are cleared before validation. Never throw/unwind
 * across a callback boundary.
 * A retained notifier is the only handle safe to keep after view
 * destruction. */
typedef int32_t OneUiViewResult;
#define ONEUI_VIEW_OK 0
#define ONEUI_VIEW_INVALID_ARGUMENT -1
#define ONEUI_VIEW_WRONG_THREAD -2
#define ONEUI_VIEW_BAD_IR -3
#define ONEUI_VIEW_VERSION -4
#define ONEUI_VIEW_SCHEMA_MISMATCH -5
#define ONEUI_VIEW_UNKNOWN_ID -6
#define ONEUI_VIEW_TYPE -7
#define ONEUI_VIEW_READ_ONLY -8
#define ONEUI_VIEW_CLOSED -9
#define ONEUI_VIEW_REENTRANT -10
#define ONEUI_VIEW_BAD_KEY -11
#define ONEUI_VIEW_STALE_CHANGE -12
#define ONEUI_VIEW_FOREIGN_FAILURE -13
#define ONEUI_VIEW_OUT_OF_MEMORY -14
#define ONEUI_VIEW_LIMIT -15
#define ONEUI_VIEW_NOT_FOUND -16
#define ONEUI_VIEW_BUSY -17

typedef struct OneUiView OneUiView;
typedef struct OneUiViewMount OneUiViewMount;
typedef struct OneUiViewNotifier OneUiViewNotifier;
typedef struct OneUiViewModelVTable OneUiViewModelVTable;

#define ONEUI_VALUE_BOOL 1u
#define ONEUI_VALUE_I64 2u
#define ONEUI_VALUE_F64 3u
#define ONEUI_VALUE_UTF8 4u
#define ONEUI_SCHEMA_LIST 5u /* schema only; never returned by get */
typedef struct OneUiValue {
    uint32_t tag;
    uint32_t reserved; /* zero */
    union {
        uint32_t boolean; /* exactly 0 or 1 */
        int64_t integer;
        double number;
        struct {
            const char *data;
            size_t length;
        } utf8;
    } as;
} OneUiValue;
/* Zero-initialize the entire value, including unused union bytes. UTF-8 is
 * borrowed, contains no
 * NUL, and is valid until the next vtable call on the same
 * VM. Native code copies it before that
 * next call (including retain/release).
 * There is no value-release function. set/invoke arguments
 * are borrowed only
 * for the duration of the callback. f64 must be finite; NULL/0 is an empty
 * string. */

typedef struct OneUiViewModelRef {
    const OneUiViewModelVTable *vtable;
    void *vm;
} OneUiViewModelRef;

typedef struct OneUiViewListInfo {
    uint64_t revision;
    uint64_t length;
} OneUiViewListInfo;

#define ONEUI_LIST_RESET 0u
#define ONEUI_LIST_INSERT 1u
#define ONEUI_LIST_REMOVE 2u
#define ONEUI_LIST_MOVE 3u
#define ONEUI_LIST_UPDATE 4u
/* Exact permutation of all existing keys. Preserves item leases and field caches. */
#define ONEUI_LIST_REORDER 5u
typedef struct OneUiViewListChange {
    uint32_t struct_size;
    uint32_t kind;
    uint64_t before_revision;
    uint64_t after_revision;
    uint64_t index;
    uint64_t count;
    uint64_t to_index; /* MOVE: index after removing the source range */
    const OneUiUtf8String *keys;
    size_t key_count;
    const uint32_t *properties; /* UPDATE: item property IDs; empty=all */
    size_t property_count;
} OneUiViewListChange;
/* struct_size must equal sizeof(OneUiViewListChange). Notify after mutation;
 * every mutation
 * advances revision. RESET has zero indices and empty arrays.
 * Other changes have count nonzero
 * and key_count=count. MOVE's destination is
 * measured after removal; UPDATE alone accepts
 * property IDs (empty means all).
 * Arrays are copied during notify. Over 4096 queued deltas or 1
 * MiB becomes RESET.
 * RESET retains equal key+VM/vtable identities, replacing equal keys with new
 * VMs. */

struct OneUiViewModelVTable {
    uint32_t struct_size;
    uint32_t protocol_version;
    uint8_t schema_hash[32];
    uint32_t model_id;
    uint32_t flags; /* v1 zero */
    void (*retain)(void *vm);
    void (*release)(void *vm);
    OneUiViewResult (*connect)(void *vm, OneUiViewNotifier *notifier);
    void (*disconnect)(void *vm, OneUiViewNotifier *notifier);
    OneUiViewResult (*get)(void *vm, uint32_t property_id, OneUiValue *out);
    OneUiViewResult (*set)(void *vm, uint32_t property_id, const OneUiValue *value);
    OneUiViewResult (*invoke)(void *vm, uint32_t command_id, const OneUiValue *args, size_t argc);
    OneUiViewResult (*list_info)(void *vm, uint32_t property_id, OneUiViewListInfo *out);
    OneUiViewResult (*list_key)(void *vm, uint32_t property_id, uint64_t index,
                                OneUiValue *out_utf8);
    OneUiViewResult (*list_acquire)(void *vm, uint32_t property_id, uint64_t index,
                                    OneUiViewModelRef *out);
};
/* v1 requires the complete known prefix; larger tables require a zero tail.
 * Native code copies
 * the prefix; callback code must remain loaded through release.
 * The root VM is borrowed at
 * instantiate and retained once. list_acquire transfers
 * one child reference. Only the matching
 * release permits reclaiming either VM.
 * Lists are queried by property_id; get never returns a
 * list handle/value.
 * connect borrows its notifier (retain it to save it); disconnect is paired
 * even
 * after failed connect and observes an already closed notifier. Each view/scope
 * has its
 * own connection. All failed callback outputs must be zero.
 * get/list callbacks are read-only:
 * notify/flush return REENTRANT, never retry.
 * set/invoke may notify; updates flush after the
 * outer callback returns. Nested
 * mutable events on the same scope are rejected.
 * disconnect/destroy in a callback
 * close the gate immediately and defer VM release to the
 * outermost epilogue. */

typedef struct OneUiViewError {
    uint32_t struct_size;
    OneUiViewResult result;
    uint64_t sequence;
    uint32_t section_kind;
    uint32_t record_index;
    uint32_t source_map_index;
    uint32_t reserved;
    char message[512]; /* owned inline UTF-8, NUL terminated */
} OneUiViewError;

ONEUI_API uint32_t oneui_view_protocol_version(void);
/* UI thread only. destination is NULL for an empty window content slot, or an
 * empty Panel
 * already belonging to this window. A mount admits one live View.
 * Window close/destruction or
 * mount_destroy disconnects that View, but its
 * owned handle still needs view_destroy. The
 * destination handle itself is
 * borrowed only during creation; native ownership is retained by
 * the mount.
 * Disconnect closes events, detaches composition and releases child VMs before
 * the
 * root VM. Destroying a View also invalidates its borrowed root/host handles. */
ONEUI_API OneUiViewResult oneui_view_mount_create(OneUiWindow *window, OneUiWidget *destination,
                                                  OneUiViewMount **out);
ONEUI_API void oneui_view_mount_destroy(OneUiViewMount *mount);
/* density: 0 comfortable, 1 compact. Replaces external theme/CSS atomically. */
ONEUI_API OneUiViewResult oneui_view_mount_set_styles(OneUiViewMount *mount, OneUiUtf8String css,
                                                      uint32_t density);

ONEUI_API OneUiViewResult oneui_view_instantiate(OneUiViewMount *mount, const uint8_t *ir_bytes,
                                                 size_t len, const OneUiViewModelVTable *vtable,
                                                 void *vm, OneUiView **out);
/* Pure accessor, borrowed root; requires a live view handle. */
ONEUI_API OneUiWidget *oneui_view_root(OneUiView *view);
/* property_id=0 marks all fields dirty (lists become RESET). Notifications are
 * coalesced until
 * the next UI dispatch; flush provides a synchronous boundary
 * outside callbacks. A stale precise
 * list delta records an error and reconciles
 * the current RESET snapshot. A failed snapshot keeps
 * old list composition. */
ONEUI_API OneUiViewResult oneui_view_notify(OneUiView *view, uint32_t property_id);
ONEUI_API OneUiViewResult oneui_view_notify_list(OneUiView *view, uint32_t property_id,
                                                 const OneUiViewListChange *change);
ONEUI_API OneUiViewResult oneui_view_flush(OneUiView *view);
ONEUI_API OneUiViewResult oneui_view_disconnect(OneUiView *view);
/* Consumes the owned view handle; NULL permitted; UI thread only. */
ONEUI_API void oneui_view_destroy(OneUiView *view);
ONEUI_API OneUiViewResult oneui_view_get_error(OneUiView *view, OneUiViewError *out);

/* On success out owns one notifier reference. */
ONEUI_API OneUiViewResult oneui_view_notifier(OneUiView *view, OneUiViewNotifier **out);
ONEUI_API void oneui_view_notifier_retain(OneUiViewNotifier *notifier);
ONEUI_API void oneui_view_notifier_release(OneUiViewNotifier *notifier);
ONEUI_API OneUiViewResult oneui_view_notifier_notify(OneUiViewNotifier *notifier,
                                                     uint32_t property_id);
ONEUI_API OneUiViewResult oneui_view_notifier_notify_list(OneUiViewNotifier *notifier,
                                                          uint32_t property_id,
                                                          const OneUiViewListChange *change);

typedef struct OneUiViewScopeSegment {
    OneUiUtf8String instance_id; /* Slash-free repeat view-id, qualified by slash-free call-id/ prefixes. */
    OneUiUtf8String key;         /* Exact stable UTF-8 key, never a slash-separated path. */
} OneUiViewScopeSegment;
/* UI thread, outside callbacks. Inputs borrowed through return. At most 64
 * segments / 1 MiB of UTF-8; IDs and keys must be nonempty. Zero segments returns
 * the root. Success owns one notifier reference; release with notifier_release.
 * Missing path is NOT_FOUND. Removed/replaced scopes remain CLOSED permanently.
 * Only stable Pane/DockItem collection instances are addressable in this version. */
ONEUI_API OneUiViewResult oneui_view_resolve_scope(OneUiView *view,
    const OneUiViewScopeSegment *segments, size_t segment_count, OneUiViewNotifier **out_scope);
/* Borrowed host handle. Do not call oneui_widget_destroy on it. Re-find after
 * structural flush/reload. NULL scope denotes root; non-NULL must be a live
 * root/workspace render notifier belonging to this View; data-only scopes are
 * INVALID_ARGUMENT. Same name in different render row scopes is valid.
 * Removed handles return CLOSED until View destruction; handles must not outlive
 * their View. At most 100000 issued host identities per View (LIMIT beyond that). */
ONEUI_API OneUiViewResult
oneui_view_find_host(OneUiView *view, OneUiViewNotifier *scope, OneUiUtf8String name,
                     OneUiWidget **out);
/* Owner lease retains the foreign widget's callback owner. */
ONEUI_API OneUiViewResult oneui_view_host_attach(OneUiWidget *host, OneUiWidget *content,
                                                 void *owner, OneUiVoidCallback retain_owner,
                                                 OneUiDestroyCallback release_owner);
ONEUI_API OneUiViewResult oneui_view_host_detach(OneUiWidget *host);
/* Development reload, UI thread. Same schema required. BUSY during callbacks or
 * when an attached Host would be removed/renamed/moved. The input buffer is
 * borrowed for this call only. Failed validation keeps the previous view.
 * After success, reacquire borrowed root/host handles; do not retain those
 * handles across reload even when the underlying native Widget survives. */
ONEUI_API OneUiViewResult oneui_view_reload(OneUiView *view, const uint8_t *ir_bytes, size_t len);

/* UI-thread read-only snapshot; no widget ownership or mutation escapes.
 * name is the unique accessible name inside this View (including repeat rows).
 * Empty/ambiguous names are INVALID_ARGUMENT; absent names are NOT_FOUND.
 * Geometry is the last committed native layout, in logical client coordinates.
 * Safe in a command callback; does not flush, lay out or invoke model callbacks. */
typedef struct OneUiViewElementMetrics {
    uint32_t struct_size, scrollable;
    float x, y, width, height;
    float scroll_x, scroll_y, max_scroll_y;
} OneUiViewElementMetrics;
ONEUI_API OneUiViewResult oneui_view_element_metrics(OneUiView *view,
    OneUiUtf8String name, OneUiViewElementMetrics *out);
/** Read a named element's own resolved visibility after layout (including
 * container-query display). Ancestor/window visibility is not included.
 * UI thread only; does not flush, lay out or expose a widget handle. */
ONEUI_API OneUiViewResult oneui_view_element_visible(OneUiView *view,
    OneUiUtf8String name, int *out_visible);
/** Read native keyboard focus for one uniquely named element; UI thread only. */
ONEUI_API OneUiViewResult oneui_view_element_focused(OneUiView *view,
    OneUiUtf8String name, int *out_focused);
/** Focus a uniquely named element of the mount's active view on its UI thread.
 * No widget handle escapes. Rejects ambiguous/missing names and closed mounts.
 * Call outside VM callbacks; out_focused reports native focus eligibility. */
ONEUI_API OneUiViewResult oneui_view_mount_focus_element(OneUiViewMount *mount,
    OneUiUtf8String name, int focus_visible, int *out_focused);
#ifdef __cplusplus
}
#endif
#endif
