use oneui::{terminal_style, TerminalCell, TerminalColor};
use oneui_sys as sys;

pub(crate) struct Echo {
    pub text: String,
    pub rows: u16,
    pub columns: u16,
    pub raw: *mut sys::OneUiWidget,
}
impl Echo {
    pub fn trim(&mut self) {
        if self.text.len() > 32768 {
            let at = self
                .text
                .char_indices()
                .find(|(i, _)| *i >= self.text.len() - 16384)
                .map(|(i, _)| i)
                .unwrap_or(0);
            self.text.drain(..at);
        }
    }
    pub fn append(&mut self, text: &str) {
        // Bound incoming paste before copying it; keep only complete UTF-8 scalars.
        let tail = text
            .char_indices()
            .find(|(i, _)| *i >= text.len().saturating_sub(16384))
            .map(|(i, _)| i)
            .unwrap_or(0);
        self.text.extend(
            text[tail..]
                .chars()
                .filter(|c| !c.is_control() || *c == '\n'),
        );
        self.trim();
        self.paint();
    }

    pub fn paint(&self) {
        let rows = self.rows.clamp(1, 256);
        let cols = self.columns.clamp(2, 512);
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
