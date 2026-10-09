use oneui::{TerminalCell, TerminalColor, TerminalView};

// Retain the safe owner while C++/NativeHost retains the underlying widget.
pub fn terminal(mut on_text: impl FnMut(&str) + 'static) -> Result<TerminalView, oneui::Error> {
    let mut terminal = TerminalView::new()?;
    let mut cells = vec![
        TerminalCell {
            foreground: TerminalColor::rgb(229, 236, 232),
            background: TerminalColor::rgb(28, 34, 40),
            ..Default::default()
        };
        4
    ];
    cells[0].text = "A".into();
    terminal.set_grid(2, 2, &cells);
    terminal.set_cursor_blinking(false);
    terminal.set_on_text_input(move |text| on_text(&text));
    Ok(terminal)
}
