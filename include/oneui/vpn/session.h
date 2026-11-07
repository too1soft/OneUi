#pragma once
#include "oneui/vpn/c_api.h"
#include <thread>

namespace oneui::vpn {
// Renderer-independent owner-thread state machine. Native adapters own system
// handles; a protocol engine owns encryption and declares actual connectivity.
class ONEUI_VPN_API Session final {
  public:
    explicit Session(OneUiVpnHostV1 host);
    Session(const Session &) = delete;
    Session &operator=(const Session &) = delete;
    OneUiVpnResult connect(const OneUiVpnConfigV1 &);
    OneUiVpnResult disconnect();
    OneUiVpnResult report(uint64_t attempt, int32_t event, int32_t error);
    OneUiVpnResult snapshot(OneUiVpnSnapshotV1 &) const;
    OneUiVpnResult destroyable() const;

  private:
    OneUiVpnResult checkThread() const;
    void change(int32_t state, int32_t error = 0);
    OneUiVpnHostV1 host_{};
    std::thread::id owner_;
    OneUiVpnSnapshotV1 state_{};
    bool cleanupPending_ = false;
    bool stopRequested_ = false;
};
} // namespace oneui::vpn
