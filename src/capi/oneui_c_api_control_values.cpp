#include "oneui_c_api_controls_internal.h"
#include "oneui_c_api_error.h"
#include "oneui/controls/remote_input_region.h"
#include "oneui/controls/icon_view.h"
#include <algorithm>
#include <cmath>
#include <climits>
#include <cstring>
#include <cwchar>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace oneui::capi;

namespace oneui::capi {

int clampIconSymbol(int symbol) {
    return std::clamp(symbol, 0, static_cast<int>(oneui::IconSymbol::OutlineWorkbenchBroadcast));
}

void copyUtf8Field(const std::string& value, char* buffer, std::size_t bufferLength) {
    if (!buffer || bufferLength == 0) {
        return;
    }
    const std::size_t count = std::min(bufferLength - 1, value.size());
    std::copy_n(value.data(), count, buffer);
    buffer[count] = '\0';
}

oneui::FocusRingStyleOverride toFocusRingOverride(const OneUiFocusRingStyle& style) {
    oneui::FocusRingStyleOverride result;
    result.color = toNativeColor(style.color);
    result.width = style.width;
    result.offset = style.offset;
    result.radius = style.radius;
    result.visible = style.visible != 0;
    return result;
}

OneUiPointerButton toCButton(oneui::PointerButton button) {
    switch (button) {
    case oneui::PointerButton::Left:
        return OneUiPointerButtonLeft;
    case oneui::PointerButton::Right:
        return OneUiPointerButtonRight;
    case oneui::PointerButton::Middle:
        return OneUiPointerButtonMiddle;
    case oneui::PointerButton::X1:
        return OneUiPointerButtonX1;
    case oneui::PointerButton::X2:
        return OneUiPointerButtonX2;
    case oneui::PointerButton::None:
    default:
        return OneUiPointerButtonNone;
    }
}

OneUiPointerButton toCButton(oneui::MouseButton button) {
    switch (button) {
    case oneui::MouseButton::Left:
        return OneUiPointerButtonLeft;
    case oneui::MouseButton::Right:
        return OneUiPointerButtonRight;
    case oneui::MouseButton::Middle:
        return OneUiPointerButtonMiddle;
    case oneui::MouseButton::None:
    default:
        return OneUiPointerButtonNone;
    }
}

} // namespace oneui::capi
