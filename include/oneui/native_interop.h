#pragma once
#include "oneui/oneui_c_api.h"
#include "oneui/widget.h"
#include <memory>

namespace oneui {
// C++ side of the existing opaque C widget ABI. Copies the native ownership;
// never destroys the caller's handle. Call on the UI thread with a live handle.
// This C++ function is NOT an extern-C function and is not called by Rust.
// Retaining the native widget does not retain foreign callback storage: pass
// its owner to Mount::registerNative or keep the foreign owner alive separately.
ONEUI_API std::shared_ptr<Widget> retainNativeWidget(const OneUiWidget* handle);
}
