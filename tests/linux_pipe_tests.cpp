#include "platform/linux/pipe_write.h"
#include <fcntl.h>
#include <iostream>

int main() {
    int fd[2];
    if (pipe2(fd, O_CLOEXEC | O_NONBLOCK) < 0)
        return 1;
    close(fd[0]);
    sigset_t previous, after;
    pthread_sigmask(SIG_SETMASK, nullptr, &previous);
    // The default SIGPIPE disposition would terminate the test without the guard.
    const auto result = oneui::linux_platform::writePipe(fd[1], "text", 4);
    const int error = errno;
    pthread_sigmask(SIG_SETMASK, nullptr, &after);
    close(fd[1]);
    if (result != -1 || error != EPIPE || sigismember(&previous, SIGPIPE) != sigismember(&after, SIGPIPE)) {
        std::cerr << "Interrupted clipboard transfer changed signal state\n";
        return 1;
    }
    return 0;
}
