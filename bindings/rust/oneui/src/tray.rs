//! Generic application-owned content for the existing window tray icon.
use super::*;

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct TrayMenuItem {
    pub id: i32,
    pub text: String,
    pub enabled: bool,
    pub separator: bool,
}

impl UiDispatcher {
    /// Replace the menu on the owning UI thread. Empty restores the default menu.
    pub fn set_tray_menu(&self, items: &[TrayMenuItem]) -> Result<bool, Error> {
        if std::thread::current().id() != self.state.ui_thread {
            return Err(Error::WrongThread);
        }
        let strings: Vec<_> = items
            .iter()
            .map(|i| wide_null_terminated(&i.text))
            .collect();
        let native: Vec<_> = items
            .iter()
            .zip(&strings)
            .map(|(i, s)| sys::OneUiTrayMenuItem {
                id: i.id,
                text: s.as_ptr(),
                enabled: i32::from(i.enabled),
                separator: i32::from(i.separator),
            })
            .collect();
        self.state
            .with_raw(|raw| unsafe {
                sys::oneui_window_set_tray_menu(raw, native.as_ptr(), native.len()) != 0
            })
            .ok_or(Error::WindowClosed)
    }
    pub fn set_tray_tooltip(&self, text: &str) -> Result<bool, Error> {
        if std::thread::current().id() != self.state.ui_thread {
            return Err(Error::WrongThread);
        }
        let text = wide_null_terminated(text);
        self.state
            .with_raw(|raw| unsafe { sys::oneui_window_set_tray_tooltip(raw, text.as_ptr()) != 0 })
            .ok_or(Error::WindowClosed)
    }
}
/// Checked state is controlled by the application; activation only reports an ID.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum MenuCheck {
    #[default]
    None,
    Check,
    Radio,
}

/// Shared menu tree for the tray and application menu bar.
/// IDs must be unique among leaves; radio groups are local to sibling items.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct NativeMenuItem {
    pub id: i32,
    pub text: String,
    pub enabled: bool,
    pub separator: bool,
    pub check: MenuCheck,
    pub checked: bool,
    pub radio_group: u32,
    pub shortcut: Option<KeyChord>,
    pub children: Vec<NativeMenuItem>,
}
impl NativeMenuItem {
    pub fn new(id: i32, text: impl Into<String>) -> Self {
        Self {
            id,
            text: text.into(),
            enabled: true,
            separator: false,
            check: MenuCheck::None,
            checked: false,
            radio_group: 0,
            shortcut: None,
            children: Vec::new(),
        }
    }
    pub fn submenu(text: impl Into<String>, children: Vec<Self>) -> Self {
        Self {
            children,
            ..Self::new(0, text)
        }
    }
    pub fn separator() -> Self {
        Self {
            separator: true,
            ..Self::new(0, "")
        }
    }
}

// CString buffers and child Vec allocations stay stable when this owner moves.
struct OwnedMenu {
    _strings: Vec<CString>,
    _children: Vec<OwnedMenu>,
    raw: Vec<sys::OneUiNativeMenuItemUtf8>,
}
impl OwnedMenu {
    fn new(items: &[NativeMenuItem]) -> Result<Self, Error> {
        Self::copy(items, 1, &mut 1024)
    }
    fn copy(items: &[NativeMenuItem], depth: usize, budget: &mut usize) -> Result<Self, Error> {
        if depth > 8 || items.len() > *budget {
            return Err(Error::InvalidMenu);
        }
        *budget -= items.len();
        let mut result = Self {
            _strings: Vec::new(),
            _children: Vec::new(),
            raw: Vec::new(),
        };
        for item in items {
            if item.text.len() > 16384 {
                return Err(Error::InvalidMenu);
            }
            let text = CString::new(item.text.as_str()).map_err(|_| Error::InvalidMenu)?;
            let key = CString::new(item.shortcut.as_ref().map(|s| s.key.as_str()).unwrap_or(""))
                .map_err(|_| Error::InvalidMenu)?;
            let child = if item.children.is_empty() {
                None
            } else {
                Some(Self::copy(&item.children, depth + 1, budget)?)
            };
            result.raw.push(sys::OneUiNativeMenuItemUtf8 {
                id: item.id,
                text: text.as_ptr(),
                flags: u32::from(item.enabled)
                    | (u32::from(item.separator) << 1)
                    | match item.check {
                        MenuCheck::None => 0,
                        MenuCheck::Check => 4,
                        MenuCheck::Radio => 8,
                    }
                    | (u32::from(item.checked) << 4),
                radio_group: item.radio_group,
                shortcut_key: key.as_ptr(),
                shortcut_modifiers: item
                    .shortcut
                    .as_ref()
                    .map(|s| s.modifiers.bits())
                    .unwrap_or(0),
                children: child
                    .as_ref()
                    .map(|c| c.raw.as_ptr())
                    .unwrap_or(std::ptr::null()),
                child_count: item.children.len(),
            });
            result._strings.push(text);
            result._strings.push(key);
            if let Some(child) = child {
                result._children.push(child);
            }
        }
        Ok(result)
    }
}

impl UiDispatcher {
    /// Replace the tray menu. Empty restores the localized default menu.
    pub fn set_native_tray_menu(&self, items: &[NativeMenuItem]) -> Result<bool, Error> {
        self.set_native_menu(items, false)
    }
    /// Mac system menu; Windows/Linux client-area menu. Empty removes the menu.
    /// Shortcuts use KeyModifiers::PRIMARY for Command on Mac / Control elsewhere.
    pub fn set_menu_bar(&self, items: &[NativeMenuItem]) -> Result<bool, Error> {
        self.set_native_menu(items, true)
    }
    fn set_native_menu(&self, items: &[NativeMenuItem], bar: bool) -> Result<bool, Error> {
        if std::thread::current().id() != self.state.ui_thread {
            return Err(Error::WrongThread);
        }
        let menu = OwnedMenu::new(items)?;
        self.state
            .with_raw(|raw| unsafe {
                if bar {
                    sys::oneui_window_set_menu_bar_utf8(raw, menu.raw.as_ptr(), menu.raw.len()) != 0
                } else {
                    sys::oneui_window_set_tray_menu_utf8(raw, menu.raw.as_ptr(), menu.raw.len())
                        != 0
                }
            })
            .ok_or(Error::WindowClosed)
    }
    /// Copy tightly packed straight-alpha RGBA8 pixels (1..=256 per dimension).
    /// Zero dimensions and an empty slice restore the default icon.
    pub fn set_tray_icon_rgba(
        &self,
        width: u32,
        height: u32,
        rgba: &[u8],
        template_image: bool,
    ) -> Result<bool, Error> {
        if std::thread::current().id() != self.state.ui_thread {
            return Err(Error::WrongThread);
        }
        let reset = width == 0 && height == 0 && rgba.is_empty();
        if !reset
            && (width == 0
                || height == 0
                || width > 256
                || height > 256
                || rgba.len() != width as usize * height as usize * 4)
        {
            return Err(Error::InvalidImagePixels {
                reason: "tray icon must contain width * height * 4 RGBA bytes, at most 256 x 256",
            });
        }
        self.state
            .with_raw(|raw| unsafe {
                sys::oneui_window_set_tray_icon_rgba(
                    raw,
                    if reset {
                        std::ptr::null()
                    } else {
                        rgba.as_ptr()
                    },
                    width,
                    height,
                    width as usize * 4,
                    i32::from(template_image),
                ) != 0
            })
            .ok_or(Error::WindowClosed)
    }
}

// Clone before invoking: a callback can clear itself or drop its owning Window.
// Recursive activation is ignored rather than aliasing an active FnMut borrow.
pub(super) type MenuCallback = Rc<RefCell<Box<dyn FnMut(i32)>>>;
unsafe extern "C" fn run_menu_callback(value: i32, data: *mut std::ffi::c_void) {
    if data.is_null() {
        return;
    }
    let callback = unsafe { &*data.cast::<MenuCallback>() }.clone();
    run_callback_guarded("window.menu_action", || {
        if let Ok(mut handler) = callback.try_borrow_mut() {
            handler(value);
        }
    });
}
impl Window {
    pub fn set_native_tray_menu(&self, items: &[NativeMenuItem]) -> Result<bool, Error> {
        self.dispatcher().set_native_tray_menu(items)
    }
    pub fn set_menu_bar(&self, items: &[NativeMenuItem]) -> Result<bool, Error> {
        self.dispatcher().set_menu_bar(items)
    }
    pub fn set_tray_icon_rgba(
        &self,
        width: u32,
        height: u32,
        rgba: &[u8],
        template_image: bool,
    ) -> Result<bool, Error> {
        self.dispatcher()
            .set_tray_icon_rgba(width, height, rgba, template_image)
    }
    /// Receives the ID of an enabled tray menu leaf.
    pub fn set_on_tray_action<F: FnMut(i32) + 'static>(&mut self, callback: F) {
        self.clear_tray_action_callback();
        self.tray_action_callback = Some(Box::new(Rc::new(RefCell::new(Box::new(callback)))));
        let data = (self.tray_action_callback.as_deref_mut().unwrap() as *mut MenuCallback).cast();
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_set_on_tray_action(raw, Some(run_menu_callback), data)
        });
    }
    pub fn clear_tray_action_callback(&mut self) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_set_on_tray_action(raw, None, std::ptr::null_mut())
        });
        self.tray_action_callback = None;
    }
    /// Receives the ID of an enabled application menu leaf.
    pub fn set_on_menu_action<F: FnMut(i32) + 'static>(&mut self, callback: F) {
        self.clear_menu_action_callback();
        self.menu_action_callback = Some(Box::new(Rc::new(RefCell::new(Box::new(callback)))));
        let data = (self.menu_action_callback.as_deref_mut().unwrap() as *mut MenuCallback).cast();
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_set_on_menu_action(raw, Some(run_menu_callback), data)
        });
    }
    pub fn clear_menu_action_callback(&mut self) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_set_on_menu_action(raw, None, std::ptr::null_mut())
        });
        self.menu_action_callback = None;
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn copied_menu_keeps_nested_utf8_and_shortcuts_alive() {
        let mut leaf = NativeMenuItem::new(7, "保存");
        leaf.shortcut = Some(KeyChord {
            key: "s".into(),
            modifiers: KeyModifiers::PRIMARY,
        });
        leaf.check = MenuCheck::Check;
        leaf.checked = true;
        let model = vec![NativeMenuItem::submenu("文件", vec![leaf])];
        let copied = OwnedMenu::new(&model).unwrap();
        drop(model);
        let moved = vec![copied];
        let root = &moved[0].raw[0];
        unsafe {
            assert_eq!(CStr::from_ptr(root.text).to_str().unwrap(), "文件");
            assert_eq!((*root.children).id, 7);
            assert_eq!((*root.children).flags, 21);
            assert_eq!(
                CStr::from_ptr((*root.children).text).to_str().unwrap(),
                "保存"
            );
            assert_eq!((*root.children).shortcut_modifiers, 16);
        }
    }
    #[test]
    fn menu_copy_rejects_nul_and_excessive_depth() {
        assert!(OwnedMenu::new(&[NativeMenuItem::new(1, "bad\0text")]).is_err());
        let mut menu = NativeMenuItem::new(1, "Leaf");
        for _ in 0..8 {
            menu = NativeMenuItem::submenu("Parent", vec![menu]);
        }
        assert!(OwnedMenu::new(&[menu]).is_err());
    }
    #[test]
    fn menu_callback_can_release_its_owner() {
        let owner: Rc<RefCell<Option<Box<MenuCallback>>>> = Rc::new(RefCell::new(None));
        let weak = Rc::downgrade(&owner);
        *owner.borrow_mut() = Some(Box::new(Rc::new(RefCell::new(Box::new(move |_| {
            weak.upgrade().unwrap().borrow_mut().take();
        })))));
        let ptr = owner.borrow_mut().as_deref_mut().unwrap() as *mut MenuCallback;
        unsafe {
            run_menu_callback(1, ptr.cast());
        }
        assert!(owner.borrow().is_none());
    }
}
