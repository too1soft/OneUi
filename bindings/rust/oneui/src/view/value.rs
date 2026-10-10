use super::{ViewError, VmResult};
use crate::sys;

/// Owned callback value. The adapter copies text to its own scratch storage.
#[derive(Clone, Debug, PartialEq)]
pub enum Value {
    Bool(bool),
    I64(i64),
    F64(f64),
    Utf8(String),
}
impl Value {
    pub fn boolean(&self) -> VmResult<bool> {
        if let Self::Bool(v) = self {
            Ok(*v)
        } else {
            Err(ViewError::TYPE)
        }
    }
    pub fn integer(&self) -> VmResult<i64> {
        if let Self::I64(v) = self {
            Ok(*v)
        } else {
            Err(ViewError::TYPE)
        }
    }
    pub fn number(&self) -> VmResult<f64> {
        if let Self::F64(v) = self {
            Ok(*v)
        } else {
            Err(ViewError::TYPE)
        }
    }
    pub fn text(&self) -> VmResult<&str> {
        if let Self::Utf8(v) = self {
            Ok(v)
        } else {
            Err(ViewError::TYPE)
        }
    }
    pub(crate) fn borrow(&self) -> VmResult<sys::OneUiValue> {
        let mut raw = sys::OneUiValue::default();
        match self {
            Self::Bool(v) => {
                raw.tag = 1;
                raw.data.boolean = u32::from(*v);
            }
            Self::I64(v) => {
                raw.tag = 2;
                raw.data.integer = *v;
            }
            Self::F64(v) if v.is_finite() => {
                raw.tag = 3;
                raw.data.number = *v;
            }
            Self::Utf8(v) if !v.contains('\0') => {
                raw.tag = 4;
                raw.data.utf8 = sys::OneUiValueUtf8 {
                    data: v.as_ptr().cast(),
                    length: v.len(),
                };
            }
            _ => return Err(ViewError::TYPE),
        }
        Ok(raw)
    }
    pub(crate) unsafe fn copy(raw: &sys::OneUiValue) -> VmResult<Self> {
        if raw.reserved != 0 {
            return Err(ViewError::TYPE);
        }
        match raw.tag {
            1 if raw.data.boolean <= 1 => Ok(Self::Bool(raw.data.boolean != 0)),
            2 => Ok(Self::I64(raw.data.integer)),
            3 if raw.data.number.is_finite() => Ok(Self::F64(raw.data.number)),
            4 => {
                let v = raw.data.utf8;
                if v.length == 0 {
                    return Ok(Self::Utf8(String::new()));
                }
                if v.data.is_null() || v.length > isize::MAX as usize {
                    return Err(ViewError::TYPE);
                }
                let bytes = std::slice::from_raw_parts(v.data.cast(), v.length);
                let text = std::str::from_utf8(bytes).map_err(|_| ViewError::TYPE)?;
                if text.contains('\0') {
                    return Err(ViewError::TYPE);
                }
                Ok(Self::Utf8(text.to_owned()))
            }
            _ => Err(ViewError::TYPE),
        }
    }
}
