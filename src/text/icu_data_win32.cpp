#include <mutex>
#include "unicode/udata.h"

extern const unsigned char oneuiEmbeddedIcuData[];

// Resolve SkUnicode's loader before the optional file-based Skia archive member
// is selected. Works in both a DLL and an independently linked static consumer.
bool SkLoadICU() {
    static std::once_flag once;
    static bool ready = false;
    std::call_once(once, [] {
        UErrorCode error = U_ZERO_ERROR;
        udata_setCommonData(oneuiEmbeddedIcuData, &error);
        if (U_SUCCESS(error)) {
            udata_setFileAccess(UDATA_NO_FILES, &error);
        }
        ready = U_SUCCESS(error);
    });
    return ready;
}
