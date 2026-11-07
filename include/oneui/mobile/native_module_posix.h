#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace oneui::mobile {
// Platform-independent implementation behind JNI and HarmonyOS N-API. Go
// libraries stay resident; releasing a handle prevents new calls and does not
// unload code being used by an already executing native call.
class NativeModulePosix final {
public:
    static uint64_t open(const std::string &library, const std::string &symbol);
    // Typed native SDK consumers (for example a Widget factory) may resolve a
    // different public entry point from the same resident, packaged loader.
    static void *resolve(const std::string &library, const std::string &symbol);
    static int call(uint64_t handle, int command, std::vector<unsigned char> request, std::vector<unsigned char> &reply);
    static void release(uint64_t handle);
};
}
