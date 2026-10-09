use oneui::{Error, TextArea};

pub fn text() -> Result<TextArea, Error> {
    let input = TextArea::new("输入说明")?;
    input.set_text("中文 · 👩‍💻 · العربية\n第二行");
    Ok(input)
}
