#pragma once
#include "ir.h"

namespace oneview {
// A separate build artifact; the stable OneView binary protocol is unchanged.
std::string emitRequirements(const Image& image);
}
