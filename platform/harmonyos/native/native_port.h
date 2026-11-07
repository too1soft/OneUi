#pragma once
#include "napi_support.h"
#include "oneui/vpn/backend.h"
#include "oneui/vpn/posix_packet_pump.h"

namespace oneui::harmony {
struct NativePort {
    napi_env owner = nullptr;
    const vpn::Backend *backend = nullptr;
    uint64_t host = 0;
    mobile::NativeServiceQueue services;
    vpn::PosixPacketPump packets;
    std::mutex lifecycle;
    bool closing = false, closed = false;
    static OneUiVpnNativeCallbacks callbacks(NativePort &port);
};
std::shared_ptr<NativePort> port(napi_env env, napi_value id);
void wipe(std::string &text);
}
