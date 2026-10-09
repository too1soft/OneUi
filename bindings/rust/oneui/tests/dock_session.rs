use oneui::layout::dock_session::{DockDestination, DockSession, DockSessionError};
use oneui::{Application, Panel, QuitPolicy, StyleSheet, TextField, Window, WindowOptions};
use std::rc::Rc;

fn rust_owner_survives_reparent_and_is_released_once() {
    let app = Application::new("org.oneui.tests.rustdock", "Rust Dock").unwrap();
    app.set_quit_policy(QuitPolicy::Explicit);
    let a = Window::new(&WindowOptions::default()).unwrap();
    let b = Window::new(&WindowOptions::default()).unwrap();
    app.add_window(&a, true);
    app.add_window(&b, false);
    let left = Rc::new(Panel::new().unwrap());
    let right = Rc::new(Panel::new().unwrap());
    a.set_content(left.as_widget());
    b.set_content(right.as_widget());
    let session = DockSession::new(&app).unwrap();
    let sheet = StyleSheet::from_css("Input { color:#e5ece8; background-color:#1c2228; }").unwrap();
    session.set_style_sheet(&sheet).unwrap();
    drop(sheet); // The session retains the native sheet for later host creation.
    let first = session.attach_host(&a, left).unwrap();
    let second = session.attach_host(&b, right).unwrap();
    let editor = Rc::new(TextField::new("Edit").unwrap());
    editor.set_text("Original text");
    let identity = Rc::as_ptr(&editor);
    let weak = Rc::downgrade(&editor);
    session
        .add_pane(
            1,
            "Editor",
            editor,
            |e| e.as_widget(),
            DockDestination::tabs(first),
        )
        .unwrap();
    assert!(weak.upgrade().is_some());
    let mut updated = StyleSheet::from_css("Input { color:#102218; }").unwrap();
    session.set_style_sheet(&updated).unwrap();
    updated.replace_css("Input { color:#e5ece8; }").unwrap();
    session.set_style_sheet(&updated).unwrap();
    assert_eq!(Rc::as_ptr(&weak.upgrade().unwrap()), identity);
    {
        let _cancelled = session.begin_drag(1).unwrap();
    }
    assert_eq!(session.location(1).unwrap().host, first);
    for _ in 0..30 {
        session
            .begin_drag(1)
            .unwrap()
            .commit(DockDestination::tabs(second))
            .unwrap();
        session.move_pane(1, DockDestination::tabs(first)).unwrap();
    }
    assert_eq!(session.panes(None), [1]);
    assert_eq!(session.hosts(), [first, second]);
    assert!(weak.upgrade().is_some());
    assert_eq!(
        session.move_pane(1, DockDestination::tabs(99)),
        Err(DockSessionError::InvalidHost)
    );
    session.remove_pane(1).unwrap();
    assert!(weak.upgrade().is_none());
    drop(session);
    app.request_quit();
}
fn main() {
    // AppKit requires native UI work on the process main thread.
    rust_owner_survives_reparent_and_is_released_once();
    println!("Rust dock owner retention and release passed");
}
