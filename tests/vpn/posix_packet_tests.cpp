#include "oneui/vpn/posix_packet_pump.h"
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace {
std::mutex mutex;
std::condition_variable changed;
std::deque<std::vector<unsigned char>> inbound, outbound;
int inject(uint64_t host, void *bytes, int length, int) {
    assert(host == 42);
    std::lock_guard<std::mutex> lock(mutex);
    auto *p = static_cast<unsigned char *>(bytes);
    inbound.emplace_back(p, p + length); changed.notify_all(); return 0;
}
int receive(uint64_t host, void *bytes, int capacity, int timeout) {
    assert(host == 42 && capacity == 65535);
    std::unique_lock<std::mutex> lock(mutex);
    if (!changed.wait_for(lock, std::chrono::milliseconds(timeout), [] { return !outbound.empty(); })) return -3;
    auto packet = std::move(outbound.front()); outbound.pop_front();
    std::memcpy(bytes, packet.data(), packet.size()); return static_cast<int>(packet.size());
}
}
int main() {
    int pair[2]; assert(socketpair(AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0, pair) == 0);
    oneui::vpn::Backend backend; backend.inject = inject; backend.receive = receive;
    oneui::vpn::PosixPacketPump pump;
    assert(!pump.start(backend, 0, pair[0]));
    assert(pump.start(backend, 42, pair[0])); assert(!pump.start(backend, 42, pair[0]));
    const unsigned char a[]{0x45, 1, 2, 3, 4}, b[]{0x60, 9, 8, 7};
    assert(write(pair[1], a, sizeof(a)) == sizeof(a));
    {
        std::unique_lock<std::mutex> lock(mutex);
        assert(changed.wait_for(lock, std::chrono::seconds(2), [] { return !inbound.empty(); }));
        assert(inbound.front() == std::vector<unsigned char>(a, a + sizeof(a)));
        outbound.emplace_back(b, b + sizeof(b)); changed.notify_all();
    }
    pollfd ready{pair[1], POLLIN, 0}; assert(poll(&ready, 1, 2000) == 1);
    unsigned char bytes[32]; assert(read(pair[1], bytes, sizeof(bytes)) == sizeof(b)); assert(!std::memcmp(bytes, b, sizeof(b)));
    const auto before = std::chrono::steady_clock::now(); pump.stop();
    assert(std::chrono::steady_clock::now() - before < std::chrono::seconds(1));
    assert(!pump.running() && pump.error() == 0); assert(fcntl(pair[0], F_GETFD) >= 0);
    assert(pump.start(backend, 42, pair[0])); pump.stop(); pump.stop();
    int stream[2]; assert(pipe(stream) == 0); assert(!pump.start(backend, 42, stream[0]));
    close(stream[0]); close(stream[1]); close(pair[0]); close(pair[1]);
}
