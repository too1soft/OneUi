#include "data_transfer_pipe.h"
#include "pipe_write.h"
#include <algorithm>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <utility>

namespace oneui::linux_platform {
namespace {
bool nonblocking(int fd) {
    const int flags = fcntl(fd, F_GETFL);
    return flags >= 0 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) >= 0;
}
} // namespace
DataPipeReader::~DataPipeReader() { reset(); }
DataPipeReader::DataPipeReader(DataPipeReader &&other) noexcept { *this = std::move(other); }
DataPipeReader &DataPipeReader::operator=(DataPipeReader &&other) noexcept {
    if (this != &other) {
        reset();
        fd_ = std::exchange(other.fd_, -1);
        deadline_ = other.deadline_;
        limit_ = other.limit_;
        bytes_ = std::move(other.bytes_);
    }
    return *this;
}
bool DataPipeReader::start(int fd, double deadline, std::size_t limit) {
    reset();
    if (fd < 0)
        return false;
    if (!nonblocking(fd)) {
        ::close(fd);
        return false;
    }
    fd_ = fd;
    deadline_ = deadline;
    limit_ = limit;
    return true;
}
void DataPipeReader::reset() {
    if (fd_ >= 0)
        ::close(fd_);
    fd_ = -1;
    bytes_.clear();
}
PipeTransferState DataPipeReader::pump(double now) {
    if (fd_ < 0 || now >= deadline_)
        return PipeTransferState::Failed;
    char buffer[8192];
    // Bound each UI iteration even when the peer produces data continuously.
    for (int i = 0; i < 64; ++i) {
        auto count = ::read(fd_, buffer, sizeof(buffer));
        if (count == 0)
            return PipeTransferState::Complete;
        if (count < 0)
            return errno == EAGAIN || errno == EINTR ? PipeTransferState::Pending
                                                     : PipeTransferState::Failed;
        if (std::size_t(count) > limit_ - bytes_.size())
            return PipeTransferState::Failed;
        bytes_.append(buffer, std::size_t(count));
    }
    return PipeTransferState::Pending;
}
DataPipeWriters::~DataPipeWriters() { clear(); }
void DataPipeWriters::clear() {
    for (auto &value : pending_)
        ::close(value.fd);
    pending_.clear();
}
bool DataPipeWriters::send(int fd, const std::string &bytes, double deadline) {
    if (fd < 0)
        return false;
    if (pending_.size() >= 16 || bytes.size() > 16u * 1024u * 1024u || !nonblocking(fd)) {
        ::close(fd);
        return false;
    }
    try {
        pending_.push_back({fd, bytes, 0, deadline});
    } catch (...) {
        ::close(fd);
        throw;
    }
    return true;
}
void DataPipeWriters::pump(double now) {
    for (auto it = pending_.begin(); it != pending_.end();) {
        bool finished = it->offset == it->bytes.size() || now >= it->deadline;
        if (!finished) {
            const auto count =
                writePipe(it->fd, it->bytes.data() + it->offset,
                          std::min<std::size_t>(65536, it->bytes.size() - it->offset));
            if (count > 0)
                it->offset += std::size_t(count);
            else if (count < 0 && errno != EAGAIN && errno != EINTR)
                finished = true;
            finished |= it->offset == it->bytes.size();
        }
        if (finished) {
            ::close(it->fd);
            it = pending_.erase(it);
        } else
            ++it;
    }
}
void DataPipeWriters::pollDescriptors(std::vector<pollfd> &result) const {
    for (auto &send : pending_)
        result.push_back({send.fd, POLLOUT, 0});
}
int DataPipeWriters::constrainTimeout(int timeout, double now) const {
    for (auto &send : pending_)
        timeout = std::min(timeout, std::max(0, int(send.deadline - now)));
    return timeout;
}
} // namespace oneui::linux_platform
