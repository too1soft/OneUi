#![cfg_attr(target_os = "windows", windows_subsystem = "windows")]
use oneui::{ui::*, Label, TextField};
use std::{cell::Cell, rc::Rc};

fn main() -> Result<(), UiError> {
    let count = Rc::new(Cell::new(0));
    let value = Rc::new(Label::new("0")?);
    let name = Rc::new(TextField::new("为这次实验取个名字")?);
    let count_action = count.clone();
    let value_action = value.clone();
    let root = scroll(page([
        page_header(
            "从内容开始，布局交给 OneUI。",
            "共享主题、自动换行和原生控件。缩窄窗口，观察内容如何重新排列。",
            [],
        ),
        section(
            "实验设置",
            "组件负责间距、字体与层级，应用只描述内容。",
            [
                form_row("实验名称", "使用原生文本框输入。", widget(name)),
                metric("操作次数", widget(value), "次"),
                toolbar([button("运行一次", move || {
                    let next = count_action.get() + 1;
                    count_action.set(next);
                    value_action.set_text(&next.to_string());
                })
                .class("primary")]),
            ],
        ),
        empty_state("还没有保存的实验", "运行后可继续添加自己的数据与操作。", []),
    ]));
    App::new("FORM / LAB — Rust 组件示例", root)
        .size(1000, 720)
        .run()?;
    Ok(())
}
