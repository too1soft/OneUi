//! Handwritten retained Rust reference for SettingsProbe.one. No IR is consumed.
use crate::{page::SettingsBasicVm, themes::THEMES, vm::Settings};
use oneui::view::VmContext;
use oneui::{
    Color, Flex, Label, Stack, StackAlign, StackDirection, StackEngine, Switch, TextField, Widget,
};
use std::{
    cell::{Cell, RefCell},
    rc::Rc,
};

pub const CSS: &str = "Column { gap:0px; } Text { color:#1e2823; font-size:14px; }";

fn column() -> Stack {
    let stack = Stack::new(StackDirection::Column).unwrap();
    stack.set_engine(StackEngine::Yoga).unwrap();
    stack.set_align(StackAlign::Stretch);
    stack.set_gap(0.0);
    stack
}
fn label(text: &str) -> Rc<Label> {
    let label = Rc::new(Label::new(text).unwrap());
    label.set_font_size(14.0);
    label.set_font_weight(400);
    label.set_color(Color {
        r: 30,
        g: 40,
        b: 35,
        a: 255,
    });
    label.set_text_wrapping(true);
    label
}
fn add(stack: &Stack, widget: &Widget, shrink: f32) {
    stack.add(widget);
    stack
        .set_flex(
            widget,
            Flex {
                shrink,
                ..Flex::content()
            },
        )
        .unwrap();
}
pub struct ThemeRow {
    _stack: Stack,
    _title: Rc<Label>,
    selected: Rc<Label>,
}
pub struct Manual {
    pub root: Stack,
    pub input: Rc<TextField>,
    toggle: Rc<Switch>,
    status: Rc<Label>,
    error: Rc<Label>,
    theme: Rc<Label>,
    _repeat: Stack,
    rows: Vec<ThemeRow>,
    applying: Rc<Cell<bool>>,
    previous_theme: Cell<usize>,
    pub setter_calls: Cell<u64>,
}
impl Manual {
    pub fn new(vm: &Rc<RefCell<Settings>>) -> Self {
        let root = column();
        let status = label(&vm.borrow().status);
        let error = label(&vm.borrow().error);
        let theme = label(vm.borrow().selected_theme());
        let applying = Rc::new(Cell::new(false));
        let mut input = TextField::new("").unwrap();
        input.set_text(&vm.borrow().font_family);
        input.as_widget().set_preferred_size(194.0, 40.0);
        let weak = Rc::downgrade(vm);
        let guard = applying.clone();
        let state_label = status.clone();
        let error_label = error.clone();
        input.set_on_changed(move |value| {
            if guard.get() {
                return;
            }
            if let Some(vm) = weak.upgrade() {
                vm.borrow_mut()
                    .set_font_family(value, &VmContext::default())
                    .unwrap();
                state_label.set_text(&vm.borrow().status);
                error_label.set_text(&vm.borrow().error);
            }
        });
        let input = Rc::new(input);
        let mut toggle = Switch::new("").unwrap();
        toggle.set_checked(vm.borrow().compact);
        let weak = Rc::downgrade(vm);
        let guard = applying.clone();
        let state_label = status.clone();
        let error_label = error.clone();
        toggle.set_on_changed(move |value| {
            if guard.get() {
                return;
            }
            if let Some(vm) = weak.upgrade() {
                vm.borrow_mut()
                    .set_compact(value, &VmContext::default())
                    .unwrap();
                state_label.set_text(&vm.borrow().status);
                error_label.set_text(&vm.borrow().error);
            }
        });
        let toggle = Rc::new(toggle);
        add(&root, input.as_widget(), 1.0);
        add(&root, toggle.as_widget(), 1.0);
        for item in [&status, &error, &theme] {
            add(&root, item.as_widget(), 0.0);
        }
        let repeat = column();
        let mut rows = Vec::new();
        for (index, title) in THEMES.iter().enumerate() {
            let row = column();
            let title = label(title);
            let selected = label("Selected");
            selected
                .as_widget()
                .set_visible(index == vm.borrow().theme.selected.get());
            add(&row, title.as_widget(), 0.0);
            add(&row, selected.as_widget(), 0.0);
            add(&repeat, row.as_widget(), 1.0);
            rows.push(ThemeRow {
                _stack: row,
                _title: title,
                selected,
            });
        }
        add(&root, repeat.as_widget(), 1.0);
        Self {
            root,
            input,
            toggle,
            status,
            error,
            theme,
            _repeat: repeat,
            rows,
            applying,
            previous_theme: Cell::new(vm.borrow().theme.selected.get()),
            setter_calls: Cell::new(0),
        }
    }
    pub fn scalar(&self, vm: &Settings) {
        self.applying.set(true);
        self.toggle.set_checked(vm.compact);
        self.applying.set(false);
        self.setter_calls.set(self.setter_calls.get() + 1);
    }
    pub fn text(&self, vm: &Settings) {
        self.status.set_text(&vm.status);
        self.setter_calls.set(self.setter_calls.get() + 1);
    }
    pub fn theme(&self, vm: &Settings) {
        let index = vm.theme.selected.get();
        let previous = self.previous_theme.replace(index);
        self.theme.set_text(vm.selected_theme());
        if index != previous {
            self.rows[previous].selected.as_widget().set_visible(false);
            self.rows[index].selected.as_widget().set_visible(true);
        }
        self.setter_calls.set(self.setter_calls.get() + 3);
    }
}
