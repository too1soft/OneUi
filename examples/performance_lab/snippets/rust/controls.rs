use oneui::{Button, Error};

pub fn button() -> Result<Button, Error> {
    let mut button = Button::new("执行")?;
    button.set_on_click(|| println!("clicked on the UI thread"));
    // Retain the returned wrapper for as long as its callback is needed.
    // Dropping the wrapper clears its callback, even if a parent owns the widget.
    Ok(button)
}
