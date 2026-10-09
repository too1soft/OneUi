#include "platform/shared/skia_gl_setup.h"

#include "include/core/SkData.h"
#include "include/core/SkString.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <system_error>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace oneui::rendering {
namespace {

namespace fs = std::filesystem;

// Entries larger than this are not worth a disk round trip.
constexpr std::size_t kMaxEntryBytes = 4u << 20;
constexpr std::uint32_t kMagic = 0x4F555343; // "OUSC"

std::uint64_t fnv1a(const void* data, std::size_t size) {
    auto bytes = static_cast<const unsigned char*>(data);
    std::uint64_t hash = 1469598103934665603ull;
    for (std::size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 1099511628211ull;
    }
    return hash;
}

fs::path executableStem() {
#ifdef _WIN32
    wchar_t buffer[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length > 0 && length < MAX_PATH) {
        return fs::path(buffer).stem();
    }
#else
    std::error_code error;
    const auto self = fs::read_symlink("/proc/self/exe", error);
    if (!error) {
        return self.stem();
    }
#endif
    return "app";
}

fs::path cacheDirectory() {
    std::error_code error;
#ifdef _WIN32
    if (const wchar_t* custom = _wgetenv(L"ONEUI_SHADER_CACHE_DIR"); custom && *custom) {
        return fs::path(custom);
    }
    if (const wchar_t* local = _wgetenv(L"LOCALAPPDATA"); local && *local) {
        return fs::path(local) / "OneUI" / "ShaderCache" / executableStem();
    }
#else
    if (const char* custom = std::getenv("ONEUI_SHADER_CACHE_DIR"); custom && *custom) {
        return fs::path(custom);
    }
    if (const char* xdg = std::getenv("XDG_CACHE_HOME"); xdg && *xdg) {
        return fs::path(xdg) / "oneui" / "shaders" / executableStem();
    }
    if (const char* home = std::getenv("HOME"); home && *home) {
        return fs::path(home) / ".cache" / "oneui" / "shaders" / executableStem();
    }
#endif
    return {};
}

class DiskShaderCache final : public GrContextOptions::PersistentCache {
public:
    explicit DiskShaderCache(fs::path directory) : directory_(std::move(directory)) {}

    sk_sp<SkData> load(const SkData& key) override {
        std::lock_guard lock(mutex_);
        std::ifstream file(entryPath(key), std::ios::binary);
        if (!file) {
            return nullptr;
        }
        std::uint32_t magic = 0;
        std::uint64_t keySize = 0;
        std::uint64_t dataSize = 0;
        file.read(reinterpret_cast<char*>(&magic), sizeof magic);
        file.read(reinterpret_cast<char*>(&keySize), sizeof keySize);
        file.read(reinterpret_cast<char*>(&dataSize), sizeof dataSize);
        if (!file || magic != kMagic || keySize != key.size() || dataSize > kMaxEntryBytes) {
            return nullptr;
        }
        // The stored key is compared byte for byte: a hash collision must
        // never hand Skia a program built for a different shader.
        std::vector<char> storedKey(static_cast<std::size_t>(keySize));
        file.read(storedKey.data(), static_cast<std::streamsize>(storedKey.size()));
        if (!file || std::memcmp(storedKey.data(), key.data(), storedKey.size()) != 0) {
            return nullptr;
        }
        auto data = SkData::MakeUninitialized(static_cast<std::size_t>(dataSize));
        file.read(static_cast<char*>(data->writable_data()), static_cast<std::streamsize>(dataSize));
        return file ? data : nullptr;
    }

    void store(const SkData& key, const SkData& data, const SkString&) override {
        if (data.size() > kMaxEntryBytes) {
            return;
        }
        std::lock_guard lock(mutex_);
        std::error_code error;
        fs::create_directories(directory_, error);
        if (error) {
            return;
        }
        const auto target = entryPath(key);
        // Write a private temporary file and rename it into place so a crash
        // or a concurrent writer never leaves a partially written entry.
        auto temporary = target;
        temporary += ".tmp" + std::to_string(++sequence_);
        {
            std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
            if (!file) {
                return;
            }
            const std::uint32_t magic = kMagic;
            const std::uint64_t keySize = key.size();
            const std::uint64_t dataSize = data.size();
            file.write(reinterpret_cast<const char*>(&magic), sizeof magic);
            file.write(reinterpret_cast<const char*>(&keySize), sizeof keySize);
            file.write(reinterpret_cast<const char*>(&dataSize), sizeof dataSize);
            file.write(static_cast<const char*>(key.data()), static_cast<std::streamsize>(key.size()));
            file.write(static_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
            if (!file) {
                file.close();
                fs::remove(temporary, error);
                return;
            }
        }
        fs::rename(temporary, target, error);
        if (error) {
            fs::remove(temporary, error);
        }
    }

private:
    fs::path entryPath(const SkData& key) const {
        char name[32];
        std::snprintf(name, sizeof name, "%016llx.bin",
            static_cast<unsigned long long>(fnv1a(key.data(), key.size())));
        return directory_ / name;
    }

    fs::path directory_;
    std::mutex mutex_;
    std::uint64_t sequence_ = 0;
};

} // namespace

GrContextOptions::PersistentCache* shaderCache() {
    static DiskShaderCache* cache = [] () -> DiskShaderCache* {
        auto directory = cacheDirectory();
        if (directory.empty()) {
            return nullptr;
        }
        // Intentionally leaked: Ganesh contexts may outlive static destruction
        // order at process exit, and the cache owns no external resources.
        return new DiskShaderCache(std::move(directory));
    }();
    return cache;
}

} // namespace oneui::rendering
