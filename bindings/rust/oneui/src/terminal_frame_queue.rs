use super::super::terminal_render::apply_terminal_frame;
use super::*;
use std::time::{Duration, Instant};

pub(super) const MAX_UPDATES_PER_TURN: usize = 4;
const TURN_BUDGET: Duration = Duration::from_millis(1);

impl TerminalViewHandle {
    pub(super) fn submit_update(
        &self,
        frame: TerminalFrame,
        selection: TerminalSelectionUpdate,
    ) -> Result<(), Error> {
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }

        *self
            .state
            .pending_update
            .lock()
            .expect("terminal pending update lock poisoned") =
            Some(TerminalFrameUpdate { frame, selection });

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
        apply_terminal_frame(
            raw,
            &update.frame,
            last_applied.as_ref(),
            self.honor_application_cursor_style.load(Ordering::Acquire),
        );
        match update.selection {
            TerminalSelectionUpdate::Preserve => {}
            TerminalSelectionUpdate::Set(selection) => unsafe {
                sys::oneui_terminal_view_set_selection(
                    raw,
                    selection.start_row,
                    selection.start_column,
                    selection.end_row,
                    selection.end_column,
                )
            },
            TerminalSelectionUpdate::Clear => unsafe {
                sys::oneui_terminal_view_clear_selection(raw)
            },
        }
        *last_applied = Some(update.frame);
        true
    }
}
