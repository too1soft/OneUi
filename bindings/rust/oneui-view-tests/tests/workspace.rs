use crate::{model::*, Content};
use oneui::view::*;
use std::{
    cell::{Cell, RefCell},
    rc::Rc,
};

pub fn run() {
    let w = crate::window();
    let mount = ViewMount::new(&w, None).unwrap();
    let vm = Rc::new(RefCell::new(Vm::new()));
    let row = vm.borrow().rows.get(0).unwrap().clone();
    let mut view = View::instantiate(&mount, page::spec(), vm.clone()).unwrap();
    view.reload(include_bytes!(concat!(
        env!("OUT_DIR"),
        "/workspace.oneview"
    )))
    .unwrap();
    let path = [ScopeSegment {
        instance_id: "panes",
        key: "a",
    }];
    let scope = view.resolve_scope(&path).unwrap();
    assert_eq!(row.borrow().connections, 2); // Data lease + render connection.
    let drops = Rc::new(Cell::new(0));
    let content = Rc::new(Content {
        label: oneui::Label::new("persistent").unwrap(),
        drops: drops.clone(),
    });
    let host = view.find_host_in(&scope, "terminal").unwrap();
    host.attach(content.clone(), |c| c.label.as_widget())
        .unwrap();
    drop(content);
    for _ in 0..50 {
        let delta = vm.borrow_mut().rows.move_item(0, 0).unwrap();
        view.notifier()
            .unwrap()
            .notify_list(page::ROWS, &delta)
            .unwrap();
        view.flush().unwrap();
        assert_eq!(drops.get(), 0);
        assert_eq!(row.borrow().connections, 2);
    }
    let delta = vm.borrow_mut().rows.remove(0).unwrap();
    view.notifier()
        .unwrap()
        .notify_list(page::ROWS, &delta)
        .unwrap();
    view.flush().unwrap();
    assert_eq!(drops.get(), 1);
    assert_eq!(host.detach(), Err(ViewError::CLOSED));
    assert_eq!(scope.notify(PropertyId::ALL), Err(ViewError::CLOSED));
    assert!(matches!(
        view.resolve_scope(&path),
        Err(ViewError::NOT_FOUND)
    ));
    assert_eq!(row.borrow().connections, 0);
}
