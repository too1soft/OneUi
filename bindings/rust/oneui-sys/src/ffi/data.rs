use std::ffi::c_int;

use crate::{OneUiRichListItemUtf8, OneUiVirtualListRichMetrics, OneUiWidget};

unsafe extern "C" {
    pub fn oneui_virtual_list_set_rich_items_utf8(
        list: *mut OneUiWidget,
        items: *const OneUiRichListItemUtf8,
        count: usize,
    );
    pub fn oneui_virtual_list_update_rich_item_utf8(
        list: *mut OneUiWidget,
        index: usize,
        item: *const OneUiRichListItemUtf8,
    ) -> c_int;
    pub fn oneui_virtual_list_set_rich_metrics(
        list: *mut OneUiWidget,
        indicator_space: f32,
        indicator_diameter: f32,
        badge_height: f32,
        badge_radius: f32,
        badge_horizontal_padding: f32,
        title_badge_gap: f32,
        trailing_width: f32,
        trailing_gap: f32,
    );
    pub fn oneui_virtual_list_rich_metrics(
        list: *mut OneUiWidget,
        out_metrics: *mut OneUiVirtualListRichMetrics,
    ) -> c_int;
}
