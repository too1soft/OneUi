#pragma once
#include "backend.h"
#include <atomic>
#include <mutex>
#include <thread>

namespace oneui::vpn {
// Owns a duplicate of a NONBLOCK packet FD, never the borrowed descriptor.
// Stop/join before releasing the Go host or a native callback context. This
// object is reusable after stop; callbacks must not call stop reentrantly.
class PosixPacketPump final {
public:
    PosixPacketPump() = default;
    ~PosixPacketPump();
    PosixPacketPump(const PosixPacketPump &) = delete;
    PosixPacketPump &operator=(const PosixPacketPump &) = delete;
    bool start(const Backend &backend, uint64_t host, int borrowedFd);
    void stop();
    bool running() const { return running_.load(); }
    int error() const { return error_.load(); }
private:
    std::mutex lifecycle_;
    Backend backend_;
    uint64_t host_ = 0;
    int fd_ = -1;
    std::atomic<bool> running_{false};
    std::atomic<int> error_{0};
    std::thread input_, output_;
    void input();
    void output();
    void fail(int code);
    bool ready(short events);
    void join();
};
}
