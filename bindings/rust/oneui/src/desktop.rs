//! Desktop handler activation without evaluating a command string in a shell.
use std::{io, path::Path};

/// Opens an existing local path through its registered desktop handler.
/// Success means the request was accepted, not that another app finished opening.
pub fn open_path(path: impl AsRef<Path>) -> io::Result<()> {
    activate(path.as_ref().canonicalize()?.as_os_str())
}

/// Activates a URI. Apply the application's scheme and authorization policy
/// before passing untrusted links to this desktop boundary.
pub fn open_uri(uri: &str) -> io::Result<()> {
    let valid_scheme = uri.split_once(':').is_some_and(|(scheme, _)| {
        !scheme.is_empty()
            && scheme.bytes().enumerate().all(|(i, b)| {
                b.is_ascii_alphabetic() || (i > 0 && (b.is_ascii_digit() || b"+-.".contains(&b)))
            })
    });
    if !valid_scheme || uri.len() > 8192 || uri.chars().any(|c| c.is_control() || c.is_whitespace())
    {
        return Err(io::Error::new(
            io::ErrorKind::InvalidInput,
            "invalid desktop URI",
        ));
    }
    activate(std::ffi::OsStr::new(uri))
}

#[cfg(any(target_os = "macos", target_os = "linux"))]
fn command(target: &std::ffi::OsStr) -> std::process::Command {
    #[cfg(target_os = "macos")]
    let mut command = {
        let mut command = std::process::Command::new("/usr/bin/open");
        command.arg("--");
        command
    };
    #[cfg(target_os = "linux")]
    let mut command = std::process::Command::new("xdg-open");
    command.arg(target);
    command
}

#[cfg(any(target_os = "macos", target_os = "linux"))]
fn activate(target: &std::ffi::OsStr) -> io::Result<()> {
    use std::process::Stdio;
    let mut child = command(target)
        .stdin(Stdio::null())
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .spawn()?;
    std::thread::spawn(move || {
        let _ = child.wait();
    });
    Ok(())
}

#[cfg(windows)]
fn activate(target: &std::ffi::OsStr) -> io::Result<()> {
    use std::os::windows::ffi::OsStrExt;
    #[link(name = "shell32")]
    unsafe extern "system" {
        fn ShellExecuteW(
            window: *mut std::ffi::c_void,
            operation: *const u16,
            file: *const u16,
            parameters: *const u16,
            directory: *const u16,
            show: i32,
        ) -> isize;
    }
    let target: Vec<u16> = target.encode_wide().chain(Some(0)).collect();
    let operation: Vec<u16> = "open\0".encode_utf16().collect();
    // SAFETY: the terminated strings remain alive throughout the call.
    let result = unsafe {
        ShellExecuteW(
            std::ptr::null_mut(),
            operation.as_ptr(),
            target.as_ptr(),
            std::ptr::null(),
            std::ptr::null(),
            1,
        )
    };
    if result <= 32 {
        Err(io::Error::other(format!(
            "desktop handler rejected request ({result})"
        )))
    } else {
        Ok(())
    }
}

#[cfg(not(any(windows, target_os = "macos", target_os = "linux")))]
fn activate(_: &std::ffi::OsStr) -> io::Result<()> {
    Err(io::Error::new(
        io::ErrorKind::Unsupported,
        "desktop activation unavailable",
    ))
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn rejects_options_and_control_characters_before_launch() {
        for uri in [
            "--help",
            "https://example.com/\n",
            "1bad:value",
            "",
            "http://a b",
        ] {
            assert_eq!(
                open_uri(uri).unwrap_err().kind(),
                io::ErrorKind::InvalidInput
            );
        }
    }
    #[cfg(any(target_os = "macos", target_os = "linux"))]
    #[test]
    fn target_is_one_literal_argument() {
        let target = std::ffi::OsStr::new("https://example.com/$(touch%20bad);a&b");
        let cmd = command(target);
        assert_eq!(cmd.get_args().last(), Some(target));
        assert_ne!(cmd.get_program(), "sh");
    }
}
