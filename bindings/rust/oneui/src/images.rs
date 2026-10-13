//! Images bindings.

use super::visual_style::Color;
use super::{
    run_void_callback, sys, traced_callback, Error, IconSymbol, InteractionTrace, Rc, RefCell,
    VoidCallback, Widget,
};

pub struct Icon {
    widget: Widget,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ImageContentMode {
    Contain = 0,
    Cover = 1,
    Stretch = 2,
    ActualSize = 3,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ImageContentAlignment {
    Start = 0,
    Center = 1,
    End = 2,
}

/// Renderer-neutral RGBA image widget. OneUI copies the supplied pixels.
pub struct ImageView {
    widget: Widget,
}

impl ImageView {
    pub fn new() -> Result<Self, Error> {
        let widget = Widget::from_raw(unsafe { sys::oneui_image_view_create() })?;
        Ok(Self { widget })
    }

    pub fn set_rgba(&self, pixels: &[u8], width: u32, height: u32) -> Result<(), Error> {
        let row_bytes = width.checked_mul(4).ok_or(Error::InvalidImagePixels {
            reason: "image row is too wide",
        })?;
        let required = row_bytes
            .checked_mul(height)
            .ok_or(Error::InvalidImagePixels {
                reason: "image is too large",
            })? as usize;
        if width == 0
            || height == 0
            || pixels.len() < required
            || width > i32::MAX as u32
            || height > i32::MAX as u32
        {
            return Err(Error::InvalidImagePixels {
                reason: "invalid RGBA dimensions or buffer length",
            });
        }
        let accepted = unsafe {
            sys::oneui_image_view_set_rgba(
                self.widget.as_raw(),
                pixels.as_ptr(),
                pixels.len(),
                width as i32,
                height as i32,
                row_bytes as i32,
            )
        };
        if accepted == 1 {
            Ok(())
        } else {
            Err(Error::InvalidImagePixels {
                reason: "OneUI rejected the RGBA pixels",
            })
        }
    }

    pub fn clear(&self) {
        unsafe { sys::oneui_image_view_clear(self.widget.as_raw()) };
    }

    pub fn set_content_mode(&self, mode: ImageContentMode) {
        unsafe { sys::oneui_image_view_set_content_mode(self.widget.as_raw(), mode as i32) };
    }

    pub fn set_content_alignment(
        &self,
        horizontal: ImageContentAlignment,
        vertical: ImageContentAlignment,
    ) {
        unsafe {
            sys::oneui_image_view_set_content_alignment(
                self.widget.as_raw(),
                horizontal as i32,
                vertical as i32,
            )
        };
    }

    pub fn set_corner_radius(&self, radius: f32) {
        unsafe { sys::oneui_image_view_set_corner_radius(self.widget.as_raw(), radius) };
    }

    pub fn set_background(&self, color: Color) {
        unsafe {
            sys::oneui_image_view_set_background(
                self.widget.as_raw(),
                color.r,
                color.g,
                color.b,
                color.a,
            )
        };
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

impl Icon {
    pub fn new(symbol: IconSymbol) -> Result<Self, Error> {
        let widget = Widget::from_raw(unsafe { sys::oneui_icon_create(symbol as i32) })?;
        Ok(Self { widget })
    }

    pub fn set_symbol(&self, symbol: IconSymbol) {
        unsafe { sys::oneui_icon_set_symbol(self.widget.as_raw(), symbol as i32) };
    }

    pub fn set_color(&self, color: Color) {
        unsafe {
            sys::oneui_icon_set_color(self.widget.as_raw(), color.r, color.g, color.b, color.a)
        };
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

pub struct IconButton {
    widget: Widget,
    callback: Option<Box<VoidCallback>>,
}

impl IconButton {
    pub fn new(symbol: IconSymbol) -> Result<Self, Error> {
        let widget = Widget::from_raw(unsafe { sys::oneui_icon_button_create(symbol as i32) })?;
        Ok(Self {
            widget,
            callback: None,
        })
    }

    pub fn set_symbol(&self, symbol: IconSymbol) {
        unsafe { sys::oneui_icon_button_set_symbol(self.widget.as_raw(), symbol as i32) };
    }

    #[track_caller]
    pub fn set_on_click<F>(&mut self, callback: F)
    where
        F: FnMut() + 'static,
    {
        let trace = InteractionTrace::at("IconButton", "click", std::panic::Location::caller());
        self.clear_on_click();
        self.callback = Some(Box::new(VoidCallback {
            handler: Rc::new(RefCell::new(Box::new(traced_callback(trace, callback)))),
        }));
        let user_data = (self
            .callback
            .as_deref_mut()
            .expect("icon button callback was just installed")
            as *mut VoidCallback)
            .cast();
        unsafe {
            sys::oneui_icon_button_set_on_click(
                self.widget.as_raw(),
                Some(run_void_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_click(&mut self) {
        unsafe {
            sys::oneui_icon_button_set_on_click(self.widget.as_raw(), None, std::ptr::null_mut())
        };
        self.callback = None;
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

impl Drop for IconButton {
    fn drop(&mut self) {
        self.clear_on_click();
    }
}
