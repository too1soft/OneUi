use super::*;
use page::SettingsBasicCommand as Command;

#[test]
fn settings_validation_and_defaults() {
    let mut vm = vm::Settings::default();
    let cx = VmContext::default();
    assert_eq!(vm.themes.len(), 12);
    vm.set_scale("NaN".into(), &cx).unwrap();
    vm.invoke(Command::Save, &cx).unwrap();
    assert!(!vm.error.is_empty());
    vm.invoke(Command::Reset, &cx).unwrap();
    assert_eq!(vm.scale, "1.00");
    assert!(vm.error.is_empty());
    assert!(vm.set_language(9, &cx).is_err());
    assert!(vm.set_titlebar(-1, &cx).is_err());
}
#[test]
fn failure_retry_cancel_and_stale_result() {
    let mut vm = vm::Settings::default();
    let cx = VmContext::default();
    vm.invoke(Command::FailNext, &cx).unwrap();
    vm.invoke(Command::Save, &cx).unwrap();
    assert_eq!(vm.status, "操作失败");
    assert!(!vm.error.is_empty());
    vm.invoke(Command::Save, &cx).unwrap();
    assert_eq!(vm.status, "设置已保存");
    let request = Rc::new(RefCell::new(None));
    let target = request.clone();
    vm.start = Some(Box::new(move |r| {
        *target.borrow_mut() = Some(r);
    }));
    vm.invoke(Command::ChooseBackground, &cx).unwrap();
    assert!(vm.busy);
    vm.invoke(Command::CancelBackground, &cx).unwrap();
    vm.complete(request.borrow_mut().take().unwrap().perform())
        .unwrap();
    assert!(vm.background_path.is_empty());
    assert!(!vm.busy);
}
#[test]
fn theme_selection_preserves_item_identity() {
    use page::ThemeItemVm;
    let vm = vm::Settings::default();
    let item = vm.themes.get(8).unwrap().clone();
    item.borrow_mut()
        .invoke(page::ThemeItemCommand::Select, &VmContext::default())
        .unwrap();
    assert_eq!(vm.selected_theme(), themes::THEMES[8]);
    assert!(item.borrow().selected());
    assert!(Rc::ptr_eq(&item, vm.themes.get(8).unwrap()));
}
