//! Short-lived native models keep the compatibility facade Clone/Send like its IDs.
//! All layout rules and mutations execute in the shared C++ SDK.
use super::snapshot::{rect, rectangle, Encoded};
use super::*;
use crate::sys;
use std::{
    ffi::c_void,
    panic::{catch_unwind, AssertUnwindSafe},
    ptr::NonNull,
};
pub(super) fn checked(value: i32) -> Result<(), DockError> {
    match value {
        0 => Ok(()),
        2 => Err(DockError::DuplicateId),
        3 => Err(DockError::MissingPane),
        4 => Err(DockError::InvalidTarget),
        _ => Err(DockError::InvalidGeometry),
    }
}
struct Native(NonNull<sys::OneUiDockModel>);
impl Native {
    fn new<K: Clone + Ord>(encoded: &Encoded<K>, sequence: u64) -> Result<Self, DockError> {
        let mut error = 0;
        let raw = unsafe { sys::oneui_dock_model_create(&encoded.raw(), &mut error) };
        checked(error)?;
        let value = Self(NonNull::new(raw).ok_or(DockError::InvalidGeometry)?);
        unsafe { sys::oneui_dock_model_preserve_sequence(value.0.as_ptr(), sequence) };
        Ok(value)
    }
}
impl Drop for Native {
    fn drop(&mut self) {
        unsafe { sys::oneui_dock_model_destroy(self.0.as_ptr()) };
    }
}
pub(super) fn validate<K: Clone + Ord>(state: &DockSnapshot<K>) -> Result<u64, DockError> {
    let encoded = Encoded::new(state);
    let native = Native::new(&encoded, 0)?;
    Ok(unsafe { sys::oneui_dock_model_sequence(native.0.as_ptr()) })
}
pub(super) fn edit<K: Clone + Ord>(
    state: &DockSnapshot<K>,
    sequence: u64,
    command: i32,
    id: Option<&K>,
    target: Option<&K>,
    bounds: Rect,
    value: f32,
    path: &[bool],
) -> Result<(DockSnapshot<K>, u64), DockError> {
    let encoded = Encoded::new(state);
    let native = Native::new(&encoded, sequence)?;
    let bits: Vec<u8> = path.iter().map(|v| u8::from(*v)).collect();
    checked(unsafe {
        sys::oneui_dock_model_apply(
            native.0.as_ptr(),
            command,
            id.map(|key| encoded.id(key)).unwrap_or(0),
            target.map(|key| encoded.id(key)).unwrap_or(0),
            rect(bounds),
            value,
            bits.as_ptr(),
            bits.len(),
        )
    })?;
    struct Output<'a, K> {
        encoded: &'a Encoded<K>,
        state: Option<DockSnapshot<K>>,
    }
    unsafe extern "C" fn receive<K: Clone + Ord>(
        snapshot: *const sys::OneUiDockSnapshot,
        data: *mut c_void,
    ) {
        let value = &mut *data.cast::<Output<'_, K>>();
        let _ = catch_unwind(AssertUnwindSafe(|| {
            value.state = Some(value.encoded.decode(&*snapshot))
        }));
    }
    let mut output = Output {
        encoded: &encoded,
        state: None,
    };
    checked(unsafe {
        sys::oneui_dock_model_snapshot(
            native.0.as_ptr(),
            Some(receive::<K>),
            (&mut output as *mut Output<'_, K>).cast(),
        )
    })?;
    let sequence = unsafe { sys::oneui_dock_model_sequence(native.0.as_ptr()) };
    Ok((output.state.ok_or(DockError::InvalidGeometry)?, sequence))
}
pub(super) fn geometry<K: Clone + Ord>(
    root: &DockNode<K>,
    bounds: Rect,
    gap: f32,
    policy: impl Fn(&K) -> PaneMinimum,
) -> Result<DockGeometry<K>, DockError> {
    let state = root_state(root.clone());
    let encoded = Encoded::new(&state);
    let native = Native::new(&encoded, 0)?;
    struct Data<'a, K, F> {
        keys: &'a [K],
        policy: F,
        output: DockGeometry<K>,
        failed: bool,
    }
    unsafe extern "C" fn minimum<K: Clone + Ord, F: Fn(&K) -> PaneMinimum>(
        id: u64,
        data: *mut c_void,
    ) -> sys::OneUiDockSize {
        let value = &mut *data.cast::<Data<'_, K, F>>();
        match catch_unwind(AssertUnwindSafe(|| {
            (value.policy)(&value.keys[id as usize - 1])
        })) {
            Ok(size) => sys::OneUiDockSize {
                width: size.width,
                height: size.height,
            },
            Err(_) => {
                value.failed = true;
                sys::OneUiDockSize {
                    width: f32::NAN,
                    height: f32::NAN,
                }
            }
        }
    }
    unsafe extern "C" fn receive<K: Clone + Ord, F>(
        kind: i32,
        id: u64,
        axis: i32,
        path: *const u8,
        length: usize,
        bounds: sys::OneUiRect,
        data: *mut c_void,
    ) {
        let value = &mut *data.cast::<Data<'_, K, F>>();
        if catch_unwind(AssertUnwindSafe(|| {
            if kind == 0 {
                value
                    .output
                    .panes
                    .insert(value.keys[id as usize - 1].clone(), rectangle(bounds));
            } else {
                let path = if length == 0 {
                    Vec::new()
                } else {
                    std::slice::from_raw_parts(path, length)
                        .iter()
                        .map(|v| *v != 0)
                        .collect()
                };
                value.output.dividers.push(DividerGeometry {
                    path,
                    axis: if axis == 0 {
                        DockAxis::Horizontal
                    } else {
                        DockAxis::Vertical
                    },
                    bounds: rectangle(bounds),
                });
            }
        }))
        .is_err()
        {
            value.failed = true;
        }
    }
    fn run<K: Clone + Ord, F: Fn(&K) -> PaneMinimum>(
        native: &Native,
        data: &mut Data<'_, K, F>,
        bounds: Rect,
        gap: f32,
    ) -> i32 {
        let ptr = (data as *mut Data<'_, K, F>).cast();
        unsafe {
            sys::oneui_dock_model_geometry(
                native.0.as_ptr(),
                rect(bounds),
                gap,
                Some(minimum::<K, F>),
                ptr,
                Some(receive::<K, F>),
                ptr,
            )
        }
    }
    let mut data = Data {
        keys: &encoded.keys,
        policy,
        output: DockGeometry {
            panes: BTreeMap::new(),
            dividers: Vec::new(),
        },
        failed: false,
    };
    checked(run(&native, &mut data, bounds, gap))?;
    if data.failed {
        return Err(DockError::InvalidGeometry);
    }
    Ok(data.output)
}
pub(super) fn root_state<K>(node: DockNode<K>) -> DockSnapshot<K> {
    DockSnapshot {
        root: Some(node),
        floating: BTreeMap::new(),
        hidden: BTreeMap::new(),
        maximized: None,
    }
}

pub(super) fn minimum<K: Clone + Ord>(
    root: &DockNode<K>,
    gap: f32,
    policy: &impl Fn(&K) -> PaneMinimum,
) -> PaneMinimum {
    struct Data<'a, K, F> {
        keys: &'a [K],
        policy: &'a F,
    }
    unsafe extern "C" fn invoke<K, F: Fn(&K) -> PaneMinimum>(
        id: u64,
        data: *mut c_void,
    ) -> sys::OneUiDockSize {
        let data = &*data.cast::<Data<'_, K, F>>();
        match catch_unwind(AssertUnwindSafe(|| {
            (data.policy)(&data.keys[id as usize - 1])
        })) {
            Ok(size) => sys::OneUiDockSize {
                width: size.width,
                height: size.height,
            },
            Err(_) => sys::OneUiDockSize {
                width: f32::NAN,
                height: f32::NAN,
            },
        }
    }
    fn run<K, F: Fn(&K) -> PaneMinimum>(
        native: &Native,
        gap: f32,
        data: &Data<'_, K, F>,
        result: &mut sys::OneUiDockSize,
    ) {
        unsafe {
            sys::oneui_dock_model_minimum(
                native.0.as_ptr(),
                gap,
                Some(invoke::<K, F>),
                (data as *const Data<'_, K, F>).cast_mut().cast(),
                result,
            )
        };
    }
    let encoded = Encoded::new(&root_state(root.clone()));
    let mut size = sys::OneUiDockSize {
        width: f32::NAN,
        height: f32::NAN,
    };
    if let Ok(native) = Native::new(&encoded, 0) {
        run(
            &native,
            gap,
            &Data {
                keys: &encoded.keys,
                policy,
            },
            &mut size,
        );
    }
    PaneMinimum {
        width: size.width,
        height: size.height,
    }
}
