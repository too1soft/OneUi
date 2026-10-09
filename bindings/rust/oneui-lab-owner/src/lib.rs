//! Sample ABI v1: UI-thread only. C++ retains this owner while its NativeHosts live.
//! Local echo fixture only, not a PTY/ANSI emulator.
use oneui::{TerminalColor, TerminalView, TextField};
use oneui_sys as sys;
use std::{cell::RefCell, ffi::c_void, rc::Rc};

mod echo;
use echo::Echo;

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
            terminal.set_font_family(
                "Cascadia Mono, Menlo, DejaVu Sans Mono, Consolas, PingFang SC, Microsoft YaHei UI",
            );
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
            terminal.set_on_text_input(move |text| value.borrow_mut().append(&text));
            let value = echo.clone();
            terminal.set_on_paste(move |text| value.borrow_mut().append(&text));
            let value = echo.clone();
            terminal.set_on_raw_key(move |key| {
                if !key.pressed {
                    return;
                }
                let mut e = value.borrow_mut();
                if key.virtual_key == 13 {
                    e.text.push_str("\n> ");
                    e.trim();
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
    std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| match Owner::new() {
        Ok(owner) => Box::into_raw(Box::new(owner)).cast(),
        Err(_) => std::ptr::null_mut(),
    }))
    .unwrap_or(std::ptr::null_mut())
}
#[no_mangle]
pub unsafe extern "C" fn terminal_demo_widget_v1(
    owner: *mut c_void,
    index: u32,
) -> *mut sys::OneUiWidget {
    std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
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
    }))
    .unwrap_or(std::ptr::null_mut())
}
#[no_mangle]
pub unsafe extern "C" fn terminal_demo_clear_v1(owner: *mut c_void, index: u32) {
    std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        if let Some(owner) = owner.cast::<Owner>().as_ref() {
            if let Some(e) = owner.echoes.get(index as usize) {
                let mut e = e.borrow_mut();
                e.text = "> ".into();
                e.paint();
            }
        }
    }))
    .unwrap_or(())
}
#[no_mangle]
pub unsafe extern "C" fn terminal_demo_destroy_v1(owner: *mut c_void) {
    std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        if !owner.is_null() {
            drop(Box::from_raw(owner.cast::<Owner>()));
        }
    }))
    .unwrap_or(())
}

// No shell execution: the fixture only recognizes these local demonstration commands.
#[no_mangle]
pub unsafe extern "C" fn terminal_demo_command_v1(
    owner: *mut c_void,
    index: u32,
    command: sys::OneUiUtf8String,
) {
    std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
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
                "status" => {
                    "api-gateway: healthy  |  job-worker: healthy  |  metrics-agent: healthy\n"
                }
                _ => "Local echo only. Try help, ls, status or clear.\n",
            });
            echo.text.push_str("> ");
        }
        echo.trim();
        echo.paint();
    }))
    .unwrap_or(())
}
