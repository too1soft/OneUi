#pragma once
#include <cstddef>
#include <windows.h>

#include <shlobj.h>

namespace oneui::platform {
// Validate untrusted OLE CF_HDROP before handing it to DragQueryFile.
inline bool validDropFiles(const DROPFILES *drop, std::size_t bytes) {
    if (!drop || bytes < sizeof(DROPFILES) || bytes > 16u * 1024u * 1024u ||
        drop->pFiles < sizeof(DROPFILES) || drop->pFiles >= bytes)
        return false;
    const auto data = reinterpret_cast<const unsigned char *>(drop) + drop->pFiles;
    const auto remaining = bytes - drop->pFiles;
    if (drop->fWide) {
        if (drop->pFiles % alignof(wchar_t) || remaining < 2 * sizeof(wchar_t))
            return false;
        auto text = reinterpret_cast<const wchar_t *>(data);
        for (std::size_t i = 1; i < remaining / sizeof(wchar_t); ++i)
            if (!text[i - 1] && !text[i])
                return true;
    } else {
        for (std::size_t i = 1; i < remaining; ++i)
            if (!data[i - 1] && !data[i])
                return true;
    }
    return false;
}
} // namespace oneui::platform
