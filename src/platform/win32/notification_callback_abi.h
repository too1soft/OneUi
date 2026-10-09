#pragma once

#include <windows.h>
#include <unknwn.h>

// This COM interface has no imported entry points. Expose its SDK declaration
// even when compiling for Win7, so the same binary can activate notifications
// on newer systems. Restore the deployment baseline before any other include;
// notification availability is checked by the runtime backend.
#pragma push_macro("NTDDI_VERSION")
#undef NTDDI_VERSION
#define NTDDI_VERSION NTDDI_WIN10
#include <NotificationActivationCallback.h>
#pragma pop_macro("NTDDI_VERSION")
