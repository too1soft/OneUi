use oneui_mobile::{Error, Lifecycle, Phase, Pointer, PointerKind, Runtime, Viewport};

#[test]
fn native_runtime_survives_background_and_surface_recreation() {
    let mut runtime = Runtime::new().unwrap();
    runtime
        .set_viewport(Viewport {
            width_px: 1080.,
            height_px: 2400.,
            scale: 3.,
            safe_top_px: 90.,
            safe_bottom_px: 60.,
            ..Viewport::default()
        })
        .unwrap();
    runtime.set_surface(true).unwrap();
    runtime.set_lifecycle(Lifecycle::Active).unwrap();
    let frame = runtime.take_frame(100).unwrap().unwrap();
    assert_eq!(frame.content_width, 360.);
    assert_eq!(frame.content_height, 750.);
    assert!(runtime.take_frame(101).unwrap().is_none());
    runtime
        .push_pointer(Pointer {
            id: 42,
            phase: Phase::Down,
            kind: PointerKind::Touch,
            x: 300.,
            y: 600.,
            pressure: 1.,
            time_ns: 1,
            primary: false,
        })
        .unwrap();
    let pointer = runtime.poll_pointer().unwrap().unwrap();
    assert_eq!((pointer.x, pointer.y, pointer.primary), (100., 200., true));
    runtime.set_lifecycle(Lifecycle::Background).unwrap();
    assert_eq!(
        runtime.poll_pointer().unwrap().unwrap().phase,
        Phase::Cancel
    );
    assert!(!runtime.state().unwrap().needs_frame);
    runtime.set_surface(false).unwrap();
    runtime.set_surface(true).unwrap();
    runtime.set_lifecycle(Lifecycle::Active).unwrap();
    assert_eq!(
        runtime.take_frame(102).unwrap().unwrap().surface_generation,
        2
    );
    runtime.set_lifecycle(Lifecycle::Destroyed).unwrap();
    assert_eq!(runtime.invalidate(), Err(Error::InvalidState));
}
