use oneui::layout::dock_session::{DockDestination, DockSession, DockSessionError};
use oneui::TextField;
use std::rc::Rc;

pub fn dock_pane(session: &DockSession<'_>, host: u64) -> Result<(), DockSessionError> {
    let editor = Rc::new(TextField::new("文档").map_err(|_| DockSessionError::Failed)?);
    editor.set_text("迁移保留原控件与 Rust 所有者。");
    // The session retains this Rc, including callback ownership; it does not
    // reconstruct the editor. It borrows its Application for the whole lifetime.
    session.add_pane(
        42,
        "文档",
        editor,
        |value| value.as_widget(),
        DockDestination::tabs(host),
    )
}
pub fn move_pane(session: &DockSession<'_>, target_host: u64) -> Result<(), DockSessionError> {
    session.move_pane(42, DockDestination::tabs(target_host))
}

pub fn dock_theme(
    session: &DockSession<'_>,
    sheet: &oneui::StyleSheet,
) -> Result<(), DockSessionError> {
    session.set_style_sheet(sheet)
}
