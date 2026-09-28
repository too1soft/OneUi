//! Page patterns using the same opt-in theme and retained native tree.
use super::*;

pub fn page_header(
    title: impl Into<String>,
    description: impl Into<String>,
    actions: impl IntoIterator<Item = Node>,
) -> Node {
    column([
        flow([
            paragraph(title).class("heading").basis(340.0).grow(1.0),
            toolbar(actions),
        ]),
        paragraph(description).class("muted"),
    ])
    .class("page-header")
}

pub fn section(
    title: impl Into<String>,
    description: impl Into<String>,
    content: impl IntoIterator<Item = Node>,
) -> Node {
    let mut nodes = vec![
        text(title).class("section"),
        paragraph(description).class("muted"),
    ];
    nodes.extend(content);
    surface(nodes)
}

/// `value` may be `widget(Rc<Label>)` for a live value without rebuilding.
pub fn metric(name: impl Into<String>, value: Node, unit: impl Into<String>) -> Node {
    column([muted(name), row([value.class("metric-value"), muted(unit)])])
        .class("metric")
        .shrink(0.0)
}

/// A visual form layout. Set the native control's accessible description/name
/// through its own API where available; this does not create a label relation.
pub fn form_row(label: impl Into<String>, hint: impl Into<String>, control: Node) -> Node {
    flow([
        column([text(label), paragraph(hint).class("muted")])
            .basis(240.0)
            .grow(1.0),
        control.basis(220.0).grow(1.0),
    ])
    .class("form-row")
}

pub fn empty_state(
    title: impl Into<String>,
    description: impl Into<String>,
    actions: impl IntoIterator<Item = Node>,
) -> Node {
    section(title, description, [toolbar(actions)]).class("empty-state")
}

/// A small app entry point: owns the theme, tree, callbacks and window until exit.
/// Requires a Yoga-enabled native build; Ui::new remains the legacy entry point.
pub struct App {
    title: String,
    root: Node,
    width: i32,
    height: i32,
    css: String,
}
impl App {
    pub fn new(title: impl Into<String>, root: Node) -> Self {
        Self {
            title: title.into(),
            root,
            width: 1100,
            height: 760,
            css: DEFAULT_CSS.into(),
        }
    }
    pub fn size(mut self, width: i32, height: i32) -> Self {
        self.width = width;
        self.height = height;
        self
    }
    pub fn theme(mut self, css: impl Into<String>) -> Self {
        self.css = css.into();
        self
    }
    pub fn build(self) -> Result<MountedApp, UiError> {
        if self.width <= 0 || self.height <= 0 {
            return Err(Error::InvalidLayout.into());
        }
        let ui = Ui::with_css_and_engine(self.root, &self.css, StackEngine::Yoga)?;
        let window = Window::new(&WindowOptions {
            title: self.title,
            width: self.width,
            height: self.height,
            ..Default::default()
        })?;
        window.set_default_font_family("Microsoft YaHei UI");
        ui.mount(&window);
        Ok(MountedApp { window, ui })
    }
    pub fn run(self) -> Result<i32, UiError> {
        Ok(self.build()?.run())
    }
}

/// Keep this owner alive while the window is open. Public fields support
/// dispatcher setup and theme updates without raw pointers or global state.
pub struct MountedApp {
    pub window: Window,
    pub ui: Ui,
}
impl MountedApp {
    pub fn run(self) -> i32 {
        self.ui.run(self.window)
    }
}
