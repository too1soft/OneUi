//! Sample ABI v1: UI-thread only. C++ retains this owner while its NativeHosts live.
//! Local echo fixture only, not a PTY/ANSI emulator.
use oneui::{terminal_style, TerminalCell, TerminalColor, TerminalView, TextField};
use oneui_sys as sys;
use std::{cell::RefCell, ffi::c_void, rc::Rc};

struct Echo {
    text: String,
    rows: u16,
    columns: u16,
    raw: *mut sys::OneUiWidget,
}
impl Echo {
    fn paint(&self) {
        let rows = self.rows.max(1);
        let cols = self.columns.max(2);
        let bg = TerminalColor::rgb(28, 34, 40);
        let fg = TerminalColor::rgb(229, 236, 232);
        let mut cells = vec![
            TerminalCell {
                background: bg,
                foreground: fg,
                ..Default::default()
            };
            rows as usize * cols as usize
        ];
        let mut row = 0usize;
        let mut col = 0usize;
        for c in self.text.chars() {
            let newline = c == '\n';
            let wide = (c as u32) >= 0x2e80;
            let width = if wide { 2 } else { 1 };
            if newline || col + width > cols as usize {
                row += 1;
                col = 0;
            }
            if row >= rows as usize {
                cells.rotate_left(cols as usize);
                let start = cells.len() - cols as usize;
                cells[start..].fill(TerminalCell {
                    background: bg,
                    foreground: fg,
                    ..Default::default()
                });
                row = rows as usize - 1;
            }
            if newline {
                continue;
            }
            let i = row * cols as usize + col;
            cells[i].text = c.to_string();
            if wide {
                cells[i].style = terminal_style::WIDE;
                cells[i + 1].style = terminal_style::WIDE_CONTINUATION;
            }
            col += width;
        }
        let native: Vec<_> = cells
            .iter()
            .map(|c| sys::OneUiTerminalCellUtf8 {
                text: sys::OneUiUtf8String::from_str(&c.text),
                foreground: c.foreground.into(),
                background: c.background.into(),
                style: c.style,
                hyperlink_id: 0,
                underline_style: 0,
                underline_color_set: 0,
                underline_color: fg.into(),
            })
            .collect();
        unsafe {
            sys::oneui_terminal_view_set_grid_utf8(
                self.raw,
                rows,
                cols,
                native.as_ptr(),
                native.len(),
            );
            sys::oneui_terminal_view_set_cursor(
                self.raw,
                row.min(rows as usize - 1) as u16,
                col.min(cols as usize - 1) as u16,
                1,
            );
        }
    }
}
struct Owner {
    terminals: Vec<TerminalView>,
    input: TextField,
    echoes: Vec<Rc<RefCell<Echo>>>,
}
impl Owner {
    fn new() -> Result<Self, oneui::Error> {
        let mut terminals = Vec::new();
        let mut echoes = Vec::new();
        for index in 0..8 {
            let mut terminal = TerminalView::new()?;
            terminal.set_font_family("Cascadia Mono, Consolas, Microsoft YaHei UI");
            terminal.set_font_size(15.0);
            terminal.set_line_height(1.35);
            terminal.set_cursor_blinking(false);
            terminal.set_palette(
                TerminalColor::rgb(28, 34, 40),
                TerminalColor::rgb(229, 236, 232),
                TerminalColor::rgb(196, 237, 135),
            );
            let raw = unsafe { terminal.as_widget().with_native_handle(|p| p) };
            let echo=Rc::new(RefCell::new(Echo{text:format!("OneUI sandbox / session {}\nAll connections and files shown here are simulated.\n\n> status\nSERVICE          STATE       UPTIME      PORT\napi-gateway      healthy     12d 06h     8080\njob-worker       healthy     12d 06h     9001\nmetrics-agent    healthy      8d 14h     9100\n\n> ls /srv/workspace\nconfig/    logs/       services/    scripts/\nREADME.md  compose.yaml  gateway.toml  deploy.sh\n\nReady. Type in the terminal or run help below.\n> ",index+1),rows:24,columns:100,raw}));
            let value = echo.clone();
            terminal.set_on_text_input(move |text| {
                let mut e = value.borrow_mut();
                e.text
                    .extend(text.chars().filter(|c| !c.is_control() || *c == '\n'));
                if e.text.len() > 32768 {
                    let at = e
                        .text
                        .char_indices()
                        .find(|(i, _)| *i >= 16384)
                        .map(|(i, _)| i)
                        .unwrap_or(0);
                    e.text.drain(..at);
                }
                e.paint();
            });
            let value = echo.clone();
            terminal.set_on_paste(move |text| {
                let mut e = value.borrow_mut();
                e.text
                    .extend(text.chars().filter(|c| !c.is_control() || *c == '\n'));
                if e.text.len() > 32768 {
                    let at = e
                        .text
                        .char_indices()
                        .find(|(i, _)| *i >= 16384)
                        .map(|(i, _)| i)
                        .unwrap_or(0);
                    e.text.drain(..at);
                }
                e.paint();
            });
            let value = echo.clone();
            terminal.set_on_raw_key(move |key| {
                if !key.pressed {
                    return;
                }
                let mut e = value.borrow_mut();
                if key.virtual_key == 13 {
                    e.text.push_str("\n> ");
                    e.paint();
                } else if key.virtual_key == 8 && !e.text.ends_with("> ") {
                    e.text.pop();
                    e.paint();
                }
            });
            let value = echo.clone();
            terminal.set_on_viewport_changed(move |size| {
                let mut e = value.borrow_mut();
                e.rows = size.rows;
                e.columns = size.columns;
                e.paint();
            });
            echo.borrow().paint();
            terminals.push(terminal);
            echoes.push(echo);
        }
        let input = TextField::new("输入中文；切换标签和主题后保留内容")?;
        Ok(Self {
            terminals,
            input,
            echoes,
        })
    }
}
// All entry points receive only C-compatible scalars and opaque borrowed handles.
#[no_mangle]
pub extern "C" fn terminal_demo_abi_version() -> u32 {
    1
}
#[no_mangle]
pub extern "C" fn terminal_demo_create_v1() -> *mut c_void {
    match Owner::new() {
        Ok(owner) => Box::into_raw(Box::new(owner)).cast(),
        Err(_) => std::ptr::null_mut(),
    }
}
#[no_mangle]
pub unsafe extern "C" fn terminal_demo_widget_v1(
    owner: *mut c_void,
    index: u32,
) -> *mut sys::OneUiWidget {
    let Some(owner) = owner.cast::<Owner>().as_ref() else {
        return std::ptr::null_mut();
    };
    let widget = match index {
        0 | 1 => owner.terminals[index as usize].as_widget(),
        3..=8 => owner.terminals[index as usize - 1].as_widget(),
        2 => owner.input.as_widget(),
        _ => return std::ptr::null_mut(),
    };
    widget.with_native_handle(|p| p)
}
#[no_mangle]
pub unsafe extern "C" fn terminal_demo_clear_v1(owner: *mut c_void, index: u32) {
    if let Some(owner) = owner.cast::<Owner>().as_ref() {
        if let Some(e) = owner.echoes.get(index as usize) {
            let mut e = e.borrow_mut();
            e.text = "> ".into();
            e.paint();
        }
    }
}
#[no_mangle]
pub unsafe extern "C" fn terminal_demo_destroy_v1(owner: *mut c_void) {
    if !owner.is_null() {
        drop(Box::from_raw(owner.cast::<Owner>()));
    }
}

// No shell execution: the fixture only recognizes these local demonstration commands.
#[no_mangle]
pub unsafe extern "C" fn terminal_demo_command_v1(
    owner: *mut c_void,
    index: u32,
    command: sys::OneUiUtf8String,
) {
    let Some(owner) = owner.cast::<Owner>().as_ref() else {
        return;
    };
    let Some(echo) = owner.echoes.get(index as usize) else {
        return;
    };
    if command.data.is_null() || command.length > 32768 {
        return;
    }
    let bytes = std::slice::from_raw_parts(command.data.cast::<u8>(), command.length);
    let command = String::from_utf8_lossy(bytes);
    let command = command.trim();
    let mut echo = echo.borrow_mut();
    if command == "clear" {
        echo.text = "> ".into();
    } else {
        echo.text.push_str(command);
        echo.text.push('\n');
        echo.text.push_str(match command {
            "help" => "Local demo commands: help, ls, status, clear. No system shell.\n",
            "ls" => "config/  logs/  services/  scripts/  README.md  compose.yaml\n",
            "status" => "api-gateway: healthy  |  job-worker: healthy  |  metrics-agent: healthy\n",
            _ => "Local echo only. Try help, ls, status or clear.\n",
        });
        echo.text.push_str("> ");
    }
    echo.paint();
}
