#include "platform/linux/data_transfer_pipe.h"
#include <fcntl.h>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
using namespace oneui::linux_platform;
namespace {
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
void transferAndBounds() {
    int fds[2];
    require(pipe2(fds, O_CLOEXEC) == 0, "pipe failed");
    DataPipeReader reader;
    DataPipeWriters writers;
    require(reader.start(fds[0], 100, 1024 * 1024), "reader failed");
    const std::string text(700000, 'x');
    require(writers.send(fds[1], text, 100), "writer failed");
    auto state = PipeTransferState::Pending;
    for (int i = 0; i < 200 && state == PipeTransferState::Pending; ++i) {
        writers.pump(1);
        state = reader.pump(1);
    }
    require(state == PipeTransferState::Complete && reader.bytes() == text,
            "Partial transfer lost bytes");
    require(pipe2(fds, O_CLOEXEC) == 0, "pipe failed");
    reader.start(fds[0], 100, 2);
    writers.send(fds[1], "oversize", 100);
    writers.pump(1);
    require(reader.pump(1) == PipeTransferState::Failed, "Receive limit ignored");
    require(reader.pump(101) == PipeTransferState::Failed, "Deadline ignored");
}
void ownershipAndClosedPeer() {
    int fds[2];
    require(pipe2(fds, O_CLOEXEC) == 0, "pipe failed");
    DataPipeWriters writers;
    ::close(fds[0]);
    writers.send(fds[1], "closed", 100);
    writers.pump(1); // Must not terminate the process with SIGPIPE.
    require(fcntl(fds[1], F_GETFD) == -1, "Closed peer leaked a writer");
    require(pipe2(fds, O_CLOEXEC) == 0, "pipe failed");
    {
        DataPipeReader original;
        original.start(fds[0], 10, 2);
        DataPipeReader moved(std::move(original));
        require(original.fd() == -1, "Move retained duplicate descriptor ownership");
    }
    require(fcntl(fds[0], F_GETFD) == -1, "Reader leaked descriptor");
    ::close(fds[1]);
}
} // namespace
int main() {
    try {
        transferAndBounds();
        ownershipAndClosedPeer();
        std::cout << "Data pipe bounds and ownership passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
