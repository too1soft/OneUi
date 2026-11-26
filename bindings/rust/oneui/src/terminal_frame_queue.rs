use super::super::terminal_render::apply_terminal_frame;
use super::*;
use std::any::Any;
use std::time::{Duration, Instant};

const MAX_UPDATES_PER_TURN: usize = 16;
const TURN_BUDGET: Duration = Duration::from_millis(2);

/// A one-shot replacement for an already queued terminal frame.
///
/// A replacement uses the existing dispatcher callback. It expires when
/// the window takes the frame or another submission replaces it.
pub struct PendingTerminalFrame {
    state: Arc<TerminalViewState>,
    marker: Arc<()>,
}

impl PendingTerminalFrame {
    pub fn replace<T: AsRef<str> + PartialEq + Send + 'static>(
        self,
        frame: TerminalFrame<T>,
    ) -> bool {
        let mut pending = self
            .state
            .pending_update
            .lock()
            .expect("terminal pending update lock poisoned");
        let Some(update) = pending.as_mut() else {
            return false;
        };
        if self.state.raw.load(Ordering::Acquire).is_null()
            || !update
                .replacement
                .as_ref()
                .is_some_and(|marker| Arc::ptr_eq(marker, &self.marker))
        {
            return false;
        }
        update.frame = Box::new(frame);
        update.replacement = None;
        true
    }
}

pub(super) trait QueuedTerminalFrame: Send {
    fn as_any(&self) -> &dyn Any;

    fn apply(
        &self,
        raw: *mut sys::OneUiWidget,
        previous: Option<&dyn QueuedTerminalFrame>,
        honor_application_cursor_style: bool,
    );
}

impl<T: AsRef<str> + PartialEq + Send + 'static> QueuedTerminalFrame for TerminalFrame<T> {
    fn as_any(&self) -> &dyn Any {
        self
    }

    fn apply(
        &self,
        raw: *mut sys::OneUiWidget,
        previous: Option<&dyn QueuedTerminalFrame>,
        honor_application_cursor_style: bool,
    ) {
        let previous = previous.and_then(|frame| frame.as_any().downcast_ref::<Self>());
        apply_terminal_frame(raw, self, previous, honor_application_cursor_style);
    }
}

impl TerminalViewHandle {
    /// Offers replacement only for a queued frame with no selection command.
    pub fn pending_frame_replacement(&self) -> Option<PendingTerminalFrame> {
        let mut pending = self
            .state
            .pending_update
            .lock()
            .expect("terminal pending update lock poisoned");
        if self.state.raw.load(Ordering::Acquire).is_null()
            || !self.state.update_scheduled.load(Ordering::Acquire)
        {
            return None;
        }
        let update = pending.as_mut()?;
        if !matches!(update.selection, TerminalFrameSelection::Preserve) {
            return None;
        }
        Some(PendingTerminalFrame {
            state: Arc::clone(&self.state),
            marker: Arc::clone(update.replacement.get_or_insert_with(|| Arc::new(()))),
        })
    }

    pub(super) fn submit_update<T: AsRef<str> + PartialEq + Send + 'static>(
        &self,
        frame: TerminalFrame<T>,
        selection: TerminalFrameSelection,
    ) -> Result<(), Error> {
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }

        *self
            .state
            .pending_update
            .lock()
            .expect("terminal pending update lock poisoned") = Some(TerminalFrameUpdate {
            frame: Box::new(frame),
            selection,
            replacement: None,
        });

        self.schedule_pending_frame()
    }

    fn schedule_pending_frame(&self) -> Result<(), Error> {
        if self.state.update_scheduled.swap(true, Ordering::AcqRel) {
            return Ok(());
        }

        let handle = self.clone();
        if let Err(error) = self
            .dispatcher
            .dispatch(move || handle.drain_pending_frame())
        {
            self.state.update_scheduled.store(false, Ordering::Release);
            self.state
                .pending_update
                .lock()
                .expect("terminal pending update lock poisoned")
                .take();
            return Err(error);
        }
        Ok(())
    }

    fn drain_pending_frame(&self) {
        let deadline = Instant::now() + TURN_BUDGET;
        for _ in 0..MAX_UPDATES_PER_TURN {
            if !self.state.apply_pending_frame() || Instant::now() >= deadline {
                break;
            }
        }
        self.state.update_scheduled.store(false, Ordering::Release);
        let has_pending = self
            .state
            .pending_update
            .lock()
            .expect("terminal pending update lock poisoned")
            .is_some();
        if has_pending {
            // Return to the native loop so input and paint can run between
            // updates. The worker and this callback compete for the same slot.
            let _ = self.schedule_pending_frame();
        }
    }
}

impl TerminalViewState {
    pub(super) fn apply_pending_frame(&self) -> bool {
        let Some(update) = self
            .pending_update
            .lock()
            .expect("terminal pending update lock poisoned")
            .take()
        else {
            return false;
        };
        let raw = self.raw.load(Ordering::Acquire);
        if raw.is_null() {
            return false;
        }
        let mut last_applied = self
            .last_applied_frame
            .lock()
            .expect("terminal last applied frame lock poisoned");
        update.frame.apply(
            raw,
            last_applied.as_deref(),
            self.honor_application_cursor_style.load(Ordering::Acquire),
        );
        match update.selection {
            TerminalFrameSelection::Preserve => {}
            TerminalFrameSelection::Set(selection) => unsafe {
                sys::oneui_terminal_view_set_selection(
                    raw,
                    selection.start_row,
                    selection.start_column,
                    selection.end_row,
                    selection.end_column,
                )
            },
            TerminalFrameSelection::Clear => unsafe {
                sys::oneui_terminal_view_clear_selection(raw)
            },
        }
        *last_applied = Some(update.frame);
        true
    }
}
