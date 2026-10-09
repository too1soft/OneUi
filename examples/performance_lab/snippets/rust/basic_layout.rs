use oneui::{Error, Label, Stack, StackDirection};

pub fn layout() -> Result<Stack, Error> {
    let row = Stack::new(StackDirection::Row)?;
    row.set_gap(12.0);
    let label = Label::new("布局示例")?;
    row.add(label.as_widget());
    // The native parent retains the label. Controls with callbacks should also
    // retain their Rust wrappers, e.g. inside a page controller held by Rc.
    Ok(row)
}
