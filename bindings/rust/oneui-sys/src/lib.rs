//! Raw bindings for OneUI's portable UTF-8 C ABI.
//!
//! All UTF-8 strings are borrowed for the duration of the FFI call. Callback
//! bytes are also borrowed and must be copied before the callback returns.

use std::ffi::c_uint;

pub mod ffi;
mod types;

pub use ffi::*;
pub use types::*;

pub const UTF8_ABI_VERSION: c_uint = 45;

mod title_bar_presentation;
pub use title_bar_presentation::*;

mod abi_types;
pub use abi_types::*;
mod foundation_api;
pub use foundation_api::*;
mod data_api;
pub use data_api::*;
mod control_api;
pub use control_api::*;
mod window_api;
pub use window_api::*;
mod media_api;
pub use media_api::*;
