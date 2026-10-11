// Font services share the UTF-8 validation and exception boundary of this TU.
int oneui_text_measure_width_utf8(OneUiUtf8String text, OneUiUtf8String family,
                                float size, int weight, float scale, float* width) {
    try {
        if (!width || !std::isfinite(size) || size <= 0 || !std::isfinite(scale) ||
            scale <= 0 || weight < 1 || weight > 1000 ||
            (!text.data && text.length) || (!family.data && family.length)) return 0;
        const auto value = utf8OrEmpty(text);
        const auto font = utf8OrEmpty(family);
        if ((text.length && utf8FromWide(value) != std::string(text.data, text.length)) ||
            (family.length && utf8FromWide(font) != std::string(family.data, family.length))) return 0;
        oneui::text::LayoutOptions options;
        options.text.wrap = oneui::TextWrapMode::NoWrap;
        options.family = font;
        options.size = size;
        options.weight = weight;
        options.scale = scale;
        *width = oneui::text::Layout::make(value, options)->width();
        return 1;
    } catch (...) { oneui::capi::recordCurrentException(__func__); return 0; }
}
