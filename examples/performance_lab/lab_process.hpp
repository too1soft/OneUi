#pragma once
#include <atomic>
#include <filesystem>
#include <string>
#include <vector>

namespace lab {
struct ProcessResult {
    bool started = false, cancelled = false;
    int exitCode = -1;
    std::string error;
};
std::filesystem::path executablePath();
void setResultsDirectory(std::filesystem::path);
const std::filesystem::path &resultsDirectory();
// Launches an executable directly, without a command shell. An empty executable
// selects this process's executable. Only the child
// started by this call is terminated when the owning page cancels the job.
ProcessResult runLabProcess(const std::vector<std::wstring> &, const std::filesystem::path &log,
                            const std::atomic<bool> &cancelled,
                            const std::filesystem::path &executable = {});
} // namespace lab
