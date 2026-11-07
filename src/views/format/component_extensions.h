#pragma once
#include "component_contract.h"
#include <filesystem>

namespace oneview {
// Parse bounded, declarative metadata. No native code or process-global state.
ComponentContracts parseComponentExtensions(const std::string &text);
ComponentContracts loadComponentExtensions(const std::vector<std::filesystem::path> &files);
bool externalComponent(const std::string &name);
void validateComponentStatic(const ComponentContract &, const std::string &, const std::string &);
}
