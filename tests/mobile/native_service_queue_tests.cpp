#include "oneui/mobile/native_service_queue.h"
#include <cassert>
#include <future>
#include <thread>
#include <vector>

using namespace oneui::mobile;
using namespace std::chrono_literals;
NativeServiceRequest take(NativeServiceQueue &queue) {
    NativeServiceRequest request;
    const auto end = std::chrono::steady_clock::now() + 2s;
    while (!queue.take(request) && std::chrono::steady_clock::now() < end) std::this_thread::yield();
    assert(request.id != 0); return request;
}
int main() {
    NativeServiceQueue queue;
    auto call = std::async(std::launch::async, [&] { return queue.invoke(3, "private key alias", 2s); });
    auto request = take(queue); assert(request.operation == 3 && request.payload == "private key alias");
    assert(queue.complete(request.id, 0, "private blob"));
    assert(!queue.complete(request.id, 0, "duplicate"));
    auto reply = call.get(); assert(reply.code == 0 && reply.payload == "private blob");
    assert(!queue.complete(request.id, 0, "late"));
    for (int i = 0; i < 50; ++i) assert(queue.invoke(1, "timeout", 1ms).code == -3);
    NativeServiceRequest empty; assert(!queue.take(empty));
    auto timeout = std::async(std::launch::async, [&] { return queue.invoke(2, "socket", 20ms); });
    request = take(queue); assert(timeout.get().code == -3); assert(!queue.complete(request.id, 0, "late permission"));
    std::vector<std::future<NativeServiceReply>> calls;
    for (int i = 0; i < 16; ++i) {
        calls.push_back(std::async(std::launch::async, [&] { return queue.invoke(1, "bounded", 2s); }));
        take(queue);
    }
    assert(queue.invoke(1, "seventeenth", 1ms).code == -2);
    queue.close();
    for (auto &pending : calls) assert(pending.get().code == -1);
    assert(queue.invoke(1, "closed", 1ms).code == -1);
    assert(!queue.take(empty)); assert(!queue.complete(request.id, 0, "closed"));
}
