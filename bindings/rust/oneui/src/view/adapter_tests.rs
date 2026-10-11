use super::*;
use std::cell::Cell;

struct Vm {
    text: String,
    panic: bool,
    drops: Rc<Cell<usize>>,
    nested: Option<Rc<RefCell<Vm>>>,
}
impl Drop for Vm {
    fn drop(&mut self) {
        self.drops.set(self.drops.get() + 1);
    }
}
fn spec() -> ModelSpec<Vm> {
    ModelSpec {
        schema: [0; 32],
        model: 1,
        get: |vm, _| {
            assert!(!vm.panic, "getter panic");
            Ok(Value::Utf8(vm.text.clone()))
        },
        set: |vm, _, v, _| {
            assert!(!vm.panic, "setter panic");
            vm.text = v.text()?.to_owned();
            Ok(())
        },
        invoke: |vm, _, _, _| {
            if let Some(nested) = &vm.nested {
                nested.try_borrow_mut().map_err(|_| ViewError::REENTRANT)?;
            }
            assert!(!vm.panic, "command panic");
            Ok(())
        },
        list_info: |_, _| Err(ViewError::UNKNOWN_ID),
        list_key: |_, _, _| Err(ViewError::UNKNOWN_ID),
        list_item: |_, _, _| Err(ViewError::UNKNOWN_ID),
        connect: |vm, _| {
            assert!(!vm.panic, "connect panic");
            Ok(())
        },
        disconnect: |vm, _| {
            assert!(!vm.panic, "disconnect panic");
        },
    }
}
fn vm() -> Rc<RefCell<Vm>> {
    Rc::new(RefCell::new(Vm {
        text: "中🙂文".into(),
        panic: false,
        drops: Rc::new(Cell::new(0)),
        nested: None,
    }))
}
fn call_get(control: &Rc<model::Control>) -> (i32, sys::OneUiValue) {
    let reference = model::Control::borrowed(control);
    let mut out = sys::OneUiValue::default();
    let result = unsafe { control.table.get.unwrap()(reference.vm, 1, &mut out) };
    (result, out)
}
fn values_and_borrowing() {
    let vm = vm();
    let control = spec().bind(vm.clone()).0;
    let (code, out) = call_get(&control);
    assert_eq!(code, 0);
    vm.borrow_mut().text = "changed".into();
    // VM's string is not the returned scratch address.
    assert_eq!(
        unsafe { Value::copy(&out) }.unwrap(),
        Value::Utf8("中🙂文".into())
    );
    let borrow = vm.borrow_mut();
    let (code, out) = call_get(&control);
    assert_eq!(code, ViewError::REENTRANT.code());
    assert_eq!(out.tag, 0);
    drop(borrow);
    assert_eq!(call_get(&control).0, 0);
}
fn panics_and_zero_outputs() {
    let vm = vm();
    let control = spec().bind(vm.clone()).0;
    vm.borrow_mut().panic = true;
    let (code, out) = call_get(&control);
    assert_eq!(code, ViewError::FOREIGN_FAILURE.code());
    assert_eq!(out.tag, 0);
    let p = model::Control::borrowed(&control).vm;
    let value = Value::Utf8("set".into());
    assert_eq!(
        unsafe { control.table.set.unwrap()(p, 1, &value.borrow().unwrap()) },
        -13
    );
    assert_eq!(
        unsafe { control.table.invoke.unwrap()(p, 1, std::ptr::null(), 0) },
        -13
    );
    vm.borrow_mut().panic = false;
    assert_eq!(call_get(&control).0, 0);
}
fn callback_reentry() {
    let vm = vm();
    vm.borrow_mut().nested = Some(vm.clone());
    let control = spec().bind(vm.clone()).0;
    let p = model::Control::borrowed(&control).vm;
    assert_eq!(
        unsafe { control.table.invoke.unwrap()(p, 1, std::ptr::null(), 0) },
        -10
    );
    vm.borrow_mut().nested = None; // break deliberate test-only cycle
}
fn leases_release_once() {
    let vm = vm();
    let drops = vm.borrow().drops.clone();
    let control = spec().bind(vm.clone()).0;
    let p = model::Control::borrowed(&control).vm;
    let release = control.table.release.unwrap();
    unsafe { control.table.retain.unwrap()(p) };
    drop(control);
    drop(vm);
    assert_eq!(drops.get(), 0);
    unsafe { release(p) };
    assert_eq!(drops.get(), 1);
}
fn stable_list_deltas() {
    let mut list = StableList::new();
    let first = vm();
    let a = list.insert(0, "a", first.clone()).unwrap();
    assert_eq!((a.before, a.after, a.kind), (0, 1, 1));
    assert_eq!(list.insert(0, "a", vm()).unwrap_err(), ViewError::BAD_KEY);
    list.insert(1, "b", vm()).unwrap();
    list.move_item(0, 1).unwrap();
    assert!(Rc::ptr_eq(list.get(1).unwrap(), &first));
    let update = list.update(1, &[PropertyId::new(1)]).unwrap();
    assert_eq!(update.kind, 4);
    assert_eq!(update.properties, vec![1]);
    let revision = list.revision();
    for keys in [
        vec!["a"],
        vec!["a", "a"],
        vec!["a", "missing"],
        vec!["a", "b", "extra"],
    ] {
        assert_eq!(
            list.reorder(keys.into_iter().map(str::to_owned).collect())
                .unwrap_err(),
            ViewError::BAD_KEY
        );
        assert_eq!(list.revision(), revision);
        assert_eq!(list.key(0), Some("b"));
    }
    let change = list.reorder(vec!["a".into(), "b".into()]).unwrap();
    assert_eq!(change.kind, sys::ONEUI_LIST_REORDER);
    assert!(Rc::ptr_eq(list.get(0).unwrap(), &first));
    list.reorder(vec!["b".into(), "a".into()]).unwrap();
    list.remove(0).unwrap();
    assert!(Rc::ptr_eq(list.get(0).unwrap(), &first));
    let replacement = vm();
    list.reset(vec![("a".into(), replacement.clone())]).unwrap();
    assert!(Rc::ptr_eq(list.get(0).unwrap(), &replacement));
}
fn dispatcher_mount_checks_lifetime_and_thread() {
    use std::sync::{Arc, Mutex};
    let dispatcher = crate::UiDispatcher {
        state: Arc::new(super::super::dispatcher::WindowState {
            raw: Mutex::new(None),
            ui_thread: std::thread::current().id(),
        }),
    };
    assert!(matches!(
        ViewMount::from_dispatcher(&dispatcher, None),
        Err(ViewError::CLOSED)
    ));
    let result =
        std::thread::spawn(move || ViewMount::from_dispatcher(&dispatcher, None).err().unwrap())
            .join()
            .unwrap();
    assert_eq!(result, ViewError::WRONG_THREAD);
}

pub(crate) fn cases() -> Vec<(&'static str, fn())> {
    vec![
        ("view.values_and_borrowing", values_and_borrowing),
        ("view.panics_and_zero_outputs", panics_and_zero_outputs),
        ("view.callback_reentry", callback_reentry),
        ("view.leases_release_once", leases_release_once),
        ("view.stable_list_deltas", stable_list_deltas),
        (
            "view.dispatcher_mount_checks_lifetime_and_thread",
            dispatcher_mount_checks_lifetime_and_thread,
        ),
    ]
}
