# Yoga 3.2.1's single-flex-child fast path assumes a single line and zeros
# that child's basis even for wrapping rows. Keep the shortcut for nowrap.
# This is deliberately an exact, idempotent patch to the pinned source.
set(path "${ONEUI_YOGA_SOURCE}/yoga/algorithm/CalculateLayout.cpp")
file(READ "${path}" source)
set(before "  if (sizingModeMainDim == SizingMode::StretchFit) {\n    for (auto child : children) {")
set(after "  if (sizingModeMainDim == SizingMode::StretchFit &&\n      node->style().flexWrap() == Wrap::NoWrap) {\n    for (auto child : children) {")
string(FIND "${source}" "${after}" applied)
if(applied EQUAL -1)
    string(FIND "${source}" "${before}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Pinned Yoga wrap patch no longer matches; review upstream before updating")
    endif()
    string(REPLACE "${before}" "${after}" source "${source}")
    file(WRITE "${path}" "${source}")
endif()
