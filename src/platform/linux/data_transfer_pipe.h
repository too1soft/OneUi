#pragma once
#include <cstddef>
#include <poll.h>
#include <string>
#include <vector>

namespace oneui::linux_platform {
enum class PipeTransferState { Pending, Complete, Failed };
// Owns the reader. The foreign writer remains blocking; only our end changes.
class DataPipeReader final {
  public:
    DataPipeReader() = default;
    ~DataPipeReader();
    DataPipeReader(DataPipeReader &&) noexcept;
    DataPipeReader &operator=(DataPipeReader &&) noexcept;
    DataPipeReader(const DataPipeReader &) = delete;
    DataPipeReader &operator=(const DataPipeReader &) = delete;
    bool start(int fd, double deadline, std::size_t limit);
    PipeTransferState pump(double now);
    void reset();
    int fd() const { return fd_; }
    double deadline() const { return deadline_; }
    const std::string &bytes() const { return bytes_; }

  private:
    int fd_ = -1;
    double deadline_ = 0;
    std::size_t limit_ = 0;
    std::string bytes_;
};
class DataPipeWriters final {
  public:
    ~DataPipeWriters();
    DataPipeWriters() = default;
    DataPipeWriters(const DataPipeWriters &) = delete;
    DataPipeWriters &operator=(const DataPipeWriters &) = delete;
    // Takes fd ownership on success and failure. Limits simultaneous transfers.
    bool send(int fd, const std::string &, double deadline);
    void pump(double now);
    void clear();
    void pollDescriptors(std::vector<pollfd> &) const;
    int constrainTimeout(int timeout, double now) const;

  private:
    struct Send {
        int fd;
        std::string bytes;
        std::size_t offset;
        double deadline;
    };
    std::vector<Send> pending_;
};
} // namespace oneui::linux_platform
