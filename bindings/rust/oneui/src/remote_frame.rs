//! Remote frame bindings.

use super::{sys, Arc, Error};

/// Pixel layout accepted by [`crate::RealtimeFrameView`].
///
/// NV12 is reserved by the C ABI but is not exposed here until OneUI has a
/// native conversion path; remote decoders should currently submit BGRA or
/// RGBA frames.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(i32)]
pub enum PixelFormat {
    Bgra8888 = 0,
    Rgba8888 = 1,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(i32)]
pub enum VideoScaleMode {
    ActualSize = 0,
    Fit = 1,
    Fill = 2,
    Stretch = 3,
}

/// Immutable decoded video frame that may be produced on any worker thread.
///
/// The pixel allocation is reference-counted and transferred to OneUI without
/// another full-frame copy. `stride == 0` selects the tightly packed stride.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct RemoteFrame {
    pub pixels: Arc<[u8]>,
    pub width: i32,
    pub height: i32,
    pub stride: i32,
    pub format: PixelFormat,
    pub frame_id: u64,
    pub timestamp_us: u64,
}

/// Copied dirty rectangle for a [`RemoteFrameDamage`] update.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct RemoteFramePatch {
    pub pixels: Arc<[u8]>,
    pub x: i32,
    pub y: i32,
    pub width: i32,
    pub height: i32,
    pub stride: i32,
}

impl RemoteFramePatch {
    pub fn new(
        pixels: impl Into<Arc<[u8]>>,
        x: i32,
        y: i32,
        width: i32,
        height: i32,
        stride: i32,
    ) -> Self {
        Self {
            pixels: pixels.into(),
            x,
            y,
            width,
            height,
            stride,
        }
    }
}

/// A batch of dirty rectangles applied to the current compatible remote frame.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct RemoteFrameDamage {
    pub width: i32,
    pub height: i32,
    pub format: PixelFormat,
    pub patches: Vec<RemoteFramePatch>,
    pub frame_id: u64,
    pub timestamp_us: u64,
}

impl RemoteFrameDamage {
    pub fn new(
        width: i32,
        height: i32,
        format: PixelFormat,
        patches: Vec<RemoteFramePatch>,
        frame_id: u64,
        timestamp_us: u64,
    ) -> Result<Self, Error> {
        let mut damage = Self {
            width,
            height,
            format,
            patches,
            frame_id,
            timestamp_us,
        };
        damage.normalize_and_validate()?;
        Ok(damage)
    }

    pub(super) fn normalize_and_validate(&mut self) -> Result<(), Error> {
        const MAX_PATCHES: usize = 64;
        if self.width <= 0 || self.height <= 0 {
            return Err(Error::InvalidVideoFrame {
                reason: "damage frame dimensions must be positive",
            });
        }
        if self.patches.is_empty() || self.patches.len() > MAX_PATCHES {
            return Err(Error::InvalidVideoFrame {
                reason: "damage must contain between 1 and 64 patches",
            });
        }
        for patch in &mut self.patches {
            if patch.width <= 0
                || patch.height <= 0
                || patch.x < 0
                || patch.y < 0
                || patch.x > self.width - patch.width
                || patch.y > self.height - patch.height
            {
                return Err(Error::InvalidVideoFrame {
                    reason: "damage patch is outside the remote frame",
                });
            }
            let row_bytes = patch.width.checked_mul(4).ok_or(Error::InvalidVideoFrame {
                reason: "damage row byte count overflowed",
            })?;
            if patch.stride == 0 {
                patch.stride = row_bytes;
            }
            if patch.stride < row_bytes {
                return Err(Error::InvalidVideoFrame {
                    reason: "damage stride is smaller than a pixel row",
                });
            }
            let required_bytes = usize::try_from(patch.height - 1)
                .ok()
                .and_then(|height| height.checked_mul(patch.stride as usize))
                .and_then(|prefix| prefix.checked_add(row_bytes as usize))
                .ok_or(Error::InvalidVideoFrame {
                    reason: "damage byte count overflowed",
                })?;
            if patch.pixels.len() < required_bytes {
                return Err(Error::InvalidVideoFrame {
                    reason: "damage pixel buffer is shorter than its metadata requires",
                });
            }
        }
        Ok(())
    }
}

impl RemoteFrame {
    pub fn new(
        pixels: impl Into<Arc<[u8]>>,
        width: i32,
        height: i32,
        stride: i32,
        format: PixelFormat,
        frame_id: u64,
        timestamp_us: u64,
    ) -> Result<Self, Error> {
        let mut frame = Self {
            pixels: pixels.into(),
            width,
            height,
            stride,
            format,
            frame_id,
            timestamp_us,
        };
        frame.normalize_and_validate()?;
        Ok(frame)
    }

    pub(super) fn normalize_and_validate(&mut self) -> Result<(), Error> {
        if self.width <= 0 || self.height <= 0 {
            return Err(Error::InvalidVideoFrame {
                reason: "width and height must be positive",
            });
        }
        let width = usize::try_from(self.width).map_err(|_| Error::InvalidVideoFrame {
            reason: "width is outside the supported range",
        })?;
        let height = usize::try_from(self.height).map_err(|_| Error::InvalidVideoFrame {
            reason: "height is outside the supported range",
        })?;
        let row_bytes = width.checked_mul(4).ok_or(Error::InvalidVideoFrame {
            reason: "row byte count overflowed",
        })?;
        if row_bytes > i32::MAX as usize {
            return Err(Error::InvalidVideoFrame {
                reason: "row byte count exceeds the native stride range",
            });
        }
        if self.stride == 0 {
            self.stride = row_bytes as i32;
        }
        if self.stride < 0 || (self.stride as usize) < row_bytes {
            return Err(Error::InvalidVideoFrame {
                reason: "stride is smaller than a pixel row",
            });
        }
        let required_bytes = (height - 1)
            .checked_mul(self.stride as usize)
            .and_then(|prefix| prefix.checked_add(row_bytes))
            .ok_or(Error::InvalidVideoFrame {
                reason: "frame byte count overflowed",
            })?;
        if self.pixels.len() < required_bytes {
            return Err(Error::InvalidVideoFrame {
                reason: "pixel buffer is shorter than the frame metadata requires",
            });
        }
        Ok(())
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(i32)]
pub enum RemoteCursorMode {
    Default = 0,
    Hidden = 1,
    Bitmap = 2,
}

/// Premultiplied RGBA bitmap used for a server-rendered remote cursor.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct RemoteCursorImage {
    pub pixels: Arc<[u8]>,
    pub width: i32,
    pub height: i32,
    pub stride: i32,
    pub hotspot_x: i32,
    pub hotspot_y: i32,
}

impl RemoteCursorImage {
    pub fn new(
        pixels: impl Into<Arc<[u8]>>,
        width: i32,
        height: i32,
        stride: i32,
        hotspot_x: i32,
        hotspot_y: i32,
    ) -> Result<Self, Error> {
        let mut image = Self {
            pixels: pixels.into(),
            width,
            height,
            stride,
            hotspot_x,
            hotspot_y,
        };
        image.normalize_and_validate()?;
        Ok(image)
    }

    pub(super) fn normalize_and_validate(&mut self) -> Result<(), Error> {
        const MAX_CURSOR_DIMENSION: i32 = 512;
        if self.width <= 0
            || self.height <= 0
            || self.width > MAX_CURSOR_DIMENSION
            || self.height > MAX_CURSOR_DIMENSION
        {
            return Err(Error::InvalidRemoteCursor {
                reason: "cursor dimensions must be between 1 and 512 pixels",
            });
        }
        let row_bytes = self
            .width
            .checked_mul(4)
            .ok_or(Error::InvalidRemoteCursor {
                reason: "cursor row byte count overflowed",
            })?;
        if self.stride == 0 {
            self.stride = row_bytes;
        }
        if self.stride < row_bytes {
            return Err(Error::InvalidRemoteCursor {
                reason: "cursor stride is smaller than a pixel row",
            });
        }
        if self.hotspot_x < 0
            || self.hotspot_x >= self.width
            || self.hotspot_y < 0
            || self.hotspot_y >= self.height
        {
            return Err(Error::InvalidRemoteCursor {
                reason: "cursor hotspot is outside the bitmap",
            });
        }
        let required_bytes = usize::try_from(self.height - 1)
            .ok()
            .and_then(|height| height.checked_mul(self.stride as usize))
            .and_then(|prefix| prefix.checked_add(row_bytes as usize))
            .ok_or(Error::InvalidRemoteCursor {
                reason: "cursor byte count overflowed",
            })?;
        if self.pixels.len() < required_bytes {
            return Err(Error::InvalidRemoteCursor {
                reason: "cursor pixel buffer is shorter than its metadata requires",
            });
        }
        Ok(())
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum RemotePointerButton {
    None,
    Left,
    Right,
    Middle,
    X1,
    X2,
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub struct RemotePointerEvent {
    pub window_x: f32,
    pub window_y: f32,
    pub content_x: f32,
    pub content_y: f32,
    pub normalized_x: f32,
    pub normalized_y: f32,
    pub remote_x: f32,
    pub remote_y: f32,
    pub button: RemotePointerButton,
    pub pressed: bool,
    pub wheel_delta_x: i32,
    pub wheel_delta_y: i32,
}

impl From<sys::OneUiRemotePointerEvent> for RemotePointerEvent {
    fn from(value: sys::OneUiRemotePointerEvent) -> Self {
        let button = match value.button {
            1 => RemotePointerButton::Left,
            2 => RemotePointerButton::Right,
            3 => RemotePointerButton::Middle,
            4 => RemotePointerButton::X1,
            5 => RemotePointerButton::X2,
            _ => RemotePointerButton::None,
        };
        Self {
            window_x: value.window_x,
            window_y: value.window_y,
            content_x: value.content_x,
            content_y: value.content_y,
            normalized_x: value.normalized_x,
            normalized_y: value.normalized_y,
            remote_x: value.remote_x,
            remote_y: value.remote_y,
            button,
            pressed: value.pressed != 0,
            wheel_delta_x: value.wheel_delta_x,
            wheel_delta_y: value.wheel_delta_y,
        }
    }
}
