#pragma once
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace oneui::mobile {
struct NativeServiceRequest { uint64_t id = 0; int operation = 0; std::string payload; };
struct NativeServiceReply { int code = -1; std::string payload; };
// Worker-thread native callbacks wait here while the platform event thread
// drains requests and asynchronously acknowledges its actual OS operations.
// Bounded to 16 concurrent requests, 4 MiB aggregate, 1 MiB data plus
// at most 4 KiB metadata per payload.
// close() wakes every waiter; stale/late/duplicate replies are rejected.
class NativeServiceQueue final {
public:
    ~NativeServiceQueue();
    NativeServiceReply invoke(int operation, std::string payload, std::chrono::milliseconds timeout);
    bool take(NativeServiceRequest &request);
    bool complete(uint64_t id, int code, std::string payload);
    void close();
private:
    struct Pending {
        int operation = 0;
        std::string input, output;
        int code = -1;
        size_t charged = 0;
        bool delivered = false, done = false;
        std::condition_variable changed;
        ~Pending();
    };
    std::mutex mutex_;
    std::condition_variable drained_;
    std::map<uint64_t, std::shared_ptr<Pending>> pending_;
    std::deque<uint64_t> ready_;
    uint64_t next_ = 0;
    size_t bytes_ = 0;
    bool closed_ = false;
};
}
