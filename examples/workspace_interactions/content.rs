//! Native content only. All controls, chrome and workspace layout live in .one.
use crate::vm::Model;
use oneui::{
    view::{ScopeSegment, View, VmResult},
    TerminalCell, TerminalColor, TerminalView, UiDispatcher, Widget,
};
use std::{
    cell::{Cell, RefCell},
    collections::BTreeMap,
    rc::{Rc, Weak},
};
#[derive(Default)]
pub struct Counts {
    pub created: Cell<usize>,
    pub dropped: Cell<usize>,
    pub viewport: Cell<usize>,
    pub attached: Cell<usize>,
}
pub struct Session {
    pub terminal: TerminalView,
    pub key: String,
    counts: Rc<Counts>,
    last: RefCell<String>,
}
impl Drop for Session {
    fn drop(&mut self) {
        self.counts.dropped.set(self.counts.dropped.get() + 1);
    }
}
impl Session {
    fn new(
        key: String,
        vm: Weak<RefCell<Model>>,
        dispatcher: UiDispatcher,
        counts: Rc<Counts>,
    ) -> Self {
        let mut terminal = TerminalView::new().unwrap();
        terminal.set_font_family("Consolas");
        terminal.set_font_size(13.);
        terminal.set_cursor_blinking(false);
        let weak = vm.clone();
        let input_key = key.clone();
        let d = dispatcher.clone();
        terminal.set_on_text_input(move |text| {
            let vm = weak.clone();
            let key = input_key.clone();
            let _ = d.dispatch_local(move || {
                if let Some(vm) = vm.upgrade() {
                    vm.borrow_mut().input(&key, &text, false).unwrap();
                }
            });
        });
        let weak = vm.clone();
        let input_key = key.clone();
        let d = dispatcher.clone();
        terminal.set_on_paste(move |text| {
            let vm = weak.clone();
            let key = input_key.clone();
            let _ = d.dispatch_local(move || {
                if let Some(vm) = vm.upgrade() {
                    vm.borrow_mut().input(&key, &text, true).unwrap();
                }
            });
        });
        let weak = vm;
        let input_key = key.clone();
        terminal.set_on_scroll(move |_| {
            let vm = weak.clone();
            let key = input_key.clone();
            let _ = dispatcher.dispatch_local(move || {
                if let Some(vm) = vm.upgrade() {
                    vm.borrow_mut()
                        .command(crate::page::WorkspaceCommand::Activate { pane: key })
                        .unwrap();
                    vm.borrow_mut()
                        .command(crate::page::WorkspaceCommand::History)
                        .unwrap();
                }
            });
        });
        let count = counts.clone();
        terminal.set_on_viewport_changed(move |_| count.viewport.set(count.viewport.get() + 1));
        counts.created.set(counts.created.get() + 1);
        let value = Self {
            terminal,
            key,
            counts,
            last: RefCell::new(String::new()),
        };
        value.render("", false, 0);
        value
    }
    pub fn widget(&self) -> &Widget {
        self.terminal.as_widget()
    }
    pub fn address(&self) -> usize {
        unsafe { self.widget().with_native_handle(|p| p as usize) }
    }
    pub fn render(&self, echo: &str, history: bool, tick: u64) {
        let title=format!("{} | {} | tick {tick:04}\n\n$ workspace --local\nready: no shell, no network\n\n{echo}",self.key,if history{"history"}else{"live"});
        if *self.last.borrow() == title {
            return;
        }
        *self.last.borrow_mut() = title.clone();
        let mut cells = vec![TerminalCell::default(); 80 * 24];
        for (row, line) in title.lines().take(24).enumerate() {
            for (col, ch) in line.chars().take(80).enumerate() {
                cells[row * 80 + col] = TerminalCell {
                    text: ch.to_string(),
                    foreground: TerminalColor::rgb(183, 213, 192),
                    background: TerminalColor::rgb(20, 29, 25),
                    ..Default::default()
                };
            }
        }
        self.terminal.set_grid(24, 80, &cells);
    }
}
#[derive(Default)]
pub struct Content {
    pub sessions: BTreeMap<String, Rc<Session>>,
    pub counts: Rc<Counts>,
}
impl Content {
    pub fn sync(
        &mut self,
        view: &View,
        vm: &Rc<RefCell<Model>>,
        dispatcher: &UiDispatcher,
    ) -> VmResult {
        let keys: Vec<_> = vm.borrow().panes.iter().map(|p| p.0.to_string()).collect();
        self.sessions.retain(|key, _| keys.contains(key));
        for key in keys {
            if !self.sessions.contains_key(&key) {
                let session = Rc::new(Session::new(
                    key.clone(),
                    Rc::downgrade(vm),
                    dispatcher.clone(),
                    self.counts.clone(),
                ));
                let scope = view.resolve_scope(&[ScopeSegment {
                    instance_id: "pool",
                    key: &key,
                }])?;
                view.find_host_in(&scope, "terminal")?
                    .attach(session.clone(), Session::widget)?;
                self.counts.attached.set(self.counts.attached.get() + 1);
                self.sessions.insert(key, session);
            }
        }
        Ok(())
    }
    pub fn render(&self, vm: &Model, tick: u64) {
        for (key, session) in &self.sessions {
            let history = vm
                .panes
                .iter()
                .find(|p| p.0 == key)
                .is_some_and(|p| p.1.borrow().history);
            let echo = vm
                .broadcast_state
                .received
                .get(key)
                .map(String::as_str)
                .unwrap_or("");
            session.terminal.set_font_family(&vm.font);
            session.render(echo, history, if history { 0 } else { tick });
        }
    }
}
