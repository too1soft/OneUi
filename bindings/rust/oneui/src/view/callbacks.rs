//! Every foreign entry, including retain/release and cleanup, contains panic.
use super::{model::Control, Value, ViewError, ViewNotifier, VmResult};
use crate::sys;
use std::{ffi::c_void, rc::Rc};

fn guarded(action: impl FnOnce() -> VmResult) -> i32 {
    #[cfg(feature = "view-diagnostics")]
    super::diagnostics::record(|s| s.callbacks += 1);
    super::super::run_callback_guarded("view.adapter", action)
        .unwrap_or(Err(ViewError::FOREIGN_FAILURE))
        .map(|_| 0)
        .unwrap_or_else(|e| e.code())
}
unsafe fn local(vm: *mut c_void) -> Rc<Control> {
    let pointer = vm.cast::<Control>();
    Rc::increment_strong_count(pointer);
    Rc::from_raw(pointer)
}
unsafe extern "C" fn retain(vm: *mut c_void) {
    guarded(|| {
        Rc::increment_strong_count(vm.cast::<Control>());
        Ok(())
    });
}
unsafe extern "C" fn release(vm: *mut c_void) {
    guarded(|| {
        drop(Rc::from_raw(vm.cast::<Control>()));
        Ok(())
    });
}
unsafe extern "C" fn connect(vm: *mut c_void, notifier: *mut sys::OneUiViewNotifier) -> i32 {
    #[cfg(feature = "view-diagnostics")]
    super::diagnostics::record(|s| {
        s.live_connections += 1;
        s.peak_connections = s.peak_connections.max(s.live_connections);
    });
    guarded(|| {
        let owner = local(vm);
        let notifier = ViewNotifier::borrowed(notifier)?;
        owner
            .connections
            .borrow_mut()
            .notifiers
            .push(notifier.clone());
        owner.model.connect(&super::VmContext {
            notifiers: vec![notifier],
        })
    })
}
unsafe extern "C" fn disconnect(vm: *mut c_void, notifier: *mut sys::OneUiViewNotifier) {
    #[cfg(feature = "view-diagnostics")]
    super::diagnostics::record(|s| s.live_connections -= 1);
    guarded(|| {
        let owner = local(vm);
        // Remove before invoking user cleanup, including on panic/borrow failure.
        let removed = {
            let mut cx = owner.connections.borrow_mut();
            cx.notifiers
                .iter()
                .position(|n| n.raw.as_ptr() == notifier)
                .map(|i| cx.notifiers.remove(i))
        };
        if let Some(notifier) = removed {
            owner.model.disconnect(&super::VmContext {
                notifiers: vec![notifier],
            })?;
        }
        Ok(())
    });
}
fn write(owner: &Control, value: Value, out: *mut sys::OneUiValue) -> VmResult {
    let mut scratch = owner
        .scratch
        .try_borrow_mut()
        .map_err(|_| ViewError::REENTRANT)?;
    *scratch = value;
    let raw = scratch.borrow()?;
    unsafe {
        *out = raw;
    }
    Ok(())
}
unsafe extern "C" fn get(vm: *mut c_void, id: u32, out: *mut sys::OneUiValue) -> i32 {
    #[cfg(feature = "view-diagnostics")]
    super::diagnostics::record(|s| s.gets += 1);
    *out = sys::OneUiValue::default();
    guarded(|| {
        let owner = local(vm);
        write(&owner, owner.model.get(id)?, out)
    })
}
unsafe extern "C" fn set(vm: *mut c_void, id: u32, value: *const sys::OneUiValue) -> i32 {
    #[cfg(feature = "view-diagnostics")]
    super::diagnostics::record(|s| s.sets += 1);
    guarded(|| {
        let owner = local(vm);
        let value = Value::copy(&*value)?;
        let cx = owner.connections.borrow().clone();
        owner.model.set(id, &value, &cx)
    })
}
unsafe extern "C" fn invoke(
    vm: *mut c_void,
    id: u32,
    args: *const sys::OneUiValue,
    count: usize,
) -> i32 {
    #[cfg(feature = "view-diagnostics")]
    super::diagnostics::record(|s| s.invokes += 1);
    guarded(|| {
        let owner = local(vm);
        let args = if count == 0 {
            &[]
        } else {
            std::slice::from_raw_parts(args, count)
        };
        let values = args
            .iter()
            .map(|v| Value::copy(v))
            .collect::<VmResult<Vec<_>>>()?;
        let cx = owner.connections.borrow().clone();
        owner.model.invoke(id, &values, &cx)
    })
}
unsafe extern "C" fn list_info(vm: *mut c_void, id: u32, out: *mut sys::OneUiViewListInfo) -> i32 {
    #[cfg(feature = "view-diagnostics")]
    super::diagnostics::record(|s| s.list_info += 1);
    *out = sys::OneUiViewListInfo::default();
    guarded(|| {
        let owner = local(vm);
        *out = owner.model.list_info(id)?;
        owner
            .children
            .borrow_mut()
            .retain(|_, value| value.strong_count() != 0);
        Ok(())
    })
}
unsafe extern "C" fn list_key(
    vm: *mut c_void,
    id: u32,
    index: u64,
    out: *mut sys::OneUiValue,
) -> i32 {
    #[cfg(feature = "view-diagnostics")]
    super::diagnostics::record(|s| s.list_keys += 1);
    *out = sys::OneUiValue::default();
    guarded(|| {
        let owner = local(vm);
        let index = usize::try_from(index).map_err(|_| ViewError::LIMIT)?;
        write(&owner, Value::Utf8(owner.model.list_key(id, index)?), out)
    })
}
unsafe extern "C" fn list_acquire(
    vm: *mut c_void,
    id: u32,
    index: u64,
    out: *mut sys::OneUiViewModelRef,
) -> i32 {
    #[cfg(feature = "view-diagnostics")]
    super::diagnostics::record(|s| s.list_acquires += 1);
    *out = sys::OneUiViewModelRef {
        vm: std::ptr::null_mut(),
        vtable: std::ptr::null(),
    };
    guarded(|| {
        let owner = local(vm);
        let child = owner.child(id, usize::try_from(index).map_err(|_| ViewError::LIMIT)?)?;
        *out = Control::borrowed(&child);
        let _ = Rc::into_raw(child); // transfers exactly one lease
        Ok(())
    })
}
pub(crate) fn table(schema_hash: [u8; 32], model_id: u32) -> sys::OneUiViewModelVTable {
    sys::OneUiViewModelVTable {
        struct_size: std::mem::size_of::<sys::OneUiViewModelVTable>() as u32,
        protocol_version: 1,
        schema_hash,
        model_id,
        flags: 0,
        retain: Some(retain),
        release: Some(release),
        connect: Some(connect),
        disconnect: Some(disconnect),
        get: Some(get),
        set: Some(set),
        invoke: Some(invoke),
        list_info: Some(list_info),
        list_key: Some(list_key),
        list_acquire: Some(list_acquire),
    }
}
