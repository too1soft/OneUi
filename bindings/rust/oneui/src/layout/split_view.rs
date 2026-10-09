//! Native split layout and divider callbacks.
use super::super::{
    run_callback_guarded, sys, traced_value_callback, Color, Error, Insets, InteractionTrace,
    Widget,
};
use std::cell::RefCell;
use std::rc::Rc;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SplitOrientation {
    Horizontal = 0,
    Vertical = 1,
}

struct FloatChangedCallback {
    handler: Rc<RefCell<Box<dyn FnMut(f32) + 'static>>>,
}

unsafe extern "C" fn run_float_changed_callback(value: f32, user_data: *mut std::ffi::c_void) {
    if user_data.is_null() {
        return;
    }
    // Product code may clear this callback or drop the SplitView. Copy ownership
    // before entering it, without keeping a reference into the callback token.
    let handler = unsafe { (&*user_data.cast::<FloatChangedCallback>()).handler.clone() };
    // A nested event loop must not create two mutable calls to the same FnMut.
    let Ok(mut callback) = handler.try_borrow_mut() else {
        return;
    };
    run_callback_guarded("value.float_changed", || callback(value));
}

/// Native resizable two-pane layout.
///
/// OneUI owns divider hit testing and drag capture. Product code supplies pane
/// content, minimum extents, and optionally persists ratio changes.
pub struct SplitView {
    widget: Widget,
    ratio_changed_callback: Option<Box<FloatChangedCallback>>,
    ratio_committed_callback: Option<Box<FloatChangedCallback>>,
}

impl SplitView {
    pub fn new(orientation: SplitOrientation) -> Result<Self, Error> {
        let widget = Widget::from_raw(unsafe { sys::oneui_split_view_create(orientation as i32) })?;
        Ok(Self {
            widget,
            ratio_changed_callback: None,
            ratio_committed_callback: None,
        })
    }

    pub fn set_first(&self, child: &Widget) {
        unsafe { sys::oneui_split_view_set_first(self.widget.as_raw(), child.as_raw()) };
    }

    pub fn set_second(&self, child: &Widget) {
        unsafe { sys::oneui_split_view_set_second(self.widget.as_raw(), child.as_raw()) };
    }

    pub fn set_orientation(&self, orientation: SplitOrientation) {
        unsafe { sys::oneui_split_view_set_orientation(self.widget.as_raw(), orientation as i32) };
    }

    pub fn set_ratio(&self, ratio: f32) {
        unsafe { sys::oneui_split_view_set_ratio(self.widget.as_raw(), ratio) };
    }

    pub fn ratio(&self) -> f32 {
        unsafe { sys::oneui_split_view_ratio(self.widget.as_raw()) }
    }

    pub fn set_gap(&self, gap: f32) {
        unsafe { sys::oneui_split_view_set_gap(self.widget.as_raw(), gap) };
    }

    /// Colors of the divider at rest and while hovered, dragged or keyboard-focused.
    pub fn set_divider_colors(&self, normal: Color, active: Color) {
        unsafe {
            sys::oneui_split_view_set_divider_colors(
                self.widget.as_raw(),
                normal.into(),
                active.into(),
            )
        };
    }

    pub fn set_padding(&self, padding: Insets) {
        unsafe { sys::oneui_split_view_set_padding(self.widget.as_raw(), padding.into()) };
    }

    pub fn set_resizable(&self, resizable: bool) {
        unsafe { sys::oneui_split_view_set_resizable(self.widget.as_raw(), i32::from(resizable)) };
    }

    pub fn set_minimum_pane_extent(&self, first: f32, second: f32) {
        unsafe {
            sys::oneui_split_view_set_minimum_pane_extent(self.widget.as_raw(), first, second)
        };
    }

    #[track_caller]
    pub fn set_on_ratio_changed<F>(&mut self, callback: F)
    where
        F: FnMut(f32) + 'static,
    {
        let trace =
            InteractionTrace::at("SplitView", "ratio_changed", std::panic::Location::caller());
        self.clear_on_ratio_changed();
        self.ratio_changed_callback = Some(Box::new(FloatChangedCallback {
            handler: Rc::new(RefCell::new(Box::new(traced_value_callback(
                trace, callback,
            )))),
        }));
        let user_data = (self
            .ratio_changed_callback
            .as_deref_mut()
            .expect("split ratio callback was just installed")
            as *mut FloatChangedCallback)
            .cast();
        unsafe {
            sys::oneui_split_view_set_on_ratio_changed(
                self.widget.as_raw(),
                Some(run_float_changed_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_ratio_changed(&mut self) {
        unsafe {
            sys::oneui_split_view_set_on_ratio_changed(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.ratio_changed_callback = None;
    }

    #[track_caller]
    pub fn set_on_ratio_committed<F>(&mut self, callback: F)
    where
        F: FnMut(f32) + 'static,
    {
        let trace = InteractionTrace::at(
            "SplitView",
            "ratio_committed",
            std::panic::Location::caller(),
        );
        self.clear_on_ratio_committed();
        self.ratio_committed_callback = Some(Box::new(FloatChangedCallback {
            handler: Rc::new(RefCell::new(Box::new(traced_value_callback(
                trace, callback,
            )))),
        }));
        let user_data = (self
            .ratio_committed_callback
            .as_deref_mut()
            .expect("split ratio committed callback was just installed")
            as *mut FloatChangedCallback)
            .cast();
        unsafe {
            sys::oneui_split_view_set_on_ratio_committed(
                self.widget.as_raw(),
                Some(run_float_changed_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_ratio_committed(&mut self) {
        unsafe {
            sys::oneui_split_view_set_on_ratio_committed(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.ratio_committed_callback = None;
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

impl Drop for SplitView {
    fn drop(&mut self) {
        self.clear_on_ratio_changed();
        self.clear_on_ratio_committed();
    }
}

#[cfg(test)]
pub(crate) mod tests {
    use super::{run_float_changed_callback, FloatChangedCallback, SplitOrientation, SplitView};
    use std::cell::{Cell, RefCell};
    use std::rc::Rc;

    pub(crate) fn callback_survives_owner_destruction() {
        struct DropFlag(Rc<Cell<bool>>);
        impl Drop for DropFlag {
            fn drop(&mut self) {
                self.0.set(true);
            }
        }

        for committed in [false, true] {
            let owner = Rc::new(RefCell::new(Some(
                SplitView::new(SplitOrientation::Horizontal).unwrap(),
            )));
            let weak_owner = Rc::downgrade(&owner);
            let dropped = Rc::new(Cell::new(false));
            let observed = Rc::new(Cell::new(false));
            let observed_in_callback = observed.clone();
            let flag = DropFlag(dropped.clone());
            let callback = move |value: f32| {
                assert_eq!(value, 0.75);
                drop(weak_owner.upgrade().unwrap().borrow_mut().take());
                assert!(!flag.0.get(), "active handler must outlive its owner");
                observed_in_callback.set(true);
            };
            let data = {
                let mut borrowed = owner.borrow_mut();
                let split = borrowed.as_mut().unwrap();
                if committed {
                    split.set_on_ratio_committed(callback);
                    split.ratio_committed_callback.as_deref_mut().unwrap()
                        as *mut FloatChangedCallback
                } else {
                    split.set_on_ratio_changed(callback);
                    split.ratio_changed_callback.as_deref_mut().unwrap()
                        as *mut FloatChangedCallback
                }
            };
            // Exercise the actual Rust C callback with its registered token.
            // The token is valid on entry and intentionally destroyed inside.
            unsafe { run_float_changed_callback(0.75, data.cast()) };
            assert!(owner.borrow().is_none());
            assert!(observed.get(), "the callback must reach its last statement");
            assert!(dropped.get(), "the handler must be released on return");
        }
    }

    pub(crate) fn callback_reentry_and_panic_are_contained() {
        let calls = Rc::new(Cell::new(0));
        let calls_in_callback = calls.clone();
        let token = Rc::new(Cell::new(std::ptr::null_mut()));
        let token_in_callback = token.clone();
        let mut callback = Box::new(FloatChangedCallback {
            handler: Rc::new(RefCell::new(Box::new(move |value| {
                calls_in_callback.set(calls_in_callback.get() + 1);
                // The outer call retains FnMut's exclusive borrow.
                unsafe { run_float_changed_callback(value, token_in_callback.get()) };
                if value == 0.25 {
                    panic!("expected split callback panic");
                }
            }))),
        });
        token.set((&mut *callback as *mut FloatChangedCallback).cast());
        unsafe {
            run_float_changed_callback(0.25, token.get());
            run_float_changed_callback(0.75, token.get());
            run_float_changed_callback(0.75, std::ptr::null_mut());
        }
        assert_eq!(calls.get(), 2, "reentry is skipped and later calls recover");
    }
}
