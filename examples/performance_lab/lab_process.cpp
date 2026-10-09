#include "lab_process.hpp"
#include "internal/unicode.h"
#include <chrono>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
extern char **environ;
#endif

namespace lab {
namespace {
std::filesystem::path outputDirectory;
}
void setResultsDirectory(std::filesystem::path path) { outputDirectory = std::move(path); }
const std::filesystem::path &resultsDirectory() { return outputDirectory; }
std::filesystem::path executablePath() {
#ifdef _WIN32
    std::vector<wchar_t> buffer(32768);
    const auto length = GetModuleFileNameW(nullptr, buffer.data(), DWORD(buffer.size()));
    if (!length || length >= buffer.size())
        throw std::runtime_error("Executable path unavailable");
    return std::filesystem::path(std::wstring(buffer.data(), length));
#elif defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> buffer(size);
    if (_NSGetExecutablePath(buffer.data(), &size))
        throw std::runtime_error("Executable path unavailable");
    return std::filesystem::canonical(buffer.data());
#else
    std::vector<char> buffer(32768);
    const auto length = readlink("/proc/self/exe", buffer.data(), buffer.size());
    if (length <= 0 || std::size_t(length) == buffer.size())
        throw std::runtime_error("Executable path unavailable");
    return std::filesystem::path(std::string(buffer.data(), length));
#endif
}
#ifdef _WIN32
namespace {
std::wstring quoteArgument(const std::wstring &value) {
    std::wstring result = L"\"";
    std::size_t slashes = 0;
    for (auto ch : value) {
        if (ch == L'\\') {
            ++slashes;
            continue;
        }
        result.append(ch == L'"' ? slashes * 2 + 1 : slashes, L'\\');
        result += ch;
        slashes = 0;
    }
    result.append(slashes * 2, L'\\');
    result += L'"';
    return result;
}
} // namespace
#endif
ProcessResult runLabProcess(const std::vector<std::wstring> &arguments,
                            const std::filesystem::path &log, const std::atomic<bool> &cancelled,
                            const std::filesystem::path &target) {
    ProcessResult result;
    if (cancelled.load()) {
        result.cancelled = true;
        return result;
    }
    const auto executable = target.empty() ? executablePath() : target;
    if (!log.parent_path().empty())
        std::filesystem::create_directories(log.parent_path());
#ifdef _WIN32
    SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
    auto output = CreateFileW(log.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &security, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (output == INVALID_HANDLE_VALUE) {
        result.error = "Cannot open benchmark log";
        return result;
    }
    auto input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &security,
                             OPEN_EXISTING, 0, nullptr);
    SIZE_T size = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
    std::vector<unsigned char> storage(size);
    auto attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
    bool initialized = InitializeProcThreadAttributeList(attributes, 1, 0, &size) != FALSE;
    HANDLE handles[]{output, input};
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = input;
    startup.StartupInfo.hStdOutput = output;
    startup.StartupInfo.hStdError = output;
    startup.lpAttributeList = attributes;
    std::wstring command = quoteArgument(executable.wstring());
    for (auto &argument : arguments)
        command += L" " + quoteArgument(argument);
    PROCESS_INFORMATION process{};
    const bool created = input != INVALID_HANDLE_VALUE && initialized &&
                         UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                                   handles, sizeof(handles), nullptr, nullptr) &&
                         CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, TRUE,
                                        EXTENDED_STARTUPINFO_PRESENT | CREATE_NO_WINDOW, nullptr,
                                        nullptr, &startup.StartupInfo, &process);
    const auto error = GetLastError();
    if (initialized)
        DeleteProcThreadAttributeList(attributes);
    if (input != INVALID_HANDLE_VALUE)
        CloseHandle(input);
    CloseHandle(output);
    if (!created) {
        result.error = "CreateProcess failed: " + std::to_string(error);
        return result;
    }
    result.started = true;
    CloseHandle(process.hThread);
    for (;;) {
        auto wait = WaitForSingleObject(process.hProcess, 50);
        if (wait == WAIT_OBJECT_0)
            break;
        if (wait == WAIT_FAILED || cancelled.load()) {
            result.cancelled = cancelled.load();
            TerminateProcess(process.hProcess, 130);
            WaitForSingleObject(process.hProcess, INFINITE);
            break;
        }
    }
    DWORD code = 0;
    GetExitCodeProcess(process.hProcess, &code);
    result.exitCode = int(code);
    CloseHandle(process.hProcess);
#else
    std::vector<std::string> values{executable.string()};
    for (auto &argument : arguments)
        values.push_back(oneui::unicode::toUtf8(argument));
    std::vector<char *> argv;
    for (auto &value : values)
        argv.push_back(value.data());
    argv.push_back(nullptr);
    posix_spawn_file_actions_t actions;
    int error = posix_spawn_file_actions_init(&actions);
    if (error) {
        result.error = std::strerror(error);
        return result;
    }
    error = posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    if (!error)
        error = posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, log.c_str(),
                                                 O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (!error)
        error = posix_spawn_file_actions_adddup2(&actions, STDOUT_FILENO, STDERR_FILENO);
    pid_t pid = 0;
    if (!error)
        error = posix_spawn(&pid, executable.c_str(), &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    if (error) {
        result.error = std::strerror(error);
        return result;
    }
    result.started = true;
    int status = 0;
    for (;;) {
        const auto waited = waitpid(pid, &status, WNOHANG);
        if (waited == pid)
            break;
        if (waited < 0 && errno != EINTR) {
            result.error = std::strerror(errno);
            return result;
        }
        if (cancelled.load()) {
            result.cancelled = true;
            kill(pid, SIGTERM);
            bool reaped = false;
            for (int i = 0; i < 20; ++i) {
                if (waitpid(pid, &status, WNOHANG) == pid) {
                    reaped = true;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(25));
            }
            if (!reaped) {
                kill(pid, SIGKILL);
                while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
                }
            }
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    result.exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
#endif
    return result;
}
} // namespace lab
