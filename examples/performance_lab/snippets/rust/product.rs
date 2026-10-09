use oneui::{AppShell, Error, Label, TextField, TopBar};

// Retain the controller while displayed, including wrappers with callbacks.
pub struct Product {
    pub shell: AppShell,
    pub toolbar: TopBar,
    pub editor: TextField,
}

impl Product {
    pub fn new() -> Result<Self, Error> {
        let shell = AppShell::new()?;
        let toolbar = TopBar::new()?;
        toolbar.set_leading(Some(Label::new("本地工作区")?.as_widget()));
        toolbar.set_leading_width(240.0);
        shell.set_header(Some(toolbar.as_widget()));
        shell.set_header_height(48.0);
        shell.set_sidebar(Some(Label::new("设备列表")?.as_widget()));
        shell.set_sidebar_width(180.0);
        let editor = TextField::new("输入草稿")?;
        editor.set_text("保留的草稿");
        shell.set_content(Some(editor.as_widget()));
        shell.set_footer(Some(Label::new("本地模拟 · 无网络连接")?.as_widget()));
        shell.set_footer_height(28.0);
        shell.set_footer_span_sidebar(true);
        Ok(Self {
            shell,
            toolbar,
            editor,
        })
    }

    pub fn resize(&self, width: f32) {
        // The native editor and its text/selection remain in the content slot.
        self.shell.set_sidebar_visible(width >= 760.0);
    }
}
