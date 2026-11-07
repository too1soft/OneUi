#include "oneui/mobile/private_directory.h"
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

namespace oneui::mobile {
namespace {
struct Fd { int value; ~Fd() { if (value >= 0) close(value); } };
void require(bool condition) { if (!condition) throw std::runtime_error("Private directory rejected"); }
void owned(int fd, bool privateMode) {
    struct stat value{};
    const auto uid = getuid();
    require(fd >= 0 && fstat(fd, &value) == 0 && S_ISDIR(value.st_mode) && value.st_uid == uid);
    mode_t rejected = privateMode ? 077 : (S_IWGRP | S_IWOTH);
#if defined(__OHOS__)
    // HarmonyOS HAP filesDir is group-writable by the app's own per-app group.
    // The private child still requires 0700-style permissions.
    if (!privateMode && value.st_gid == uid) rejected = S_IWOTH;
#endif
    require((value.st_mode & rejected) == 0);
}
}
int openPrivateDirectory(const std::string &path) {
    require(path.size() > 2 && path.size() <= 4096 && path.front() == '/' && path.find('\0') == std::string::npos);
    const auto slash = path.find_last_of('/'); const auto name = path.substr(slash + 1);
    require(!name.empty() && name != "." && name != "..");
    char *resolved = realpath(path.substr(0, slash).c_str(), nullptr);
    require(resolved != nullptr); const std::string parent(resolved); free(resolved);
    Fd base{open(parent.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW)}; owned(base.value, false);
    require(mkdirat(base.value, name.c_str(), 0700) == 0 || errno == EEXIST);
    Fd folder{openat(base.value, name.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW)}; owned(folder.value, true);
    const int result = fcntl(folder.value, F_DUPFD_CLOEXEC, 0); require(result >= 0); return result;
}
}
