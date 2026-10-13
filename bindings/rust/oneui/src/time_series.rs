//! Time series bindings.

use super::visual_style::{Color, Insets};
use super::{
    run_callback_guarded, sys, Arc, AtomicBool, AtomicPtr, Error, Mutex, Ordering, UiDispatcher,
    Widget,
};

impl UiDispatcher {
    /// Creates a thread-safe multi-series producer for an operational chart.
    pub fn time_series_chart_handle(&self, chart: &TimeSeriesChart) -> TimeSeriesChartHandle {
        TimeSeriesChartHandle {
            state: Arc::clone(&chart.state),
            dispatcher: self.clone(),
        }
    }
}

/// One named series in an operational chart. `None` values are rendered as
/// real gaps rather than being joined to adjacent samples.
#[derive(Clone, Debug, PartialEq)]
pub struct TimeSeries {
    pub name: String,
    pub color: Color,
    pub values: Vec<Option<f64>>,
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct TimeSeriesThreshold {
    pub value: f64,
    pub color: Color,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct TimeSeriesInspection {
    pub index: Option<usize>,
    pub pinned: bool,
}

pub(super) struct TimeSeriesInspectionCallback {
    pub(super) handler: Box<dyn FnMut(TimeSeriesInspection) + 'static>,
}

pub(super) unsafe extern "C" fn run_time_series_inspection_callback(
    index: std::ffi::c_int,
    pinned: std::ffi::c_int,
    user_data: *mut std::ffi::c_void,
) {
    if user_data.is_null() {
        return;
    }
    let callback = unsafe { &mut *user_data.cast::<TimeSeriesInspectionCallback>() };
    let event = TimeSeriesInspection {
        index: usize::try_from(index).ok(),
        pinned: pinned != 0,
    };
    run_callback_guarded("time_series.inspection", || (callback.handler)(event));
}

fn set_native_time_series(raw: *mut sys::OneUiWidget, series: &[TimeSeries]) {
    let values = series
        .iter()
        .map(|item| {
            item.values
                .iter()
                .map(|value| value.unwrap_or(f64::NAN))
                .collect::<Vec<_>>()
        })
        .collect::<Vec<_>>();
    let native = series
        .iter()
        .zip(values.iter())
        .map(|(item, values)| sys::OneUiTimeSeriesUtf8 {
            name: sys::OneUiUtf8String::from_str(&item.name),
            color: item.color.into(),
            values: values.as_ptr(),
            value_count: values.len(),
        })
        .collect::<Vec<_>>();
    unsafe { sys::oneui_time_series_chart_set_series(raw, native.as_ptr(), native.len()) };
}

struct TimeSeriesChartState {
    raw: AtomicPtr<sys::OneUiWidget>,
    pending_series: Mutex<Option<Vec<TimeSeries>>>,
    update_scheduled: AtomicBool,
}

/// Thread-safe producer for a mounted operational chart. Bursts collapse to
/// the newest complete data revision before being applied on the UI thread.
#[derive(Clone)]
pub struct TimeSeriesChartHandle {
    state: Arc<TimeSeriesChartState>,
    dispatcher: UiDispatcher,
}

impl TimeSeriesChartHandle {
    pub fn set_series(&self, series: Vec<TimeSeries>) -> Result<(), Error> {
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }
        *self
            .state
            .pending_series
            .lock()
            .expect("time-series pending data lock poisoned") = Some(series);
        if self.state.update_scheduled.swap(true, Ordering::AcqRel) {
            return Ok(());
        }
        let state = Arc::clone(&self.state);
        let dispatched_state = Arc::clone(&state);
        if let Err(error) = self
            .dispatcher
            .dispatch(move || Self::drain_pending_series(&dispatched_state))
        {
            state.update_scheduled.store(false, Ordering::Release);
            state
                .pending_series
                .lock()
                .expect("time-series pending data lock poisoned")
                .take();
            return Err(error);
        }
        Ok(())
    }

    fn drain_pending_series(state: &TimeSeriesChartState) {
        loop {
            let series = state
                .pending_series
                .lock()
                .expect("time-series pending data lock poisoned")
                .take();
            let raw = state.raw.load(Ordering::Acquire);
            if raw.is_null() {
                state.update_scheduled.store(false, Ordering::Release);
                return;
            }
            if let Some(series) = series {
                set_native_time_series(raw, &series);
            }
            state.update_scheduled.store(false, Ordering::Release);
            if state
                .pending_series
                .lock()
                .expect("time-series pending data lock poisoned")
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

/// Native multi-series chart with gaps, thresholds and pointer/keyboard
/// inspection for operational dashboards.
pub struct TimeSeriesChart {
    widget: Widget,
    state: Arc<TimeSeriesChartState>,
    inspection_callback: Option<Box<TimeSeriesInspectionCallback>>,
}

impl TimeSeriesChart {
    pub fn new() -> Result<Self, Error> {
        let widget = Widget::from_raw(unsafe { sys::oneui_time_series_chart_create() })?;
        Ok(Self {
            state: Arc::new(TimeSeriesChartState {
                raw: AtomicPtr::new(widget.as_raw()),
                pending_series: Mutex::new(None),
                update_scheduled: AtomicBool::new(false),
            }),
            widget,
            inspection_callback: None,
        })
    }

    pub fn set_series(&self, series: &[TimeSeries]) {
        self.state
            .pending_series
            .lock()
            .expect("time-series pending data lock poisoned")
            .take();
        set_native_time_series(self.widget.as_raw(), series);
    }

    pub fn set_range(&self, minimum: f64, maximum: f64) {
        unsafe { sys::oneui_time_series_chart_set_range(self.widget.as_raw(), minimum, maximum) };
    }

    pub fn set_grid_lines(&self, count: i32) {
        unsafe { sys::oneui_time_series_chart_set_grid_lines(self.widget.as_raw(), count) };
    }

    pub fn set_visual_style(
        &self,
        smooth_curves: bool,
        area_fill: bool,
        dashed_grid: bool,
        axes_visible: bool,
        line_width: f32,
        fill_alpha: u8,
    ) {
        unsafe {
            sys::oneui_time_series_chart_set_visual_style(
                self.widget.as_raw(),
                i32::from(smooth_curves),
                i32::from(area_fill),
                i32::from(dashed_grid),
                i32::from(axes_visible),
                line_width,
                fill_alpha,
            )
        };
    }

    pub fn set_plot_insets(&self, insets: Insets) {
        unsafe {
            sys::oneui_time_series_chart_set_plot_insets(
                self.widget.as_raw(),
                sys::OneUiInsets {
                    top: insets.top,
                    right: insets.right,
                    bottom: insets.bottom,
                    left: insets.left,
                },
            )
        };
    }

    /// Set the number of interior vertical grid lines (0 hides them) and grid stroke width.
    pub fn set_grid_style(&self, vertical_lines: i32, line_width: f32) {
        unsafe {
            sys::oneui_time_series_chart_set_grid_style(
                self.widget.as_raw(),
                vertical_lines,
                line_width,
            )
        };
    }
    /// Labels ordered from the top horizontal grid line to the bottom. Colors/font follow CSS.
    pub fn set_axis_labels(&self, labels: &[String]) {
        let values = labels
            .iter()
            .map(|s| sys::OneUiUtf8String::from_str(s))
            .collect::<Vec<_>>();
        unsafe {
            sys::oneui_time_series_chart_set_axis_labels_utf8(
                self.widget.as_raw(),
                values.as_ptr(),
                values.len(),
            )
        };
    }
    /// Ordered positions in [0, 1] shared by series. Empty restores equal spacing.
    /// Returns false for invalid input, leaving previous positions unchanged.
    pub fn set_sample_positions(&self, positions: &[f64]) -> bool {
        unsafe {
            sys::oneui_time_series_chart_set_sample_positions(
                self.widget.as_raw(),
                positions.as_ptr(),
                positions.len(),
            ) != 0
        }
    }
    /// Set the endpoint marker dimensions; non-positive or non-finite values are ignored.
    pub fn set_latest_point_size(&self, width: f32, height: f32) {
        unsafe {
            sys::oneui_time_series_chart_set_latest_point_size(self.widget.as_raw(), width, height)
        };
    }

    pub fn set_latest_point_visible(&self, visible: bool) {
        unsafe {
            sys::oneui_time_series_chart_set_latest_point_visible(
                self.widget.as_raw(),
                i32::from(visible),
            )
        };
    }

    pub fn set_thresholds(&self, thresholds: &[TimeSeriesThreshold]) {
        let native = thresholds
            .iter()
            .map(|threshold| sys::OneUiTimeSeriesThreshold {
                value: threshold.value,
                color: threshold.color.into(),
            })
            .collect::<Vec<_>>();
        unsafe {
            sys::oneui_time_series_chart_set_thresholds(
                self.widget.as_raw(),
                native.as_ptr(),
                native.len(),
            )
        };
    }

    pub fn set_inspection(&self, inspection: TimeSeriesInspection) {
        let index = inspection
            .index
            .and_then(|index| i32::try_from(index).ok())
            .unwrap_or(-1);
        unsafe {
            sys::oneui_time_series_chart_set_inspection(
                self.widget.as_raw(),
                index,
                i32::from(inspection.pinned),
            )
        };
    }

    pub fn inspection(&self) -> TimeSeriesInspection {
        let index = unsafe { sys::oneui_time_series_chart_inspection_index(self.widget.as_raw()) };
        TimeSeriesInspection {
            index: usize::try_from(index).ok(),
            pinned: unsafe {
                sys::oneui_time_series_chart_inspection_pinned(self.widget.as_raw()) != 0
            },
        }
    }

    pub fn set_on_inspection_changed<F>(&mut self, callback: F)
    where
        F: FnMut(TimeSeriesInspection) + 'static,
    {
        self.clear_on_inspection_changed();
        self.inspection_callback = Some(Box::new(TimeSeriesInspectionCallback {
            handler: Box::new(callback),
        }));
        let user_data = (self
            .inspection_callback
            .as_deref_mut()
            .expect("time-series callback was just installed")
            as *mut TimeSeriesInspectionCallback)
            .cast();
        unsafe {
            sys::oneui_time_series_chart_set_on_inspection_changed(
                self.widget.as_raw(),
                Some(run_time_series_inspection_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_inspection_changed(&mut self) {
        unsafe {
            sys::oneui_time_series_chart_set_on_inspection_changed(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.inspection_callback = None;
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

impl Drop for TimeSeriesChart {
    fn drop(&mut self) {
        self.clear_on_inspection_changed();
        self.state
            .raw
            .store(std::ptr::null_mut(), Ordering::Release);
        self.state
            .pending_series
            .lock()
            .expect("time-series pending data lock poisoned")
            .take();
        self.state.update_scheduled.store(false, Ordering::Release);
    }
}
