#![allow(dead_code, non_camel_case_types, unused_imports)]
mod page {
    include!(concat!(env!("OUT_DIR"), "/settings.rs"));
    pub fn probe_spec<V: SettingsBasicVm>() -> oneui::view::ViewSpec<V> {
        oneui::view::ViewSpec::from_generated(
            include_bytes!(concat!(env!("OUT_DIR"), "/SettingsProbe.oneview")),
            model_SettingsBasic::<V>(),
        )
    }
}
mod bench;
mod presentation;
#[cfg(test)]
mod tests;
mod themes;
mod vm;
use oneui::view::{View, ViewMount, VmContext};
use oneui::{Window, WindowOptions};
use page::SettingsBasicVm;
use std::{cell::RefCell, rc::Rc};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<_> = std::env::args().collect();
    if let Some(i) = args.iter().position(|a| a == "--bench") {
        let count = std::env::var("ONEUI_BENCH_SAMPLES")
            .ok()
            .map(|s| s.parse().unwrap())
            .unwrap_or(1000);
        let rounds = std::env::var("ONEUI_BENCH_ROUNDS")
            .ok()
            .map(|s| s.parse().unwrap())
            .unwrap_or(5);
        return bench::run(std::path::Path::new(&args[i + 1]), count, rounds);
    }
    let smoke = args.iter().any(|v| v == "--smoke");
    let capture = args
        .iter()
        .position(|v| v == "--capture")
        .map(|i| std::path::PathBuf::from(&args[i + 1]));
    let window = Window::new(&WindowOptions {
        title: "OneUI · 设置 → 基础".into(),
        width: if args.iter().any(|a| a == "--narrow") {
            560
        } else {
            1040
        },
        height: 850,
        ..WindowOptions::default()
    })
    .map_err(|e| format!("{e:?}"))?;
    let mount = Rc::new(ViewMount::new(&window, None)?);
    let vm = Rc::new(RefCell::new(vm::Settings::default()));
    if args.iter().any(|a| a == "--dark") {
        vm.borrow().theme.selected.set(6);
    }
    mount.set_styles(&presentation::css(vm.borrow().theme.selected.get()), false)?;
    let view = Rc::new(RefCell::new(View::instantiate(
        &mount,
        page::spec(),
        vm.clone(),
    )?));
    let dispatcher = window.dispatcher();
    presentation::connect(&vm, &mount, &dispatcher);
    let _watcher = if let Some(i) = args.iter().position(|a| a == "--watch-ir") {
        Some(
            oneui::view::ViewReloadWatcher::new(
                &dispatcher,
                Rc::downgrade(&view),
                std::path::PathBuf::from(&args[i + 1]),
                |result| eprintln!("IR reload: {result:?}"),
            )
            .map_err(|e| format!("{e:?}"))?,
        )
    } else {
        None
    };
    let weak = Rc::downgrade(&vm);
    let close = dispatcher.clone();
    let (result, _subscription) = dispatcher
        .latest_signal(move |completion: vm::Completion| {
            if let Some(vm) = weak.upgrade() {
                vm.borrow_mut()
                    .complete(completion)
                    .expect("async completion");
                if smoke {
                    assert_eq!(vm.borrow().status, "设置已保存");
                    close.close().unwrap();
                }
            }
        })
        .map_err(|e| format!("{e:?}"))?;
    vm.borrow_mut().start = Some(Box::new(move |request| {
        let result = result.clone();
        std::thread::spawn(move || {
            let _ = result.send(request.perform());
        });
    }));
    window.show();
    if smoke {
        let weak = Rc::downgrade(&vm);
        dispatcher
            .dispatch_local(move || {
                weak.upgrade()
                    .unwrap()
                    .borrow_mut()
                    .invoke(page::SettingsBasicCommand::Save, &VmContext::default())
                    .unwrap();
            })
            .map_err(|e| format!("{e:?}"))?;
    }
    if let Some(path) = capture {
        let capture_dispatcher = dispatcher.clone();
        dispatcher
            .request_animation_frame_local(move |_| {
                assert!(
                    capture_dispatcher.capture_frame_png(&path),
                    "capture failed"
                );
                capture_dispatcher.close().unwrap();
            })
            .map_err(|e| format!("{e:?}"))?;
    }
    let code = window.run();
    drop(view);
    if smoke {
        println!("PASS settings: pure Rust VM, generated IR, worker result, view/window teardown");
    }
    if code != 0 {
        return Err(format!("window exited {code}").into());
    }
    Ok(())
}
