//! Native root event injection, no OS input. The optional bridge belongs to this
//! executable; ordinary `cargo test` has no native bridge build prerequisite.
use crate::{page::WorkspaceCommand as C, App};
use std::{
    cell::RefCell,
    ffi::{c_void, CString},
    path::PathBuf,
    rc::Rc,
    time::Instant,
};
#[path = "acceptance_actions.rs"]
mod actions;
extern "C" {
    fn workspace_acceptance_start(
        root: *mut c_void,
        callback: extern "C" fn(f64, f64, f64, f64, f64, i32, i32),
    );
    fn workspace_acceptance_stop();
    pub fn workspace_acceptance_preview(root: *mut c_void);
    pub fn workspace_acceptance_phase() -> i32;
    pub fn workspace_acceptance_redraw();
    pub fn workspace_acceptance_rect(name: *const i8, kind: i32, rect: *mut f32) -> i32;
    pub fn workspace_acceptance_pointer(
        phase: i32,
        x: f32,
        y: f32,
        button: i32,
        alt: i32,
        clicks: i32,
    ) -> i32;
    pub fn workspace_acceptance_key(key: i32, control: i32, shift: i32) -> i32;
    pub fn workspace_acceptance_text(composition: i32);
    pub fn workspace_acceptance_address(widget: *mut c_void) -> usize;
}
thread_local! {static DRIVER:RefCell<Option<Rc<RefCell<Driver>>>>=const{RefCell::new(None)};}
pub struct Sample {
    scene: usize,
    cycle: usize,
    phase: i32,
    visible: i32,
    work: f64,
    present: f64,
    interval: f64,
    mutation: f64,
    update: f64,
    census: f64,
    frame: usize,
}
pub struct Driver {
    app: Rc<RefCell<App>>,
    samples: Vec<Sample>,
    scene: usize,
    cycle: usize,
    repeats: usize,
    frames: usize,
    queued: bool,
    first: bool,
    early: bool,
    acceptance: bool,
    previous: f64,
    mutation: f64,
    output: PathBuf,
    started: Instant,
    scene_started: Instant,
    baseline: Vec<(String, usize, usize)>,
}
pub fn start(app: Rc<RefCell<App>>, args: &[String]) {
    let flag = |s: &str| args.iter().any(|a| a == s);
    let value = |s: &str| {
        args.iter()
            .position(|a| a == s)
            .and_then(|i| args.get(i + 1))
    };
    let output = value("--output")
        .map(PathBuf::from)
        .unwrap_or_else(|| "workspace-frames.csv".into());
    let baseline = app
        .borrow()
        .content
        .sessions
        .iter()
        .map(|(k, v)| (k.clone(), v.address(), Rc::as_ptr(v) as usize))
        .collect();
    let driver = Rc::new(RefCell::new(Driver {
        app: app.clone(),
        samples: Vec::with_capacity(200_000),
        scene: 0,
        cycle: 0,
        repeats: value("--repeats")
            .map(|v| v.parse().unwrap())
            .unwrap_or(500),
        frames: 0,
        queued: false,
        first: flag("--first-split"),
        early: flag("--early"),
        acceptance: flag("--acceptance"),
        previous: 0.,
        mutation: 0.,
        output,
        started: Instant::now(),
        scene_started: Instant::now(),
        baseline,
    }));
    DRIVER.with(|d| *d.borrow_mut() = Some(driver));
    unsafe {
        app.borrow()
            .root
            .as_widget()
            .with_native_handle(|p| workspace_acceptance_start(p.cast(), frame));
    }
}
pub fn stop() {
    unsafe {
        workspace_acceptance_stop();
    }
    DRIVER.with(|d| d.borrow_mut().take());
}
extern "C" fn frame(
    now: f64,
    work: f64,
    present: f64,
    update: f64,
    census: f64,
    phase: i32,
    visible: i32,
) {
    let result = std::panic::catch_unwind(|| {
        DRIVER.with(|slot| {
            let Some(driver) = slot.borrow().clone() else {
                return;
            };
            let mut d = driver.borrow_mut();
            d.frames += 1;
            if std::env::var_os("ONEUI_WORKSPACE_TRACE").is_some() {
                eprintln!(
                    "frame={} scene={} phase={} visible={}",
                    d.frames, d.scene, phase, visible
                );
            }
            if d.scene > 0 {
                let sample = Sample {
                    scene: d.scene,
                    cycle: d.cycle,
                    phase,
                    visible,
                    work: work + d.mutation,
                    present,
                    interval: if d.previous > 0. {
                        now - d.previous
                    } else {
                        0.
                    },
                    mutation: d.mutation,
                    update,
                    census,
                    frame: d.frames,
                };
                d.samples.push(sample);
            }
            d.mutation = 0.;
            d.previous = now;
            if d.queued {
                return;
            }
            let ready = if d.scene == 0 {
                d.frames >= if d.early { 1 } else { 16 }
            } else {
                phase == 0 && (d.scene != 13 || d.scene_started.elapsed().as_millis() >= 200)
            };
            let dispatcher = d.app.borrow().dispatcher.clone();
            if ready {
                d.queued = true;
                drop(d);
                dispatcher
                    .dispatch_local(move || {
                        if std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
                            advance(driver)
                        }))
                        .is_err()
                        {
                            std::process::exit(2);
                        }
                    })
                    .unwrap();
            } else if d.scene == 0 || d.scene == 13 {
                drop(d);
                dispatcher
                    .request_animation_frame_local(move |_| unsafe {
                        workspace_acceptance_redraw();
                    })
                    .unwrap();
            }
        })
    });
    if result.is_err() {
        eprintln!("acceptance frame panicked");
        std::process::exit(2);
    }
}
fn advance(driver: Rc<RefCell<Driver>>) {
    let mut d = driver.borrow_mut();
    d.queued = false;
    // Drain transitionSettled removal before the next command can reuse its key.
    d.app.borrow_mut().flush();
    if d.acceptance {
        if d.scene == 0 {
            click("four");
            d.app.borrow_mut().flush();
            d.scene = 1;
            return;
        }
        actions::run(&mut d);
        d.queued = true;
        let next = driver.clone();
        let dispatcher = d.app.borrow().dispatcher.clone();
        drop(d);
        dispatcher
            .dispatch_local(move || {
                let mut d = next.borrow_mut();
                if !d
                    .app
                    .borrow()
                    .vm
                    .borrow()
                    .broadcast_state
                    .received
                    .values()
                    .any(|v| v.contains("echo local"))
                {
                    eprintln!("FAIL root terminal text callback");
                    std::process::exit(2);
                }
                d.finish();
            })
            .unwrap();
        return;
    }
    if d.first && d.scene == 1 {
        d.finish();
        return;
    }
    if d.scene == 18 {
        d.cycle += 1;
        if d.cycle % 10 == 0 {
            println!("completed {} cycles", d.cycle);
        }
        if d.cycle == d.repeats {
            d.finish();
            return;
        }
        d.scene = 0;
    }
    d.scene += 1;
    let start = Instant::now();
    d.scene_started = start;
    if std::env::var_os("ONEUI_WORKSPACE_TRACE").is_some() {
        eprintln!("advance scene={}", d.scene);
    }
    let app = d.app.clone();
    match d.scene {
        1 => {
            if d.cycle > 0 {
                let mut a = app.borrow_mut();
                {
                    let mut m = a.vm.borrow_mut();
                    m.motion = false;
                    m.preset(1).unwrap();
                }
                a.flush();
                a.vm.borrow_mut().motion = true;
            }
            click("two");
        }
        2 => click("four"),
        3 => {
            app.borrow().vm.borrow_mut().preset(8).unwrap();
        }
        4 => click("sixteen"),
        5 => {
            let a = app.borrow();
            let mut m = a.vm.borrow_mut();
            for n in &mut m.forest.nodes {
                if n.kind == "split" {
                    n.ratio = 0.4;
                }
            }
            m.revision += 1;
            m.publish().unwrap();
        }
        6 => click("balance"),
        7 => {
            let r = rect("session/0", 1);
            pointer(0, r[0] + 50., r[1] + 14., 2);
            pointer(2, r[0] + 50., r[1] + 14., 2);
        }
        8 => click("restore"),
        9 => {
            let r = rect("session/15", 1);
            pointer(0, r[0] + r[2] - 16., r[1] + 14., 1);
            pointer(2, r[0] + r[2] - 16., r[1] + 14., 1);
        }
        10 => click("sixteen"),
        11 => {
            let a = app.borrow();
            let mut m = a.vm.borrow_mut();
            m.forest.tab("session/0");
            m.revision += 1;
            m.publish().unwrap();
        }
        12 => {
            let a = app.borrow();
            let mut m = a.vm.borrow_mut();
            m.forest.split("session/0", "session/1", "left");
            m.revision += 1;
            m.publish().unwrap();
        }
        13 => unsafe {
            app.borrow().root.as_widget().with_native_handle(|p| {
                workspace_acceptance_preview(p.cast());
            });
        },
        14 => {
            let r = rect("session/1", 2);
            pointer(2, r[0] + r[2] * 0.15, r[1] + r[3] * 0.5, 1);
        }
        15..=18 => {
            let a = app.borrow();
            let mut m = a.vm.borrow_mut();
            m.settings = d.scene == 15;
            m.compact = if d.scene <= 16 {
                "sftp"
            } else if d.scene == 17 {
                "monitor"
            } else {
                "terminal"
            }
            .into();
            m.publish_tools().unwrap();
            m.publish().unwrap();
        }
        _ => unreachable!(),
    }
    app.borrow_mut().flush();
    d.mutation = start.elapsed().as_secs_f64() * 1000.;
    // A no-op scene still needs one measured frame to expose the condition.
    unsafe {
        workspace_acceptance_redraw();
    }
}
impl Driver {
    fn finish(&mut self) {
        use std::io::Write;
        let mut out = std::io::BufWriter::new(std::fs::File::create(&self.output).unwrap());
        writeln!(
            out,
            "scene,cycle,phase,visible,work_submit_ms,present_ms,interval_ms,mutation_ms,native_update_ms,census_ms,frame"
        )
        .unwrap();
        for s in &self.samples {
            writeln!(
                out,
                "{},{},{},{},{:.6},{:.6},{:.6},{:.6},{:.6},{:.6},{}",
                s.scene,
                s.cycle,
                s.phase,
                s.visible,
                s.work,
                s.present,
                s.interval,
                s.mutation,
                s.update,
                s.census,
                s.frame
            )
            .unwrap();
        }
        for scene in 1..=18 {
            let mut values: Vec<_> = self
                .samples
                .iter()
                .filter(|s| s.scene == scene && s.phase != 2)
                .map(|s| s.work)
                .collect();
            values.sort_by(f64::total_cmp);
            if !values.is_empty() {
                println!(
                    "scene={scene} frames={} max={:.4} p95={:.4} slow={}",
                    values.len(),
                    values.last().unwrap(),
                    values[(values.len() - 1) * 95 / 100],
                    values.iter().filter(|v| **v > 16.).count()
                );
            }
        }
        println!(
            "elapsed_s={:.2} first={} early={} cycles={}",
            self.started.elapsed().as_secs_f64(),
            self.first,
            self.early,
            self.cycle
        );
        self.app.borrow().dispatcher.close().unwrap();
    }
}
pub fn rect(name: &str, kind: i32) -> [f32; 4] {
    let mut r = [0.; 4];
    let n = CString::new(name).unwrap();
    assert_ne!(
        unsafe { workspace_acceptance_rect(n.as_ptr(), kind, r.as_mut_ptr()) },
        0,
        "missing rect {name}"
    );
    r
}
pub fn pointer(phase: i32, x: f32, y: f32, clicks: i32) {
    unsafe {
        workspace_acceptance_pointer(phase, x, y, 1, 0, clicks);
    }
}
pub fn click(name: &str) {
    let r = rect(name, 0);
    pointer(0, r[0] + r[2] * 0.5, r[1] + r[3] * 0.5, 1);
    pointer(2, r[0] + r[2] * 0.5, r[1] + r[3] * 0.5, 1);
}
