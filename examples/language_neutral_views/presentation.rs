use crate::vm::Settings;
use oneui::{view::ViewMount, UiDispatcher};
use std::{
    cell::{Cell, RefCell},
    rc::Rc,
};

// Capture the settled native frame, not the first frame of the initial palette
// transition. Keep pumping the UI; never block its animation scheduler.
pub fn capture_settled(
    dispatcher: UiDispatcher,
    path: std::path::PathBuf,
    start: std::time::Instant,
) {
    let next = dispatcher.clone();
    dispatcher
        .request_animation_frame_local(move |_| {
            if start.elapsed() < std::time::Duration::from_millis(350) {
                capture_settled(next, path, start);
            } else {
                std::fs::write(
                    path.with_extension("json"),
                    next.layout_snapshot_json().unwrap(),
                )
                .unwrap();
                assert!(next.capture_frame_png(&path), "capture failed");
                next.close().unwrap();
            }
        })
        .expect("schedule capture");
}

pub fn css(index: usize) -> String {
    let accents = [
        "#28613d", "#536270", "#806246", "#227565", "#286881", "#966238", "#84c49a", "#a6b5c4",
        "#73b9dc", "#85bd93", "#c1a0df", "#dfa18c",
    ];
    let css = include_str!("theme.css").replace("#28613d", accents[index]);
    if index < 6 {
        css
    } else {
        css.replace("#21302b", "#e3eae5")
            .replace("#62716a", "#a5b5ac")
            .replace("#ffffff", "#242d28")
            .replace("#f3f6f3", "#18201b")
            .replace("#c6d0c8", "#43574a")
            .replace("#e7eee8", "#35453b")
            .replace("#d5e2d7", "#455e4e")
    }
}

pub fn connect(vm: &Rc<RefCell<Settings>>, mount: &Rc<ViewMount>, dispatcher: &UiDispatcher) {
    let weak_vm = Rc::downgrade(vm);
    let weak_mount = Rc::downgrade(mount);
    let queued = Rc::new(Cell::new(false));
    let dispatcher = dispatcher.clone();
    *vm.borrow().theme.presentation.borrow_mut() = Some(Rc::new(move || {
        if queued.replace(true) {
            return;
        }
        let queued = queued.clone();
        let vm = weak_vm.clone();
        let mount = weak_mount.clone();
        let _ = dispatcher.dispatch_local(move || {
            queued.set(false);
            if let (Some(vm), Some(mount)) = (vm.upgrade(), mount.upgrade()) {
                let vm = vm.borrow();
                mount
                    .set_styles(&css(vm.theme.selected.get()), vm.compact)
                    .expect("valid theme CSS");
            }
        });
    }));
}
