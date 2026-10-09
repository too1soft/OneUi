#pragma once

#include "oneui/export.h"
#include <string>
#include <vector>

namespace oneui {
struct FontFamilyInfo {
    std::string family; // UTF-8 system family name, usable by OneUI font setters.
    bool monospaced = false; // Fixed pitch as reported by the family's normal face.
};

// Fresh, sorted, unique snapshot of installed system families, excluding fonts
// registered only with OneUI. Windows: DirectWrite/GDI; macOS: CoreText;
// Linux: Fontconfig. No window or UI thread required. May block on OS discovery:
// call from a worker and retain the result for font pickers. Refresh explicitly
// after installation changes. Empty is valid on systems with no installed fonts;
// throws std::runtime_error when the platform manager cannot be created.
ONEUI_API std::vector<FontFamilyInfo> enumerateSystemFonts();
} // namespace oneui
