use super::super::terminal_render::apply_terminal_frame;
use super::*;
use std::any::Any;
use std::time::{Duration, Instant};

const MAX_UPDATES_PER_TURN: usize = 16;
const TURN_BUDGET: Duration = Duration::from_millis(2);

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TerminalFrameRefreshOutcome {
    Replaced,
    QueuedAfterTake,
    FrameTaken,
    QueueAdvanced,
    ViewUnavailable,
}

/// A one-shot refresh tied to a particular terminal submission.
pub struct PendingTerminalFrame {
    handle: TerminalViewHandle,
    sequence: u64,
    allow_after_take: bool,
}

#[derive(Default)]
pub(super) struct TerminalFrameQueue {
    pub(super) pending: Option<TerminalFrameUpdate>,
    sequence: u64,
    can_refresh: bool,
}

impl TerminalFrameQueue {
    fn push(&mut self, update: TerminalFrameUpdate) -> Option<TerminalFrameUpdate> {
        self.sequence = self.sequence.wrapping_add(1);
        self.can_refresh = matches!(update.selection, TerminalFrameSelection::Preserve);
        self.pending.replace(update)
    }
}

impl PendingTerminalFrame {
    /// Replaces the original update only while it remains queued.
    pub fn replace<T: AsRef<str> + PartialEq + Send + 'static>(
        self,
        frame: TerminalFrame<T>,
    ) -> bool {
        self.complete(frame, false) == TerminalFrameRefreshOutcome::Replaced
    }

    /// Refreshes this submission. Recovery reservations can enqueue the frame
    /// after the original was taken as long as there has been no newer update.
    pub fn commit<T: AsRef<str> + PartialEq + Send + 'static>(
        self,
        frame: TerminalFrame<T>,
    ) -> TerminalFrameRefreshOutcome {
        let allow_after_take = self.allow_after_take;
        self.complete(frame, allow_after_take)
    }

    fn complete<T: AsRef<str> + PartialEq + Send + 'static>(
        self,
        frame: TerminalFrame<T>,
        allow_after_take: bool,
    ) -> TerminalFrameRefreshOutcome {
        use TerminalFrameRefreshOutcome::*;

        let update = TerminalFrameUpdate {
            frame: Box::new(frame),
            selection: TerminalFrameSelection::Preserve,
        };
        let mut queue = self
            .handle
            .state
            .frame_queue
            .lock()
            .expect("terminal pending update lock poisoned");
        if self.handle.state.raw.load(Ordering::Acquire).is_null() {
            return ViewUnavailable;
        }
        if queue.sequence != self.sequence || !queue.can_refresh {
            return QueueAdvanced;
        }
        let had_pending = queue.pending.is_some();
        if !had_pending && !allow_after_take {
            return FrameTaken;
        }
        let previous = queue.push(update);
        drop(queue);
        // The displaced frame can release a large owned grid (or user text).
        // Let the window take the replacement while that storage is released.
        drop(previous);
        if had_pending {
            Replaced
        } else if self.handle.schedule_pending_frame().is_ok() {
            QueuedAfterTake
        } else {
            ViewUnavailable
        }
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
        self.reserve_frame_refresh(false)
    }

    /// Reserves the latest selection-preserving submission. With recovery
    /// enabled it can be refreshed even after the window takes it.
    pub fn reserve_frame_refresh(&self, allow_after_take: bool) -> Option<PendingTerminalFrame> {
        let queue = self
            .state
            .frame_queue
            .lock()
            .expect("terminal pending update lock poisoned");
        if self.state.raw.load(Ordering::Acquire).is_null() || !queue.can_refresh {
            return None;
        }
        match queue.pending {
            Some(_) if !self.state.update_scheduled.load(Ordering::Acquire) => return None,
            None if !allow_after_take => return None,
            _ => {}
        }
        let sequence = queue.sequence;
        drop(queue);
        Some(PendingTerminalFrame {
            handle: self.clone(),
            sequence,
            allow_after_take,
        })
    }

    /// Queues a selection-preserving frame only if the window queue is empty.
    /// An update that arrived in the meantime retains its original position.
    pub fn submit_frame_if_queue_empty<T: AsRef<str> + PartialEq + Send + 'static>(
        &self,
        frame: TerminalFrame<T>,
    ) -> bool {
        let update = TerminalFrameUpdate {
            frame: Box::new(frame),
            selection: TerminalFrameSelection::Preserve,
        };
        let mut queue = self
            .state
            .frame_queue
            .lock()
            .expect("terminal pending update lock poisoned");
        if self.state.raw.load(Ordering::Acquire).is_null() || queue.pending.is_some() {
            return false;
        }
        let previous = queue.push(update);
        drop(queue);
        drop(previous);
        self.schedule_pending_frame().is_ok()
    }

    pub(super) fn submit_update<T: AsRef<str> + PartialEq + Send + 'static>(
        &self,
        frame: TerminalFrame<T>,
        selection: TerminalFrameSelection,
    ) -> Result<(), Error> {
        let update = TerminalFrameUpdate {
            frame: Box::new(frame),
            selection,
        };
        let mut queue = self
            .state
            .frame_queue
            .lock()
            .expect("terminal pending update lock poisoned");
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }
        let previous = queue.push(update);
        drop(queue);
        drop(previous);

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
            let mut queue = self
                .state
                .frame_queue
                .lock()
                .expect("terminal pending update lock poisoned");
            self.state.update_scheduled.store(false, Ordering::Release);
            queue.can_refresh = false;
            let removed = queue.pending.take();
            drop(queue);
            drop(removed);
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
            .frame_queue
            .lock()
            .expect("terminal pending update lock poisoned")
            .pending
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
            .frame_queue
            .lock()
            .expect("terminal pending update lock poisoned")
            .pending
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
