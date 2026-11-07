#pragma once
#include "oneui/oneui_view_c_api.h"

#ifdef __cplusplus
extern "C" {
#endif
#define ONEUI_COMPONENT_ABI_V1 1u
typedef struct OneUiComponentRegistry OneUiComponentRegistry;

/* RGBA is packed as 0xRRGGBBAA. Values describe the current inherited theme
 * and class rules. Query again for hover/active/focus style states. */
typedef struct OneUiComponentStyleV1 {
    uint32_t struct_size;
    uint32_t foreground, background, border;
    float font_size, radius, border_width;
} OneUiComponentStyleV1;

typedef struct OneUiComponentHostV1 {
    uint32_t struct_size, abi_version;
    void *token;
    OneUiViewResult (*emit)(void *, uint32_t event_id, const OneUiValue *, size_t);
    OneUiViewResult (*change)(void *, uint32_t property_id, const OneUiValue *);
    OneUiViewResult (*style)(void *, uint32_t states, OneUiComponentStyleV1 *);
} OneUiComponentHostV1;

typedef struct OneUiComponentFactoryV1 {
    uint32_t struct_size, abi_version;
    /* One complete .onecomponents document containing exactly one component.
     * The compiler consumes this SAME file; IDs, types and semantic version
     * are checked before calling any factory or ViewModel code. */
    OneUiUtf8String contract;
    void *data;
    /* Optional paired callbacks retain factory data until the registry and
     * all widgets have released it. Plugin code must remain resident. */
    void (*retain_data)(void *);
    void (*release_data)(void *);
    /* children and their handles are borrowed through return. The returned
     * widget handle transfers ownership to OneUI. Return a NEW, unattached
     * widget; instance must not retain that widget strongly. Set instance
     * before acquiring resources. release_instance is paired even on failure.
     * host is borrowed until release_instance. All callbacks use the UI thread. */
    OneUiViewResult (*create)(void *data, const OneUiComponentHostV1 *host,
                             OneUiWidget *const *children, size_t child_count,
                             void **instance, OneUiWidget **widget);
    OneUiViewResult (*set)(void *instance, uint32_t property_id, const OneUiValue *);
    /* Disconnect is called once before releasing the ViewModel or replacing
     * a view. Stop event subscriptions here; late emits return CLOSED. */
    void (*disconnect)(void *instance);
    void (*release_instance)(void *instance);
    void (*style_changed)(void *instance);
} OneUiComponentFactoryV1;

ONEUI_API uint32_t oneui_component_abi_version(void);
ONEUI_API OneUiViewResult oneui_component_registry_create_v1(OneUiComponentRegistry **out);
ONEUI_API OneUiViewResult oneui_component_registry_register_v1(
    OneUiComponentRegistry *, const OneUiComponentFactoryV1 *);
/* Owner thread only. Existing mounts/widgets retain their immutable snapshot. */
ONEUI_API OneUiViewResult oneui_component_registry_destroy(OneUiComponentRegistry *);
/* Set before instantiate. A mount and its children inherit an immutable copy.
 * NULL selects builtins only. Active mounts reject registry replacement. */
ONEUI_API OneUiViewResult oneui_view_mount_set_components_v1(OneUiViewMount *, OneUiComponentRegistry *);
#ifdef __cplusplus
}
#endif
