#pragma once
#include <memory>
#include <string>

namespace oneui::mobile {
// Authenticated same-UID AF_UNIX channel in an application's private directory.
// Framed opaque messages, 1 MiB maximum, no payload logging. Each direction is
// serialized; receive/accept poll for at most 250 ms. close joins active I/O.
class LocalChannel final {
public:
    static std::shared_ptr<LocalChannel> listen(const std::string &directory, const std::string &name, const std::string &token);
    static std::shared_ptr<LocalChannel> connect(const std::string &directory, const std::string &name, const std::string &token);
    ~LocalChannel();
    LocalChannel(const LocalChannel &) = delete;
    std::shared_ptr<LocalChannel> accept(); // null on poll timeout
    bool receive(std::string &message); // false on poll timeout, including partial frame
    void send(const std::string &message);
    void close(); // Retain owner and retry if filesystem cleanup fails.
private:
    struct State;
    std::unique_ptr<State> state_;
    LocalChannel();
};
}
