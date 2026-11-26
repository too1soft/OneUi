use super::frame_queue::QueuedTerminalFrame;
use super::{
    TerminalCell, TerminalCursor, TerminalCursorStyle, TerminalFrame, TerminalView,
    TerminalViewState,
};
use crate::{Window, WindowOptions};
use std::any::Any;
use std::sync::atomic::{AtomicBool, AtomicUsize, Ordering};
use std::sync::{mpsc, Arc, Weak};
use std::time::Duration;

fn frame(value: usize) -> TerminalFrame {
    TerminalFrame {
        rows: 1,
        columns: 2,
        cells: vec![
            TerminalCell {
                text: value.to_string(),
                ..TerminalCell::default()
            },
            TerminalCell::default(),
        ],
        cursor: TerminalCursor {
            row: 0,
            column: (value % 2) as u16,
            visible: true,
        },
        cursor_style: TerminalCursorStyle::Block,
        cursor_blinking: false,
        cursor_style_from_application: false,
        mouse_reporting: false,
        first_visible_line_number: 1,
        history_rows: 0,
        scrollback_offset: 0,
    }
}

pub(crate) fn pending_replacement_preserves_ownership_of_the_queued_update() {
    let window = Window::new(&WindowOptions::default()).expect("create test window");
    let terminal = TerminalView::new().expect("create terminal");
    window.set_content(terminal.as_widget());
    let handle = window.terminal_view_handle(&terminal);
    assert!(handle.pending_frame_replacement().is_none());

    handle.submit_frame(frame(1)).unwrap();
    let stale = handle.pending_frame_replacement().unwrap();
    handle.submit_frame(frame(2)).unwrap();
    assert!(!stale.replace(frame(3)));

    let active = handle.pending_frame_replacement().unwrap();
    let duplicate = handle.pending_frame_replacement().unwrap();
    assert!(active.replace(frame(4)));
    assert!(!duplicate.replace(frame(5)));
    let taken = handle.pending_frame_replacement().unwrap();
    terminal.apply_pending_frame();
    assert!(!taken.replace(frame(6)));
    let last = terminal.state.last_applied_frame.lock().unwrap();
    let last = last
        .as_ref()
        .unwrap()
        .as_any()
        .downcast_ref::<TerminalFrame>()
        .unwrap();
    assert_eq!(last.cells[0].text, "4");

    handle.submit_frame_clearing_selection(frame(7)).unwrap();
    assert!(handle.pending_frame_replacement().is_none());
}

pub(crate) fn pending_replacement_releases_displaced_storage_without_the_queue_lock() {
    struct DropProbe {
        state: Weak<TerminalViewState>,
        unlocked: Arc<AtomicBool>,
    }
    impl QueuedTerminalFrame for DropProbe {
        fn as_any(&self) -> &dyn Any {
            self
        }

        fn apply(
            &self,
            _: *mut crate::sys::OneUiWidget,
            _: Option<&dyn QueuedTerminalFrame>,
            _: bool,
        ) {
            panic!("the displaced frame must not be applied");
        }
    }
    impl Drop for DropProbe {
        fn drop(&mut self) {
            if let Some(state) = self.state.upgrade() {
                self.unlocked
                    .store(state.pending_update.try_lock().is_ok(), Ordering::Release);
            }
        }
    }

    let window = Window::new(&WindowOptions::default()).expect("create test window");
    let terminal = TerminalView::new().expect("create terminal");
    window.set_content(terminal.as_widget());
    let handle = window.terminal_view_handle(&terminal);
    handle.submit_frame(frame(1)).unwrap();
    let unlocked = Arc::new(AtomicBool::new(false));
    terminal
        .state
        .pending_update
        .lock()
        .unwrap()
        .as_mut()
        .unwrap()
        .frame = Box::new(DropProbe {
        state: Arc::downgrade(&terminal.state),
        unlocked: Arc::clone(&unlocked),
    });
    let reservation = handle.pending_frame_replacement().unwrap();
    assert!(reservation.replace(frame(2)));
    assert!(unlocked.load(Ordering::Acquire));
}

pub(crate) fn empty_queue_recovery_cannot_displace_a_pending_frame() {
    let window = Window::new(&WindowOptions::default()).expect("create test window");
    let terminal = TerminalView::new().expect("create terminal");
    window.set_content(terminal.as_widget());
    let handle = window.terminal_view_handle(&terminal);

    handle.submit_frame_clearing_selection(frame(1)).unwrap();
    assert!(!handle.submit_frame_if_queue_empty(frame(2)));
    assert!(handle.pending_frame_replacement().is_none());
    terminal.apply_pending_frame();

    assert!(handle.submit_frame_if_queue_empty(frame(3)));
    assert!(handle.pending_frame_replacement().is_some());
    assert!(!handle.submit_frame_if_queue_empty(frame(4)));
    terminal.apply_pending_frame();
    let last = terminal.state.last_applied_frame.lock().unwrap();
    let last = last
        .as_ref()
        .unwrap()
        .as_any()
        .downcast_ref::<TerminalFrame>()
        .unwrap();
    assert_eq!(last.cells[0].text, "3");
}

pub(crate) fn updates_yield_to_other_window_callbacks() {
    const UPDATES: usize = 12;
    let window = Window::new(&WindowOptions::default()).expect("create test window");
    let mut terminal = TerminalView::new().expect("create terminal");
    window.set_content(terminal.as_widget());
    let handle = window.terminal_view_handle(&terminal);
    let applied = Arc::new(AtomicUsize::new(0));
    let applied_at_other_callback = Arc::new(AtomicUsize::new(0));
    let after_apply = Arc::clone(&applied);
    let producer = handle.clone();
    let close = window.dispatcher();
    terminal.set_on_cursor_changed(move || {
        let count = after_apply.fetch_add(1, Ordering::Relaxed) + 1;
        if count < UPDATES {
            // Submit during native application of the current frame.
            producer
                .submit_frame(frame(count + 1))
                .expect("refill queue");
            // Hold this delivery past the window-work budget after it has
            // refilled the slot, so the next frame must wait for a new turn.
            std::thread::sleep(Duration::from_millis(3));
        } else {
            let close_window = close.clone();
            close
                .dispatch(move || close_window.request_close())
                .unwrap();
        }
    });

    handle.submit_frame(frame(1)).expect("queue first frame");
    let count = Arc::clone(&applied);
    let sample = Arc::clone(&applied_at_other_callback);
    window
        .dispatch(move || sample.store(count.load(Ordering::Relaxed), Ordering::Relaxed))
        .expect("queue a separate UI callback");

    let (finish, done) = mpsc::channel();
    let watchdog = window.dispatcher();
    let timeout = std::thread::spawn(move || {
        if done.recv_timeout(Duration::from_secs(5)).is_err() {
            let close = watchdog.clone();
            let _ = watchdog.dispatch(move || close.request_close());
        }
    });
    let exit = window.run();
    finish.send(()).ok();
    timeout.join().unwrap();

    assert_eq!(exit, 0);
    assert_eq!(
        applied.load(Ordering::Relaxed),
        UPDATES,
        "final frame was applied"
    );
    assert_eq!(
        applied_at_other_callback.load(Ordering::Relaxed),
        1,
        "the first terminal callback must yield after using its work budget",
    );
}
