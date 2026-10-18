//! Select bindings.

use super::list::{run_list_changed_callback, ListChangedCallback};
use super::{
    sys, traced_value_callback, Arc, AtomicPtr, Error, InteractionTrace, Ordering, UiDispatcher,
    Widget,
};

impl UiDispatcher {
    /// Creates a thread-safe item producer for a select mounted in this window.
    pub fn select_handle(&self, select: &Select) -> SelectHandle {
        SelectHandle {
            state: Arc::clone(&select.state),
            dispatcher: self.clone(),
        }
    }
}

/// A compact native option selector with keyboard navigation and light dismiss.
pub struct Select {
    widget: Widget,
    state: Arc<SelectState>,
    changed_callback: Option<Box<ListChangedCallback>>,
}

struct SelectState {
    raw: AtomicPtr<sys::OneUiWidget>,
}

/// Thread-safe producer for replacing the choices and selection of a mounted select.
#[derive(Clone)]
pub struct SelectHandle {
    state: Arc<SelectState>,
    dispatcher: UiDispatcher,
}

impl SelectHandle {
    pub fn set_items(&self, items: Vec<String>, selected_index: i32) -> Result<(), Error> {
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }
        let state = Arc::clone(&self.state);
        self.dispatcher.dispatch(move || {
            let raw = state.raw.load(Ordering::Acquire);
            if raw.is_null() {
                return;
            }
            let native_items = items
                .iter()
                .map(|item| sys::OneUiUtf8String::from_str(item))
                .collect::<Vec<_>>();
            unsafe {
                sys::oneui_select_set_items_utf8(raw, native_items.as_ptr(), native_items.len());
                sys::oneui_select_set_selected_index(raw, selected_index);
            }
        })
    }

    pub fn set_selected_index(&self, index: i32) -> Result<(), Error> {
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }
        let state = Arc::clone(&self.state);
        self.dispatcher.dispatch(move || {
            let raw = state.raw.load(Ordering::Acquire);
            if !raw.is_null() {
                unsafe { sys::oneui_select_set_selected_index(raw, index) };
            }
        })
    }
}

impl Select {
    pub fn new(items: &[String]) -> Result<Self, Error> {
        let widget = Widget::from_raw(unsafe { sys::oneui_select_create() })?;
        let select = Self {
            state: Arc::new(SelectState {
                raw: AtomicPtr::new(widget.as_raw()),
            }),
            widget,
            changed_callback: None,
        };
        select.set_items(items);
        Ok(select)
    }

    pub fn set_items(&self, items: &[String]) {
        let native_items: Vec<sys::OneUiUtf8String> = items
            .iter()
            .map(|item| sys::OneUiUtf8String::from_str(item))
            .collect();
        unsafe {
            sys::oneui_select_set_items_utf8(
                self.widget.as_raw(),
                native_items.as_ptr(),
                native_items.len(),
            )
        };
    }

    pub fn set_selected_index(&self, index: i32) {
        unsafe { sys::oneui_select_set_selected_index(self.widget.as_raw(), index) };
    }

    pub fn selected_index(&self) -> i32 {
        unsafe { sys::oneui_select_selected_index(self.widget.as_raw()) }
    }

    #[track_caller]
    pub fn set_on_changed<F>(&mut self, callback: F)
    where
        F: FnMut(i32) + 'static,
    {
        let trace = InteractionTrace::at("Select", "changed", std::panic::Location::caller());
        self.clear_on_changed();
        self.changed_callback = Some(Box::new(ListChangedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .changed_callback
            .as_deref_mut()
            .expect("select callback was just installed")
            as *mut ListChangedCallback)
            .cast();
        unsafe {
            sys::oneui_select_set_on_changed(
                self.widget.as_raw(),
                Some(run_list_changed_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_changed(&mut self) {
        unsafe {
            sys::oneui_select_set_on_changed(self.widget.as_raw(), None, std::ptr::null_mut())
        };
        self.changed_callback = None;
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

impl Drop for Select {
    fn drop(&mut self) {
        self.clear_on_changed();
        self.state
            .raw
            .store(std::ptr::null_mut(), Ordering::Release);
    }
}
