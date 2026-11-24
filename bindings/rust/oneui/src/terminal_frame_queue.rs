use super::super::terminal_render::apply_terminal_frame;
use super::*;

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

        if self.state.update_scheduled.swap(true, Ordering::AcqRel) {
            return Ok(());
        }

        let state = Arc::clone(&self.state);
        let dispatched_state = Arc::clone(&state);
        if let Err(error) = self
            .dispatcher
            .dispatch(move || Self::drain_pending_frame(&dispatched_state))
        {
            state.update_scheduled.store(false, Ordering::Release);
            state
                .pending_update
                .lock()
                .expect("terminal pending update lock poisoned")
                .take();
            return Err(error);
        }
        Ok(())
    }

    fn drain_pending_frame(state: &TerminalViewState) {
        loop {
            state.apply_pending_frame();

            state.update_scheduled.store(false, Ordering::Release);
            if state
                .pending_update
                .lock()
                .expect("terminal pending update lock poisoned")
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

impl TerminalViewState {
    pub(super) fn apply_pending_frame(&self) {
        let update = self
            .pending_update
            .lock()
            .expect("terminal pending update lock poisoned")
            .take();
        let raw = self.raw.load(Ordering::Acquire);
        if raw.is_null() {
            return;
        }
        if let Some(update) = update {
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
        }
    }
}
