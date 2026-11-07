#pragma once
#include <cstdint>
#include <string>
#include "native_bridge.h"

namespace oneui::vpn {
struct Backend {
    using Create = uint64_t (*)(char *, int, int, OneUiVpnNativeCallbacks *);
    using Inject = int (*)(uint64_t, void *, int, int);
    using Receive = int (*)(uint64_t, void *, int, int);
    using Close = int (*)(uint64_t);
    using Changed = int (*)(uint64_t);
    Create create = nullptr;
    Inject inject = nullptr;
    Receive receive = nullptr;
    Close close = nullptr;
    Changed changed = nullptr;
};
// Only packaged sonames are accepted. One Go runtime per process stays resident
// even when its ABI is rejected; unloading an initialized Go runtime is unsafe.
const Backend *loadBackend(const std::string &library);
const Backend *currentBackend();
}
