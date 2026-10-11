// Widget-free and native label producers share guarded delivery.
struct LabelState {
    raw: AtomicPtr<sys::OneUiWidget>,
    closed: AtomicBool,
    sink: Option<Arc<dyn Fn(String, Vec<LabelTextSpan>) + Send + Sync>>,
    pending_content: Mutex<Option<LabelContentUpdate>>,
    update_scheduled: AtomicBool,
}

/// Thread-safe producer for label text owned by a window.
///
/// Worker threads submit only the latest value. OneUI coalesces bursts and
/// applies text on the owning UI thread, matching the terminal frame handle's
/// lifetime and thread-affinity guarantees.
#[derive(Clone)]
pub struct LabelHandle {
    state: Arc<LabelState>,
    dispatcher: UiDispatcher,
    guard: Option<Arc<dyn Fn() -> bool + Send + Sync>>,
    retention_guard: Option<Arc<dyn Fn() -> bool + Send + Sync>>,
    retained: Option<Arc<Mutex<Option<LabelContentUpdate>>>>,
}

/// Owns a widget-free text receiver. Dropping it rejects all queued/late producers.
pub struct TextSinkSubscription {
    state: Arc<LabelState>,
    _ui_thread: std::marker::PhantomData<Rc<()>>,
}
impl Drop for TextSinkSubscription {
    fn drop(&mut self) {
        self.state.closed.store(true, Ordering::Release);
        self.state.pending_content.lock().unwrap().take();
    }
}
impl LabelHandle {
    /// Receives coalesced text on the dispatcher thread without constructing a widget.
    /// The callback should retain only weak references to its presentation consumer.
    /// Scoped producers preserve the same visibility/generation guards as native labels.
    pub fn text_sink(
        dispatcher: &UiDispatcher,
        receive: impl Fn(String, Vec<LabelTextSpan>) + Send + Sync + 'static,
    ) -> (Self, TextSinkSubscription) {
        let handle = Self {
            state: Arc::new(LabelState {
                raw: AtomicPtr::new(std::ptr::null_mut()),
                closed: AtomicBool::new(false),
                sink: Some(Arc::new(receive)),
                pending_content: Mutex::new(None),
                update_scheduled: AtomicBool::new(false),
            }),
            dispatcher: dispatcher.clone(),
            guard: None,
            retention_guard: None,
            retained: None,
        };
        let subscription = TextSinkSubscription {
            state: handle.state.clone(),
            _ui_thread: std::marker::PhantomData,
        };
        (handle, subscription)
    }

    /// Scopes this producer without changing other producers of the same label.
    /// Checked under the submission lock and again immediately before painting.
    /// Guards must be fast, non-blocking, and must not call back into this handle.
    /// Additional guards compose with the existing scope (for example session
    /// identity followed by connection generation). The first guard controls
    /// visibility; additional guards also reject stale retained values.
    /// One latest value is retained, even while its visibility guard is false.
    pub fn with_update_guard<F>(&self, guard: F) -> Self
    where
        F: Fn() -> bool + Send + Sync + 'static,
    {
        let previous = self.guard.clone();
        let guard: Arc<dyn Fn() -> bool + Send + Sync> = Arc::new(guard);
        let retain_check = guard.clone();
        let previous_retention = self.retention_guard.clone();
        let retention_guard = self.retained.as_ref().map(|_| {
            Arc::new(move || {
                previous_retention.as_ref().is_none_or(|check| check()) && retain_check()
            }) as Arc<dyn Fn() -> bool + Send + Sync>
        });
        Self {
            state: self.state.clone(),
            dispatcher: self.dispatcher.clone(),
            guard: Some(Arc::new(move || {
                previous.as_ref().is_none_or(|check| check()) && guard()
            })),
            retention_guard,
            retained: Some(
                self.retained
                    .clone()
                    .unwrap_or_else(|| Arc::new(Mutex::new(None))),
            ),
        }
    }

    /// Resubmits this scoped producer's latest value, retaining its original
    /// guards. Returns false when it has no currently applicable value.
    pub fn reapply_latest(&self) -> Result<bool, Error> {
        let latest = self
            .retained
            .as_ref()
            .and_then(|value| value.lock().expect("label retained lock poisoned").clone());
        let Some(update) = latest.filter(|update| update.is_current()) else {
            return Ok(false);
        };
        self.set_content(update)?;
        Ok(true)
    }

    pub fn set_text(&self, text: impl Into<String>) -> Result<(), Error> {
        self.set_content(LabelContentUpdate {
            text: text.into(),
            spans: Vec::new(),
            guard: self.guard.clone(),
            retention_guard: self.retention_guard.clone(),
        })
    }

    /// Atomically submits text and inline ranges to the owning UI thread.
    /// Bursts are coalesced to the newest complete revision.
    pub fn set_rich_text(
        &self,
        text: impl Into<String>,
        spans: Vec<LabelTextSpan>,
    ) -> Result<(), Error> {
        let text = text.into();
        validate_label_text_spans(&text, &spans)?;
        self.set_content(LabelContentUpdate {
            text,
            spans,
            guard: self.guard.clone(),
            retention_guard: self.retention_guard.clone(),
        })
    }

    fn set_content(&self, update: LabelContentUpdate) -> Result<(), Error> {
        if self.state.closed.load(Ordering::Acquire)
            || (self.state.raw.load(Ordering::Acquire).is_null() && self.state.sink.is_none())
        {
            return Err(Error::WidgetDestroyed);
        }

        if let Some(retained) = &self.retained {
            let mut retained = retained.lock().expect("label retained lock poisoned");
            if !update.retention_guard.as_ref().is_none_or(|check| check()) {
                return Ok(());
            }
            *retained = Some(update.clone());
        }
        {
            let mut pending = self
                .state
                .pending_content
                .lock()
                .expect("label pending content lock poisoned");
            // A now-background producer cannot replace the active producer's
            // pending value with a stale update that would later be discarded.
            if !update.is_current() {
                return Ok(());
            }
            *pending = Some(update);
        }
        if self.state.update_scheduled.swap(true, Ordering::AcqRel) {
            return Ok(());
        }

        let state = Arc::clone(&self.state);
        let dispatched_state = Arc::clone(&state);
        if let Err(error) = self
            .dispatcher
            .dispatch(move || Self::drain_pending_text(&dispatched_state))
        {
            state.update_scheduled.store(false, Ordering::Release);
            state
                .pending_content
                .lock()
                .expect("label pending content lock poisoned")
                .take();
            return Err(error);
        }
        Ok(())
    }

    fn drain_pending_text(state: &LabelState) {
        loop {
            let content = state
                .pending_content
                .lock()
                .expect("label pending content lock poisoned")
                .take();
            let raw = state.raw.load(Ordering::Acquire);
            if state.closed.load(Ordering::Acquire) || (raw.is_null() && state.sink.is_none()) {
                state.update_scheduled.store(false, Ordering::Release);
                return;
            }
            if let Some(content) = content.filter(|content| content.is_current()) {
                trace_ui_task("label update started");
                if let Some(sink) = &state.sink {
                    sink(content.text, content.spans);
                } else {
                    let _ = apply_label_content(raw, &content.text, &content.spans);
                }
                trace_ui_task("label update completed");
            }

            state.update_scheduled.store(false, Ordering::Release);
            if state
                .pending_content
                .lock()
                .expect("label pending content lock poisoned")
                .is_none()
            {
                return;
            }
            if !state.update_scheduled.swap(true, Ordering::AcqRel) {
                continue;
            }
            return;
        }
    }
}

#[cfg(test)]
mod text_sink_tests {
    use super::*;
    use std::sync::atomic::AtomicU64;
    pub(super) fn guarded_sink_preserves_background_generation_and_closed_lifetime() {
        let window = Window::new(&WindowOptions::default()).unwrap();
        let value = Arc::new(Mutex::new(String::new()));
        let captured = value.clone();
        let (base, subscription) = LabelHandle::text_sink(&window.dispatcher(), move |text, _| {
            *captured.lock().unwrap() = text
        });
        let active = Arc::new(AtomicBool::new(true));
        let selected = active.clone();
        let scoped = base.with_update_guard(move || selected.load(Ordering::Acquire));
        scoped.set_text("pending").unwrap();
        active.store(false, Ordering::Release);
        LabelHandle::drain_pending_text(&base.state);
        assert_eq!(*value.lock().unwrap(), "");
        let generation = Arc::new(AtomicU64::new(1));
        let current = generation.clone();
        let old = scoped.with_update_guard(move || current.load(Ordering::Acquire) == 1);
        generation.store(2, Ordering::Release);
        let current = generation.clone();
        let fresh = scoped.with_update_guard(move || current.load(Ordering::Acquire) == 2);
        std::thread::spawn(move || {
            fresh.set_text("background latest").unwrap();
            old.set_text("stale").unwrap();
        })
        .join()
        .unwrap();
        base.set_text("foreground").unwrap();
        LabelHandle::drain_pending_text(&base.state);
        assert_eq!(*value.lock().unwrap(), "foreground");
        active.store(true, Ordering::Release);
        assert!(scoped.reapply_latest().unwrap());
        LabelHandle::drain_pending_text(&base.state);
        assert_eq!(*value.lock().unwrap(), "background latest");
        scoped.set_text("queued after activation").unwrap();
        drop(subscription);
        LabelHandle::drain_pending_text(&base.state);
        assert_eq!(*value.lock().unwrap(), "background latest");
        assert!(matches!(
            scoped.set_text("late"),
            Err(Error::WidgetDestroyed)
        ));
        assert!(matches!(
            scoped.reapply_latest(),
            Err(Error::WidgetDestroyed)
        ));
    }
}
