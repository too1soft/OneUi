//! Retained composition with content-sized defaults and an opt-in theme.
//! Build descriptions first; native allocation and error handling happen once
//! in `Ui::new`. Keep the Ui alive while mounted, or use `Ui::run`.
use crate::{
    Button, Error, Flex, FlexBasis, Insets, Label, Stack, StackAlign, StackDirection, StackEngine,
    StackJustify, StyleSheet, StyleSheetError, Widget, Window, WindowOptions,
};
use std::rc::Rc;

mod components;
pub use components::*;

pub const DEFAULT_CSS: &str = include_str!("ui/default.css");

#[derive(Debug)]
pub enum UiError {
    Native(Error),
    Style(StyleSheetError),
}
impl From<Error> for UiError {
    fn from(e: Error) -> Self {
        Self::Native(e)
    }
}
impl From<StyleSheetError> for UiError {
    fn from(e: StyleSheetError) -> Self {
        Self::Style(e)
    }
}
impl std::fmt::Display for UiError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{self:?}")
    }
}
impl std::error::Error for UiError {}

/// Own a native wrapper, including its callback registrations, in a UI tree.
pub trait Control {
    fn widget(&self) -> &Widget;
}
macro_rules! control {
    ($($ty:ty),*) => { $(impl Control for $ty {
        fn widget(&self) -> &Widget { self.as_widget() }
    })* };
}
control!(
    Button,
    Label,
    Stack,
    crate::TextField,
    crate::ProgressBar,
    crate::TimeSeriesChart,
    crate::VirtualList,
    crate::Panel,
    crate::ScrollView,
    crate::Switch
);
impl<T: Control> Control for Rc<T> {
    fn widget(&self) -> &Widget {
        (**self).widget()
    }
}

enum Kind {
    Text(String),
    Paragraph(String),
    Button(String, Box<dyn FnMut()>),
    Stack(StackDirection, Vec<Node>),
    Native(Box<dyn Control>),
    Scroll(Box<Node>),
}
pub struct Node {
    kind: Kind,
    classes: String,
    flex: Flex,
    gap: Option<f32>,
    padding: Option<f32>,
    align: Option<StackAlign>,
    justify: StackJustify,
    wrap: bool,
}
impl Node {
    fn new(kind: Kind) -> Self {
        let mut flex = Flex::content();
        if matches!(&kind, Kind::Button(..)) {
            flex.shrink = 0.0;
        }
        Self {
            kind,
            classes: String::new(),
            flex,
            gap: None,
            padding: None,
            align: None,
            justify: StackJustify::Start,
            wrap: false,
        }
    }
    pub fn class(mut self, class: &str) -> Self {
        if !self.classes.is_empty() {
            self.classes.push(' ');
        }
        self.classes.push_str(class);
        self
    }
    /// Share the parent's remaining main-axis space. Nested content retains
    /// its natural cross-axis size. Call `.min(...)` to protect readability.
    pub fn grow(mut self, weight: f32) -> Self {
        self.flex.grow = weight;
        self
    }
    pub fn shrink(mut self, weight: f32) -> Self {
        self.flex.shrink = weight;
        self
    }
    pub fn min(mut self, extent: f32) -> Self {
        self.flex.min = extent;
        self
    }
    pub fn max(mut self, extent: f32) -> Self {
        self.flex.max = extent;
        self
    }
    pub fn basis(mut self, extent: f32) -> Self {
        self.flex.basis = FlexBasis::Length(extent);
        self
    }
    pub fn gap(mut self, gap: f32) -> Self {
        self.gap = Some(gap);
        self
    }
    pub fn padding(mut self, padding: f32) -> Self {
        self.padding = Some(padding);
        self
    }
    pub fn align(mut self, align: StackAlign) -> Self {
        self.align = Some(align);
        self
    }
    pub fn justify(mut self, justify: StackJustify) -> Self {
        self.justify = justify;
        self
    }
    /// Wrap child items onto new lines. Requires the Yoga engine.
    pub fn wrap(mut self) -> Self {
        self.wrap = true;
        self
    }
    fn build(self, sheet: &StyleSheet, engine: StackEngine) -> Result<Built, UiError> {
        if [self.gap, self.padding]
            .into_iter()
            .flatten()
            .any(|v| !v.is_finite() || v < 0.0)
        {
            return Err(Error::InvalidLayout.into());
        }
        let mut children = Vec::new();
        let control: Box<dyn Control> = match self.kind {
            Kind::Text(value) => Box::new(Label::new(&value)?),
            Kind::Paragraph(value) => {
                let label = Label::new(&value)?;
                label.set_text_wrapping(true);
                Box::new(label)
            }
            Kind::Button(value, callback) => {
                let mut button = Button::new(&value)?;
                button.set_on_click(callback);
                Box::new(button)
            }
            Kind::Native(control) => control,
            Kind::Scroll(node) => {
                let scroll = crate::ScrollView::new()?;
                let child = node.build(sheet, engine)?;
                scroll.set_content(child.control.widget());
                children.push(child);
                Box::new(scroll)
            }
            Kind::Stack(direction, nodes) => {
                let stack = Stack::new(direction)?;
                stack.set_engine(engine)?;
                stack.set_wrap(self.wrap)?;
                stack.set_align(self.align.unwrap_or(if direction == StackDirection::Row {
                    StackAlign::Center
                } else {
                    StackAlign::Stretch
                }));
                stack.set_justify(self.justify);
                for node in nodes {
                    let flex = node.flex;
                    let child = node.build(sheet, engine)?;
                    stack.add(child.control.widget());
                    stack.set_flex(child.control.widget(), flex)?;
                    children.push(child);
                }
                // Explicit per-instance geometry wins over CSS defaults.
                stack.as_widget().set_classes(&self.classes)?;
                stack.as_widget().apply_style_sheet(sheet);
                if let Some(gap) = self.gap {
                    stack.set_gap(gap);
                }
                if let Some(p) = self.padding {
                    stack.set_padding(Insets {
                        top: p,
                        right: p,
                        bottom: p,
                        left: p,
                    });
                }
                return Ok(Built {
                    control: Box::new(stack),
                    children,
                    gap: self.gap,
                    padding: self.padding,
                });
            }
        };
        control.widget().set_classes(&self.classes)?;
        control.widget().apply_style_sheet(sheet);
        Ok(Built {
            control,
            children,
            gap: None,
            padding: None,
        })
    }
}
pub fn text(value: impl Into<String>) -> Node {
    Node::new(Kind::Text(value.into()))
}
/// A width-aware, multiline label. Use Yoga to propagate its measured height.
pub fn paragraph(value: impl Into<String>) -> Node {
    Node::new(Kind::Paragraph(value.into())).shrink(0.0)
}
/// Wrapping row with content-sized children.
pub fn flow(children: impl IntoIterator<Item = Node>) -> Node {
    row(children).wrap()
}
/// Equal-width columns which form new rows when preferred_width no longer fits.
/// Items can shrink below that width when the entire container is narrower.
pub fn responsive_columns(preferred_width: f32, children: impl IntoIterator<Item = Node>) -> Node {
    columns(
        children
            .into_iter()
            .map(|node| node.basis(preferred_width).grow(1.0)),
    )
    .wrap()
}
pub fn heading(value: impl Into<String>) -> Node {
    text(value).class("heading")
}
pub fn muted(value: impl Into<String>) -> Node {
    text(value).class("muted")
}
pub fn button(value: impl Into<String>, callback: impl FnMut() + 'static) -> Node {
    Node::new(Kind::Button(value.into(), Box::new(callback)))
}
pub fn primary_button(value: impl Into<String>, callback: impl FnMut() + 'static) -> Node {
    button(value, callback).class("primary")
}
pub fn row(children: impl IntoIterator<Item = Node>) -> Node {
    Node::new(Kind::Stack(
        StackDirection::Row,
        children.into_iter().collect(),
    ))
}
pub fn column(children: impl IntoIterator<Item = Node>) -> Node {
    Node::new(Kind::Stack(
        StackDirection::Column,
        children.into_iter().collect(),
    ))
}
/// Equal-height content columns; individual `.grow(...)` values control width.
pub fn columns(children: impl IntoIterator<Item = Node>) -> Node {
    row(children).align(StackAlign::Stretch)
}
pub fn surface(children: impl IntoIterator<Item = Node>) -> Node {
    column(children).class("surface")
}
pub fn toolbar(children: impl IntoIterator<Item = Node>) -> Node {
    row(children).class("toolbar").shrink(0.0)
}
pub fn page(children: impl IntoIterator<Item = Node>) -> Node {
    column(children).class("page")
}
/// Vertical overflow for a Yoga content tree, measured at viewport width.
pub fn scroll(content: Node) -> Node {
    Node::new(Kind::Scroll(Box::new(content)))
}
pub fn widget(control: impl Control + 'static) -> Node {
    Node::new(Kind::Native(Box::new(control)))
}

struct Built {
    control: Box<dyn Control>,
    children: Vec<Built>,
    gap: Option<f32>,
    padding: Option<f32>,
}
impl Built {
    fn apply_sheet(&self, sheet: &StyleSheet) {
        self.control.widget().apply_style_sheet(sheet);
        if let Some(gap) = self.gap {
            unsafe {
                crate::sys::oneui_stack_set_gap(self.control.widget().as_raw(), gap);
            }
        }
        if let Some(p) = self.padding {
            unsafe {
                crate::sys::oneui_stack_set_padding(
                    self.control.widget().as_raw(),
                    Insets {
                        top: p,
                        right: p,
                        bottom: p,
                        left: p,
                    }
                    .into(),
                );
            }
        }
        for child in &self.children {
            child.apply_sheet(sheet);
        }
    }
}
/// Owns all wrappers and callbacks. Dropping it unbinds callbacks even though
/// a native Window may still retain the visual tree. Prefer `run` for apps.
pub struct Ui {
    root: Built,
    sheet: StyleSheet,
}
impl Ui {
    pub fn new(root: Node) -> Result<Self, UiError> {
        Self::with_css(root, DEFAULT_CSS)
    }
    pub fn with_css(root: Node, css: &str) -> Result<Self, UiError> {
        Self::with_css_and_engine(root, css, StackEngine::Legacy)
    }
    pub fn with_engine(root: Node, engine: StackEngine) -> Result<Self, UiError> {
        Self::with_css_and_engine(root, DEFAULT_CSS, engine)
    }
    pub fn with_css_and_engine(
        root: Node,
        css: &str,
        engine: StackEngine,
    ) -> Result<Self, UiError> {
        let sheet = StyleSheet::from_css(css)?;
        Ok(Self {
            root: root.build(&sheet, engine)?,
            sheet,
        })
    }
    pub fn as_widget(&self) -> &Widget {
        self.root.control.widget()
    }
    pub fn mount(&self, window: &Window) {
        window.set_content(self.as_widget());
        self.root.apply_sheet(&self.sheet);
    }
    /// Reapply this composition's theme without changing another window's
    /// default sheet. Native invalidation schedules a layout/paint if mounted.
    pub fn replace_css(&mut self, css: &str) -> Result<(), UiError> {
        self.sheet.replace_css(css)?;
        self.root.apply_sheet(&self.sheet);
        Ok(())
    }
    pub fn run(self, window: Window) -> i32 {
        self.mount(&window);
        window.center_on_active_monitor();
        window.show();
        window.run()
    }
}
pub fn run(title: &str, root: Node) -> Result<i32, UiError> {
    let ui = Ui::new(root)?;
    let window = Window::new(&WindowOptions {
        title: title.into(),
        ..Default::default()
    })?;
    Ok(ui.run(window))
}
