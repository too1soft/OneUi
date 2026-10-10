#pragma once
#include "ast.h"
#include "ir.h"
namespace oneview {
void validateChildren(const Node &, const ComponentContract &, const Location &);
void validateStatic(const std::string &, const std::string &, const std::string &);
}
