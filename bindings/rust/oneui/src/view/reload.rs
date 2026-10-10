//! Development-only IR file watching. Compilation remains the build tool's job.
use super::{View, ViewError};
use crate::{Error, LocalSignalSubscription, UiDispatcher};
use std::{
    cell::RefCell,
    path::PathBuf,
    rc::Weak,
    sync::mpsc,
    thread::{self, JoinHandle},
    time::{Duration, Instant, SystemTime},
};

#[derive(Debug)]
pub enum ReloadError {
    File(String),
    View(ViewError),
}

/// Watches a compiler-produced `.oneview`, debounces writes for 200 ms, and
/// delivers owned bytes through the window dispatcher. This owner must stay on
/// the UI thread. Drop cancels queued delivery and joins the worker.
pub struct ViewReloadWatcher {
    subscription: Option<LocalSignalSubscription>,
    stop: mpsc::Sender<()>,
    worker: Option<JoinHandle<()>>,
}

fn stamp(path: &PathBuf) -> Option<(SystemTime, u64)> {
    let info = std::fs::metadata(path).ok()?;
    Some((info.modified().ok()?, info.len()))
}

impl ViewReloadWatcher {
    pub fn new(
        dispatcher: &UiDispatcher,
        view: Weak<RefCell<View>>,
        path: PathBuf,
        report: impl Fn(Result<(), ReloadError>) + 'static,
    ) -> Result<Self, Error> {
        let (signal, subscription) =
            dispatcher.latest_signal(move |result: Result<Vec<u8>, String>| {
                let Some(view) = view.upgrade() else { return };
                let result = result.map_err(ReloadError::File).and_then(|bytes| {
                    let mut view = view
                        .try_borrow_mut()
                        .map_err(|_| ReloadError::View(ViewError::BUSY))?;
                    view.reload(&bytes).map_err(ReloadError::View)
                });
                report(result);
            })?;
        let (stop, receive) = mpsc::channel();
        let worker = thread::spawn(move || {
            let mut previous = stamp(&path);
            let mut pending = None;
            loop {
                match receive.recv_timeout(Duration::from_millis(50)) {
                    Err(mpsc::RecvTimeoutError::Timeout) => {}
                    _ => break,
                }
                let current = stamp(&path);
                if current != previous {
                    previous = current;
                    pending = Some(Instant::now());
                }
                if pending.is_some_and(|at| at.elapsed() >= Duration::from_millis(200)) {
                    pending = None;
                    let result = if previous.is_some_and(|(_, size)| size > 32 * 1024 * 1024) {
                        Err("IR exceeds the 32 MiB development input limit".into())
                    } else {
                        std::fs::read(&path).map_err(|error| format!("{}: {error}", path.display()))
                    };
                    if signal.send(result).is_err() {
                        break;
                    }
                }
            }
        });
        Ok(Self {
            subscription: Some(subscription),
            stop,
            worker: Some(worker),
        })
    }
}
impl Drop for ViewReloadWatcher {
    fn drop(&mut self) {
        self.subscription.take();
        let _ = self.stop.send(());
        if let Some(worker) = self.worker.take() {
            let _ = worker.join();
        }
    }
}
