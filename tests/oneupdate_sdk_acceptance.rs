//! External-consumer acceptance fixture for the pinned OneUpdate checkout.
//! Copy into oneupdate-installer/src and add a cfg(test) module declaration.
//! Never compiled into the product installer. Run only in a disposable VM.
use super::*;
use std::fs::OpenOptions;
use std::io::Write;

#[cfg(feature = "oneui")]
pub(super) fn capture_after_settling(window: std::rc::Rc<oneui::Window>, action: impl FnOnce() + 'static) {
    type Completion = std::rc::Rc<std::cell::RefCell<Option<Box<dyn FnOnce()>>>>;
    fn frame(window: std::rc::Rc<oneui::Window>, deadline: std::time::Instant, action: Completion) {
        let next = std::rc::Rc::clone(&window);
        window.dispatcher().request_animation_frame_local(move |_| {
            if std::time::Instant::now() >= deadline {
                let complete = action.borrow_mut().take();
                if let Some(complete) = complete { complete(); }
            } else {
                frame(next, deadline, action);
            }
        }).expect("schedule settled capture");
    }
    frame(window, std::time::Instant::now() + std::time::Duration::from_millis(500),
          std::rc::Rc::new(std::cell::RefCell::new(Some(Box::new(action)))));
}

fn record(path: &Path, phase: &str) -> Result<()> {
    let mut file = OpenOptions::new().create(true).append(true).open(path)?;
    writeln!(file, "{},{}", time::OffsetDateTime::now_utc(), phase)?;
    file.flush()?;
    println!("OneUI consumer acceptance: {phase}");
    Ok(())
}

fn stop_owned_test_clients(root: &Path) -> Result<()> {
    use windows_sys::Win32::{Foundation::{CloseHandle, INVALID_HANDLE_VALUE},
        System::{Diagnostics::ToolHelp::{CreateToolhelp32Snapshot, Process32FirstW, Process32NextW,
                                        PROCESSENTRY32W, TH32CS_SNAPPROCESS},
                 Threading::{OpenProcess, QueryFullProcessImageNameW, TerminateProcess,
                             PROCESS_QUERY_LIMITED_INFORMATION, PROCESS_TERMINATE}}};
    let prefix = format!("{}\\versions\\", root.to_string_lossy()).to_ascii_lowercase();
    unsafe {
        let snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        anyhow::ensure!(snapshot != INVALID_HANDLE_VALUE, "Cannot enumerate test processes");
        let mut entry: PROCESSENTRY32W = std::mem::zeroed();
        entry.dwSize = std::mem::size_of::<PROCESSENTRY32W>() as u32;
        let mut present = Process32FirstW(snapshot, &mut entry) != 0;
        while present {
            let process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, 0, entry.th32ProcessID);
            if !process.is_null() {
                let mut path = vec![0u16; 32768];
                let mut length = path.len() as u32;
                let owned = QueryFullProcessImageNameW(process, 0, path.as_mut_ptr(), &mut length) != 0 &&
                    String::from_utf16_lossy(&path[..length as usize]).to_ascii_lowercase().starts_with(&prefix);
                CloseHandle(process);
                if owned {
                    // These are fresh clients launched by this fixture, never
                    // applications outside its fixed, disposable release root.
                    let target = OpenProcess(PROCESS_TERMINATE, 0, entry.th32ProcessID);
                    if !target.is_null() { TerminateProcess(target, 0); CloseHandle(target); }
                }
            }
            present = Process32NextW(snapshot, &mut entry) != 0;
        }
        CloseHandle(snapshot);
    }
    std::thread::sleep(std::time::Duration::from_millis(500));
    Ok(())
}

fn prepared(bundle: &Path, root: &Path) -> Result<InstallJob> {
    let args = Args::try_parse_from([
        bundle.as_os_str(), std::ffi::OsStr::new("--install-root"), root.as_os_str(),
    ])?;
    let mut job = prepare_install_job(&args, bundle)?;
    anyhow::ensure!(job.product_spec.default_scope == "user", "User-scope QA bundle required");
    anyhow::ensure!(job.has_embedded_payload && job.has_embedded_agent, "Complete offline bundle required");
    // The fixture models the GUI's accepted-agreements result. The signed
    // plan, payload/Agent checks, transaction and health policy stay intact.
    // This is not a product flag and does not exercise checkbox interaction.
    job.legal_acceptance = Some("interactive".to_owned());
    job.interactive = false;
    Ok(job)
}

fn product_process(executable: &Path, arguments: &[std::ffi::OsString]) -> Result<()> {
    use std::os::windows::process::CommandExt;
    let mut process = std::process::Command::new(executable).args(arguments)
        .creation_flags(0x08000000).stdin(std::process::Stdio::null())
        .stdout(std::process::Stdio::null()).stderr(std::process::Stdio::null()).spawn()?;
    let start = std::time::Instant::now();
    loop {
        if let Some(status) = process.try_wait()? {
            anyhow::ensure!(status.success(), "Product process failed: {status}");
            return Ok(());
        }
        if start.elapsed() > std::time::Duration::from_secs(150) {
            let _ = process.kill();
            anyhow::bail!("Product process exceeded acceptance timeout");
        }
        std::thread::sleep(std::time::Duration::from_millis(50));
    }
}

fn product_install(bundle: &Path, root: &Path) -> Result<()> {
    let job = prepared(bundle, root)?;
    // Seed only the draft identity and accepted-document result that the GUI
    // would produce. The unmodified product then runs every transaction in
    // its own real process, including its current-executable copy behavior.
    InstallLayout::new(root).initialize(&job.plan.app_key)?;
    record_legal_acceptance(&job, "interactive")?;
    product_process(bundle, &["--quiet".into(), "--install-root".into(), root.as_os_str().to_owned()])
}

#[test]
#[cfg(feature = "oneui")]
#[ignore = "real bundle UI; requires the validation frame-capture runtime"]
fn actual_bundle_frame() -> Result<()> {
    let bundle = PathBuf::from(std::env::var_os("ONEUI_ACCEPTANCE_FRAME_BUNDLE")
        .context("Real offline bundle required")?);
    let root = PathBuf::from(std::env::var_os("ONEUI_ACCEPTANCE_FRAME_ROOT")
        .context("Unused frame-comparison root required")?);
    anyhow::ensure!(!root.exists(), "Frame comparisons must not target an installation");
    let runtime = tokio::runtime::Builder::new_multi_thread().enable_all().build()?;
    let job = prepared(&bundle, &root)?;
    // oneui_shell's existing cfg(test) capture branch drains widget mutations
    // before painting and never starts an installation or displays a window.
    oneui_shell(job, runtime.handle().clone())
}

#[test]
#[ignore = "real offline install/repair/update/uninstall; isolated VM only"]
fn actual_offline_lifecycle() -> Result<()> {
    anyhow::ensure!(std::env::var("ONEUI_ACCEPTANCE_ISOLATED_VM").as_deref() == Ok("yes"),
                    "An explicitly isolated VM is required");
    let first = PathBuf::from(std::env::var_os("ONEUI_ACCEPTANCE_FIRST").context("First bundle required")?);
    let update = PathBuf::from(std::env::var_os("ONEUI_ACCEPTANCE_UPDATE").context("Update bundle required")?);
    let root = PathBuf::from(std::env::var_os("ONEUI_ACCEPTANCE_ROOT").context("Isolated install root required")?);
    let report = PathBuf::from(std::env::var_os("ONEUI_ACCEPTANCE_REPORT").context("Report required")?);
    anyhow::ensure!(root == Path::new("C:\\OneUIDemand\\lifecycle-app"),
                    "Use the fixed VM acceptance root");
    let runtime = tokio::runtime::Builder::new_multi_thread().enable_all().build()?;
    runtime.block_on(async {
        if root.exists() {
            let state = InstallLayout::new(&root).load()?;
            let marker = root.join("data").join("oneui-acceptance-user-data.txt");
            anyhow::ensure!(state.current_version.is_none() ||
                std::fs::read(&marker).unwrap_or_default() == b"retained acceptance data",
                "Existing installation does not belong to this fixture");
            stop_owned_test_clients(&root)?;
            uninstall_flow::uninstall_current(&first, &root, true).await?;
            anyhow::ensure!(!root.exists(), "Incomplete-install cleanup failed");
            record(&report, if state.current_version.is_none() { "rolled-back-first-install-uninstall-pass" }
                   else { "previous-fixture-install-uninstall-pass" })?;
        }
        record(&report, "install-start")?;
        let initial = prepared(&first, &root)?;
        let version = initial.plan.version.clone();
        product_install(&first, &root)?;
        let layout = InstallLayout::new(&root);
        anyhow::ensure!(layout.load()?.current_version.as_deref() == Some(version.as_str()), "Initial version mismatch");
        anyhow::ensure!(layout.active_path()?.is_dir(), "Active release missing");
        let marker = root.join("data").join("oneui-acceptance-user-data.txt");
        std::fs::write(&marker, b"retained acceptance data")?;
        record(&report, "install-pass")?;

        stop_owned_test_clients(&root)?;
        product_install(&first, &root)?;
        anyhow::ensure!(std::fs::read(&marker)? == b"retained acceptance data", "Repair lost user data");
        record(&report, "repair-pass")?;

        stop_owned_test_clients(&root)?;
        let next = prepared(&update, &root)?;
        let next_version = next.plan.version.clone();
        let same_release = next_version == version;
        anyhow::ensure!(!same_release || std::env::var("ONEUI_ACCEPTANCE_RUNTIME_REPAIR").as_deref() == Ok("yes"),
                        "Explicitly select runtime repair when reusing the signed release");
        product_install(&update, &root)?;
        anyhow::ensure!(layout.load()?.current_version.as_deref() == Some(next_version.as_str()), "Update version mismatch");
        anyhow::ensure!(std::fs::read(&marker)? == b"retained acceptance data", "Update lost user data");
        record(&report, if same_release { "installer-runtime-repair-pass" } else { "update-pass" })?;

        if let Some(stub) = std::env::var_os("ONEUI_ACCEPTANCE_EXPECTED_STUB") {
            let expected = std::fs::read(stub)?;
            for name in ["oneupdate-launcher.exe", "oneupdate-uninstall.exe"] {
                let installed = std::fs::read(root.join(name))?;
                anyhow::ensure!(installed.starts_with(&expected), "Installed runtime did not use the candidate stub: {name}");
            }
            record(&report, "installed-runtime-prefix-pass")?;
        }

        stop_owned_test_clients(&root)?;
        product_process(&root.join("oneupdate-uninstall.exe"), &["--quiet".into()])?;
        let start = std::time::Instant::now();
        while root.exists() && start.elapsed() < std::time::Duration::from_secs(60) {
            std::thread::sleep(std::time::Duration::from_millis(100));
        }
        anyhow::ensure!(!root.exists(), "Uninstall did not remove the isolated installation");
        record(&report, "uninstall-pass")?;
        Ok(())
    })
}
