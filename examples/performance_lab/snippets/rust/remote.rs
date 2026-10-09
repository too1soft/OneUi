use oneui::{
    Error, OverlayHost, PixelFormat, RealtimeFrameView, RemoteFrame, RemoteInputRegion,
    VideoScaleMode,
};
use std::sync::Arc;

// Retain the Rust input owner as well as the widget tree, so callbacks remain alive.
pub struct RemoteExample {
    pub host: OverlayHost,
    pub frame: RealtimeFrameView,
    pub input: RemoteInputRegion,
}
impl RemoteExample {
    pub fn new(on_text: impl FnMut(String) + 'static) -> Result<Self, Error> {
        let frame = RealtimeFrameView::new()?;
        let pixels: Arc<[u8]> = vec![
            196, 237, 135, 255, 48, 98, 78, 255, 48, 98, 78, 255, 196, 237, 135, 255,
        ]
        .into();
        frame.submit_frame(RemoteFrame::new(
            pixels,
            2,
            2,
            8,
            PixelFormat::Rgba8888,
            1,
            0,
        )?)?;
        let mut input = RemoteInputRegion::new()?;
        frame.set_scale_mode(VideoScaleMode::Fit);
        input.set_scale_mode(VideoScaleMode::Fit);
        input.set_remote_size(2.0, 2.0);
        input.set_on_text_input(on_text);
        let host = OverlayHost::new()?;
        host.set_content(frame.as_widget());
        host.add_overlay(input.as_widget(), 0);
        Ok(Self { host, frame, input })
    }
    pub fn deactivate(&self) {
        self.input.release_all_inputs();
    }
}
impl Drop for RemoteExample {
    fn drop(&mut self) {
        self.deactivate();
    }
}
