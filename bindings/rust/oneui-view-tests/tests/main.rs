mod compile_fail;
mod model;
mod slots;
mod watcher;
mod workspace;
use model::*;
use oneui::{view::*, Window, WindowOptions};
use std::{
    cell::{Cell, RefCell},
    rc::Rc,
};

fn main() {
    let cases: &[(&str, fn())] = &[
        (
            "generated property and typed command dispatch",
            page::dispatch_contract,
        ),
        ("single-root slots and 50 bidirectional reloads", slots::run),
        ("dynamic scope and content lease", workspace::run),
        ("generated IR matches standalone IR", identical_ir),
        ("Rust VM lifecycle and two views", lifecycle),
        ("get/connect panic contains unwind", panics),
        ("disconnect/drop panic releases once", cleanup_panics),
        ("host retains complete Rust owner", host_owner),
        ("list RESET preserves/replaces identity", list_identity),
        ("50 IR reloads retain Rust VM connections", reload_lifecycle),
        (
            "watcher debounces partial writes and joins on drop",
            watcher::run,
        ),
        (
            "type/readonly/command/Send compile failures",
            compile_fail::run,
        ),
    ];
    for (name, test) in cases {
        test();
        println!("PASS {name}");
    }
    println!("{} Rust view integration scenarios passed", cases.len());
}
fn window() -> Window {
    Window::new(&WindowOptions::default()).unwrap()
}
fn identical_ir() {
    assert_eq!(
        page::IR,
        include_bytes!(concat!(env!("OUT_DIR"), "/page.oneview"))
    );
}
fn lifecycle() {
    let first = window();
    let second = window();
    let a = ViewMount::new(&first, None).unwrap();
    let b = ViewMount::new(&second, None).unwrap();
    let vm = Rc::new(RefCell::new(Vm::new()));
    let drops = vm.borrow().drops.clone();
    let mut va = View::instantiate(&a, page::spec(), vm.clone()).unwrap();
    let vb = View::instantiate(&b, page::spec(), vm.clone()).unwrap();
    assert_eq!(vm.borrow().connections, 2);
    vm.borrow_mut().title = "状态🙂".into();
    let na = va.notifier().unwrap();
    let nb = vb.notifier().unwrap();
    na.notify(page::TITLE).unwrap();
    nb.notify(page::TITLE).unwrap();
    va.flush().unwrap();
    vb.flush().unwrap();
    va.disconnect().unwrap();
    assert_eq!(na.notify(page::TITLE), Err(ViewError::CLOSED));
    assert_eq!(vm.borrow().connections, 1);
    drop(second); // window before view/mount
    assert_eq!(nb.notify(page::TITLE), Err(ViewError::CLOSED));
    assert_eq!(vm.borrow().connections, 0);
    drop(vm);
    drop(va);
    drop(vb);
    drop(b);
    drop(a);
    assert_eq!(drops.get(), 1);
}
fn panics() {
    for phase in [1, 2] {
        let window = window();
        let mount = ViewMount::new(&window, None).unwrap();
        let vm = Rc::new(RefCell::new(Vm::new()));
        vm.borrow_mut().panic = phase;
        assert!(matches!(
            View::instantiate(&mount, page::spec(), vm.clone()),
            Err(ViewError::FOREIGN_FAILURE)
        ));
        assert_eq!(vm.borrow().connections, 0);
        vm.borrow_mut().panic = 0;
        let view = View::instantiate(&mount, page::spec(), vm.clone()).unwrap();
        let borrowed = vm.borrow_mut();
        view.notifier().unwrap().notify(page::TITLE).unwrap();
        assert_eq!(view.flush(), Err(ViewError::REENTRANT));
        drop(borrowed);
        view.notifier().unwrap().notify(page::TITLE).unwrap();
        view.flush().unwrap();
    }
}
struct Content {
    label: oneui::Label,
    drops: Rc<Cell<usize>>,
}
impl Drop for Content {
    fn drop(&mut self) {
        self.drops.set(self.drops.get() + 1);
    }
}
fn host_owner() {
    let w = window();
    let mount = ViewMount::new(&w, None).unwrap();
    let view = View::instantiate(&mount, page::spec(), Rc::new(RefCell::new(Vm::new()))).unwrap();
    let drops = Rc::new(Cell::new(0));
    let content = Rc::new(Content {
        label: oneui::Label::new("content").unwrap(),
        drops: drops.clone(),
    });
    view.find_host("content")
        .unwrap()
        .attach(content.clone(), |c| c.label.as_widget())
        .unwrap();
    drop(content);
    assert_eq!(drops.get(), 0);
    drop(view);
    assert_eq!(drops.get(), 1);
}
fn cleanup_panics() {
    for panic in [3, 4] {
        let w = window();
        let mount = ViewMount::new(&w, None).unwrap();
        let vm = Rc::new(RefCell::new(Vm::new()));
        vm.borrow_mut().panic = panic;
        let drops = vm.borrow().drops.clone();
        let view = View::instantiate(&mount, page::spec(), vm.clone()).unwrap();
        let notifier = view.notifier().unwrap();
        drop(vm);
        drop(view);
        assert_eq!(drops.get(), 1);
        assert_eq!(notifier.notify(page::TITLE), Err(ViewError::CLOSED));
    }
}
fn list_identity() {
    let w = window();
    let mount = ViewMount::new(&w, None).unwrap();
    let vm = Rc::new(RefCell::new(Vm::new()));
    let view = View::instantiate(&mount, page::spec(), vm.clone()).unwrap();
    let notifier = view.notifier().unwrap();
    let row = vm.borrow().rows.get(0).unwrap().clone();
    assert_eq!(row.borrow().connections, 1);
    for _ in 0..50 {
        let delta = vm
            .borrow_mut()
            .rows
            .reset(vec![("a".into(), row.clone())])
            .unwrap();
        notifier.notify_list(page::ROWS, &delta).unwrap();
        view.flush().unwrap();
        assert_eq!(row.borrow().connections, 1);
    }
    let replacement = Rc::new(RefCell::new(Row::new("a")));
    let delta = vm
        .borrow_mut()
        .rows
        .reset(vec![("a".into(), replacement.clone())])
        .unwrap();
    notifier.notify_list(page::ROWS, &delta).unwrap();
    view.flush().unwrap();
    assert_eq!(row.borrow().connections, 0);
    assert_eq!(replacement.borrow().connections, 1);
    drop(view);
    assert_eq!(replacement.borrow().connections, 0);
}
fn reload_lifecycle() {
    let w = window();
    let mount = ViewMount::new(&w, None).unwrap();
    let vm = Rc::new(RefCell::new(Vm::new()));
    let mut view = View::instantiate(&mount, page::spec(), vm.clone()).unwrap();
    let notifier = view.notifier().unwrap();
    let row = vm.borrow().rows.get(0).unwrap().clone();
    for _ in 0..50 {
        view.reload(page::IR).unwrap();
        assert_eq!(vm.borrow().connections, 1);
        assert_eq!(row.borrow().connections, 1);
    }
    assert_eq!(view.reload(b"invalid"), Err(ViewError::BAD_IR));
    vm.borrow_mut().title = "after reload".into();
    notifier.notify(page::TITLE).unwrap();
    view.flush().unwrap();
    drop(view);
    assert_eq!(vm.borrow().connections, 0);
    assert_eq!(row.borrow().connections, 0);
}
