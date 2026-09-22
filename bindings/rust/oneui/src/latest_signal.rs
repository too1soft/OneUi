//! A bounded latest-value bridge from services to UI-owned state.
//! The subscription and callback never leave the window thread; only T crosses it.
use crate::{Error, UiDispatcher};
use std::{
    cell::RefCell,
    collections::BTreeMap,
    marker::PhantomData,
    rc::Rc,
    sync::{
        atomic::{AtomicBool, AtomicU64, Ordering},
        Arc, Mutex,
    },
};
thread_local! { static CALLBACKS: RefCell<BTreeMap<u64,Rc<dyn Fn()>>> = RefCell::new(BTreeMap::new()); }
static NEXT_ID: AtomicU64 = AtomicU64::new(1);
struct Shared<T> {
    value: Mutex<Option<T>>,
    queued: AtomicBool,
    active: AtomicBool,
}
pub struct LatestSignal<T> {
    shared: Arc<Shared<T>>,
    dispatcher: UiDispatcher,
    id: u64,
}
impl<T> Clone for LatestSignal<T> {
    fn clone(&self) -> Self {
        Self {
            shared: Arc::clone(&self.shared),
            dispatcher: self.dispatcher.clone(),
            id: self.id,
        }
    }
}
/// Dropping on the window thread cancels queued deliveries and releases the callback.
pub struct LocalSignalSubscription {
    id: u64,
    cancel: Box<dyn Fn()>,
    _local: PhantomData<Rc<()>>,
}
impl Drop for LocalSignalSubscription {
    fn drop(&mut self) {
        (self.cancel)();
        CALLBACKS.with(|all| {
            all.borrow_mut().remove(&self.id);
        });
    }
}
impl<T: Send + 'static> LatestSignal<T> {
    /// Replaces an unconsumed value. At most one UI delivery is queued.
    pub fn send(&self, value: T) -> Result<(), Error> {
        let mut pending = self.shared.value.lock().unwrap_or_else(|e| e.into_inner());
        if !self.shared.active.load(Ordering::Acquire) {
            return Err(Error::WidgetDestroyed);
        }
        *pending = Some(value);
        drop(pending);
        if self.shared.queued.swap(true, Ordering::AcqRel) {
            return Ok(());
        }
        let id = self.id;
        let shared = Arc::clone(&self.shared);
        let result = self.dispatcher.dispatch(move || {
            shared.queued.store(false, Ordering::Release);
            if !shared.active.load(Ordering::Acquire) {
                return;
            }
            let callback = CALLBACKS.with(|all| all.borrow().get(&id).cloned());
            if let Some(callback) = callback {
                callback();
            }
        });
        if result.is_err() {
            self.shared.queued.store(false, Ordering::Release);
            self.shared
                .value
                .lock()
                .unwrap_or_else(|e| e.into_inner())
                .take();
        }
        result
    }
}
impl UiDispatcher {
    /// Register on the owning window thread. F may capture Rc and native controls.
    /// Workers receive only the Send signal; retain the subscription with the view.
    pub fn latest_signal<T: Send + 'static, F: Fn(T) + 'static>(
        &self,
        callback: F,
    ) -> Result<(LatestSignal<T>, LocalSignalSubscription), Error> {
        if std::thread::current().id() != self.state.ui_thread {
            return Err(Error::WrongThread);
        }
        let id = NEXT_ID.fetch_add(1, Ordering::Relaxed);
        let shared = Arc::new(Shared {
            value: Mutex::new(None),
            queued: AtomicBool::new(false),
            active: AtomicBool::new(true),
        });
        let delivery = Arc::clone(&shared);
        CALLBACKS.with(|all| {
            all.borrow_mut().insert(
                id,
                Rc::new(move || {
                    let value = delivery
                        .value
                        .lock()
                        .unwrap_or_else(|e| e.into_inner())
                        .take();
                    if let Some(value) = value {
                        callback(value);
                    }
                }),
            );
        });
        let cancel = Arc::clone(&shared);
        Ok((
            LatestSignal {
                shared,
                dispatcher: self.clone(),
                id,
            },
            LocalSignalSubscription {
                id,
                cancel: Box::new(move || {
                    cancel.active.store(false, Ordering::Release);
                    cancel
                        .value
                        .lock()
                        .unwrap_or_else(|e| e.into_inner())
                        .take();
                }),
                _local: PhantomData,
            },
        ))
    }
}
