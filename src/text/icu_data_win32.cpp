#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <limits>
#include <mutex>
#include <vector>
#include <zlib.h>
#include "unicode/udata.h"

extern const unsigned char oneuiEmbeddedIcuDataCompressed[];
extern const std::size_t oneuiEmbeddedIcuDataCompressedSize;
extern const std::size_t oneuiEmbeddedIcuDataOriginalSize;
extern const unsigned char oneuiEmbeddedIcuDataSha256[32];

namespace {

bool sha256(const unsigned char* data, std::size_t size, std::array<unsigned char, 32>& digest) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD objectSize = 0;
    DWORD copied = 0;
    std::vector<unsigned char> object;
    bool ready = false;

    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) goto cleanup;
    if (BCryptGetProperty(
            algorithm,
            BCRYPT_OBJECT_LENGTH,
            reinterpret_cast<PUCHAR>(&objectSize),
            sizeof(objectSize),
            &copied,
            0) < 0 || objectSize == 0) goto cleanup;
    object.resize(objectSize);
    if (BCryptCreateHash(algorithm, &hash, object.data(), objectSize, nullptr, 0, 0) < 0) goto cleanup;

    for (std::size_t offset = 0; offset < size;) {
        const auto chunk = static_cast<ULONG>(std::min<std::size_t>(
            size - offset,
            std::numeric_limits<ULONG>::max()));
        if (BCryptHashData(hash, const_cast<PUCHAR>(data + offset), chunk, 0) < 0) goto cleanup;
        offset += chunk;
    }
    if (BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) < 0) goto cleanup;
    ready = true;

cleanup:
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    return ready;
}

} // namespace

// Resolve SkUnicode's loader before the optional file-based Skia archive member
// is selected. Works in both a DLL and an independently linked static consumer.
bool SkLoadICU() {
    static std::once_flag once;
    static bool ready = false;
    static std::vector<std::max_align_t> storage;
    std::call_once(once, [] {
        const auto words = (oneuiEmbeddedIcuDataOriginalSize + sizeof(std::max_align_t) - 1)
            / sizeof(std::max_align_t);
        storage.resize(words);
        auto* data = reinterpret_cast<unsigned char*>(storage.data());
        uLongf decodedSize = static_cast<uLongf>(oneuiEmbeddedIcuDataOriginalSize);
        if (uncompress(
                data,
                &decodedSize,
                oneuiEmbeddedIcuDataCompressed,
                static_cast<uLong>(oneuiEmbeddedIcuDataCompressedSize)) != Z_OK
            || decodedSize != oneuiEmbeddedIcuDataOriginalSize) {
            storage.clear();
            return;
        }
        std::array<unsigned char, 32> digest{};
        if (!sha256(data, oneuiEmbeddedIcuDataOriginalSize, digest)
            || std::memcmp(digest.data(), oneuiEmbeddedIcuDataSha256, digest.size()) != 0) {
            storage.clear();
            return;
        }
        UErrorCode error = U_ZERO_ERROR;
        udata_setCommonData(data, &error);
        if (U_SUCCESS(error)) {
            udata_setFileAccess(UDATA_NO_FILES, &error);
        }
        ready = U_SUCCESS(error);
        if (!ready) storage.clear();
    });
    return ready;
}
