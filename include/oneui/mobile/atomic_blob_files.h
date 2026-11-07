#pragma once
#include <string>
#include <vector>

namespace oneui::mobile {
// Ciphertext-only, fixed-name files in an application-owned private directory.
// These primitives do not encrypt. The platform secure-store adapter must
// authenticate before returning plaintext and keep its wrapping key in the OS.
class AtomicBlobFiles final {
public:
    static std::vector<unsigned char> read(const std::string &directory, const std::string &keyHash);
    static void write(const std::string &directory, const std::string &keyHash, const std::vector<unsigned char> &ciphertext);
};
}
