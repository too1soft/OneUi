use crate::{page::*, themes::*};
use oneui::view::{PropertyId, StableList, VmContext, VmResult};
use std::rc::Rc;

#[derive(Clone, Debug)]
pub struct Request {
    pub generation: u64,
    pub background: bool,
    pub fail: bool,
}
#[derive(Clone, Debug)]
pub struct Completion {
    pub request: Request,
    pub result: Result<String, String>,
}
impl Request {
    // The worker owns values only. No Rc, View, notifier or UI VM crosses threads.
    pub fn perform(self) -> Completion {
        let result = if self.fail {
            Err("模拟保存失败，请重试".into())
        } else {
            Ok(if self.background {
                "demo/background.png"
            } else {
                "设置已保存"
            }
            .into())
        };
        Completion {
            request: self,
            result,
        }
    }
}
pub struct Settings {
    pub themes: StableList<Theme>,
    pub languages: StableList<Choice>,
    pub titlebars: StableList<Choice>,
    pub close_actions: StableList<Choice>,
    pub theme: Rc<ThemeState>,
    pub language: i64,
    pub titlebar: i64,
    pub compact: bool,
    pub close_action: i64,
    pub launch_at_login: bool,
    pub start_minimized: bool,
    pub font_family: String,
    pub scale: String,
    pub background_path: String,
    pub background_strength: String,
    pub content_opacity: String,
    pub status: String,
    pub error: String,
    pub busy: bool,
    generation: u64,
    fail_next: bool,
    pub start: Option<Box<dyn Fn(Request)>>,
}
impl Default for Settings {
    fn default() -> Self {
        let theme = Rc::new(ThemeState::default());
        Self {
            themes: themes(&theme),
            theme,
            languages: choices(&["简体中文", "English", "日本語"]),
            titlebars: choices(&["系统标题栏", "应用标题栏"]),
            close_actions: choices(&["退出应用", "最小化到托盘"]),
            language: 0,
            titlebar: 0,
            compact: false,
            close_action: 0,
            launch_at_login: false,
            start_minimized: false,
            font_family: "Segoe UI".into(),
            scale: "1.00".into(),
            background_path: String::new(),
            background_strength: "35".into(),
            content_opacity: "95".into(),
            status: "设置已就绪".into(),
            error: String::new(),
            busy: false,
            generation: 0,
            fail_next: false,
            start: None,
        }
    }
}
impl Settings {
    fn changed(&mut self, cx: &VmContext, id: PropertyId) -> VmResult {
        self.status = "有未保存的更改".into();
        self.error.clear();
        if id == COMPACT {
            if let Some(changed) = self.theme.presentation.borrow().as_ref() {
                changed();
            }
        }
        cx.notify(id)?;
        cx.notify(STATUS)?;
        cx.notify(ERROR)
    }
    fn validate(&self) -> Result<(), String> {
        for (value, low, high, name) in [
            (&self.scale, 0.75, 2.0, "界面比例"),
            (&self.background_strength, 0.0, 100.0, "背景强度"),
            (&self.content_opacity, 0.0, 100.0, "内容不透明度"),
        ] {
            let number = value.parse::<f64>().unwrap_or(f64::NAN);
            if !(low..=high).contains(&number) {
                return Err(format!("{name}应在 {low}–{high} 之间"));
            }
        }
        if self.font_family.trim().is_empty() {
            return Err("字体族不能为空".into());
        }
        Ok(())
    }
    pub fn complete(&mut self, completion: Completion) -> VmResult {
        if completion.request.generation != self.generation {
            return Ok(());
        }
        self.busy = false;
        match completion.result {
            Ok(value) => {
                self.error.clear();
                if completion.request.background {
                    self.background_path = value;
                    self.status = "背景已选择".into();
                } else {
                    self.status = value;
                }
            }
            Err(error) => {
                self.error = error;
                self.status = "操作失败".into();
            }
        }
        self.theme.notify_roots(PropertyId::ALL)
    }
    fn reset(&mut self) -> VmResult {
        self.generation += 1;
        self.busy = false;
        self.fail_next = false;
        self.language = 0;
        self.titlebar = 0;
        self.compact = false;
        self.close_action = 0;
        self.launch_at_login = false;
        self.start_minimized = false;
        self.font_family = "Segoe UI".into();
        self.scale = "1.00".into();
        self.background_path.clear();
        self.background_strength = "35".into();
        self.content_opacity = "95".into();
        self.error.clear();
        self.status = "已恢复默认".into();
        self.theme.select(0)
    }
}
macro_rules! scalar {
    ($get:ident,$set:ident,$ty:ty,$id:ident) => {
        fn $get(&self) -> $ty {
            self.$get
        }
        fn $set(&mut self, value: $ty, cx: &VmContext) -> VmResult {
            self.$get = value;
            self.changed(cx, $id)
        }
    };
}
macro_rules! text {
    ($get:ident,$set:ident,$id:ident) => {
        fn $get(&self) -> &str {
            &self.$get
        }
        fn $set(&mut self, value: String, cx: &VmContext) -> VmResult {
            self.$get = value;
            self.changed(cx, $id)
        }
    };
}
impl SettingsBasicVm for Settings {
    type ThemesItem = Theme;
    type LanguagesItem = Choice;
    type TitlebarsItem = Choice;
    type CloseActionsItem = Choice;
    fn themes(&self) -> &StableList<Theme> {
        &self.themes
    }
    fn languages(&self) -> &StableList<Choice> {
        &self.languages
    }
    fn titlebars(&self) -> &StableList<Choice> {
        &self.titlebars
    }
    fn close_actions(&self) -> &StableList<Choice> {
        &self.close_actions
    }
    fn language(&self) -> i64 {
        self.language
    }
    fn set_language(&mut self, v: i64, cx: &VmContext) -> VmResult {
        if !(0..3).contains(&v) {
            return Err(oneui::view::ViewError::TYPE);
        }
        self.language = v;
        self.changed(cx, LANGUAGE)
    }
    fn titlebar(&self) -> i64 {
        self.titlebar
    }
    fn set_titlebar(&mut self, v: i64, cx: &VmContext) -> VmResult {
        if !(0..2).contains(&v) {
            return Err(oneui::view::ViewError::TYPE);
        }
        self.titlebar = v;
        self.changed(cx, TITLEBAR)
    }
    fn close_action(&self) -> i64 {
        self.close_action
    }
    fn set_close_action(&mut self, v: i64, cx: &VmContext) -> VmResult {
        if !(0..2).contains(&v) {
            return Err(oneui::view::ViewError::TYPE);
        }
        self.close_action = v;
        self.changed(cx, CLOSE_ACTION)
    }
    scalar!(compact, set_compact, bool, COMPACT);
    scalar!(launch_at_login, set_launch_at_login, bool, LAUNCH_AT_LOGIN);
    scalar!(start_minimized, set_start_minimized, bool, START_MINIMIZED);
    text!(font_family, set_font_family, FONT_FAMILY);
    text!(scale, set_scale, SCALE);
    text!(background_path, set_background_path, BACKGROUND_PATH);
    text!(
        background_strength,
        set_background_strength,
        BACKGROUND_STRENGTH
    );
    text!(content_opacity, set_content_opacity, CONTENT_OPACITY);
    fn status(&self) -> &str {
        &self.status
    }
    fn error(&self) -> &str {
        &self.error
    }
    fn busy(&self) -> bool {
        self.busy
    }
    fn selected_theme(&self) -> &str {
        crate::themes::THEMES[self.theme.selected.get()]
    }
    fn on_view_connect(&mut self, cx: &VmContext) -> VmResult {
        self.theme.connect(cx, true);
        Ok(())
    }
    fn on_view_disconnect(&mut self, cx: &VmContext) {
        self.theme.disconnect(cx, true);
    }
    fn invoke(&mut self, command: SettingsBasicCommand, cx: &VmContext) -> VmResult {
        use SettingsBasicCommand::*;
        match command {
            Reset => self.reset()?,
            FailNext => {
                self.fail_next = true;
                self.status = "下次操作将模拟失败".into();
            }
            CancelBackground => {
                self.generation += 1;
                self.busy = false;
                self.status = "已取消，保留原背景".into();
            }
            ClearBackground => {
                self.generation += 1;
                self.busy = false;
                self.background_path.clear();
                self.status = "已清除背景".into();
            }
            Save | ChooseBackground => {
                if self.busy {
                    return Ok(());
                }
                if let Err(error) = self.validate() {
                    self.error = error;
                    return cx.notify(ERROR);
                }
                self.generation += 1;
                self.busy = true;
                self.error.clear();
                self.status = "正在处理…".into();
                let request = Request {
                    generation: self.generation,
                    background: matches!(command, ChooseBackground),
                    fail: self.fail_next,
                };
                self.fail_next = false;
                if let Some(start) = &self.start {
                    start(request);
                } else {
                    self.complete(request.perform())?;
                }
            }
        }
        cx.notify(PropertyId::ALL)
    }
}
