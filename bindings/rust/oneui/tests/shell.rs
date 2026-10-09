use oneui::{AppShell, Insets, Label, TextField, TopBar, Window, WindowOptions};

fn main() {
    // AppKit requires UI construction on the process main thread.
    let window = Window::new(&WindowOptions::default()).unwrap();
    let shell = AppShell::new().unwrap();
    let bar = TopBar::new().unwrap();
    let editor = TextField::new("Draft").unwrap();
    let search = TextField::new("Search").unwrap();
    let sidebar = TextField::new("Device filter").unwrap();
    let footer = Label::new("Local only").unwrap();
    bar.set_leading(Some(search.as_widget()));
    bar.set_leading_width(240.0);
    bar.set_gap(8.0);
    bar.set_padding(Insets {
        top: 4.0,
        right: 4.0,
        bottom: 4.0,
        left: 4.0,
    });
    shell.set_header(Some(bar.as_widget()));
    shell.set_sidebar(Some(sidebar.as_widget()));
    shell.set_content(Some(editor.as_widget()));
    shell.set_footer(Some(footer.as_widget()));
    shell.set_sidebar_width(180.0);
    shell.set_header_height(48.0);
    shell.set_footer_height(28.0);
    shell.set_footer_span_sidebar(true);
    shell.set_gap(4.0);
    shell.set_padding(Insets {
        top: 8.0,
        right: 8.0,
        bottom: 8.0,
        left: 8.0,
    });
    window.set_content(shell.as_widget());
    let ui = window.dispatcher();
    ui.layout_snapshot_json().unwrap();
    let expanded = editor.as_widget().frame();
    assert!(expanded.width > 0.0 && expanded.x >= 188.0);
    assert!(ui.request_focus(editor.as_widget(), true).unwrap());
    for _ in 0..30 {
        shell.set_sidebar_visible(false);
        ui.layout_snapshot_json().unwrap();
        assert!(editor.as_widget().frame().width > expanded.width);
        assert!(editor.as_widget().is_focused());
        assert!(!ui.request_focus(sidebar.as_widget(), true).unwrap());
        shell.set_sidebar_visible(true);
        ui.layout_snapshot_json().unwrap();
        assert_eq!(editor.as_widget().frame(), expanded);
        assert!(editor.as_widget().is_focused());
    }
    assert!(ui.request_focus(search.as_widget(), true).unwrap());
    let action = Label::new("Status").unwrap();
    bar.add_action(action.as_widget());
    ui.layout_snapshot_json().unwrap();
    assert!(search.as_widget().is_focused());
    bar.clear_actions();
    assert!(search.as_widget().is_focused());
    bar.set_leading(None);
    assert!(!search.as_widget().is_focused());
    shell.set_sidebar(None);
    shell.set_header(None);
    shell.set_footer(None);
    ui.layout_snapshot_json().unwrap();
    assert!(editor.as_widget().frame().height > expanded.height);
    shell.set_content(None);
    ui.layout_snapshot_json().unwrap();
    println!("PASS: safe AppShell/TopBar slots, clear, 30 sidebar cycles and retained focus");
}
