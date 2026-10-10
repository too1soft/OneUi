use crate::page::*;
use oneui::view::StableList;
#[derive(Clone, Default, PartialEq)]
pub struct Node {
    pub id: String,
    pub kind: String,
    pub axis: String,
    pub ratio: f64,
    pub first: String,
    pub second: String,
    pub pane: String,
}
impl NodeVm for Node {
    fn id(&self) -> &str {
        &self.id
    }
    fn kind(&self) -> &str {
        &self.kind
    }
    fn axis(&self) -> &str {
        &self.axis
    }
    fn ratio(&self) -> f64 {
        self.ratio
    }
    fn first(&self) -> &str {
        &self.first
    }
    fn second(&self) -> &str {
        &self.second
    }
    fn pane(&self) -> &str {
        &self.pane
    }
}
#[derive(Clone, Default, PartialEq)]
pub struct Group {
    pub id: String,
    pub root: String,
}
impl GroupVm for Group {
    fn id(&self) -> &str {
        &self.id
    }
    fn root(&self) -> &str {
        &self.root
    }
}
#[derive(Clone, Default, PartialEq)]
pub struct PaneModel {
    pub id: String,
    pub title: String,
    pub path: String,
    pub active: bool,
    pub role: String,
    pub hint: String,
    pub history: bool,
    pub closing: bool,
    pub closeRequest: i64,
    pub headerVisible: bool,
    pub number: i64,
    pub canClose: bool,
}
impl PaneModelVm for PaneModel {
    fn id(&self) -> &str {
        &self.id
    }
    fn title(&self) -> &str {
        &self.title
    }
    fn path(&self) -> &str {
        &self.path
    }
    fn active(&self) -> bool {
        self.active
    }
    fn role(&self) -> &str {
        &self.role
    }
    fn hint(&self) -> &str {
        &self.hint
    }
    fn history(&self) -> bool {
        self.history
    }
    fn closing(&self) -> bool {
        self.closing
    }
    fn closeRequest(&self) -> i64 {
        self.closeRequest
    }
    fn headerVisible(&self) -> bool {
        self.headerVisible
    }
    fn number(&self) -> i64 {
        self.number
    }
    fn canClose(&self) -> bool {
        self.canClose
    }
}
#[derive(Clone, Default, PartialEq)]
pub struct TabModel {
    pub id: String,
    pub title: String,
    pub pane: String,
    pub enabled: bool,
}
impl TabModelVm for TabModel {
    fn id(&self) -> &str {
        &self.id
    }
    fn title(&self) -> &str {
        &self.title
    }
    fn pane(&self) -> &str {
        &self.pane
    }
    fn enabled(&self) -> bool {
        self.enabled
    }
}
#[derive(Clone, Default, PartialEq)]
pub struct Placement {
    pub id: String,
    pub mode: String,
    pub x: f64,
    pub y: f64,
    pub width: f64,
    pub height: f64,
    pub pinned: bool,
    pub order: i64,
}
impl PlacementVm for Placement {
    fn id(&self) -> &str {
        &self.id
    }
    fn mode(&self) -> &str {
        &self.mode
    }
    fn x(&self) -> f64 {
        self.x
    }
    fn y(&self) -> f64 {
        self.y
    }
    fn width(&self) -> f64 {
        self.width
    }
    fn height(&self) -> f64 {
        self.height
    }
    fn pinned(&self) -> bool {
        self.pinned
    }
    fn order(&self) -> i64 {
        self.order
    }
}
#[derive(Clone, Default, PartialEq)]
pub struct Font {
    pub id: String,
    pub family: String,
    pub label: String,
    pub available: bool,
}
impl FontVm for Font {
    fn id(&self) -> &str {
        &self.id
    }
    fn family(&self) -> &str {
        &self.family
    }
    fn label(&self) -> &str {
        &self.label
    }
    fn available(&self) -> bool {
        self.available
    }
}
#[derive(Clone, Default, PartialEq)]
pub struct Color {
    pub id: String,
    pub rgba: i64,
}
impl ColorVm for Color {
    fn id(&self) -> &str {
        &self.id
    }
    fn rgba(&self) -> i64 {
        self.rgba
    }
}
#[derive(Default)]
pub struct Theme {
    pub id: String,
    pub title: String,
    pub tokens: StableList<Color>,
}
impl ThemeVm for Theme {
    type TokensItem = Color;
    fn id(&self) -> &str {
        &self.id
    }
    fn title(&self) -> &str {
        &self.title
    }
    fn tokens(&self) -> &StableList<Color> {
        &self.tokens
    }
}

impl oneui_v1_TabItemVm for TabModel {
    fn id(&self) -> &str {
        &self.id
    }
    fn title(&self) -> &str {
        &self.title
    }
}
