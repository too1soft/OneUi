#pragma once
#include <string>
#include <vector>

namespace lab {
struct CodeExample {
    std::wstring language;
    std::wstring code;
};
// Generated from capabilities.json and the source files built by the lab.
std::vector<CodeExample> compiledExamples(const std::string &page);
} // namespace lab
