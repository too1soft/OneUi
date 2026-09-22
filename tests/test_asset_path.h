#pragma once

#include <cstdlib>
#include <string>

// Test-only override: native acceptance bundles must not depend on the build
// machine's source checkout. Production font discovery is unaffected.
inline std::string oneuiTestAsset(const char* name) {
    const char* configured = std::getenv("ONEUI_TEST_ASSET_ROOT");
    const std::string root = configured && *configured ? configured : ONEUI_TEXT_ASSETS;
    return root + '/' + name;
}
