pub mod page {
    include!(concat!(env!("OUT_DIR"), "/page.rs"));
}
use oneui::view::*;
use std::{
    cell::{Cell, RefCell},
    rc::Rc,
};
pub struct Row {
    pub id: String,
    pub connections: usize,
}
impl Row {
    pub fn new(id: &str) -> Self {
        Self {
            id: id.into(),
            connections: 0,
        }
    }
}
impl page::RustRowVm for Row {
    fn id(&self) -> &str {
        &self.id
    }
    fn text(&self) -> &str {
        &self.id
    }
    fn invoke(&mut self, _: page::RustRowCommand, _: &VmContext) -> VmResult {
        Ok(())
    }
    fn on_view_connect(&mut self, _: &VmContext) -> VmResult {
        self.connections += 1;
        Ok(())
    }
    fn on_view_disconnect(&mut self, _: &VmContext) {
        self.connections -= 1;
    }
}
pub struct Vm {
    pub title: String,
    pub enabled: bool,
    pub rows: StableList<Row>,
    pub drops: Rc<Cell<usize>>,
    pub connections: usize,
    pub panic: u8,
}
impl Vm {
    pub fn new() -> Self {
        let mut rows = StableList::new();
        rows.insert(0, "a", Rc::new(RefCell::new(Row::new("a"))))
            .unwrap();
        Self {
            title: "Rust".into(),
            enabled: true,
            rows,
            drops: Rc::new(Cell::new(0)),
            connections: 0,
            panic: 0,
        }
    }
}
impl Drop for Vm {
    fn drop(&mut self) {
        self.drops.set(self.drops.get() + 1);
        assert_ne!(self.panic, 4, "drop panic");
    }
}
impl page::RustPageVm for Vm {
    type RowsItem = Row;
    fn title(&self) -> &str {
        assert_ne!(self.panic, 1, "get panic");
        &self.title
    }
    fn set_title(&mut self, value: String, cx: &VmContext) -> VmResult {
        self.title = value;
        cx.notify(page::TITLE)
    }
    fn enabled(&self) -> bool {
        self.enabled
    }
    fn set_enabled(&mut self, value: bool, cx: &VmContext) -> VmResult {
        self.enabled = value;
        cx.notify(page::ENABLED)
    }
    fn status(&self) -> &str {
        "ready"
    }
    fn rows(&self) -> &StableList<Row> {
        &self.rows
    }
    fn invoke(&mut self, command: page::RustPageCommand, cx: &VmContext) -> VmResult {
        self.title = match command {
            page::RustPageCommand::Run => "run".into(),
            page::RustPageCommand::Choose { target } => target,
        };
        cx.notify(page::TITLE)
    }
    fn on_view_connect(&mut self, _: &VmContext) -> VmResult {
        self.connections += 1;
        assert_ne!(self.panic, 2, "connect panic");
        Ok(())
    }
    fn on_view_disconnect(&mut self, _: &VmContext) {
        self.connections -= 1;
        assert_ne!(self.panic, 3, "disconnect panic");
    }
}
