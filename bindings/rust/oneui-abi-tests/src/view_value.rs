use oneui_sys::*;
pub fn run() {
    extern "C" {
        fn oneui_test_view_value_layout(field: u32) -> usize;
        fn oneui_test_view_value(input: *const OneUiValue, output: *mut OneUiValue) -> i32;
    }
    let expected = [
        std::mem::size_of::<OneUiValue>(),
        std::mem::offset_of!(OneUiValue, data),
        std::mem::size_of::<OneUiValueData>(),
        std::mem::size_of::<OneUiValueUtf8>(),
        std::mem::offset_of!(OneUiValueUtf8, length),
    ];
    unsafe {
        for (index, size) in expected.into_iter().enumerate() {
            assert_eq!(size, oneui_test_view_value_layout(index as u32));
        }
        let mut input = OneUiValue::default();
        let mut output = OneUiValue::default();
        input.tag = ONEUI_VALUE_I64;
        input.data.integer = -123456789123;
        assert_eq!(oneui_test_view_value(&input, &mut output), 1);
        assert_eq!((output.tag, output.data.number), (ONEUI_VALUE_F64, 3.25));
        input = OneUiValue::default();
        input.tag = ONEUI_VALUE_BOOL;
        input.data.boolean = 1;
        assert_eq!(oneui_test_view_value(&input, &mut output), 1);
        assert_eq!(output.tag, ONEUI_VALUE_UTF8);
        assert_eq!(
            std::slice::from_raw_parts(output.data.utf8.data.cast::<u8>(), output.data.utf8.length),
            b"value"
        );
    }
    println!("PASSED view union layout and four-tag roundtrip");
}
