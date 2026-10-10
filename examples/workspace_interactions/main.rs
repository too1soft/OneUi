#![allow(non_snake_case, non_camel_case_types, unused_imports, dead_code)]
mod page {
    include!(concat!(env!("OUT_DIR"), "/workspace.rs"));
}
#[cfg(feature = "acceptance")]
mod acceptance;
mod broadcast;
mod commands;
mod content;
mod delta;
mod fixtures;
mod forest;
mod publish;
mod rows;
#[cfg(test)]
mod tests;
mod tools;
mod vm;
use oneui::{
    view::{View, ViewMount},
    Panel, UiDispatcher, Window, WindowOptions,
};
use std::{
    cell::RefCell,
    rc::Rc,
    time::{Duration, Instant},
};
pub struct App {
    pub vm: Rc<RefCell<vm::Model>>,
    pub view: View,
    pub content: content::Content,
    pub dispatcher: UiDispatcher,
    pub root: Panel,
    pub mount: ViewMount,
    pub started: Instant,
    pub drag_preview: bool,
    pub preview_started: bool,
}
impl App {
    pub fn flush(&mut self) {
        self.view.flush().unwrap_or_else(|e| {
            panic!(
                "flush {e}: {:?} / {:?}",
                self.view.last_error(),
                oneui::last_native_error()
            )
        });
        self.content
            .sync(&self.view, &self.vm, &self.dispatcher)
            .unwrap();
    }
}
fn pulse(app: Rc<RefCell<App>>, capture: Option<std::path::PathBuf>, smoke: bool) {
    let d = app.borrow().dispatcher.clone();
    d.request_animation_frame_local(move |_| {
        let mut a = app.borrow_mut();
        a.flush();
        let elapsed = a.started.elapsed();
        a.content.render(&a.vm.borrow(), elapsed.as_secs());
        #[cfg(feature = "acceptance")]
        if a.drag_preview && !a.preview_started && elapsed > Duration::from_millis(500) {
            unsafe {
                a.root.as_widget().with_native_handle(|p| {
                    acceptance::workspace_acceptance_preview(p.cast());
                });
            }
            a.preview_started = true;
        }
        if elapsed > Duration::from_millis(900) && (capture.is_some() || smoke) {
            if let Some(path) = capture {
                assert!(a.dispatcher.capture_frame_png(&path));
                std::fs::write(
                    path.with_extension("json"),
                    a.dispatcher.layout_snapshot_json().unwrap(),
                )
                .unwrap();
            }
            a.dispatcher.close().unwrap();
            return;
        }
        drop(a);
        pulse(app, capture, smoke);
    })
    .unwrap();
}
fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<_> = std::env::args().collect();
    let flag = |s: &str| args.iter().any(|a| a == s);
    let value = |s: &str| {
        args.iter()
            .position(|a| a == s)
            .and_then(|i| args.get(i + 1))
            .cloned()
    };
    let window = Window::new(&WindowOptions {
        title: "OneUI · 终端工作区".into(),
        width: value("--width")
            .map(|v| v.parse().unwrap())
            .unwrap_or(if flag("--narrow") { 880 } else { 1600 }),
        height: value("--height")
            .map(|v| v.parse().unwrap())
            .unwrap_or(1000),
        ..Default::default()
    })
    .map_err(|e| format!("{e:?}"))?;
    let root = Panel::new().unwrap();
    window.set_content(root.as_widget());
    let mount = ViewMount::new(&window, Some(root.as_widget()))?;
    mount
        .set_styles(&fixtures::css(flag("--dark")), false)
        .map_err(|e| format!("styles {e}: {:?}", oneui::last_native_error()))?;
    let vm = Rc::new(RefCell::new(vm::Model::default()));
    if let Some(count) = value("--panes") {
        vm.borrow_mut().preset(count.parse()?)?;
    }
    vm.borrow_mut().settings = flag("--settings");
    vm.borrow_mut().showPickers = !flag("--gallery");
    if let Some(path) = value("--media") {
        vm.borrow_mut().asset = path;
    }
    if flag("--floating") {
        vm.borrow_mut().command(page::WorkspaceCommand::ToolFloat)?;
    }
    if flag("--terminal-only") {
        vm.borrow_mut().compact = "terminal".into();
    }
    if let Some(tool) = value("--tool") {
        vm.borrow_mut().compact = tool;
    }
    let view = View::instantiate(&mount, page::spec(), vm.clone())
        .map_err(|e| format!("instantiate {e}: {:?}", oneui::last_native_error()))?;
    let dispatcher = window.dispatcher();
    let app = Rc::new(RefCell::new(App {
        vm,
        view,
        content: Default::default(),
        dispatcher,
        root,
        mount,
        started: Instant::now(),
        drag_preview: flag("--drag-preview"),
        preview_started: false,
    }));
    app.borrow_mut().flush();
    window.show();
    #[cfg(feature = "acceptance")]
    if flag("--acceptance") || flag("--perf") || flag("--first-split") {
        acceptance::start(app.clone(), &args);
    } else {
        pulse(
            app.clone(),
            value("--capture").map(Into::into),
            flag("--smoke"),
        );
    }
    #[cfg(not(feature = "acceptance"))]
    pulse(
        app.clone(),
        value("--capture").map(Into::into),
        flag("--smoke"),
    );
    let code = window.run();
    #[cfg(feature = "acceptance")]
    acceptance::stop();
    let counts = app.borrow().content.counts.clone();
    drop(app);
    println!(
        "content created={} attached={} dropped={} viewport={}",
        counts.created.get(),
        counts.attached.get(),
        counts.dropped.get(),
        counts.viewport.get()
    );
    if code != 0 {
        return Err(format!("window exit {code}").into());
    }
    Ok(())
}
