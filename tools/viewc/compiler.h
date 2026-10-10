#pragma once
#include "ast.h"
#include "ir.h"
#include <set>

namespace oneview
{
struct Scope
{
    std::string variable;
    uint32_t model;
    bool workspace = false;
};
struct Source
{
    fs::path path;
    std::string relative, model;
    Node root;
    std::map<std::string, fs::path> imports;
    std::vector<Node> styles;
    uint32_t scope = 0, importSpan = none;
};
struct Reference
{
    uint32_t model, id, depth, tag, item, access;
};
class ViewCompiler
{
  public:
    Image image;
    std::vector<fs::path> dependencies;
    Image compile(const fs::path &input, const fs::path &schema, const std::string &name);

  private:
    fs::path base;
    std::vector<std::set<std::string>> rowHostNames{1};
    std::map<std::string, Source> sources;
    std::set<std::string> active, hostNames, scopeNames;
    std::map<uint32_t, std::set<std::string>> stableIds, slots, references;
    Source &load(const fs::path &path, uint32_t importSpan = none);
    uint32_t makeTemplate(Source &source, const Node &node, const std::vector<Scope> &scopes, const std::string &name);
    uint32_t lower(Source &source, Node node, const std::vector<Scope> &scopes, uint32_t tmpl, uint32_t parent,
                   const std::string &path);
    uint32_t span(const Source &source, int line, int col, int endLine, int endCol);
    uint32_t span(const Source &s, const Node &n)
    {
        return span(s, n.line, n.column, n.endLine, n.endColumn);
    }
    uint32_t span(const Source &s, const Attribute &a)
    {
        return span(s, a.line, a.column, a.endLine, a.endColumn);
    }
    Reference resolve(const Source &source, const Attribute &attr, const std::vector<Scope> &scopes,
                      bool command = false);
    uint32_t value(uint32_t tag, const std::string &value);
    void attributes(Source &source, const Node &node, const std::vector<Scope> &scopes, uint32_t index,
                    const ComponentContract &component);
};
void validateChildren(const Node &node, const ComponentContract &component, const Location &at);
void validateStatic(const std::string &component, const std::string &property, const std::string &value);
} // namespace oneview
