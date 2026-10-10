use super::{check, HostRef, View, ViewError, ViewNotifier, VmResult};
use crate::sys;
use std::{marker::PhantomData, ptr::NonNull};

/// A stable render-instance segment. Slashes in a business key are literal.
#[derive(Clone, Copy, Debug)]
pub struct ScopeSegment<'a> {
    pub instance_id: &'a str,
    pub key: &'a str,
}

impl View {
    /// Resolve after instantiate/flush, outside a VM callback. The owned weak
    /// notifier can outlive removal, but never rebinds to a replacement VM.
    pub fn resolve_scope(&self, path: &[ScopeSegment<'_>]) -> VmResult<ViewNotifier> {
        let segments: Vec<_> = path
            .iter()
            .map(|s| sys::OneUiViewScopeSegment {
                instance_id: sys::OneUiUtf8String::from_str(s.instance_id),
                key: sys::OneUiUtf8String::from_str(s.key),
            })
            .collect();
        let mut raw = std::ptr::null_mut();
        check(unsafe {
            sys::oneui_view_resolve_scope(
                self.raw.as_ptr(),
                segments.as_ptr(),
                segments.len(),
                &mut raw,
            )
        })?;
        unsafe { ViewNotifier::owned(raw) }
    }

    pub fn find_host_in(&self, scope: &ViewNotifier, name: &str) -> VmResult<HostRef<'_>> {
        let mut raw = std::ptr::null_mut();
        check(unsafe {
            sys::oneui_view_find_host(
                self.raw.as_ptr(),
                scope.raw.as_ptr(),
                sys::OneUiUtf8String::from_str(name),
                &mut raw,
            )
        })?;
        Ok(HostRef {
            raw: NonNull::new(raw).ok_or(ViewError::NOT_FOUND)?,
            _view: PhantomData,
        })
    }
}
