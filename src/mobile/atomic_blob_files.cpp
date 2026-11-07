#include "oneui/mobile/atomic_blob_files.h"
#include "oneui/mobile/private_directory.h"
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <stdexcept>
#include <sys/file.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

namespace oneui::mobile {
namespace {
constexpr size_t maximum = 1024 * 1024 + 64;
struct Fd {
    int value;
    explicit Fd(int value) : value(value) { if (value < 0) throw std::runtime_error("Private blob file operation failed"); }
    ~Fd() { close(value); }
    Fd(const Fd &) = delete;
};
void require(bool condition) { if (!condition) throw std::runtime_error("Private blob file operation rejected"); }
void owned(int fd, bool directory) {
    struct stat status{}; require(fstat(fd, &status) == 0 && status.st_uid == getuid());
    require(directory ? S_ISDIR(status.st_mode) : S_ISREG(status.st_mode) && status.st_nlink == 1);
    require((status.st_mode & (S_IWGRP | S_IWOTH)) == 0);
}
Fd root(const std::string &path) {
    return Fd(openPrivateDirectory(path));
}
void validate(const std::string &hash) {
    require(hash.size() == 64);
    for (char c : hash) require((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
}
Fd lock(int directory, const std::string &hash, int mode) {
    Fd file(openat(directory, (hash + ".lock").c_str(), O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600)); owned(file.value, false);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (flock(file.value, mode | LOCK_NB) != 0) {
        require((errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) && std::chrono::steady_clock::now() < deadline);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return Fd(fcntl(file.value, F_DUPFD_CLOEXEC, 0));
}
}
std::vector<unsigned char> AtomicBlobFiles::read(const std::string &directory, const std::string &hash) {
    validate(hash); auto folder = root(directory); auto guard = lock(folder.value, hash, LOCK_SH);
    int value = openat(folder.value, (hash + ".blob").c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (value < 0 && errno == ENOENT) return {};
    Fd file(value); owned(file.value, false); struct stat status{};
    require(fstat(file.value, &status) == 0 && status.st_size > 0 && static_cast<uint64_t>(status.st_size) <= maximum);
    std::vector<unsigned char> result(static_cast<size_t>(status.st_size)); size_t offset = 0;
    while (offset < result.size()) {
        const auto count = ::read(file.value, result.data() + offset, result.size() - offset);
        if (count < 0 && errno == EINTR) continue;
        require(count > 0); offset += static_cast<size_t>(count);
    }
    return result;
}
void AtomicBlobFiles::write(const std::string &directory, const std::string &hash, const std::vector<unsigned char> &ciphertext) {
    validate(hash); require(!ciphertext.empty() && ciphertext.size() <= maximum);
    auto folder = root(directory); auto guard = lock(folder.value, hash, LOCK_EX);
    const auto temporary = hash + ".pending", target = hash + ".blob";
    // A previous interrupted write is ciphertext only. Verify its type before
    // reclaiming this exact fixed pending filename under the per-key lock.
    struct stat old{};
    if (fstatat(folder.value, temporary.c_str(), &old, AT_SYMLINK_NOFOLLOW) == 0) {
        require(S_ISREG(old.st_mode) && old.st_uid == getuid() && old.st_nlink == 1);
        require(unlinkat(folder.value, temporary.c_str(), 0) == 0);
    } else require(errno == ENOENT);
    try {
        Fd file(openat(folder.value, temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600));
        size_t offset = 0;
        while (offset < ciphertext.size()) {
            const auto count = ::write(file.value, ciphertext.data() + offset, ciphertext.size() - offset);
            if (count < 0 && errno == EINTR) continue;
            require(count > 0); offset += static_cast<size_t>(count);
        }
        require(fsync(file.value) == 0 && renameat(folder.value, temporary.c_str(), folder.value, target.c_str()) == 0);
        require(fsync(folder.value) == 0);
    } catch (...) { unlinkat(folder.value, temporary.c_str(), 0); throw; }
}
}
