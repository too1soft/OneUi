#pragma once
#include "ir.h"
namespace oneview {
std::string emitCpp(const Image &, const Bytes &, const std::string &name, const std::string &header,
                    const std::filesystem::path &sourceDirectory);
}
