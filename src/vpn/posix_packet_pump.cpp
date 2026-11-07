#include "oneui/vpn/posix_packet_pump.h"
#include <array>
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

namespace oneui::vpn {
PosixPacketPump::~PosixPacketPump() { stop(); }
bool PosixPacketPump::start(const Backend &backend, uint64_t host, int borrowedFd) {
    std::lock_guard<std::mutex> lock(lifecycle_);
    if (fd_ >= 0 || !host || !backend.inject || !backend.receive) return false;
    const int flags = fcntl(borrowedFd, F_GETFL);
    if (flags < 0 || !(flags & O_NONBLOCK)) return false;
    fd_ = fcntl(borrowedFd, F_DUPFD_CLOEXEC, 0);
    if (fd_ < 0) return false;
    backend_ = backend; host_ = host; error_ = 0; running_ = true;
    try {
        input_ = std::thread(&PosixPacketPump::input, this);
        output_ = std::thread(&PosixPacketPump::output, this);
    } catch (...) { running_ = false; join(); return false; }
    return true;
}
void PosixPacketPump::join() {
    if (input_.joinable()) input_.join();
    if (output_.joinable()) output_.join();
    if (fd_ >= 0) ::close(fd_);
    fd_ = -1; host_ = 0; backend_ = {};
}
void PosixPacketPump::stop() {
    std::lock_guard<std::mutex> lock(lifecycle_);
    running_ = false; join();
}
void PosixPacketPump::fail(int code) {
    int expected = 0; error_.compare_exchange_strong(expected, code);
    running_ = false;
}
bool PosixPacketPump::ready(short events) {
    while (running_) {
        pollfd descriptor{fd_, events, 0};
        const int result = poll(&descriptor, 1, 200);
        if (result < 0 && errno == EINTR) continue;
        if (result < 0 || descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) { fail(-4); return false; }
        if (result > 0 && descriptor.revents & events) return true;
    }
    return false;
}
void PosixPacketPump::input() {
    std::array<unsigned char, 65535> packet{};
    while (ready(POLLIN)) {
        const auto count = read(fd_, packet.data(), packet.size());
        if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) continue;
        if (count <= 0) { fail(-4); break; }
        // Timeout is backpressure, not permission to silently lose a packet.
        int result;
        do { result = backend_.inject(host_, packet.data(), static_cast<int>(count), 200); }
        while (running_ && result == -3);
        if (running_ && result != 0) fail(result);
    }
}
void PosixPacketPump::output() {
    std::array<unsigned char, 65535> packet{};
    while (running_) {
        const int count = backend_.receive(host_, packet.data(), static_cast<int>(packet.size()), 200);
        if (!running_) break;
        if (count == -3) continue;
        if (count <= 0 || count > static_cast<int>(packet.size())) { fail(count < 0 ? count : -4); break; }
        while (ready(POLLOUT)) {
            const auto sent = write(fd_, packet.data(), static_cast<size_t>(count));
            if (sent < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) continue;
            if (sent != count) fail(-4); // Packet devices cannot resume partial writes.
            break;
        }
    }
}
}
