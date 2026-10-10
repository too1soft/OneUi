use crate::{
    model::{page, Vm},
    window,
};
use oneui::view::{View, ViewMount, ViewReloadWatcher};
use std::{
    cell::{Cell, RefCell},
    fs,
    path::PathBuf,
    rc::Rc,
    time::Duration,
};
struct Fixture(PathBuf);
impl Drop for Fixture {
    fn drop(&mut self) {
        fs::remove_dir_all(&self.0).expect("watcher fixture cleanup");
    }
}
pub fn run() {
    let base = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../../build/capture/viewc-tests");
    fs::create_dir_all(&base).unwrap();
    let fixture = Fixture(base.join(format!("rust-watcher-{}", std::process::id())));
    fs::create_dir(&fixture.0).unwrap();
    let file = fixture.0.join("page.oneview");
    fs::write(&file, page::IR).unwrap();
    let window = window();
    let mount = ViewMount::new(&window, None).unwrap();
    let vm = Rc::new(RefCell::new(Vm::new()));
    let view = Rc::new(RefCell::new(
        View::instantiate(&mount, page::spec(), vm.clone()).unwrap(),
    ));
    let count = Rc::new(Cell::new(0));
    let observed = count.clone();
    let dispatcher = window.dispatcher();
    let close = dispatcher.clone();
    let watcher = ViewReloadWatcher::new(
        &dispatcher,
        Rc::downgrade(&view),
        file.clone(),
        move |result| {
            result.unwrap();
            observed.set(observed.get() + 1);
            close.close().unwrap();
        },
    )
    .unwrap();
    let writer = std::thread::spawn(move || {
        std::thread::sleep(Duration::from_millis(150));
        // Partial compiler output is never delivered until the burst settles.
        for _ in 0..50 {
            fs::write(&file, b"incomplete").unwrap();
            std::thread::sleep(Duration::from_millis(5));
        }
        fs::write(file, page::IR).unwrap();
    });
    let (stop, timed) = std::sync::mpsc::channel();
    let timeout = std::thread::spawn(move || {
        if timed.recv_timeout(Duration::from_secs(5)).is_err() {
            let _ = dispatcher.close();
        }
    });
    window.show();
    window.run();
    writer.join().unwrap();
    stop.send(()).unwrap();
    timeout.join().unwrap();
    drop(watcher);
    assert_eq!(
        count.get(),
        1,
        "file writes should debounce into one valid reload"
    );
    drop(view);
    assert_eq!(vm.borrow().connections, 0);
}
