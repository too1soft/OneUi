//! Pointer surface bindings.

use super::choice_controls::{run_bool_changed_callback, BoolChangedCallback};
use super::visual_style::{Insets, InteractiveSurfaceStyle};
use super::{
    run_callback_guarded, run_void_callback, sys, traced_callback, traced_value_callback, Error,
    InteractionTrace, Rc, RefCell, VoidCallback, Widget,
};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PointerButton {
    None,
    Left,
    Right,
    Middle,
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub struct PointerEvent {
    pub x: f32,
    pub y: f32,
    pub button: PointerButton,
    pub click_count: i32,
    pub shift: bool,
    pub control: bool,
    pub alt: bool,
}

impl From<sys::OneUiPointerEvent> for PointerEvent {
    fn from(value: sys::OneUiPointerEvent) -> Self {
        let button = match value.button {
            1 => PointerButton::Left,
            2 => PointerButton::Right,
            3 => PointerButton::Middle,
            _ => PointerButton::None,
        };
        Self {
            x: value.x,
            y: value.y,
            button,
            click_count: value.click_count,
            shift: value.shift != 0,
            control: value.control != 0,
            alt: value.alt != 0,
        }
    }
}

struct PointerCallback {
    handler: Box<dyn FnMut(PointerEvent) + 'static>,
}

unsafe extern "C" fn run_pointer_callback(
    event: *const sys::OneUiPointerEvent,
    user_data: *mut std::ffi::c_void,
) {
    if event.is_null() || user_data.is_null() {
        return;
    }
    let event = PointerEvent::from(unsafe { *event });
    let callback = unsafe { &mut *user_data.cast::<PointerCallback>() };
    run_callback_guarded("pointer.activated", || (callback.handler)(event));
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct GridReorderRequest {
    pub source_id: String,
    pub target_index: usize,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ItemDragPhase {
    Started,
    Updated,
    Dropped,
    Cancelled,
}

#[derive(Clone, Copy, Debug)]
pub struct PointerDragEvent {
    pub phase: ItemDragPhase,
    pub origin_x: f32,
    pub origin_y: f32,
    pub pointer: PointerEvent,
}
#[derive(Clone, Copy, Debug)]
#[repr(i32)]
pub enum PointerCursor {
    Default = 0,
    Hidden = 1,
    Pointer = 2,
    Text = 3,
    Crosshair = 4,
    Grab = 5,
    ResizeHorizontal = 6,
    ResizeVertical = 7,
    ResizeNorthWestSouthEast = 8,
    ResizeNorthEastSouthWest = 9,
}
struct PointerDragCallback {
    handler: Box<dyn FnMut(PointerDragEvent)>,
}
unsafe extern "C" fn run_pointer_drag_callback(
    phase: i32,
    origin_x: f32,
    origin_y: f32,
    event: *const sys::OneUiPointerEvent,
    user_data: *mut std::ffi::c_void,
) {
    if event.is_null() || user_data.is_null() {
        return;
    }
    let event = PointerDragEvent {
        phase: ItemDragPhase::from_raw(phase),
        origin_x,
        origin_y,
        pointer: PointerEvent::from(unsafe { *event }),
    };
    let callback = unsafe { &mut *user_data.cast::<PointerDragCallback>() };
    run_callback_guarded("pointer.drag", || (callback.handler)(event));
}

impl ItemDragPhase {
    pub(super) fn from_raw(value: std::ffi::c_int) -> Self {
        match value {
            0 => Self::Started,
            1 => Self::Updated,
            2 => Self::Dropped,
            _ => Self::Cancelled,
        }
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct ItemDragEvent {
    pub source_id: String,
    pub phase: ItemDragPhase,
    pub x: f32,
    pub y: f32,
}

struct GridReorderRequestedCallback {
    handler: Box<dyn FnMut(GridReorderRequest) + 'static>,
}

pub(super) struct ItemDragCallback {
    pub(super) handler: Box<dyn FnMut(ItemDragEvent) + 'static>,
}

unsafe extern "C" fn run_grid_reorder_requested_callback(
    source_id: *const std::ffi::c_char,
    source_length: usize,
    target_index: std::ffi::c_int,
    user_data: *mut std::ffi::c_void,
) {
    if source_id.is_null() || user_data.is_null() || target_index < 0 {
        return;
    }
    let source = unsafe { std::slice::from_raw_parts(source_id.cast::<u8>(), source_length) };
    let request = GridReorderRequest {
        source_id: String::from_utf8_lossy(source).into_owned(),
        target_index: target_index as usize,
    };
    let callback = unsafe { &mut *user_data.cast::<GridReorderRequestedCallback>() };
    run_callback_guarded("reorderable_grid.reorder_requested", || {
        (callback.handler)(request)
    });
}

pub(super) unsafe extern "C" fn run_item_drag_callback(
    source_id: *const std::ffi::c_char,
    source_length: usize,
    phase: std::ffi::c_int,
    x: f32,
    y: f32,
    user_data: *mut std::ffi::c_void,
) {
    if source_id.is_null() || user_data.is_null() {
        return;
    }
    let source = unsafe { std::slice::from_raw_parts(source_id.cast::<u8>(), source_length) };
    let event = ItemDragEvent {
        source_id: String::from_utf8_lossy(source).into_owned(),
        phase: ItemDragPhase::from_raw(phase),
        x,
        y,
    };
    let callback = unsafe { &mut *user_data.cast::<ItemDragCallback>() };
    run_callback_guarded("reorderable_grid.item_drag", || (callback.handler)(event));
}

/// A native responsive grid that lays out arbitrary widgets and emits
/// reorder requests without mutating product-owned domain data.
pub struct ReorderableGrid {
    widget: Widget,
    reorder_callback: Option<Box<GridReorderRequestedCallback>>,
    item_drag_callback: Option<Box<ItemDragCallback>>,
}

impl ReorderableGrid {
    pub fn new() -> Result<Self, Error> {
        let widget = Widget::from_raw(unsafe { sys::oneui_reorderable_grid_create() })?;
        Ok(Self {
            widget,
            reorder_callback: None,
            item_drag_callback: None,
        })
    }

    pub fn clear_items(&self) {
        unsafe { sys::oneui_reorderable_grid_clear_items(self.widget.as_raw()) };
    }

    pub fn add_item(&self, id: &str, child: &Widget) {
        unsafe {
            sys::oneui_reorderable_grid_add_item_utf8(
                self.widget.as_raw(),
                sys::OneUiUtf8String::from_str(id),
                child.as_raw(),
            )
        };
    }

    pub fn move_item(&self, source_id: &str, target_index: usize) -> bool {
        unsafe {
            sys::oneui_reorderable_grid_move_item_utf8(
                self.widget.as_raw(),
                sys::OneUiUtf8String::from_str(source_id),
                target_index.min(std::ffi::c_int::MAX as usize) as std::ffi::c_int,
            ) != 0
        }
    }

    pub fn set_column_count(&self, columns: usize) {
        unsafe {
            sys::oneui_reorderable_grid_set_column_count(
                self.widget.as_raw(),
                columns.min(std::ffi::c_int::MAX as usize) as std::ffi::c_int,
            )
        };
    }

    pub fn set_gaps(&self, column_gap: f32, row_gap: f32) {
        unsafe { sys::oneui_reorderable_grid_set_gaps(self.widget.as_raw(), column_gap, row_gap) };
    }

    pub fn set_item_height(&self, height: f32) {
        unsafe { sys::oneui_reorderable_grid_set_item_height(self.widget.as_raw(), height) };
    }

    pub fn content_height(&self) -> f32 {
        unsafe { sys::oneui_reorderable_grid_content_height(self.widget.as_raw()) }
    }

    pub fn set_reorder_enabled(&self, enabled: bool) {
        unsafe {
            sys::oneui_reorderable_grid_set_reorder_enabled(
                self.widget.as_raw(),
                i32::from(enabled),
            )
        };
    }

    pub fn reorder_enabled(&self) -> bool {
        unsafe { sys::oneui_reorderable_grid_reorder_enabled(self.widget.as_raw()) != 0 }
    }

    #[track_caller]
    pub fn set_on_reorder_requested<F>(&mut self, callback: F)
    where
        F: FnMut(GridReorderRequest) + 'static,
    {
        let trace = InteractionTrace::at(
            "ReorderableGrid",
            "reorder_requested",
            std::panic::Location::caller(),
        );
        self.clear_on_reorder_requested();
        self.reorder_callback = Some(Box::new(GridReorderRequestedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .reorder_callback
            .as_deref_mut()
            .expect("grid reorder callback was just installed")
            as *mut GridReorderRequestedCallback)
            .cast();
        unsafe {
            sys::oneui_reorderable_grid_set_on_reorder_requested_utf8(
                self.widget.as_raw(),
                Some(run_grid_reorder_requested_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_reorder_requested(&mut self) {
        unsafe {
            sys::oneui_reorderable_grid_set_on_reorder_requested_utf8(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.reorder_callback = None;
    }

    pub fn set_item_drag_enabled(&self, enabled: bool) {
        unsafe {
            sys::oneui_reorderable_grid_set_item_drag_enabled(
                self.widget.as_raw(),
                i32::from(enabled),
            )
        };
    }

    pub fn item_drag_enabled(&self) -> bool {
        unsafe { sys::oneui_reorderable_grid_item_drag_enabled(self.widget.as_raw()) != 0 }
    }

    #[track_caller]
    pub fn set_on_item_drag<F>(&mut self, callback: F)
    where
        F: FnMut(ItemDragEvent) + 'static,
    {
        let trace = InteractionTrace::at(
            "ReorderableGrid",
            "item_drag",
            std::panic::Location::caller(),
        );
        self.clear_on_item_drag();
        self.item_drag_callback = Some(Box::new(ItemDragCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .item_drag_callback
            .as_deref_mut()
            .expect("item drag callback was just installed")
            as *mut ItemDragCallback)
            .cast();
        unsafe {
            sys::oneui_reorderable_grid_set_on_item_drag_utf8(
                self.widget.as_raw(),
                Some(run_item_drag_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_item_drag(&mut self) {
        unsafe {
            sys::oneui_reorderable_grid_set_on_item_drag_utf8(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.item_drag_callback = None;
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

impl Drop for ReorderableGrid {
    fn drop(&mut self) {
        self.clear_on_item_drag();
        self.clear_on_reorder_requested();
    }
}

/// A reusable native card/list-row surface with hover and press transitions.
pub struct InteractiveSurface {
    widget: Widget,
    click_callback: Option<Box<VoidCallback>>,
    pointer_callback: Option<Box<PointerCallback>>,
    pointer_moved_callback: Option<Box<PointerCallback>>,
    hover_changed_callback: Option<Box<BoolChangedCallback>>,
    focus_changed_callback: Option<Box<BoolChangedCallback>>,
    context_menu_callback: Option<Box<PointerCallback>>,
    drag_callback: Option<Box<PointerDragCallback>>,
}

impl InteractiveSurface {
    pub fn new() -> Result<Self, Error> {
        let widget = Widget::from_raw(unsafe { sys::oneui_interactive_surface_create() })?;
        Ok(Self {
            widget,
            click_callback: None,
            pointer_callback: None,
            pointer_moved_callback: None,
            hover_changed_callback: None,
            focus_changed_callback: None,
            context_menu_callback: None,
            drag_callback: None,
        })
    }

    pub fn set_content(&self, child: &Widget) {
        unsafe { sys::oneui_interactive_surface_set_content(self.widget.as_raw(), child.as_raw()) };
    }

    pub fn set_pointer_cursor(&self, cursor: PointerCursor) {
        unsafe {
            sys::oneui_interactive_surface_set_pointer_cursor(self.widget.as_raw(), cursor as i32)
        };
    }
    pub fn set_on_drag(
        &mut self,
        threshold: f32,
        callback: impl FnMut(PointerDragEvent) + 'static,
    ) {
        self.clear_on_drag();
        self.drag_callback = Some(Box::new(PointerDragCallback {
            handler: Box::new(callback),
        }));
        let data = (self.drag_callback.as_deref_mut().unwrap() as *mut PointerDragCallback).cast();
        unsafe {
            sys::oneui_interactive_surface_set_on_drag(
                self.widget.as_raw(),
                Some(run_pointer_drag_callback),
                threshold,
                data,
            )
        };
    }
    pub fn clear_on_drag(&mut self) {
        unsafe {
            sys::oneui_interactive_surface_set_on_drag(
                self.widget.as_raw(),
                None,
                7.0,
                std::ptr::null_mut(),
            )
        };
        self.drag_callback = None;
    }

    pub fn set_padding(&self, padding: Insets) {
        unsafe { sys::oneui_interactive_surface_set_padding(self.widget.as_raw(), padding.into()) };
    }

    pub fn set_style(&self, style: InteractiveSurfaceStyle) {
        let style: sys::OneUiInteractiveSurfaceStyle = style.into();
        unsafe { sys::oneui_interactive_surface_set_style(self.widget.as_raw(), &style) };
    }

    #[track_caller]
    pub fn set_on_click<F>(&mut self, callback: F)
    where
        F: FnMut() + 'static,
    {
        let trace = InteractionTrace::at(
            "InteractiveSurface",
            "click",
            std::panic::Location::caller(),
        );
        self.clear_on_click();
        self.click_callback = Some(Box::new(VoidCallback {
            handler: Rc::new(RefCell::new(Box::new(traced_callback(trace, callback)))),
        }));
        let user_data = (self
            .click_callback
            .as_deref_mut()
            .expect("interactive surface click callback was just installed")
            as *mut VoidCallback)
            .cast();
        unsafe {
            sys::oneui_interactive_surface_set_on_click(
                self.widget.as_raw(),
                Some(run_void_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_click(&mut self) {
        unsafe {
            sys::oneui_interactive_surface_set_on_click(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.click_callback = None;
    }

    #[track_caller]
    pub fn set_on_pointer_activated<F>(&mut self, callback: F)
    where
        F: FnMut(PointerEvent) + 'static,
    {
        let trace = InteractionTrace::at(
            "InteractiveSurface",
            "pointer_activated",
            std::panic::Location::caller(),
        );
        self.clear_on_pointer_activated();
        self.pointer_callback = Some(Box::new(PointerCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .pointer_callback
            .as_deref_mut()
            .expect("interactive surface pointer callback was just installed")
            as *mut PointerCallback)
            .cast();
        unsafe {
            sys::oneui_interactive_surface_set_on_pointer_activated(
                self.widget.as_raw(),
                Some(run_pointer_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_pointer_activated(&mut self) {
        unsafe {
            sys::oneui_interactive_surface_set_on_pointer_activated(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.pointer_callback = None;
    }

    #[track_caller]
    pub fn set_on_pointer_moved<F>(&mut self, callback: F)
    where
        F: FnMut(PointerEvent) + 'static,
    {
        let trace = InteractionTrace::at(
            "InteractiveSurface",
            "pointer_moved",
            std::panic::Location::caller(),
        );
        self.clear_on_pointer_moved();
        self.pointer_moved_callback = Some(Box::new(PointerCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .pointer_moved_callback
            .as_deref_mut()
            .expect("interactive surface pointer-moved callback was just installed")
            as *mut PointerCallback)
            .cast();
        unsafe {
            sys::oneui_interactive_surface_set_on_pointer_moved(
                self.widget.as_raw(),
                Some(run_pointer_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_pointer_moved(&mut self) {
        unsafe {
            sys::oneui_interactive_surface_set_on_pointer_moved(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.pointer_moved_callback = None;
    }

    #[track_caller]
    pub fn set_on_hover_changed<F>(&mut self, callback: F)
    where
        F: FnMut(bool) + 'static,
    {
        let trace = InteractionTrace::at(
            "InteractiveSurface",
            "hover_changed",
            std::panic::Location::caller(),
        );
        self.clear_on_hover_changed();
        self.hover_changed_callback = Some(Box::new(BoolChangedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .hover_changed_callback
            .as_deref_mut()
            .expect("interactive surface hover callback was just installed")
            as *mut BoolChangedCallback)
            .cast();
        unsafe {
            sys::oneui_interactive_surface_set_on_hover_changed(
                self.widget.as_raw(),
                Some(run_bool_changed_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_hover_changed(&mut self) {
        unsafe {
            sys::oneui_interactive_surface_set_on_hover_changed(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.hover_changed_callback = None;
    }

    #[track_caller]
    pub fn set_on_focus_changed<F>(&mut self, callback: F)
    where
        F: FnMut(bool) + 'static,
    {
        let trace = InteractionTrace::at(
            "InteractiveSurface",
            "focus_changed",
            std::panic::Location::caller(),
        );
        self.clear_on_focus_changed();
        self.focus_changed_callback = Some(Box::new(BoolChangedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .focus_changed_callback
            .as_deref_mut()
            .expect("interactive surface hover callback was just installed")
            as *mut BoolChangedCallback)
            .cast();
        unsafe {
            sys::oneui_interactive_surface_set_on_focus_changed(
                self.widget.as_raw(),
                Some(run_bool_changed_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_focus_changed(&mut self) {
        unsafe {
            sys::oneui_interactive_surface_set_on_focus_changed(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.focus_changed_callback = None;
    }

    #[track_caller]
    pub fn set_on_context_menu_requested<F>(&mut self, callback: F)
    where
        F: FnMut(PointerEvent) + 'static,
    {
        let trace = InteractionTrace::at(
            "InteractiveSurface",
            "context_menu_requested",
            std::panic::Location::caller(),
        );
        self.clear_on_context_menu_requested();
        self.context_menu_callback = Some(Box::new(PointerCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .context_menu_callback
            .as_deref_mut()
            .expect("interactive surface context menu callback was just installed")
            as *mut PointerCallback)
            .cast();
        unsafe {
            sys::oneui_interactive_surface_set_on_context_menu_requested(
                self.widget.as_raw(),
                Some(run_pointer_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_context_menu_requested(&mut self) {
        unsafe {
            sys::oneui_interactive_surface_set_on_context_menu_requested(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.context_menu_callback = None;
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

impl Drop for InteractiveSurface {
    fn drop(&mut self) {
        self.clear_on_drag();
        self.clear_on_click();
        self.clear_on_pointer_activated();
        self.clear_on_pointer_moved();
        self.clear_on_hover_changed();
        self.clear_on_focus_changed();
        self.clear_on_context_menu_requested();
    }
}
