//! Native stack layout and its direction/alignment contract.
use super::super::{sys, Error, Insets, Widget};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum StackDirection {
    Column,
    Row,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum StackAlign {
    Start = 0,
    Center = 1,
    End = 2,
    Stretch = 3,
}

pub struct Stack {
    widget: Widget,
}

impl Stack {
    pub fn new(direction: StackDirection) -> Result<Self, Error> {
        let direction = match direction {
            StackDirection::Column => 0,
            StackDirection::Row => 1,
        };
        let widget = Widget::from_raw(unsafe { sys::oneui_stack_create(direction) })?;
        Ok(Self { widget })
    }

    pub fn add(&self, child: &Widget) {
        unsafe { sys::oneui_stack_add(self.widget.as_raw(), child.as_raw()) };
    }

    /// Change flow without replacing children or their state.
    pub fn set_direction(&self, direction: StackDirection) {
        let direction = match direction {
            StackDirection::Column => 0,
            StackDirection::Row => 1,
        };
        unsafe { sys::oneui_stack_set_direction(self.widget.as_raw(), direction) };
    }

    pub fn set_gap(&self, gap: f32) {
        unsafe { sys::oneui_stack_set_gap(self.widget.as_raw(), gap) };
    }

    pub fn set_padding(&self, padding: Insets) {
        unsafe { sys::oneui_stack_set_padding(self.widget.as_raw(), padding.into()) };
    }

    pub fn set_align(&self, align: StackAlign) {
        unsafe { sys::oneui_stack_set_align(self.widget.as_raw(), align as i32) };
    }

    pub fn content_width(&self) -> f32 {
        unsafe { sys::oneui_stack_content_width(self.widget.as_raw()) }
    }

    pub fn content_height(&self) -> f32 {
        unsafe { sys::oneui_stack_content_height(self.widget.as_raw()) }
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}
