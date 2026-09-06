#pragma once
#include <cerrno>
#include <cstddef>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>

namespace oneui::linux_platform {
// A receiver may close its pipe after poll(). Do not change the application's
// process-wide SIGPIPE disposition, or consume a signal pending before our write.
inline ssize_t writePipe(int fd, const void *data, std::size_t size) {
    sigset_t blocked, previous, pending;
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGPIPE);
    const int error = pthread_sigmask(SIG_BLOCK, &blocked, &previous);
    if (error) {
        errno = error;
        return -1;
    }
    sigpending(&pending);
    const bool alreadyPending = sigismember(&pending, SIGPIPE) == 1;
    const auto result = ::write(fd, data, size);
    const int savedError = errno;
    if (result < 0 && savedError == EPIPE && !alreadyPending) {
        timespec immediate{};
        while (sigtimedwait(&blocked, nullptr, &immediate) < 0 && errno == EINTR) {
        }
    }
    pthread_sigmask(SIG_SETMASK, &previous, nullptr);
    errno = savedError;
    return result;
}
} // namespace oneui::linux_platform
