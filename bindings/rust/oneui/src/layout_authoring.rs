use crate::{sys, Error, Stack, Widget};

/// Yoga is opt-in and requires a native library built with ONEUI_ENABLE_YOGA.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum StackEngine {
    Legacy = 0,
    Yoga = 1,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum StackJustify {
    Start = 0,
    Center = 1,
    End = 2,
    SpaceBetween = 3,
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum FlexBasis {
    Preferred,
    Length(f32),
    Content,
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Flex {
    pub grow: f32,
    pub shrink: f32,
    pub basis: FlexBasis,
    pub min: f32,
    pub max: f32,
}
impl Default for Flex {
    fn default() -> Self {
        Self {
            grow: 0.0,
            shrink: 1.0,
            basis: FlexBasis::Preferred,
            min: 0.0,
            max: f32::INFINITY,
        }
    }
}
impl Flex {
    pub fn content() -> Self {
        Self {
            basis: FlexBasis::Content,
            ..Self::default()
        }
    }
    pub fn fill(weight: f32) -> Self {
        Self {
            grow: weight,
            basis: FlexBasis::Length(0.0),
            ..Self::default()
        }
    }
}
impl Stack {
    pub fn set_engine(&self, engine: StackEngine) -> Result<(), Error> {
        if unsafe { sys::oneui_stack_set_engine(self.as_widget().as_raw(), engine as i32) } == 1 {
            Ok(())
        } else {
            Err(Error::InvalidLayout)
        }
    }
    pub fn set_wrap(&self, enabled: bool) -> Result<(), Error> {
        if unsafe { sys::oneui_stack_set_wrap(self.as_widget().as_raw(), enabled as i32) } == 1 {
            Ok(())
        } else {
            Err(Error::InvalidLayout)
        }
    }
    pub fn set_justify(&self, justify: StackJustify) {
        unsafe {
            sys::oneui_stack_set_justify(self.as_widget().as_raw(), justify as i32);
        }
    }
    /// Child must already be a direct member. Invalid constraints leave its
    /// previous configuration intact. Content is remeasured during layout.
    pub fn set_flex(&self, child: &Widget, flex: Flex) -> Result<(), Error> {
        let (mode, basis) = match flex.basis {
            FlexBasis::Preferred => (0, 0.0),
            FlexBasis::Length(value) => (1, value),
            FlexBasis::Content => (2, 0.0),
        };
        let ok = unsafe {
            sys::oneui_stack_set_flex(
                self.as_widget().as_raw(),
                child.as_raw(),
                flex.grow,
                flex.shrink,
                mode,
                basis,
                flex.min,
                flex.max,
            )
        };
        if ok == 1 {
            Ok(())
        } else {
            Err(Error::InvalidLayout)
        }
    }
    pub fn clear_flex(&self, child: &Widget) {
        unsafe {
            sys::oneui_stack_clear_flex(self.as_widget().as_raw(), child.as_raw());
        }
    }
}
impl Widget {
    /// Content measurement under available width/height; infinity is unbounded.
    pub fn measure(&self, max_width: f32, max_height: f32) -> Result<(f32, f32), Error> {
        let (mut width, mut height) = (0.0, 0.0);
        if unsafe {
            sys::oneui_widget_measure(
                self.as_raw(),
                max_width,
                max_height,
                &mut width,
                &mut height,
            )
        } == 1
        {
            Ok((width, height))
        } else {
            Err(Error::InvalidLayout)
        }
    }
    /// Unwrapped content size in logical pixels with the current font/style.
    pub fn natural_size(&self) -> Result<(f32, f32), Error> {
        let (mut width, mut height) = (0.0, 0.0);
        let ok = unsafe { sys::oneui_widget_natural_size(self.as_raw(), &mut width, &mut height) };
        if ok == 1 {
            Ok((width, height))
        } else {
            Err(Error::InvalidLayout)
        }
    }
}
