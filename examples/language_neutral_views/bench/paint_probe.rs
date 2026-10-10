//! Win32 paint-end marker: t1 is after OneUI's WM_PAINT handler (including
//! backend submission) returns. It is not a presentation-completion timestamp.
use oneui::Window;
use std::{
    cell::{Cell, RefCell},
    ffi::c_void,
    ptr,
    rc::Rc,
    time::Instant,
};
type Hwnd = *mut c_void;
type Subclass = unsafe extern "system" fn(Hwnd, u32, usize, isize, usize, usize) -> isize;
#[link(name = "comctl32")]
extern "system" {
    fn SetWindowSubclass(hwnd: Hwnd, callback: Subclass, id: usize, data: usize) -> i32;
    fn RemoveWindowSubclass(hwnd: Hwnd, callback: Subclass, id: usize) -> i32;
    fn DefSubclassProc(hwnd: Hwnd, message: u32, wparam: usize, lparam: isize) -> isize;
}
#[link(name = "user32")]
extern "system" {
    fn PostMessageW(hwnd: Hwnd, message: u32, wparam: usize, lparam: isize) -> i32;
    fn SendMessageW(hwnd: Hwnd, message: u32, wparam: usize, lparam: isize) -> isize;
    fn InvalidateRect(hwnd: Hwnd, rect: *const c_void, erase: i32) -> i32;
    fn SetFocus(hwnd: Hwnd) -> Hwnd;
    fn SetTimer(hwnd: Hwnd, id: usize, ms: u32, callback: *const c_void) -> usize;
    fn KillTimer(hwnd: Hwnd, id: usize) -> i32;
}
const STEP: u32 = 0x8000 + 501;
pub struct Samples {
    pub ns: Vec<u128>,
    pub paints: u64,
    pub cold_paint_ns: u128,
    pub error: Option<String>,
}
struct Probe {
    action: RefCell<Box<dyn FnMut(usize)>>,
    ready: Rc<Cell<bool>>,
    pending: Cell<Option<Instant>>,
    started: Instant,
    steps: usize,
    current: Cell<usize>,
    result: RefCell<Samples>,
    begun: Cell<bool>,
    idle: Cell<bool>,
    idle_paints: Cell<u64>,
}
unsafe extern "system" fn procedure(
    hwnd: Hwnd,
    message: u32,
    wparam: usize,
    lparam: isize,
    _: usize,
    data: usize,
) -> isize {
    // The Box stays alive until run() removes the subclass. Never unwind FFI.
    let outcome = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        // DefSubclassProc and SendMessage can synchronously reenter this
        // procedure. Keep no exclusive Probe borrow across either call.
        let p = &*(data as *const Probe);
        if message == 0x113 && wparam == 502 {
            KillTimer(hwnd, 502);
            println!("idle diagnostic (500 ms): {} paints", p.idle_paints.get());
            PostMessageW(hwnd, 0x10, 0, 0);
            return 0;
        }
        if message == STEP {
            if p.current.get() == p.steps {
                if cfg!(feature = "bench-diagnostics") {
                    p.idle.set(true);
                    SetTimer(hwnd, 502, 500, ptr::null());
                } else {
                    PostMessageW(hwnd, 0x10, 0, 0);
                }
                return 0;
            }
            p.ready.set(false);
            p.pending.set(Some(Instant::now()));
            (p.action.borrow_mut())(p.current.get());
            InvalidateRect(hwnd, ptr::null(), 0);
            return 0;
        }
        let result = DefSubclassProc(hwnd, message, wparam, lparam);
        if message == 0x0f {
            if p.idle.get() {
                p.idle_paints.set(p.idle_paints.get() + 1);
            }
            p.result.borrow_mut().paints += 1;
            if !p.begun.replace(true) {
                p.result.borrow_mut().cold_paint_ns = p.started.elapsed().as_nanos();
                PostMessageW(hwnd, STEP, 0, 0);
            } else if p.ready.get() {
                if let Some(start) = p.pending.take() {
                    p.result.borrow_mut().ns.push(start.elapsed().as_nanos());
                    p.current.set(p.current.get() + 1);
                    PostMessageW(hwnd, STEP, 0, 0);
                }
            }
        }
        result
    }));
    match outcome {
        Ok(result) => result,
        Err(_) => {
            (*(data as *const Probe)).result.borrow_mut().error =
                Some("paint probe action/sequence assertion failed".into());
            PostMessageW(hwnd, 0x10, 0, 0);
            0
        }
    }
}
pub fn run(
    window: &Window,
    steps: usize,
    ready: Rc<Cell<bool>>,
    action: impl FnMut(usize) + 'static,
) -> Samples {
    let probe = Box::new(Probe {
        action: RefCell::new(Box::new(action)),
        ready,
        pending: Cell::new(None),
        started: Instant::now(),
        steps,
        current: Cell::new(0),
        result: RefCell::new(Samples {
            ns: Vec::with_capacity(steps),
            paints: 0,
            cold_paint_ns: 0,
            error: None,
        }),
        begun: Cell::new(false),
        idle: Cell::new(false),
        idle_paints: Cell::new(0),
    });
    let hwnd = unsafe { window.with_platform_handle(|h| h).unwrap() };
    assert_ne!(
        unsafe { SetWindowSubclass(hwnd, procedure, 501, (&*probe as *const Probe) as usize) },
        0
    );
    window.show();
    window.run();
    unsafe {
        RemoveWindowSubclass(hwnd, procedure, 501);
    }
    assert_eq!(
        probe.result.borrow().ns.len(),
        steps,
        "incomplete paint samples: {:?}",
        probe.result.borrow().error
    );
    probe.result.into_inner()
}
pub fn focus_first(hwnd: usize) {
    unsafe {
        SetFocus(hwnd as Hwnd);
        SendMessageW(hwnd as Hwnd, 0x201, 1, (15 << 16) | 20);
        SendMessageW(hwnd as Hwnd, 0x202, 0, (15 << 16) | 20);
    }
}
pub fn character(hwnd: usize, value: usize) {
    unsafe {
        SendMessageW(
            hwnd as Hwnd,
            if value == 8 { 0x100 } else { 0x102 },
            value,
            0,
        );
    }
}
