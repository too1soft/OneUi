mod diagnostics;
mod manual;
mod paint_probe;
mod table;
mod table_vm;
use crate::{page, vm::Settings};
use oneui::view::{View, ViewMount, ViewNotifier};
use oneui::{Window, WindowOptions};
use std::{
    cell::{Cell, RefCell},
    fs,
    io::Write,
    path::Path,
    rc::Rc,
    time::Instant,
};

#[derive(Clone)]
enum Page {
    Ir(Rc<View>, ViewNotifier),
    Manual(Rc<manual::Manual>),
}
impl Page {
    fn update(&self, vm: &Rc<RefCell<Settings>>, case: &str, index: usize) {
        match case {
            "scalar" => {
                vm.borrow_mut().compact = index % 2 == 0;
                match self {
                    Self::Ir(_, n) => n.notify(page::COMPACT).unwrap(),
                    Self::Manual(v) => v.scalar(&vm.borrow()),
                }
            }
            "text" | "dispatcher" => {
                vm.borrow_mut().status = format!("value {:04}", index % 2);
                match self {
                    Self::Ir(_, n) => n.notify(page::STATUS).unwrap(),
                    Self::Manual(v) => v.text(&vm.borrow()),
                }
            }
            "themes12" => {
                vm.borrow().theme.select(index % 12).unwrap();
                if let Self::Manual(v) = self {
                    v.theme(&vm.borrow());
                }
            }
            _ => unreachable!(),
        }
    }
}

/// Small equivalent probes deliberately avoid hiding binding overhead behind
/// the full settings page's scroll/layout cost (design D5).
pub fn run(
    directory: &Path,
    count: usize,
    rounds: usize,
) -> Result<(), Box<dyn std::error::Error>> {
    fs::create_dir_all(directory)?;
    diagnostics::disable_motion();
    #[cfg(feature = "bench-diagnostics")]
    fs::write(directory.join("callback-counters.txt"), "")?;
    if std::env::var_os("ONEUI_PROFILE_TABLE_ONLY").is_some() { return table::run(directory, count, rounds); }
    let mut raw =
        std::io::BufWriter::new(fs::File::create(directory.join("settings-samples.csv"))?);
    writeln!(raw, "round,path,case,sample,elapsed_ns")?;
    let mut meta = std::io::BufWriter::new(fs::File::create(directory.join("settings-runs.csv"))?);
    writeln!(
        meta,
        "round,path,case,construction_ns,cold_paint_ns,paints,vm_strong_after_close"
    )?;
    for round in 0..rounds {
        for case in ["ui_event", "dispatcher", "scalar", "text", "themes12"] {
            for slot in 0..2 {
                let ir = (round + slot) % 2 == 0;
                let name = if ir { "ir" } else { "manual" };
                diagnostics::reset();
                std::env::set_var("ONEUI_PROFILE_CASE", format!("{name}-{}", case));
                let start = Instant::now();
                let window = Window::new(&WindowOptions {
                    title: format!("OneUI paired {name} {case}"),
                    width: 640,
                    height: 600,
                    ..WindowOptions::default()
                })
                .map_err(|e| format!("{e:?}"))?;
                let mount = Rc::new(ViewMount::new(&window, None)?);
                mount.set_styles(manual::CSS, false)?;
                let vm = Rc::new(RefCell::new(Settings::default()));
                let page = if ir {
                    let v = Rc::new(View::instantiate(&mount, page::probe_spec(), vm.clone())?);
                    let n = v.notifier()?;
                    Page::Ir(v, n)
                } else {
                    let v = Rc::new(manual::Manual::new(&vm));
                    window.set_content(v.root.as_widget());
                    Page::Manual(v)
                };
                let construction = start.elapsed().as_nanos();
                let dispatcher = window.dispatcher();
                let ready = Rc::new(Cell::new(false));
                let marker = ready.clone();
                let state = vm.clone();
                let engine = page.clone();
                let hwnd = unsafe { window.with_platform_handle(|h| h as usize).unwrap() };
                let snapshot = Rc::new(RefCell::new(None));
                let save_snapshot = snapshot.clone();
                let warmed = directory.join(format!("settings-{name}-{case}-warmup.json"));
                let samples = paint_probe::run(&window, count + 50, ready, move |index| {
                    if index == 49 && round == 0 {
                        fs::write(&warmed, dispatcher.layout_snapshot_json().unwrap()).unwrap();
                    }
                    if index == 0 {
                        *save_snapshot.borrow_mut() =
                            Some(dispatcher.layout_snapshot_json().unwrap());
                        if case == "ui_event" {
                            paint_probe::focus_first(hwnd);
                        }
                    }
                    if case == "dispatcher" {
                        let state = state.clone();
                        let engine = engine.clone();
                        let marker = marker.clone();
                        dispatcher
                            .dispatch_local(move || {
                                engine.update(&state, case, index);
                                marker.set(true);
                            })
                            .unwrap();
                    } else if case == "ui_event" {
                        let before = state.borrow().font_family.clone();
                        paint_probe::character(
                            hwnd,
                            if index % 2 == 0 { b'x' as usize } else { 8 },
                        );
                        assert_ne!(
                            state.borrow().font_family,
                            before,
                            "input event did not reach VM"
                        );
                        marker.set(true);
                    } else {
                        engine.update(&state, case, index);
                        marker.set(true);
                    }
                });
                for (i, value) in samples.ns.iter().skip(50).enumerate() {
                    writeln!(raw, "{round},{name},{case},{i},{value}")?;
                }
                if round == 0 {
                    fs::write(
                        directory.join(format!("settings-{name}-{case}.json")),
                        snapshot.borrow().as_ref().unwrap(),
                    )?;
                }
                drop(page);
                drop(mount);
                diagnostics::save(directory, name, case);
                writeln!(
                    meta,
                    "{round},{name},{case},{construction},{},{},{}",
                    samples.cold_paint_ns,
                    samples.paints,
                    Rc::strong_count(&vm)
                )?;
                raw.flush()?;
                meta.flush()?;
                println!(
                    "benchmark round {round} {name} {case}: {} samples, {} paints",
                    samples.ns.len() - 50,
                    samples.paints
                );
            }
        }
    }
    table::run(directory, count, rounds)
}
