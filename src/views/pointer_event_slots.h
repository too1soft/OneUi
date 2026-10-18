#pragma once
#include "event_dispatch.h"

namespace oneui::views {
inline unsigned pointerEventMask(const Instance& instance) {
    if (instance.element.component != "InteractiveSurface") return 0;
    unsigned result = 0;
    for (const auto& event : instance.events)
        if (event[1] >= 2 && event[1] <= 4) result |= 1u << event[1];
    return result;
}

// Prepare allocations before reload commits. Only the template-owned optional
// InteractiveSurface slots are changed; NativeHost content and native composite
// controls are never traversed or re-wired. An unchanged binding keeps its slot
// and in-progress gesture; dispatch resolves the latest VM command on each call.
inline std::function<void()> preparePointerEventSlots(
    const std::shared_ptr<Instance>& instance, unsigned previous, unsigned next) {
    const auto changed = previous ^ next;
    const auto surface = instance->element.as<InteractiveSurface>();
    check(instance->element.component == "InteractiveSurface" && bool(surface),
          ONEUI_VIEW_TYPE, "Pointer event slots require a template InteractiveSurface");
    std::function<void(const MouseEvent&)> pointer, context;
    std::function<void(const PointerDragEvent&)> drag;
    if ((changed & next) & (1u << 2))
        pointer = [fire = eventDispatcher(instance, 2)](const MouseEvent& event) {
            Value x, y, control, shift, count;
            x.tag = y.tag = 3; x.number = event.position.x; y.number = event.position.y;
            control.tag = shift.tag = 1; control.integer = event.control; shift.integer = event.shift;
            count.tag = 2; count.integer = event.clickCount;
            fire({x, y, control, shift, count});
        };
    if ((changed & next) & (1u << 3))
        context = [fire = eventDispatcher(instance, 3)](const MouseEvent& event) {
            Value x, y; x.tag = y.tag = 3;
            x.number = event.position.x; y.number = event.position.y;
            fire({x, y});
        };
    if ((changed & next) & (1u << 4))
        drag = [fire = eventDispatcher(instance, 4)](const PointerDragEvent& event) {
            Value phase, ox, oy, x, y, alt, control, shift;
            phase.tag = 2; phase.integer = static_cast<int>(event.phase);
            ox.tag = oy.tag = x.tag = y.tag = 3;
            ox.number = event.origin.x; oy.number = event.origin.y;
            x.number = event.pointer.position.x; y.number = event.pointer.position.y;
            alt.tag = control.tag = shift.tag = 1;
            alt.integer = event.pointer.alt; control.integer = event.pointer.control; shift.integer = event.pointer.shift;
            fire({phase, ox, oy, x, y, alt, control, shift});
        };
    return [surface, changed, next, pointer = std::move(pointer), context = std::move(context),
            drag = std::move(drag)]() mutable {
        if (changed & (1u << 2)) surface->setOnPointerActivated(std::move(pointer));
        if (changed & (1u << 3)) surface->setOnContextMenuRequested(std::move(context));
        if (changed & (1u << 4)) {
            surface->setOnDrag(std::move(drag));
            surface->setPointerCursor(next & (1u << 4) ? CursorKind::Grab : CursorKind::Pointer);
        }
    };
}
} // namespace oneui::views
