#include "oneui/mobile/local_channel.h"
#include "oneui/mobile/private_directory.h"
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <fcntl.h>
#include <mutex>
#include <poll.h>
#include <stdexcept>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

namespace oneui::mobile {
namespace {
using Clock = std::chrono::steady_clock;
constexpr size_t maximum = 1024 * 1024;
void require(bool condition) { if (!condition) throw std::runtime_error("Private channel operation failed"); }
void wipe(std::string &text) { volatile char *p = text.empty() ? nullptr : &text[0]; for (size_t i = 0; i < text.size(); ++i) p[i] = 0; text.clear(); }
void descriptor(int fd) {
    require(fd >= 0 && fcntl(fd, F_SETFD, FD_CLOEXEC) == 0 && fcntl(fd, F_SETFL, O_NONBLOCK) == 0);
#if defined(SO_NOSIGPIPE)
    const int enabled = 1; require(setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled)) == 0);
#endif
}
int socket() {
#if defined(SOCK_CLOEXEC)
    return ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
#else
    return ::socket(AF_UNIX, SOCK_STREAM, 0);
#endif
}
void peer(int fd) {
#if defined(__APPLE__)
    uid_t uid; gid_t gid; require(getpeereid(fd, &uid, &gid) == 0 && uid == getuid());
#else
    struct ucred identity{}; socklen_t size = sizeof(identity);
    require(getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &identity, &size) == 0 && size == sizeof(identity) && identity.uid == getuid());
#endif
}
bool wait(int fd, short events, Clock::time_point deadline, const std::atomic<bool> &active) {
    while (active) {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count();
        if (remaining <= 0) return false;
        pollfd item{fd, events, 0}; const int result = poll(&item, 1, static_cast<int>(remaining));
        if (result < 0 && errno == EINTR) continue;
        require(result >= 0);
        if (!result) return false;
        if (item.revents & events) return true;
        require(!(item.revents & (POLLHUP | POLLERR | POLLNVAL)));
    }
    throw std::runtime_error("Private channel closed");
}
void transfer(int fd, char *data, size_t size, bool write, Clock::time_point deadline, const std::atomic<bool> &active) {
    size_t offset = 0;
    while (offset < size) {
        require(active);
#if defined(MSG_NOSIGNAL)
        constexpr int sendFlags = MSG_NOSIGNAL;
#else
        constexpr int sendFlags = 0;
#endif
        const auto count = write ? ::send(fd, data + offset, size - offset, sendFlags) : ::recv(fd, data + offset, size - offset, 0);
        if (count < 0 && errno == EINTR) continue;
        if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) { require(wait(fd, write ? POLLOUT : POLLIN, deadline, active)); continue; }
        require(count > 0); offset += static_cast<size_t>(count);
    }
}
void validate(const std::string &name, const std::string &token) {
    require(!name.empty() && name.size() <= 64 && token.size() == 64);
    for (char c : name) require((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_');
    for (char c : token) require((c >= 'a' && c <= 'f') || (c >= '0' && c <= '9'));
}
sockaddr_un address(const std::string &directory, const std::string &name, int root) {
    char *resolved = realpath(directory.c_str(), nullptr); require(resolved != nullptr);
    const std::string base(resolved); free(resolved);
    struct stat opened{}, found{};
    require(fstat(root, &opened) == 0 && stat(base.c_str(), &found) == 0 && opened.st_dev == found.st_dev && opened.st_ino == found.st_ino);
    const auto path = base + '/' + name + ".sock";
    sockaddr_un value{}; value.sun_family = AF_UNIX; require(path.size() < sizeof(value.sun_path));
    std::memcpy(value.sun_path, path.c_str(), path.size() + 1); return value;
}
}
struct LocalChannel::State {
    int fd = -1, root = -1, guard = -1;
    bool listener = false;
    std::atomic<bool> active{true};
    std::mutex input, output, closing;
    std::string name, token, partial;
    dev_t device = 0; ino_t inode = 0;
    unsigned char header[4]{}; size_t headerRead = 0, bodyRead = 0;
    ~State() { if (fd >= 0) ::close(fd); if (guard >= 0) ::close(guard); if (root >= 0) ::close(root); wipe(token); wipe(partial); }
    void abort() { active = false; if (fd >= 0) shutdown(fd, SHUT_RDWR); }
    void authenticate(bool client) {
        const auto deadline = Clock::now() + std::chrono::seconds(1);
        std::string received(64, '\0');
        try {
            if (client) transfer(fd, &token[0], token.size(), true, deadline, active);
            transfer(fd, &received[0], received.size(), false, deadline, active);
            unsigned difference = 0; for (size_t i = 0; i < token.size(); ++i) difference |= static_cast<unsigned char>(token[i] ^ received[i]);
            require(difference == 0);
            if (!client) transfer(fd, &token[0], token.size(), true, deadline, active);
        } catch (...) { wipe(received); abort(); throw; }
        wipe(received); wipe(token);
    }
};
LocalChannel::LocalChannel() : state_(new State) {}
LocalChannel::~LocalChannel() { try { close(); } catch (...) {} }
std::shared_ptr<LocalChannel> LocalChannel::listen(const std::string &directory, const std::string &name, const std::string &token) {
    validate(name, token); auto result = std::shared_ptr<LocalChannel>(new LocalChannel); auto &s = *result->state_;
    s.root = openPrivateDirectory(directory); const auto target = address(directory, name, s.root);
    s.name = name + ".sock"; s.token = token;
    s.guard = openat(s.root, (name + ".lock").c_str(), O_RDWR | O_CREAT | O_NOFOLLOW | O_CLOEXEC, 0600);
    struct stat lock{};
    require(s.guard >= 0 && fstat(s.guard, &lock) == 0 && S_ISREG(lock.st_mode) && lock.st_uid == getuid() && lock.st_nlink == 1 && (lock.st_mode & 077) == 0);
    require(flock(s.guard, LOCK_EX | LOCK_NB) == 0);
    struct stat old{};
    if (fstatat(s.root, s.name.c_str(), &old, AT_SYMLINK_NOFOLLOW) == 0) {
        require(S_ISSOCK(old.st_mode) && old.st_uid == getuid() && (old.st_mode & 077) == 0);
        const int probe = socket(); require(probe >= 0);
        descriptor(probe); const int connected = ::connect(probe, reinterpret_cast<const sockaddr *>(&target), sizeof(target));
        const int failure = errno; ::close(probe); require(connected < 0 && failure == ECONNREFUSED);
        // The lifetime lock excludes another cooperating server during cleanup.
        struct stat again{}; require(fstatat(s.root, s.name.c_str(), &again, AT_SYMLINK_NOFOLLOW) == 0 && again.st_dev == old.st_dev && again.st_ino == old.st_ino);
        require(unlinkat(s.root, s.name.c_str(), 0) == 0);
    } else require(errno == ENOENT);
    s.fd = socket(); descriptor(s.fd);
    require(bind(s.fd, reinterpret_cast<const sockaddr *>(&target), sizeof(target)) == 0);
    struct stat own{}; require(fstatat(s.root, s.name.c_str(), &own, AT_SYMLINK_NOFOLLOW) == 0 && S_ISSOCK(own.st_mode) && own.st_uid == getuid());
    s.device = own.st_dev; s.inode = own.st_ino; s.listener = true;
    require(fchmodat(s.root, s.name.c_str(), 0600, 0) == 0 && ::listen(s.fd, 4) == 0); return result;
}
std::shared_ptr<LocalChannel> LocalChannel::connect(const std::string &directory, const std::string &name, const std::string &token) {
    validate(name, token); auto result = std::shared_ptr<LocalChannel>(new LocalChannel); auto &s = *result->state_;
    s.root = openPrivateDirectory(directory); const auto target = address(directory, name, s.root);
    s.fd = socket(); descriptor(s.fd);
    const int connected = ::connect(s.fd, reinterpret_cast<const sockaddr *>(&target), sizeof(target));
    if (connected < 0) {
        require(errno == EINPROGRESS && wait(s.fd, POLLOUT, Clock::now() + std::chrono::seconds(1), s.active));
        int error = 0; socklen_t length = sizeof(error); require(getsockopt(s.fd, SOL_SOCKET, SO_ERROR, &error, &length) == 0 && !error);
    }
    peer(s.fd); s.token = token; s.authenticate(true); return result;
}
std::shared_ptr<LocalChannel> LocalChannel::accept() {
    auto &s = *state_; std::lock_guard<std::mutex> lock(s.input); require(s.listener && s.active);
    if (!wait(s.fd, POLLIN, Clock::now() + std::chrono::milliseconds(250), s.active)) return {};
    auto result = std::shared_ptr<LocalChannel>(new LocalChannel); auto &child = *result->state_;
    child.fd = ::accept(s.fd, nullptr, nullptr); descriptor(child.fd); peer(child.fd);
    child.token = s.token; child.authenticate(false); return result;
}
void LocalChannel::send(const std::string &message) {
    auto &s = *state_; std::lock_guard<std::mutex> lock(s.output); require(!s.listener && s.active && !message.empty() && message.size() <= maximum);
    std::string frame(4, '\0'); const auto length = message.size();
    for (unsigned i = 0; i < 4; ++i) frame[i] = static_cast<char>(length >> (24 - 8 * i));
    frame += message;
    try { transfer(s.fd, &frame[0], frame.size(), true, Clock::now() + std::chrono::seconds(3), s.active); }
    catch (...) { wipe(frame); s.abort(); throw; }
    wipe(frame);
}
bool LocalChannel::receive(std::string &message) {
    wipe(message); auto &s = *state_; std::lock_guard<std::mutex> lock(s.input); require(!s.listener && s.active);
    const auto deadline = Clock::now() + std::chrono::milliseconds(250);
    auto read = [&](char *data, size_t size, size_t &offset) {
        while (offset < size) {
            require(s.active); const auto count = recv(s.fd, data + offset, size - offset, 0);
            if (count < 0 && errno == EINTR) continue;
            if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) { if (!wait(s.fd, POLLIN, deadline, s.active)) return false; continue; }
            require(count > 0); offset += static_cast<size_t>(count);
        }
        return true;
    };
    try {
        if (!read(reinterpret_cast<char *>(s.header), 4, s.headerRead)) return false;
        if (s.partial.empty()) {
            size_t size = 0; for (auto byte : s.header) size = (size << 8) | byte;
            require(size > 0 && size <= maximum); s.partial.resize(size);
        }
        if (!read(&s.partial[0], s.partial.size(), s.bodyRead)) return false;
        message.swap(s.partial); s.headerRead = s.bodyRead = 0; return true;
    } catch (...) { s.abort(); throw; }
}
void LocalChannel::close() {
    auto &s = *state_; std::lock_guard<std::mutex> closing(s.closing); s.abort();
    std::scoped_lock<std::mutex, std::mutex> operations(s.input, s.output);
    if (s.fd >= 0) { ::close(s.fd); s.fd = -1; }
    if (s.listener) {
        struct stat current{};
        if (fstatat(s.root, s.name.c_str(), &current, AT_SYMLINK_NOFOLLOW) == 0) {
            require(current.st_dev == s.device && current.st_ino == s.inode && S_ISSOCK(current.st_mode));
            require(unlinkat(s.root, s.name.c_str(), 0) == 0);
        } else require(errno == ENOENT);
        s.listener = false;
    }
    if (s.guard >= 0) { ::close(s.guard); s.guard = -1; }
    if (s.root >= 0) { ::close(s.root); s.root = -1; }
    wipe(s.token); wipe(s.partial);
}
}
