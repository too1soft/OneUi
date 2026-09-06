use super::sys;
use super::{
    callback_panic_handler, clear_interaction_trace_handler, emit_interaction_trace,
    run_callback_guarded, run_remote_text_input_callback, run_time_series_inspection_callback,
    run_void_handler, run_window_client_size_changed_callback, run_window_raw_key_callback,
    set_callback_panic_handler, set_interaction_trace_handler,
    should_apply_application_cursor_style, terminal_style, traced_callback, traced_value_callback,
    Button, Color, Dialog, Error, FileDialogFilter, FileDialogMode, FileDialogOptions, IconSymbol,
    Insets, InteractionTrace, InteractiveSurface, InteractiveSurfaceStateStyle,
    InteractiveSurfaceStyle, Label, List, ListItem, LogLine, LogView, Menu, OverlayAlignment,
    OverlayHost, Panel, PixelFormat, Popup, PopupInteractionMode, PopupPreferredPlacement,
    ProgressBar, PromptOptions, RawKeyEvent, RealtimeFrameView, RemoteCursorImage, RemoteFrame,
    RemoteFrameDamage, RemoteFramePatch, RemoteInputRegion, RemoteTextInputCallback,
    ReorderableGrid, ScrollView, SegmentedControl, Select, SelectionMode, SplitOrientation,
    SplitView, Stack, StackDirection, StyleSheet, Switch, Table, TableColumn, TableRow, Tabs,
    TerminalCell, TerminalColor, TerminalCursor, TerminalCursorStyle, TerminalFrame,
    TerminalSelection, TerminalUnderlineStyle, TerminalView, TextArea, TextField, TimeSeries,
    TimeSeriesChart, TimeSeriesChartHandle, TimeSeriesInspection, TimeSeriesInspectionCallback,
    TimeSeriesThreshold, TreeItem, TreeView, VirtualList, VirtualListItem, VirtualListRichMetrics,
    Window, WindowClientSizeChangedCallback, WindowOptions, WindowPlacement, WindowRawKeyCallback,
    WindowState, WindowTitleBar,
};
use std::cell::{Cell, RefCell};
use std::ptr::NonNull;
use std::rc::Rc;
use std::sync::{
    atomic::{AtomicBool, Ordering},
    Arc, Mutex, OnceLock,
};
use std::thread;

fn window_test_lock() -> &'static Mutex<()> {
    static LOCK: OnceLock<Mutex<()>> = OnceLock::new();
    LOCK.get_or_init(|| Mutex::new(()))
}

// Each test is registered once and runs on the process main thread. This is
// required by AppKit and also makes global callback/clipboard tests deterministic.
macro_rules! native_tests {
    ($(fn $name:ident() $body:block)*) => {
        $(fn $name() $body)*
        pub(super) fn run() -> std::process::ExitCode {
            super::test_runner::run(&[$((stringify!($name), $name)),*])
        }
    };
}

native_tests! {
fn scoped_commands_release_contexts_and_preserve_utf8_positions() {
    use super::{CommandResult, KeyChord, KeyModifiers, TextAffinity, TextOptions, TextPosition};
    let field = TextField::new("输入").unwrap();
    field.set_text_options(&TextOptions::default()).unwrap();
    field.set_text("中é👨‍👩‍👧‍👦");
    field.set_text_position(TextPosition { utf8_offset: 3, affinity: TextAffinity::Downstream }).unwrap();
    assert_eq!(field.text_position().unwrap().utf8_offset, 3);
    assert!(matches!(field.set_text_position(TextPosition { utf8_offset: 4, affinity: TextAffinity::Downstream }), Err(Error::InvalidTextPosition)));
    let count = Rc::new(Cell::new(0));
    let captured = count.clone();
    let token = field.as_widget().register_command("test.count", Some(KeyChord::new("k", KeyModifiers::PRIMARY)), move || captured.set(captured.get() + 1)).unwrap();
    assert_eq!(field.as_widget().query_command("test.count"), CommandResult::Enabled);
    assert_eq!(field.as_widget().execute_command("test.count"), CommandResult::Executed);
    assert_eq!(count.get(), 1);
    assert!(field.as_widget().register_command("test.count", None, || {}).is_err());
    drop(token);
    assert_eq!(field.as_widget().query_command("test.count"), CommandResult::NotFound);
    assert_eq!(Rc::strong_count(&count), 1);
    let captured = count.clone();
    let token = field.as_widget().register_command("survive", None, move || captured.set(5)).unwrap();
    drop(field);
    assert_eq!(Rc::strong_count(&count), 1);
    drop(token);
}

fn command_callback_can_drop_its_own_registration() {
    use super::{CommandRegistration, CommandResult};
    let field = TextField::new("").unwrap();
    let token: Rc<RefCell<Option<CommandRegistration>>> = Rc::new(RefCell::new(None));
    let captured = token.clone();
    *token.borrow_mut() = Some(field.as_widget().register_command("self", None, move || { captured.borrow_mut().take(); }).unwrap());
    assert_eq!(field.as_widget().execute_command("self"), CommandResult::Executed);
    assert!(token.borrow().is_none());
    assert_eq!(Rc::strong_count(&token), 1);
}

fn application_cursor_style_requires_both_user_consent_and_an_application_frame() {
    assert!(!should_apply_application_cursor_style(false, false));
    assert!(!should_apply_application_cursor_style(false, true));
    assert!(!should_apply_application_cursor_style(true, false));
    assert!(should_apply_application_cursor_style(true, true));
}

fn client_size_callback_preserves_logical_dimensions() {
    let observed = Rc::new(RefCell::new(None));
    let observed_from_callback = Rc::clone(&observed);
    let mut callback = WindowClientSizeChangedCallback {
        handler: Box::new(move |width, height| {
            *observed_from_callback.borrow_mut() = Some((width, height));
        }),
    };

    unsafe {
        run_window_client_size_changed_callback(
            1024.5,
            720.25,
            (&mut callback as *mut WindowClientSizeChangedCallback).cast(),
        );
    }

    assert_eq!(*observed.borrow(), Some((1024.5, 720.25)));
}

fn time_series_inspection_callback_preserves_index_and_pin_state() {
    let observed = Rc::new(Cell::new(None));
    let observed_from_callback = Rc::clone(&observed);
    let mut callback = TimeSeriesInspectionCallback {
        handler: Box::new(move |event| observed_from_callback.set(Some(event))),
    };

    unsafe {
        run_time_series_inspection_callback(
            17,
            1,
            (&mut callback as *mut TimeSeriesInspectionCallback).cast(),
        );
    }

    assert_eq!(
        observed.get(),
        Some(TimeSeriesInspection {
            index: Some(17),
            pinned: true,
        })
    );
}

fn time_series_chart_safe_binding_keeps_gaps_thresholds_and_handle_lifetime_safe() {
    let _guard = window_test_lock().lock().expect("window test lock");
    let window = Window::new(&WindowOptions::default()).expect("window should be created");
    let chart = TimeSeriesChart::new().expect("time-series chart should be created");
    chart.set_range(0.0, 100.0);
    chart.set_grid_lines(4);
    chart.set_visual_style(false, true, true, false, 1.0, 18);
    chart.set_plot_insets(Insets {
        top: 4.0,
        right: 4.0,
        bottom: 4.0,
        left: 4.0,
    });
    chart.set_thresholds(&[TimeSeriesThreshold {
        value: 80.0,
        color: Color::rgba(245, 158, 11, 128),
    }]);
    chart.set_series(&[TimeSeries {
        name: "CPU".to_string(),
        color: Color::rgb(77, 163, 255),
        values: vec![Some(20.0), None, Some(42.0)],
    }]);
    chart.set_inspection(TimeSeriesInspection {
        index: Some(2),
        pinned: true,
    });
    assert_eq!(
        chart.inspection(),
        TimeSeriesInspection {
            index: Some(2),
            pinned: true,
        }
    );

    let handle: TimeSeriesChartHandle = window.time_series_chart_handle(&chart);
    handle
        .set_series(vec![TimeSeries {
            name: "内存".to_string(),
            color: Color::rgb(161, 111, 255),
            values: vec![Some(35.0), Some(36.0)],
        }])
        .expect("mounted chart handle should accept a coalesced update");
    window.set_content(chart.as_widget());
    drop(chart);
    assert_eq!(handle.set_series(Vec::new()), Err(Error::WidgetDestroyed));
    window.close();
}

fn remote_text_input_callback_preserves_committed_utf8() {
    let observed = Rc::new(RefCell::new(String::new()));
    let observed_from_callback = Rc::clone(&observed);
    let mut callback = RemoteTextInputCallback {
        handler: Box::new(move |value| *observed_from_callback.borrow_mut() = value),
    };
    let value = "中文输入";

    unsafe {
        run_remote_text_input_callback(
            value.as_ptr().cast(),
            value.len(),
            (&mut callback as *mut RemoteTextInputCallback).cast(),
        );
    }

    assert_eq!(observed.borrow().as_str(), value);
}

fn window_default_font_can_be_changed_through_window_and_dispatcher() {
    let _guard = window_test_lock().lock().expect("window test lock");
    let window = Window::new(&WindowOptions::default()).expect("window should be created");
    window.set_default_font_family("Segoe UI");
    window
        .dispatcher()
        .set_default_font_family("Microsoft YaHei UI")
        .expect("UI-thread font update should succeed");
    window.set_default_font_family("");
    window.close();
}

fn reports_panics_caught_at_callback_boundaries() {
    let _guard = window_test_lock().lock().expect("window test lock");
    let reports = Arc::new(Mutex::new(Vec::new()));
    let reports_for_handler = Arc::clone(&reports);
    set_callback_panic_handler(move |report| {
        reports_for_handler
            .lock()
            .expect("callback panic reports lock")
            .push(report);
    });

    let result = run_callback_guarded("test.callback", || panic!("callback failed"));

    assert!(result.is_none());
    let reports = reports.lock().expect("callback panic reports lock");
    assert_eq!(reports.len(), 1);
    assert_eq!(reports[0].context, "test.callback");
    assert_eq!(reports[0].message, "callback failed");
    drop(reports);
    *callback_panic_handler()
        .lock()
        .expect("callback panic handler lock") = None;
}

fn interaction_trace_observer_reports_callbacks_without_changing_behavior() {
    let _guard = window_test_lock().lock().expect("window test lock");
    clear_interaction_trace_handler();
    let reports = Arc::new(Mutex::new(Vec::new()));
    let reports_for_handler = Arc::clone(&reports);
    set_interaction_trace_handler(move |trace| {
        reports_for_handler
            .lock()
            .expect("interaction trace reports lock")
            .push(trace);
    });

    let invocations = Rc::new(Cell::new(0));
    let invocations_for_callback = Rc::clone(&invocations);
    let trace = InteractionTrace {
        control: "Button",
        interaction: "click",
        source_file: "tests/interaction.rs",
        source_line: 17,
        source_column: 9,
    };
    let mut callback = traced_callback(trace, move || {
        invocations_for_callback.set(invocations_for_callback.get() + 1);
    });

    callback();
    clear_interaction_trace_handler();
    callback();

    assert_eq!(invocations.get(), 2);
    let reports = reports.lock().expect("interaction trace reports lock");
    assert_eq!(reports.as_slice(), &[trace]);
}

fn application_interaction_trace_uses_the_same_isolated_observer() {
    let _guard = window_test_lock().lock().expect("window test lock");
    clear_interaction_trace_handler();
    let reports = Arc::new(Mutex::new(Vec::new()));
    let reports_for_handler = Arc::clone(&reports);
    set_interaction_trace_handler(move |trace| {
        reports_for_handler
            .lock()
            .expect("application interaction trace reports lock")
            .push(trace);
    });
    let trace = InteractionTrace {
        control: "NativeSingleInstance",
        interaction: "arguments",
        source_file: "tests/application_interaction.rs",
        source_line: 41,
        source_column: 5,
    };

    emit_interaction_trace(trace);
    clear_interaction_trace_handler();
    emit_interaction_trace(trace);

    let reports = reports.lock().expect("application trace reports lock");
    assert_eq!(reports.as_slice(), &[trace]);
}

fn interaction_trace_observer_panics_are_isolated_from_value_callbacks() {
    let _guard = window_test_lock().lock().expect("window test lock");
    clear_interaction_trace_handler();
    set_interaction_trace_handler(|_| panic!("trace observer failed"));
    let observed = Rc::new(Cell::new(None));
    let observed_for_callback = Rc::clone(&observed);
    let trace = InteractionTrace {
        control: "Tabs",
        interaction: "changed",
        source_file: "tests/interaction.rs",
        source_line: 33,
        source_column: 7,
    };
    let mut callback = traced_value_callback(trace, move |value| {
        observed_for_callback.set(Some(value));
    });

    callback(4_i32);
    clear_interaction_trace_handler();

    assert_eq!(observed.get(), Some(4));
}

fn nested_void_command_is_ignored_instead_of_panicking() {
    let invocations = Rc::new(Cell::new(0));
    let callback_slot = Rc::new(RefCell::new(
        None::<Rc<RefCell<Box<dyn FnMut() + 'static>>>>,
    ));
    let slot_for_callback = Rc::clone(&callback_slot);
    let invocations_for_callback = Rc::clone(&invocations);
    let handler: Rc<RefCell<Box<dyn FnMut() + 'static>>> =
        Rc::new(RefCell::new(Box::new(move || {
            invocations_for_callback.set(invocations_for_callback.get() + 1);
            let nested = slot_for_callback
                .borrow()
                .as_ref()
                .expect("handler should be installed")
                .clone();
            run_void_handler("test.nested_command", &nested);
        })));
    *callback_slot.borrow_mut() = Some(Rc::clone(&handler));

    run_void_handler("test.command", &handler);

    assert_eq!(invocations.get(), 1);
}

fn window_raw_key_callback_reports_consumption_and_preserves_event_data() {
    let observed = Rc::new(Cell::new(None));
    let observed_from_callback = Rc::clone(&observed);
    let mut callback = Box::new(WindowRawKeyCallback {
        handler: Box::new(move |event| {
            observed_from_callback.set(Some(event));
            event.pressed && event.ctrl && event.virtual_key == 0x4B
        }),
    });
    let event = sys::OneUiRawKeyEvent {
        virtual_key: 0x4B,
        scan_code: 0x25,
        pressed: 1,
        repeat: 0,
        extended: 0,
        alt: 0,
        ctrl: 1,
        shift: 0,
        win: 0,
    };

    let consumed = unsafe {
        run_window_raw_key_callback(
            &event,
            (&mut *callback as *mut WindowRawKeyCallback).cast(),
        )
    };

    assert_eq!(consumed, 1);
    assert_eq!(
        observed.get(),
        Some(RawKeyEvent {
            virtual_key: 0x4B,
            scan_code: 0x25,
            pressed: true,
            repeat: false,
            extended: false,
            alt: false,
            ctrl: true,
            shift: false,
            win: false,
        })
    );
}

fn window_raw_key_callback_does_not_consume_null_or_panicking_handlers() {
    assert_eq!(
        unsafe { run_window_raw_key_callback(std::ptr::null(), std::ptr::null_mut()) },
        0
    );

    let _guard = window_test_lock().lock().expect("window test lock");
    let mut callback = Box::new(WindowRawKeyCallback {
        handler: Box::new(|_| panic!("window shortcut failed")),
    });
    let event = sys::OneUiRawKeyEvent::default();
    let consumed = unsafe {
        run_window_raw_key_callback(
            &event,
            (&mut *callback as *mut WindowRawKeyCallback).cast(),
        )
    };
    assert_eq!(consumed, 0);
}

fn creates_hidden_window_through_utf8_abi() {
    let _guard = window_test_lock().lock().expect("window test lock");
    let window = Window::new(&WindowOptions {
        title: "iShellPro 麒麟 🚀".to_owned(),
        ..WindowOptions::default()
    })
    .expect("OneUI window should be created through the UTF-8 ABI");
    window.set_title("兴业银行股份有限公司");
}

fn runtime_fullscreen_and_minimum_client_size_round_trip_through_v21() {
    let _guard = window_test_lock().lock().expect("window test lock");
    let window = Window::new(&WindowOptions::default()).expect("window should be created");
    window.set_minimum_client_size(510.0, 700.0);
    assert!(!window.is_fullscreen());
    window.set_fullscreen(true);
    assert!(window.is_fullscreen());
    window.set_fullscreen(false);
    assert!(!window.is_fullscreen());
}

fn file_dialog_options_have_safe_platform_defaults() {
    let open = FileDialogOptions::open("Open recording");
    assert_eq!(open.mode, FileDialogMode::OpenFile);
    assert!(open.initial_directory.as_os_str().is_empty());
    assert!(open.default_name.is_empty());
    assert!(open.confirm_overwrite);

    let filters = [FileDialogFilter {
        name: "Terminal recordings",
        pattern: "*.cast",
    }];
    let save = FileDialogOptions {
        filters: &filters,
        default_extension: "cast",
        ..FileDialogOptions::save("Export recording")
    };
    assert_eq!(save.mode, FileDialogMode::SaveFile);
    assert_eq!(save.filters[0].pattern, "*.cast");

    let folder = FileDialogOptions::select_folder("Choose folder");
    assert_eq!(folder.mode, FileDialogMode::SelectFolder);
}

fn round_trips_window_placement_through_the_safe_binding() {
    let _guard = window_test_lock().lock().expect("window test lock");
    let window = Window::new(&WindowOptions {
        borderless: true,
        ..WindowOptions::default()
    })
    .expect("window should be created");
    let requested = WindowPlacement {
        x: 120,
        y: 140,
        width: 720,
        height: 520,
        maximized: true,
    };

    if !window.capabilities().placement() {
        assert!(!window.set_placement(requested));
        assert!(window.placement().is_none());
        return;
    }
    assert!(window.set_placement(requested));
    let actual = window.placement().expect("placement should be available");
    assert_eq!(actual.width, requested.width);
    assert_eq!(actual.height, requested.height);
    assert!(actual.maximized);
}

fn mounts_rust_composed_content_into_a_hidden_window() {
    let _guard = window_test_lock().lock().expect("window test lock");
    let window = Window::new(&WindowOptions::default()).expect("window should be created");
    let content = Stack::new(StackDirection::Column).expect("stack should be created");
    content.set_padding(Insets {
        top: 24.0,
        right: 24.0,
        bottom: 24.0,
        left: 24.0,
    });
    let label = Label::new("iShell Pro").expect("label should be created");
    label.set_font_size(20.0);
    content.add(label.as_widget());
    window.set_content(content.as_widget());
}

fn configures_resizable_split_view_through_safe_binding() {
    let _guard = window_test_lock().lock().expect("window test lock");
    let mut split =
        SplitView::new(SplitOrientation::Horizontal).expect("split view should be created");
    let first = Panel::new().expect("first panel should be created");
    let second = Panel::new().expect("second panel should be created");
    split.set_first(first.as_widget());
    split.set_second(second.as_widget());
    split.set_gap(6.0);
    split.set_padding(Insets {
        top: 1.0,
        right: 2.0,
        bottom: 3.0,
        left: 4.0,
    });
    split.set_minimum_pane_extent(120.0, 160.0);
    split.set_resizable(true);
    split.set_ratio(0.625);
    split.set_orientation(SplitOrientation::Vertical);
    split.set_on_ratio_changed(|_| {});
    split.set_on_ratio_committed(|_| {});

    assert!((split.ratio() - 0.625).abs() < 0.001);
    split.clear_on_ratio_changed();
    split.clear_on_ratio_committed();
}

fn applies_css_theme_to_semantic_widget_nodes() {
    let _guard = window_test_lock().lock().expect("window test lock");
    let window = Window::new(&WindowOptions::default()).expect("window should be created");
    let theme = StyleSheet::from_css(
        ":root { --surface: #1e1e2e; --text: #dcdeec; }\n\
         section.workspace { background: var(--surface); }\n\
         label.page-title { color: var(--text); font-size: 18px; font-weight: 600; }",
    )
    .expect("CSS should parse");
    window.set_style_sheet(&theme);

    let root = Panel::new().expect("panel should be created");
    root.as_widget()
        .set_style_node("section", "workspace")
        .expect("classes should be valid");
    let title = Label::new("Host management").expect("label should be created");
    title
        .as_widget()
        .set_classes("page-title")
        .expect("classes should be valid");
    let content = Stack::new(StackDirection::Column).expect("stack should be created");
    content.add(title.as_widget());
    root.set_content(content.as_widget());
    window.set_content(root.as_widget());
}

fn mounts_a_styled_interactive_surface_with_native_hover_states() {
    let _guard = window_test_lock().lock().expect("window test lock");
    let window = Window::new(&WindowOptions::default()).expect("window should be created");
    let surface = InteractiveSurface::new().expect("surface should be created");
    surface.set_style(InteractiveSurfaceStyle {
        normal: InteractiveSurfaceStateStyle::solid(
            Color::rgb(33, 35, 50),
            Color::rgb(57, 60, 79),
            10.0,
        ),
        hovered: InteractiveSurfaceStateStyle::solid(
            Color::rgb(48, 51, 70),
            Color::rgb(80, 84, 113),
            10.0,
        ),
        pressed: InteractiveSurfaceStateStyle::solid(
            Color::rgb(40, 42, 58),
            Color::rgb(101, 88, 241),
            10.0,
        ),
        disabled: InteractiveSurfaceStateStyle::solid(
            Color::rgb(33, 35, 50),
            Color::rgb(57, 60, 79),
            10.0,
        ),
        focus_visible: InteractiveSurfaceStateStyle::solid(
            Color::rgb(33, 35, 50),
            Color::rgb(101, 88, 241),
            10.0,
        ),
    });
    surface.set_padding(Insets {
        top: 12.0,
        right: 12.0,
        bottom: 12.0,
        left: 12.0,
    });
    let label = Label::new("Native card").expect("label should be created");
    surface.set_content(label.as_widget());
    window.set_content(surface.as_widget());
}

fn configures_a_native_reorderable_grid_without_product_geometry() {
    let mut grid = ReorderableGrid::new().expect("grid should be created");
    grid.set_column_count(2);
    grid.set_gaps(12.0, 8.0);
    grid.set_item_height(40.0);
    grid.set_reorder_enabled(true);
    grid.set_item_drag_enabled(true);
    grid.set_on_item_drag(|_| {});

    let first = Panel::new().expect("first panel should be created");
    let second = Panel::new().expect("second panel should be created");
    let third = Panel::new().expect("third panel should be created");
    grid.add_item("alpha", first.as_widget());
    grid.add_item("beta", second.as_widget());
    grid.add_item("gamma", third.as_widget());

    assert!(grid.reorder_enabled());
    assert!(grid.item_drag_enabled());
    assert!((grid.content_height() - 88.0).abs() < 0.001);
    assert!(grid.move_item("alpha", 2));
    assert!(!grid.move_item("missing", 0));
    grid.clear_items();
    assert!(grid.content_height().abs() < 0.001);
}

fn reports_stack_content_extent_for_scroll_view_composition() {
    let stack = Stack::new(StackDirection::Column).expect("stack should be created");
    stack.set_gap(7.0);
    stack.set_padding(Insets {
        top: 2.0,
        right: 3.0,
        bottom: 4.0,
        left: 5.0,
    });
    let first = Panel::new().expect("first panel should be created");
    first.as_widget().set_preferred_size(140.0, 19.0);
    let second = Panel::new().expect("second panel should be created");
    second.as_widget().set_preferred_size(80.0, 32.0);
    let third = Panel::new().expect("third panel should be created");
    third.as_widget().set_preferred_size(120.0, 28.0);
    stack.add(first.as_widget());
    stack.add(second.as_widget());
    stack.add(third.as_widget());

    assert!((stack.content_width() - 148.0).abs() < 0.001);
    assert!((stack.content_height() - 99.0).abs() < 0.001);
}

fn mounts_safe_inventory_controls_with_structured_utf8_list_items() {
    let _guard = window_test_lock().lock().expect("window test lock");
    let window = Window::new(&WindowOptions::default()).expect("window should be created");
    let content = Stack::new(StackDirection::Column).expect("stack should be created");
    let search = TextField::new("搜索主机、标签或地址").expect("text field should be created");
    let refresh = Button::new("刷新").expect("button should be created");
    let list = List::new().expect("list should be created");
    list.set_items(&[
        ListItem {
            title: "生产 SSH\t主机".to_string(),
            detail: "10.0.0.1\n兴业银行股份有限公司".to_string(),
        },
        ListItem {
            title: "Kylin V10".to_string(),
            detail: "堡垒机直连".to_string(),
        },
    ]);
    list.set_selected_index(1);
    assert_eq!(list.selected_index(), 1);

    let scroll = ScrollView::new().expect("scroll view should be created");
    scroll.set_wheel_step(40.0);
    scroll.set_content(list.as_widget());

    content.add(search.as_widget());
    content.add(refresh.as_widget());
    content.add(scroll.as_widget());
    window.set_content(content.as_widget());
}

fn retains_text_field_change_callbacks_through_programmatic_updates() {
    let _guard = window_test_lock().lock().expect("window test lock");
    let observed = Arc::new(Mutex::new(Vec::new()));
    let mut field = TextField::new("搜索主机").expect("text field should be created");
    field.set_password_mode(true);
    field.set_password_mask('●');
    let observed_for_callback = Arc::clone(&observed);
    field.set_on_changed(move |value| {
        observed_for_callback
            .lock()
            .expect("observed values lock")
            .push(value);
    });

    field.set_text("生产堡垒机");
    assert_eq!(
        observed.lock().expect("observed values lock").as_slice(),
        ["生产堡垒机"]
    );

    let long_value =
        "rename 5a9c8e06-0977-4426-8e3f-b8518f6c6500 QA Snippet Group Renamed 中文值";
    field.set_text(long_value);
    assert_eq!(
        observed.lock().expect("observed values lock").as_slice(),
        ["生产堡垒机", long_value]
    );
}

fn mounts_stateful_native_setting_controls() {
    let _guard = window_test_lock().lock().expect("window test lock");
    let window = Window::new(&WindowOptions::default()).expect("window should be created");
    let content = Stack::new(StackDirection::Column).expect("stack should be created");
    let mut segmented =
        SegmentedControl::new(&["卡片视图".to_string(), "列表视图".to_string()])
            .expect("segmented control should be created");
    segmented.set_selected_index(1);
    assert_eq!(segmented.selected_index(), 1);
    segmented.set_on_changed(|_| {});

    let mut switch = Switch::new("启用 Docker 管理").expect("switch should be created");
    switch.set_checked(true);
    assert!(switch.checked());
    switch.set_on_changed(|_| {});

    content.add(segmented.as_widget());
    content.add(switch.as_widget());
    window.set_content(content.as_widget());
}

fn mounts_utf8_workspace_tabs_and_reports_selection_changes() {
    let _guard = window_test_lock().lock().expect("window test lock");
    let window = Window::new(&WindowOptions::default()).expect("window should be created");
    let observed = std::rc::Rc::new(std::cell::RefCell::new(None));
    let mut tabs = Tabs::new(&["生产堡垒机".to_string(), "Kylin V10 🚀".to_string()])
        .expect("tabs should be created");
    let observed_for_callback = std::rc::Rc::clone(&observed);
    tabs.set_on_changed(move |index| *observed_for_callback.borrow_mut() = Some(index));
    tabs.set_selected_index(1);

    assert_eq!(tabs.selected_index(), 1);
    assert_eq!(*observed.borrow(), Some(1));
    window.set_content(tabs.as_widget());
}

fn mounts_utf8_select_and_reports_selection_changes() {
    let _guard = window_test_lock().lock().expect("window test lock");
    let window = Window::new(&WindowOptions::default()).expect("window should be created");
    let observed = std::rc::Rc::new(std::cell::RefCell::new(None));
    let mut select = Select::new(&[
        "手动排序".to_string(),
        "按名称".to_string(),
        "最近使用".to_string(),
    ])
    .expect("select should be created");
    let observed_for_callback = std::rc::Rc::clone(&observed);
    select.set_on_changed(move |index| *observed_for_callback.borrow_mut() = Some(index));
    select.set_selected_index(2);

    assert_eq!(select.selected_index(), 2);
    assert_eq!(*observed.borrow(), Some(2));
    window.set_content(select.as_widget());
}

fn reports_native_list_selection_changes_to_rust() {
    let _guard = window_test_lock().lock().expect("window test lock");
    let window = Window::new(&WindowOptions::default()).expect("window should be created");
    let selected = std::rc::Rc::new(std::cell::RefCell::new(None));
    let mut list = List::new().expect("list should be created");
    list.set_items(&[
        ListItem {
            title: "Production".to_owned(),
            detail: "10.0.0.1".to_owned(),
        },
        ListItem {
            title: "Staging".to_owned(),
            detail: "10.0.0.2".to_owned(),
        },
    ]);
    let observed = std::rc::Rc::clone(&selected);
    list.set_on_changed(move |index| *observed.borrow_mut() = Some(index));
    list.set_selected_index(1);

    assert_eq!(*selected.borrow(), Some(1));
    window.set_content(list.as_widget());
}

fn supports_an_explicit_unselected_list_state() {
    let _guard = window_test_lock().lock().expect("window test lock");
    let window = Window::new(&WindowOptions::default()).expect("window should be created");
    let list = List::new().expect("list should be created");
    list.set_items(&[
        ListItem {
            title: "Production".to_owned(),
            detail: "10.0.0.1".to_owned(),
        },
        ListItem {
            title: "Staging".to_owned(),
            detail: "10.0.0.2".to_owned(),
        },
    ]);
    list.set_selection_required(false);
    list.set_selected_index(-1);

    assert_eq!(list.selected_index(), -1);
    window.set_content(list.as_widget());
}

fn mounts_virtual_list_with_large_structured_data_without_widget_per_row() {
    let _guard = window_test_lock().lock().expect("window test lock");
    let window = Window::new(&WindowOptions::default()).expect("window should be created");
    let mut list = VirtualList::new().expect("virtual list should be created");
    let items: Vec<ListItem> = (0..5_000)
        .map(|index| ListItem {
            title: format!("Host {index}"),
            detail: format!("10.0.{}.{}", index / 255, index % 255),
        })
        .collect();
    list.set_items(&items);
    let drag_ids: Vec<String> = (0..5_000).map(|index| format!("host-{index}")).collect();
    assert!(list.set_item_drag_ids(&drag_ids));
    assert!(!list.set_item_drag_ids(&drag_ids[..4_999]));
    let mut duplicate_drag_ids = drag_ids.clone();
    duplicate_drag_ids[4_999] = duplicate_drag_ids[0].clone();
    assert!(!list.set_item_drag_ids(&duplicate_drag_ids));
    list.set_item_drag_enabled(true);
    assert!(list.item_drag_enabled());
    list.set_on_item_drag(|_| {});
    list.set_row_height(44.0);
    list.set_selected_index(4_999);
    assert_eq!(list.selected_index(), 4_999);
    assert!(list.max_scroll_offset() >= list.scroll_offset());

    let observed = std::rc::Rc::new(std::cell::RefCell::new(None));
    let callback_observed = std::rc::Rc::clone(&observed);
    list.set_on_changed(move |index| *callback_observed.borrow_mut() = Some(index));
    list.set_selected_index(12);
    assert_eq!(*observed.borrow(), Some(12));

    let selected_sets = std::rc::Rc::new(std::cell::RefCell::new(Vec::new()));
    let selected_sets_for_callback = std::rc::Rc::clone(&selected_sets);
    list.set_on_selection_changed(move |indices| {
        *selected_sets_for_callback.borrow_mut() = indices;
    });
    list.set_selection_mode(SelectionMode::Multiple);
    list.set_selected_indices(&[2, 5, 9]);
    assert_eq!(list.selected_indices(), vec![2, 5, 9]);
    assert_eq!(*selected_sets.borrow(), vec![2, 5, 9]);
    list.set_on_activated(|_| {});
    list.set_on_edit_requested(|_| {});
    list.set_on_context_menu_requested(|_| {});
    window.set_content(list.as_widget());
}

fn mounts_rich_virtual_list_rows_and_title_bar_leading_content() {
    let _guard = window_test_lock().lock().expect("window test lock");
    let window = Window::new(&WindowOptions::default()).expect("window should be created");
    let list = VirtualList::new().expect("virtual list should be created");
    let row = VirtualListItem {
        title: "ERP management".to_owned(),
        detail: "erp-demo.wangyunchuan.cn".to_owned(),
        badge: "HTTP".to_owned(),
        trailing: "Running".to_owned(),
        indicator_color: Some(Color::rgb(34, 197, 94)),
        trailing_color: Some(Color::rgb(22, 163, 74)),
    };
    list.set_rich_items(std::slice::from_ref(&row));
    assert!(list.update_rich_item(0, &row));
    let metrics = VirtualListRichMetrics {
        trailing_width: 54.0,
        ..VirtualListRichMetrics::default()
    };
    list.set_rich_metrics(metrics);
    assert!((list.rich_metrics().trailing_width - 54.0).abs() < 0.001);

    let title_bar = WindowTitleBar::new("Workspace").expect("title bar should be created");
    let leading = Panel::new().expect("leading panel should be created");
    title_bar.set_leading(leading.as_widget());
    title_bar.set_icon(IconSymbol::Folder);
    window.set_content(list.as_widget());
}

fn mounts_multiline_log_view_with_structured_colored_lines() {
    let _guard = window_test_lock().lock().expect("window test lock");
    let window = Window::new(&WindowOptions::default()).expect("window should be created");
    let log = LogView::new().expect("log view should be created");
    log.set_font_size(13.0);
    log.set_line_height(21.0);
    log.set_lines(&[
        LogLine {
            text: "ops@node:~$ uptime".to_owned(),
            color: Color::rgb(220, 226, 240),
        },
        LogLine {
            text: "load average: 0.12, 0.08, 0.05".to_owned(),
            color: Color::rgb(116, 218, 156),
        },
    ]);
    assert!(log.content_height() >= 42.0);
    log.clear();
    log.append_line(&LogLine {
        text: "fresh line".to_owned(),
        color: Color::rgb(132, 145, 255),
    });
    window.set_content(log.as_widget());
}

fn mounts_and_removes_a_modal_dialog_from_an_overlay_host() {
    let _guard = window_test_lock().lock().expect("window test lock");
    let window = Window::new(&WindowOptions::default()).expect("window should be created");
    let overlay = OverlayHost::new().expect("overlay host should be created");
    let page = Panel::new().expect("page should be created");
    overlay.set_content(page.as_widget());

    let mut dialog =
        Dialog::new("新建主机", "仅用于原生界面交互演示").expect("dialog should be created");
    dialog.set_icon(IconSymbol::Server);
    let dialog_body = Label::new("表单内容由产品层组合，弹层能力由 OneUI 统一提供。")
        .expect("dialog body should be created");
    dialog.set_content(dialog_body.as_widget());
    let closed = Arc::new(AtomicBool::new(false));
    let closed_for_callback = Arc::clone(&closed);
    dialog.set_on_close(move || closed_for_callback.store(true, Ordering::SeqCst));
    overlay.add_modal_anchored_overlay(
        dialog.as_widget(),
        10,
        440.0,
        240.0,
        Insets::default(),
        OverlayAlignment::Center,
        OverlayAlignment::Center,
    );
    window.set_content(overlay.as_widget());
    assert!(overlay.remove_overlay(dialog.as_widget()));
    assert!(!closed.load(Ordering::SeqCst));
}

fn mounts_a_light_dismiss_native_context_menu() {
    let _guard = window_test_lock().lock().expect("window test lock");
    let window = Window::new(&WindowOptions::default()).expect("window should be created");
    let overlay = OverlayHost::new().expect("overlay host should be created");
    let page = Panel::new().expect("page should be created");
    overlay.set_content(page.as_widget());

    let anchor = Panel::new().expect("popup anchor should be created");
    anchor.as_widget().set_preferred_size(1.0, 1.0);
    let mut menu = Menu::new().expect("menu should be created");
    menu.add_header("Production", "root@10.0.0.1:22");
    assert_eq!(
        menu.add_item("Connect", Some(IconSymbol::Terminal), false),
        0
    );
    menu.add_separator();
    assert_eq!(menu.add_item("Delete", Some(IconSymbol::Trash), true), 1);
    menu.set_on_activated(|_| {});
    menu.as_widget()
        .set_preferred_size(220.0, menu.preferred_height());

    let popup = Popup::new().expect("popup should be created");
    popup.set_anchor(anchor.as_widget());
    popup.set_content(menu.as_widget());
    popup.set_anchor_rect(40.0, 50.0, 1.0, 1.0);
    popup.set_preferred_placement(PopupPreferredPlacement::BottomStart);
    popup.set_interaction_mode(PopupInteractionMode::LightDismiss);
    popup.set_open(true);
    assert!(popup.is_open());

    overlay.add_anchored_overlay(
        popup.as_widget(),
        100,
        -1.0,
        -1.0,
        Insets::default(),
        OverlayAlignment::Start,
        OverlayAlignment::Start,
    );
    window.set_content(overlay.as_widget());
    assert!(overlay.remove_overlay(popup.as_widget()));
}

fn mounts_native_tree_with_structured_id_based_items() {
    let _guard = window_test_lock().lock().expect("window test lock");
    let window = Window::new(&WindowOptions::default()).expect("window should be created");
    let tree = TreeView::new().expect("tree view should be created");
    tree.set_items(&[
        TreeItem {
            id: "platform".to_owned(),
            title: "Platform".to_owned(),
            detail: "12".to_owned(),
            expanded: true,
            ..TreeItem::default()
        },
        TreeItem {
            id: "production".to_owned(),
            parent_id: "platform".to_owned(),
            title: "Production".to_owned(),
            detail: "8".to_owned(),
            expanded: true,
        },
    ]);
    tree.set_selected_id("production");
    assert_eq!(tree.selected_id(), "production");
    assert_eq!(tree.update_external_drop_target(-1.0, -1.0), "");
    assert_eq!(tree.external_drop_target_id(), "");
    tree.clear_external_drop_target();
    assert_eq!(tree.content_height(), 64.0);
    tree.as_widget().set_preferred_size(240.0, 0.0);
    window.set_content(tree.as_widget());
}

fn terminal_dirty_ranges_preserve_sparse_updates() {
    let mut previous = TerminalFrame {
        rows: 2,
        columns: 3,
        cells: vec![TerminalCell::default(); 6],
        cursor: TerminalCursor {
            row: 0,
            column: 0,
            visible: true,
        },
        cursor_style: TerminalCursorStyle::Block,
        cursor_blinking: true,
        cursor_style_from_application: false,
        mouse_reporting: false,
        first_visible_line_number: 1,
    };
    let mut current = previous.clone();
    assert_eq!(
        super::terminal_dirty_ranges(&previous, &current),
        Vec::new()
    );

    current.cells[4].text = "X".to_owned();
    assert_eq!(
        super::terminal_dirty_ranges(&previous, &current),
        vec![4..5]
    );

    previous.cells[1].text = "before".to_owned();
    current.cells[1].text = "after".to_owned();
    assert_eq!(
        super::terminal_dirty_ranges(&previous, &current),
        vec![1..2, 4..5]
    );

    current.cells[0].hyperlink_id = 42;
    assert_eq!(
        super::terminal_dirty_ranges(&previous, &current),
        vec![0..2, 4..5]
    );
}

fn terminal_cells_preserve_extended_attributes_across_the_c_abi() {
    let native = super::native_terminal_cells(&[TerminalCell {
        text: "docs".to_owned(),
        hyperlink_id: 73,
        underline_style: TerminalUnderlineStyle::Curly,
        underline_color: Some(TerminalColor::rgb(12, 34, 56)),
        ..TerminalCell::default()
    }]);
    assert_eq!(native.len(), 1);
    assert_eq!(native[0].hyperlink_id, 73);
    assert_eq!(
        native[0].underline_style,
        TerminalUnderlineStyle::Curly as u32
    );
    assert_eq!(native[0].underline_color.r, 12);
    assert_eq!(native[0].underline_color.g, 34);
    assert_eq!(native[0].underline_color.b, 56);
    assert_eq!(native[0].underline_color_set, 1);
}

fn terminal_dirty_ranges_collapse_pathological_fragmentation() {
    let previous = TerminalFrame {
        rows: 1,
        columns: 80,
        cells: vec![TerminalCell::default(); 80],
        cursor: TerminalCursor {
            row: 0,
            column: 0,
            visible: true,
        },
        cursor_style: TerminalCursorStyle::Block,
        cursor_blinking: true,
        cursor_style_from_application: false,
        mouse_reporting: false,
        first_visible_line_number: 1,
    };
    let mut current = previous.clone();
    for index in (0..80).step_by(2) {
        current.cells[index].text = "X".to_owned();
    }

    assert_eq!(
        super::terminal_dirty_ranges(&previous, &current),
        vec![0..79]
    );
}

fn mounts_terminal_grid_with_wide_cells_and_cursor() {
    let _guard = window_test_lock().lock().expect("window test lock");
    let window = Window::new(&WindowOptions::default()).expect("window should be created");
    let terminal = TerminalView::new().expect("terminal should be created");
    terminal.set_font_size(13.0);
    terminal.set_font_family("Cascadia Mono");
    terminal.set_palette(
        TerminalColor::rgb(20, 24, 36),
        TerminalColor::rgb(220, 226, 240),
        TerminalColor::rgb(170, 190, 255),
    );
    terminal.set_grid(
        2,
        3,
        &[
            TerminalCell {
                text: "A".to_owned(),
                ..TerminalCell::default()
            },
            TerminalCell {
                text: "宽".to_owned(),
                style: terminal_style::WIDE,
                ..TerminalCell::default()
            },
            TerminalCell {
                style: terminal_style::WIDE_CONTINUATION,
                ..TerminalCell::default()
            },
            TerminalCell::default(),
            TerminalCell::default(),
            TerminalCell::default(),
        ],
    );
    terminal.set_cursor(TerminalCursor {
        row: 0,
        column: 2,
        visible: true,
    });
    terminal.select_all();
    assert!(terminal.has_selection());
    assert_eq!(terminal.copy_selection(), window.capabilities().clipboard());
    assert_eq!(terminal.selected_text(), "A宽\r\n");
    terminal.set_selection(TerminalSelection {
        start_row: 0,
        start_column: 0,
        end_row: 0,
        end_column: 1,
    });
    assert_eq!(terminal.selected_text(), "A");
    terminal.clear_selection();
    assert!(!terminal.has_selection());
    window.set_content(terminal.as_widget());
}

fn exposes_terminal_caret_geometry_for_native_overlays() {
    let _guard = window_test_lock().lock().expect("window test lock");
    let window = Window::new(&WindowOptions::default()).expect("window should be created");
    let terminal = TerminalView::new().unwrap();
    terminal.set_grid(2, 4, &vec![TerminalCell::default(); 8]);
    terminal.set_cursor(TerminalCursor {
        row: 1,
        column: 3,
        visible: true,
    });
    window.set_content(terminal.as_widget());

    let caret = terminal.text_input_caret_rect().expect("caret rectangle");
    assert!(caret.x >= 3.0);
    assert!(caret.y >= 1.0);
    assert!(caret.width >= 1.0);
    assert!(caret.height >= 1.0);
}

fn terminal_handle_submits_a_worker_frame_on_the_window_thread() {
    let _guard = window_test_lock().lock().expect("window test lock");
    let window = Window::new(&WindowOptions::default()).expect("window should be created");
    let terminal = TerminalView::new().expect("terminal should be created");
    window.set_content(terminal.as_widget());
    let handle = window.terminal_view_handle(&terminal);
    let worker = thread::spawn(move || {
        handle
            .submit_frame(TerminalFrame {
                rows: 1,
                columns: 1,
                cells: vec![TerminalCell {
                    text: "X".to_owned(),
                    ..TerminalCell::default()
                }],
                cursor: TerminalCursor {
                    row: 0,
                    column: 0,
                    visible: true,
                },
                cursor_style: TerminalCursorStyle::Bar,
                cursor_blinking: false,
                cursor_style_from_application: true,
                mouse_reporting: true,
                first_visible_line_number: 1,
            })
            .expect("worker should submit a terminal frame");
    });
    worker.join().expect("worker should finish");

    let close_dispatcher = window.dispatcher();
    window
        .dispatch(move || close_dispatcher.request_close())
        .expect("window should accept close request");
    assert_eq!(window.run(), 0);
}

fn terminal_handle_rejects_updates_after_the_view_is_destroyed() {
    let _guard = window_test_lock().lock().expect("window test lock");
    let window = Window::new(&WindowOptions::default()).expect("window should be created");
    let handle = {
        let terminal = TerminalView::new().expect("terminal should be created");
        window.terminal_view_handle(&terminal)
    };

    assert!(matches!(
        handle.submit_frame(TerminalFrame {
            rows: 1,
            columns: 1,
            cells: vec![TerminalCell::default()],
            cursor: TerminalCursor {
                row: 0,
                column: 0,
                visible: true,
            },
            cursor_style: TerminalCursorStyle::Block,
            cursor_blinking: true,
            cursor_style_from_application: false,
            mouse_reporting: false,
            first_visible_line_number: 1,
        }),
        Err(Error::WidgetDestroyed)
    ));
}

fn remote_frame_normalizes_stride_and_rejects_short_buffers() {
    let frame = RemoteFrame::new(vec![0_u8; 16], 2, 2, 0, PixelFormat::Bgra8888, 7, 700)
        .expect("tightly packed remote frame should be valid");
    assert_eq!(frame.stride, 8);

    assert!(matches!(
        RemoteFrame::new(vec![0_u8; 15], 2, 2, 8, PixelFormat::Rgba8888, 8, 800,),
        Err(Error::InvalidVideoFrame { .. })
    ));
    assert!(matches!(
        RemoteFrame::new(vec![0_u8; 16], 2, 2, 7, PixelFormat::Bgra8888, 9, 900,),
        Err(Error::InvalidVideoFrame { .. })
    ));
}

fn remote_frame_damage_normalizes_patches_and_rejects_invalid_batches() {
    let damage = RemoteFrameDamage::new(
        2,
        2,
        PixelFormat::Bgra8888,
        vec![RemoteFramePatch::new(vec![1_u8; 4], 1, 1, 1, 1, 0)],
        8,
        800,
    )
    .expect("valid damage should be accepted");
    assert_eq!(damage.patches[0].stride, 4);

    assert!(matches!(
        RemoteFrameDamage::new(
            2,
            2,
            PixelFormat::Bgra8888,
            vec![RemoteFramePatch::new(vec![1_u8; 4], 2, 1, 1, 1, 4)],
            9,
            900,
        ),
        Err(Error::InvalidVideoFrame { .. })
    ));
    assert!(matches!(
        RemoteFrameDamage::new(2, 2, PixelFormat::Bgra8888, Vec::new(), 10, 1000),
        Err(Error::InvalidVideoFrame { .. })
    ));
}

fn remote_cursor_image_normalizes_stride_and_rejects_invalid_metadata() {
    let image = RemoteCursorImage::new(vec![0_u8; 16], 2, 2, 0, 1, 1)
        .expect("tightly packed cursor should be valid");
    assert_eq!(image.stride, 8);

    assert!(matches!(
        RemoteCursorImage::new(vec![0_u8; 15], 2, 2, 8, 1, 1),
        Err(Error::InvalidRemoteCursor { .. })
    ));
    assert!(matches!(
        RemoteCursorImage::new(vec![0_u8; 16], 2, 2, 8, 2, 1),
        Err(Error::InvalidRemoteCursor { .. })
    ));
    assert!(matches!(
        RemoteCursorImage::new(vec![0_u8; 513 * 4], 513, 1, 0, 0, 0),
        Err(Error::InvalidRemoteCursor { .. })
    ));
}

fn remote_input_handle_submits_worker_cursor_updates_on_the_window_thread() {
    let _guard = window_test_lock().lock().expect("window test lock");
    let window = Window::new(&WindowOptions::default()).expect("window should be created");
    let input = RemoteInputRegion::new().expect("remote input should be created");
    window.set_content(input.as_widget());
    let handle = window.remote_input_region_handle(&input);
    let worker = thread::spawn(move || {
        handle
            .set_remote_size(1920.0, 1080.0)
            .expect("worker should update the remote size");
        handle
            .submit_cursor_image(
                RemoteCursorImage::new(vec![255_u8; 16], 2, 2, 0, 1, 1)
                    .expect("cursor should be valid"),
            )
            .expect("worker should submit a cursor image");
        handle
            .set_cursor_position(960.0, 540.0)
            .expect("worker should update the cursor position");
        handle
            .set_cursor_hidden()
            .expect("worker should hide the cursor");
        handle
            .set_cursor_default()
            .expect("worker should restore the cursor");
        handle
            .release_all_inputs()
            .expect("worker should release captured input state");
    });
    worker.join().expect("worker should finish");

    let close_dispatcher = window.dispatcher();
    window
        .dispatch(move || close_dispatcher.request_close())
        .expect("window should accept close request");
    assert_eq!(window.run(), 0);
}

fn remote_input_handle_rejects_updates_after_region_destruction() {
    let _guard = window_test_lock().lock().expect("window test lock");
    let window = Window::new(&WindowOptions::default()).expect("window should be created");
    let handle = {
        let input = RemoteInputRegion::new().expect("remote input should be created");
        window.remote_input_region_handle(&input)
    };
    assert!(matches!(
        handle.set_remote_size(1920.0, 1080.0),
        Err(Error::WidgetDestroyed)
    ));
    assert!(matches!(
        handle.set_cursor_default(),
        Err(Error::WidgetDestroyed)
    ));
    assert!(matches!(
        handle.release_all_inputs(),
        Err(Error::WidgetDestroyed)
    ));
}

fn realtime_frame_handle_submits_worker_frames_on_the_window_thread() {
    let _guard = window_test_lock().lock().expect("window test lock");
    let window = Window::new(&WindowOptions::default()).expect("window should be created");
    let frame_view = RealtimeFrameView::new().expect("frame view should be created");
    window.set_content(frame_view.as_widget());
    let handle = window.realtime_frame_view_handle(&frame_view);
    let worker = thread::spawn(move || {
        for frame_id in 1..=3 {
            handle
                .submit_frame(
                    RemoteFrame::new(
                        vec![frame_id as u8; 16],
                        2,
                        2,
                        0,
                        PixelFormat::Bgra8888,
                        frame_id,
                        frame_id * 100,
                    )
                    .expect("worker frame should be valid"),
                )
                .expect("worker should submit a realtime frame");
        }
        handle
            .submit_damage(
                RemoteFrameDamage::new(
                    2,
                    2,
                    PixelFormat::Bgra8888,
                    vec![RemoteFramePatch::new(vec![9_u8; 4], 1, 1, 1, 1, 0)],
                    4,
                    400,
                )
                .expect("worker damage should be valid"),
            )
            .expect("worker should submit realtime damage");
    });
    worker.join().expect("worker should finish");

    let close_dispatcher = window.dispatcher();
    window
        .dispatch(move || close_dispatcher.request_close())
        .expect("window should accept close request");
    assert_eq!(window.run(), 0);
}

fn realtime_frame_handle_rejects_updates_after_view_destruction() {
    let _guard = window_test_lock().lock().expect("window test lock");
    let window = Window::new(&WindowOptions::default()).expect("window should be created");
    let handle = {
        let frame_view = RealtimeFrameView::new().expect("frame view should be created");
        window.realtime_frame_view_handle(&frame_view)
    };
    let frame = RemoteFrame::new(vec![0_u8; 4], 1, 1, 0, PixelFormat::Bgra8888, 1, 100)
        .expect("remote frame should be valid");
    assert!(matches!(
        handle.submit_frame(frame),
        Err(Error::WidgetDestroyed)
    ));
}

fn label_handle_coalesces_worker_updates_on_the_window_thread() {
    let _guard = window_test_lock().lock().expect("window test lock");
    let window = Window::new(&WindowOptions::default()).expect("window should be created");
    let label = Label::new("connecting").expect("label should be created");
    window.set_content(label.as_widget());
    let handle = window.label_handle(&label);
    let worker = thread::spawn(move || {
        handle
            .set_text("authenticating")
            .expect("worker should submit label text");
        handle
            .set_text("connected")
            .expect("worker should replace pending label text");
    });
    worker.join().expect("worker should finish");

    let close_dispatcher = window.dispatcher();
    window
        .dispatch(move || close_dispatcher.request_close())
        .expect("window should accept close request");
    assert_eq!(window.run(), 0);
}

fn label_handle_rejects_updates_after_the_label_is_destroyed() {
    let _guard = window_test_lock().lock().expect("window test lock");
    let window = Window::new(&WindowOptions::default()).expect("window should be created");
    let handle = {
        let label = Label::new("temporary").expect("label should be created");
        window.label_handle(&label)
    };

    assert!(matches!(
        handle.set_text("too late"),
        Err(Error::WidgetDestroyed)
    ));
}

fn text_area_handle_coalesces_worker_updates_on_the_window_thread() {
    let _guard = window_test_lock().lock().expect("window test lock");
    let window = Window::new(&WindowOptions::default()).expect("window should be created");
    let text_area = TextArea::new("").expect("text area should be created");
    window.set_content(text_area.as_widget());
    let handle = window.text_area_handle(&text_area);
    let worker = thread::spawn(move || {
        handle
            .set_text("connecting")
            .expect("worker should submit text area content");
        handle
            .set_text("connected")
            .expect("worker should replace pending text area content");
    });
    worker.join().expect("worker should finish");

    let close_dispatcher = window.dispatcher();
    window
        .dispatch(move || close_dispatcher.request_close())
        .expect("window should accept close request");
    assert_eq!(window.run(), 0);
}

fn text_area_handle_rejects_updates_after_the_editor_is_destroyed() {
    let _guard = window_test_lock().lock().expect("window test lock");
    let window = Window::new(&WindowOptions::default()).expect("window should be created");
    let handle = {
        let text_area = TextArea::new("").expect("text area should be created");
        window.text_area_handle(&text_area)
    };

    assert!(matches!(
        handle.set_text("too late"),
        Err(Error::WidgetDestroyed)
    ));
}

fn progress_bar_clamps_values_and_worker_handle_is_lifetime_safe() {
    let _guard = window_test_lock().lock().expect("window test lock");
    let window = Window::new(&WindowOptions::default()).expect("window should be created");
    let progress = ProgressBar::new().expect("progress bar should be created");
    progress.set_value(1.5);
    assert_eq!(progress.value(), 1.0);
    window.set_content(progress.as_widget());
    let handle = window.progress_bar_handle(&progress);
    let worker = thread::spawn(move || {
        handle
            .set_value(0.25)
            .expect("worker should submit progress value");
        handle
            .set_value(0.75)
            .expect("worker should coalesce progress value");
    });
    worker.join().expect("worker should finish");

    let close_dispatcher = window.dispatcher();
    window
        .dispatch(move || close_dispatcher.request_close())
        .expect("window should accept close request");
    assert_eq!(window.run(), 0);
}

fn progress_bar_handle_rejects_updates_after_widget_destruction() {
    let _guard = window_test_lock().lock().expect("window test lock");
    let window = Window::new(&WindowOptions::default()).expect("window should be created");
    let handle = {
        let progress = ProgressBar::new().expect("progress bar should be created");
        window.progress_bar_handle(&progress)
    };
    assert!(matches!(handle.set_value(0.5), Err(Error::WidgetDestroyed)));
}

fn widget_handle_rejects_layout_updates_after_widget_destruction() {
    let _guard = window_test_lock().lock().expect("window test lock");
    let window = Window::new(&WindowOptions::default()).expect("window should be created");
    let handle = {
        let panel = Panel::new().expect("panel should be created");
        window.widget_handle(panel.as_widget())
    };
    assert!(matches!(
        handle.set_visible(false),
        Err(Error::WidgetDestroyed)
    ));
    assert!(matches!(
        handle.set_preferred_size(240.0, 120.0),
        Err(Error::WidgetDestroyed)
    ));
}

fn virtual_list_handle_coalesces_row_updates_on_the_window_thread() {
    let _guard = window_test_lock().lock().expect("window test lock");
    let window = Window::new(&WindowOptions::default()).expect("window should be created");
    let list = VirtualList::new().expect("virtual list should be created");
    list.set_items(&[
        ListItem {
            title: "Alpha".to_owned(),
            detail: "Pending".to_owned(),
        },
        ListItem {
            title: "Beta".to_owned(),
            detail: "Pending".to_owned(),
        },
    ]);
    window.set_content(list.as_widget());
    let handle = window.virtual_list_handle(&list);
    let worker = thread::spawn(move || {
        handle
            .update_item(
                1,
                ListItem {
                    title: "Beta".to_owned(),
                    detail: "Checking".to_owned(),
                },
            )
            .expect("worker should submit a row update");
        handle
            .update_item(
                1,
                ListItem {
                    title: "Beta".to_owned(),
                    detail: "Online".to_owned(),
                },
            )
            .expect("worker should replace the pending row update");
        handle
            .update_rich_item(
                0,
                VirtualListItem {
                    title: "Alpha".to_owned(),
                    detail: "Online".to_owned(),
                    badge: "HTTPS".to_owned(),
                    trailing: "Ready".to_owned(),
                    indicator_color: Some(Color::rgb(34, 197, 94)),
                    trailing_color: None,
                },
            )
            .expect("worker should submit a rich row update");
    });
    worker.join().expect("worker should finish");

    let close_dispatcher = window.dispatcher();
    window
        .dispatch(move || close_dispatcher.request_close())
        .expect("window should accept close request");
    assert_eq!(window.run(), 0);
}

fn virtual_list_handle_replaces_a_background_data_revision() {
    let _guard = window_test_lock().lock().expect("window test lock");
    let window = Window::new(&WindowOptions::default()).expect("window should be created");
    let list = VirtualList::new().expect("virtual list should be created");
    list.set_items(&[ListItem {
        title: "Loading".to_owned(),
        detail: String::new(),
    }]);
    window.set_content(list.as_widget());
    let handle = window.virtual_list_handle(&list);
    let close_dispatcher = window.dispatcher();
    let worker = thread::spawn(move || {
        handle
            .set_items(vec![
                ListItem {
                    title: "Alpha".to_owned(),
                    detail: "Ready".to_owned(),
                },
                ListItem {
                    title: "Beta".to_owned(),
                    detail: "Ready".to_owned(),
                },
            ])
            .expect("background revision should be accepted");
        close_dispatcher.request_close();
    });
    assert_eq!(window.run(), 0);
    worker.join().expect("worker should finish");
}

fn virtual_list_handle_rejects_updates_after_the_list_is_destroyed() {
    let _guard = window_test_lock().lock().expect("window test lock");
    let window = Window::new(&WindowOptions::default()).expect("window should be created");
    let handle = {
        let list = VirtualList::new().expect("virtual list should be created");
        window.virtual_list_handle(&list)
    };

    assert!(matches!(
        handle.update_item(
            0,
            ListItem {
                title: "Too late".to_owned(),
                detail: String::new(),
            },
        ),
        Err(Error::WidgetDestroyed)
    ));
}

fn table_preserves_utf8_selection_and_coalesces_background_row_updates() {
    let _guard = window_test_lock().lock().expect("window test lock");
    let window = Window::new(&WindowOptions::default()).expect("window should be created");
    let table = Table::new().expect("table should be created");
    table.set_columns(&[
        TableColumn {
            header: "主机".to_owned(),
            width: 0.0,
        },
        TableColumn {
            header: "状态".to_owned(),
            width: 84.0,
        },
    ]);
    table.set_rows(&[
        TableRow {
            cells: vec!["生产节点".to_owned(), "在线".to_owned()],
        },
        TableRow {
            cells: vec!["数据库 🚀".to_owned(), "检测中".to_owned()],
        },
    ]);
    table.set_selection_mode(SelectionMode::Multiple);
    table.set_selected_indices(&[0, 1]);
    assert_eq!(table.selected_indices(), vec![0, 1]);
    window.set_content(table.as_widget());
    let handle = window.table_handle(&table);
    let worker = thread::spawn(move || {
        handle
            .update_row(
                1,
                TableRow {
                    cells: vec!["数据库 🚀".to_owned(), "在线".to_owned()],
                },
            )
            .expect("worker should submit a table row update");
        handle
            .update_row(
                1,
                TableRow {
                    cells: vec!["数据库 🚀".to_owned(), "12 ms".to_owned()],
                },
            )
            .expect("worker should coalesce a table row update");
    });
    worker.join().expect("worker should finish");

    let close_dispatcher = window.dispatcher();
    window
        .dispatch(move || close_dispatcher.request_close())
        .expect("window should accept close request");
    assert_eq!(window.run(), 0);
}

fn table_handle_replaces_rows_and_clears_selection_atomically() {
    let _guard = window_test_lock().lock().expect("window test lock");
    let window = Window::new(&WindowOptions::default()).expect("window should be created");
    let table = Table::new().expect("table should be created");
    table.set_rows(&[
        TableRow {
            cells: vec!["旧目录".to_owned()],
        },
        TableRow {
            cells: vec!["旧文件".to_owned()],
        },
    ]);
    table.set_selection_mode(SelectionMode::Multiple);
    table.set_selected_indices(&[1]);
    window.set_content(table.as_widget());

    let handle = window.table_handle(&table);
    handle
        .set_rows_and_clear_selection(vec![TableRow {
            cells: vec!["新目录".to_owned()],
        }])
        .expect("table revision should be accepted");
    let close_dispatcher = window.dispatcher();
    window
        .dispatch(move || close_dispatcher.request_close())
        .expect("window should accept close request");

    assert_eq!(window.run(), 0);
    assert!(table.selected_indices().is_empty());
}

fn virtual_list_full_reset_discards_row_patches_from_the_previous_revision() {
    let _guard = window_test_lock().lock().expect("window test lock");
    let window = Window::new(&WindowOptions::default()).expect("window should be created");
    let list = VirtualList::new().expect("virtual list should be created");
    list.set_items(&[ListItem {
        title: "Old".to_owned(),
        detail: "Pending".to_owned(),
    }]);
    window.set_content(list.as_widget());
    let handle = window.virtual_list_handle(&list);
    handle
        .update_item(
            0,
            ListItem {
                title: "Old".to_owned(),
                detail: "Online".to_owned(),
            },
        )
        .expect("row patch should be queued");

    list.set_items(&[ListItem {
        title: "New".to_owned(),
        detail: "Unknown".to_owned(),
    }]);
    assert!(list
        .state
        .pending_items
        .lock()
        .expect("virtual list pending items lock")
        .is_empty());

    let close_dispatcher = window.dispatcher();
    window
        .dispatch(move || close_dispatcher.request_close())
        .expect("window should accept close request");
    assert_eq!(window.run(), 0);
}

fn dispatcher_runs_work_on_the_window_thread() {
    let _guard = window_test_lock().lock().expect("window test lock");
    let window = Window::new(&WindowOptions::default()).expect("window should be created");
    let dispatcher = window.dispatcher();
    let callback_thread = Arc::new(Mutex::new(None));
    let callback_thread_from_worker = Arc::clone(&callback_thread);
    let ui_thread = thread::current().id();
    let worker = thread::spawn(move || {
        let close_dispatcher = dispatcher.clone();
        dispatcher
            .dispatch(move || {
                *callback_thread_from_worker.lock().expect("callback lock") =
                    Some(thread::current().id());
                close_dispatcher.request_close();
            })
            .expect("window should accept dispatched work");
    });

    assert_eq!(window.run(), 0);
    worker.join().expect("worker should finish");
    assert_eq!(
        *callback_thread.lock().expect("callback lock"),
        Some(ui_thread)
    );
}

fn dispatcher_defers_non_send_ui_work_without_crossing_threads() {
    let _guard = window_test_lock().lock().expect("window test lock");
    let window = Window::new(&WindowOptions::default()).expect("window should be created");
    let dispatcher = window.dispatcher();
    let observed = Rc::new(Cell::new(false));
    let observed_from_task = Rc::clone(&observed);
    let close_dispatcher = dispatcher.clone();
    dispatcher
        .dispatch_local(move || {
            observed_from_task.set(true);
            close_dispatcher.request_close();
        })
        .expect("window should accept local deferred work");

    assert!(!observed.get());
    assert_eq!(window.run(), 0);
    assert!(observed.get());
}

fn window_state_allows_reentrant_ui_thread_raw_access() {
    let state = WindowState {
        raw: Mutex::new(Some(NonNull::dangling())),
        ui_thread: thread::current().id(),
    };

    let nested = state.with_raw(|outer| state.with_raw(|inner| std::ptr::eq(outer, inner)));

    assert_eq!(nested, Some(Some(true)));
}

fn dispatcher_runs_one_shot_animation_frame_on_the_window_thread() {
    let _guard = window_test_lock().lock().expect("window test lock");
    let window = Window::new(&WindowOptions::default()).expect("window should be created");
    let dispatcher = window.dispatcher();
    let observed = Rc::new(Cell::new(false));
    let observed_from_frame = Rc::clone(&observed);
    let close_dispatcher = dispatcher.clone();
    dispatcher
        .request_animation_frame_local(move |now_ms| {
            assert!(now_ms.is_finite());
            observed_from_frame.set(true);
            close_dispatcher.request_close();
        })
        .expect("window should accept animation frame work");

    assert!(!observed.get());
    assert_eq!(window.run(), 0);
    assert!(observed.get());
}

fn dispatcher_cancels_queued_work_when_window_closes() {
    let _guard = window_test_lock().lock().expect("window test lock");
    struct DropFlag(Arc<AtomicBool>);

    impl Drop for DropFlag {
        fn drop(&mut self) {
            self.0.store(true, Ordering::Release);
        }
    }

    let window = Window::new(&WindowOptions::default()).expect("window should be created");
    let dispatcher = window.dispatcher();
    let dropped = Arc::new(AtomicBool::new(false));
    let flag = DropFlag(Arc::clone(&dropped));
    dispatcher
        .dispatch(move || drop(flag))
        .expect("window should accept queued work");
    assert!(matches!(
        dispatcher.confirm_blocking("Confirm", "Continue?"),
        Err(Error::UiThreadBlockingOperation)
    ));
    assert!(matches!(
        dispatcher.prompt_blocking(
            "Authentication",
            "Enter a value",
            PromptOptions {
                placeholder: "Value",
                ..PromptOptions::default()
            }
        ),
        Err(Error::UiThreadBlockingOperation)
    ));
    let background_dispatcher = dispatcher.clone();
    let direct_prompt_result = thread::spawn(move || {
        (
            background_dispatcher.confirm("Confirm", "Continue?"),
            background_dispatcher.prompt("Prompt", "Enter a value", PromptOptions::default()),
        )
    })
    .join()
    .expect("worker should finish");
    assert!(matches!(direct_prompt_result.0, Err(Error::WrongThread)));
    assert!(matches!(direct_prompt_result.1, Err(Error::WrongThread)));
    window.close();

    assert!(dropped.load(Ordering::Acquire));
    assert!(matches!(
        dispatcher.dispatch(|| {}),
        Err(Error::WindowClosed)
    ));
    let closed_result =
        thread::spawn(move || dispatcher.confirm_blocking("Confirm", "Continue?"))
            .join()
            .expect("worker should finish");
    assert!(matches!(closed_result, Err(Error::WindowClosed)));
}
}
