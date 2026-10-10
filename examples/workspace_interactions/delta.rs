use oneui::view::{PropertyId, StableList, VmContext, VmResult};
use std::{cell::RefCell, rc::Rc};
// Normal edits use only keyed REMOVE/INSERT/MOVE/UPDATE, never RESET.
pub fn sync<T: Clone + PartialEq>(
    list: &mut StableList<T>,
    desired: Vec<(String, T)>,
    cx: &VmContext,
    property: PropertyId,
) -> VmResult {
    for i in (0..list.len()).rev() {
        if !desired.iter().any(|v| v.0 == list.key(i).unwrap()) {
            let change = list.remove(i)?;
            cx.notify_list(property, &change)?;
        }
    }
    for (i, (key, value)) in desired.into_iter().enumerate() {
        let old = list.iter().position(|v| v.0 == key);
        if let Some(old) = old {
            if old != i {
                let change = list.move_item(old, i)?;
                cx.notify_list(property, &change)?;
            }
            let changed = *list.get(i).unwrap().borrow() != value;
            if changed {
                *list.get(i).unwrap().borrow_mut() = value;
                let change = list.update(i, &[])?;
                cx.notify_list(property, &change)?;
            }
        } else {
            let change = list.insert(i, key, Rc::new(RefCell::new(value)))?;
            cx.notify_list(property, &change)?;
        }
    }
    Ok(())
}
