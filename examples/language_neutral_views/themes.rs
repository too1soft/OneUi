use crate::page::{ThemeItemCommand, ThemeItemVm, ThemeItem_properties, SELECTED_THEME};
use oneui::view::{PropertyId, StableList, VmContext, VmResult};
use std::{
    cell::{Cell, RefCell},
    rc::Rc,
};

pub const THEMES: [&str; 12] = [
    "纸白", "雾灰", "沙岩", "薄荷", "海盐", "暖杏", "墨黑", "石墨", "深海", "森林", "紫夜", "赤陶",
];
#[derive(Default)]
pub struct ThemeState {
    pub selected: Cell<usize>,
    roots: RefCell<Vec<VmContext>>,
    rows: RefCell<Vec<VmContext>>,
    pub presentation: RefCell<Option<Rc<dyn Fn()>>>,
}
impl ThemeState {
    pub fn connect(&self, cx: &VmContext, root: bool) {
        if root { &self.roots } else { &self.rows }
            .borrow_mut()
            .push(cx.clone());
    }
    pub fn disconnect(&self, cx: &VmContext, root: bool) {
        if root { &self.roots } else { &self.rows }
            .borrow_mut()
            .retain(|v| !v.same_connections(cx));
    }
    pub fn select(&self, index: usize) -> VmResult {
        self.selected.set(index);
        for cx in self.rows.borrow().iter() {
            cx.notify(ThemeItem_properties::SELECTED)?;
        }
        for cx in self.roots.borrow().iter() {
            cx.notify(SELECTED_THEME)?;
        }
        if let Some(changed) = self.presentation.borrow().as_ref() {
            changed();
        }
        Ok(())
    }
    pub fn notify_roots(&self, property: PropertyId) -> VmResult {
        for cx in self.roots.borrow().iter() {
            cx.notify(property)?;
        }
        Ok(())
    }
}
pub struct Theme {
    id: String,
    index: usize,
    state: Rc<ThemeState>,
}
impl ThemeItemVm for Theme {
    fn id(&self) -> &str {
        &self.id
    }
    fn title(&self) -> &str {
        THEMES[self.index]
    }
    fn group(&self) -> &str {
        if self.index < 6 {
            "浅色"
        } else {
            "深色"
        }
    }
    fn selected(&self) -> bool {
        self.state.selected.get() == self.index
    }
    fn invoke(&mut self, _: ThemeItemCommand, _: &VmContext) -> VmResult {
        self.state.select(self.index)
    }
    fn on_view_connect(&mut self, cx: &VmContext) -> VmResult {
        self.state.connect(cx, false);
        Ok(())
    }
    fn on_view_disconnect(&mut self, cx: &VmContext) {
        self.state.disconnect(cx, false);
    }
}
pub fn themes(state: &Rc<ThemeState>) -> StableList<Theme> {
    let mut items = StableList::new();
    for index in 0..THEMES.len() {
        let id = format!("theme-{index}");
        items
            .insert(
                index,
                id.clone(),
                Rc::new(RefCell::new(Theme {
                    id,
                    index,
                    state: state.clone(),
                })),
            )
            .unwrap();
    }
    items
}
pub struct Choice {
    id: String,
    value: String,
}
impl crate::page::oneui_v1_StringItemVm for Choice {
    fn id(&self) -> &str {
        &self.id
    }
    fn value(&self) -> &str {
        &self.value
    }
}
pub fn choices(values: &[&str]) -> StableList<Choice> {
    let mut list = StableList::new();
    for (i, value) in values.iter().enumerate() {
        list.insert(
            i,
            i.to_string(),
            Rc::new(RefCell::new(Choice {
                id: i.to_string(),
                value: (*value).into(),
            })),
        )
        .unwrap();
    }
    list
}
