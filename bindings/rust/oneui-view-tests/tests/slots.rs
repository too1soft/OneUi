use crate::model::*;
use oneui::view::*;
use std::{cell::RefCell, rc::Rc};

pub fn run() {
    let w = crate::window();
    let mount = ViewMount::new(&w, None).unwrap();
    let vm = Rc::new(RefCell::new(Vm::new()));
    let mut view = View::instantiate(&mount, page::spec(), vm.clone()).unwrap();
    let single = include_bytes!(concat!(env!("OUT_DIR"), "/slot-Single.oneview"));
    let wrapped = include_bytes!(concat!(env!("OUT_DIR"), "/slot-Wrapped.oneview"));
    assert_eq!(&single[8..12], &[1, 0, 1, 0]);
    assert_eq!(&wrapped[8..12], &[1, 0, 0, 0]);
    for _ in 0..50 {
        for ir in [single.as_slice(), wrapped.as_slice()] {
            view.reload(ir).unwrap();
            assert_eq!(vm.borrow().connections, 1);
            vm.borrow_mut().title = "slot reload".into();
            view.notifier().unwrap().notify(page::TITLE).unwrap();
            view.flush().unwrap();
        }
    }
    let mut unknown_flags = single.to_vec();
    let entry = 128 + 14 * 32; // SLOT directory, kind 15.
    let start = u64::from_le_bytes(unknown_flags[entry + 8..entry + 16].try_into().unwrap()) as usize;
    unknown_flags[start + 12..start + 16].copy_from_slice(&4u32.to_le_bytes());
    assert_eq!(view.reload(&unknown_flags), Err(ViewError::BAD_IR));
    view.notifier().unwrap().notify(page::TITLE).unwrap();
    view.flush().unwrap();
    drop(view);
    assert_eq!(vm.borrow().connections, 0);
}
