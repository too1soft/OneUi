#include "compiler.h"
#include <cmath>

namespace oneview
{
Source &ViewCompiler::load(const fs::path &path, uint32_t importSpan)
{
    auto absolute = fs::weakly_canonical(path);
    auto key = absolute.generic_string();
    if (sources.count(key))
        return sources.at(key);
    Source s;
    s.importSpan = importSpan;
    s.path = absolute;
    s.relative = absolute.lexically_relative(base).generic_string();
    auto text = read(absolute);
    require(text.size() <= max_file && utf8(text), key + ":1:1: SOURCE_UTF8/LIMIT");
    auto nodes = Parser{text, key}.all();
    bool found = false;
    for (auto &n : nodes)
    {
        Location at{key, uint32_t(n.line), uint32_t(n.column)};
        if (n.tag == "template")
        {
            if (found || n.children.size() != 1 || !n.text.empty())
                at.fail("TEMPLATE_ROOT: exactly one root required");
            for (auto &a : n.attrs)
                if (a.first != "view-model")
                    at.fail("TEMPLATE_ATTRIBUTE: unknown attribute");
            if (n.attrs.count("view-model"))
                s.model = n.attrs.at("view-model").value;
            s.root = n.children[0];
            found = true;
        }
        else if (n.tag == "import")
        {
            if (n.attrs.size() != 2 || !n.attrs.count("name") || !n.attrs.count("src") || !n.children.empty() ||
                !n.text.empty())
                at.fail("TEMPLATE_IMPORT: name and src required");
            auto alias = n.attrs.at("name").value, target = n.attrs.at("src").value;
            if (!std::regex_match(alias, std::regex("[A-Z][A-Za-z0-9_]*")) || registry().count(alias) ||
                target.empty() || fs::path(target).is_absolute() || target.find(':') != std::string::npos ||
                !s.imports.emplace(alias, absolute.parent_path() / target).second)
                at.fail("TEMPLATE_IMPORT: invalid/duplicate import");
        }
        else if (n.tag == "style")
        {
            if (n.attrs.size() != 1 || !n.attrs.count("scoped"))
                at.fail("CSS_SCOPE: scoped required");
            s.styles.push_back(n);
        }
        else
            at.fail("TEMPLATE_SYNTAX: expected import/template/style");
    }
    require(found, key + ":1:1: TEMPLATE_ROOT: missing template");
    s.scope = uint32_t(sources.size() + 1);
    dependencies.push_back(absolute);
    auto &stored = sources.emplace(key, std::move(s)).first->second;
    Bytes scopeBytes(stored.relative.begin(), stored.relative.end());
    scopeBytes.push_back(0);
    auto templateName = stored.path.stem().generic_string();
    scopeBytes.insert(scopeBytes.end(), templateName.begin(), templateName.end());
    auto scope = "one-s-" + hex(sha256(scopeBytes)).substr(0, 16);
    require(scopeNames.insert(scope).second, "CSS_SCOPE: hash collision");
    for (auto &n : stored.styles)
    {
        auto lowered = oneui::ui::syntax::css(n.text, scope, stored.relative, n.line);
        if(lowered.find("@container")!=std::string::npos)requireWorkspace(image,workspaceQueries);
        image.add(
            CSS, {stored.scope, image.sid(n.text), image.sid(lowered), image.sid(stored.relative), span(stored, n), 0});
    }
    return stored;
}
uint32_t ViewCompiler::span(const Source &s, int line, int col, int endLine, int endCol)
{
    return image.add(SMAP, {image.sid(s.relative), uint32_t(line), uint32_t(col), uint32_t(endLine), uint32_t(endCol),
                            s.importSpan});
}
Reference ViewCompiler::resolve(const Source &s, const Attribute &a, const std::vector<Scope> &scopes, bool command)
{
    Location at{s.relative, uint32_t(a.line), uint32_t(a.column)};
    if (!identifier(a.value))
        at.fail("EXPRESSION: only member references are supported");
    uint32_t depth = 0;
    std::string member = a.value;
    // A repeat keeps its template's unqualified VM; an imported template starts
    // from the current call-site VM without adding an extra lexical depth.
    uint32_t selected = uint32_t(scopes.size() - 1);
    for (uint32_t i = 0; i < scopes.size(); ++i)
        if (scopes[i].variable.empty())
        {
            selected = i;
            break;
        }
    for (uint32_t i = 0; i < scopes.size(); ++i)
    {
        if (!scopes[i].variable.empty() && member.rfind(scopes[i].variable + ".", 0) == 0)
        {
            selected = i;
            member = member.substr(scopes[i].variable.size() + 1);
            break;
        }
    }
    depth = selected;
    const auto &m = image.schema.model(scopes[selected].model);
    if (command)
    {
        auto c = m.commands.find(member);
        if (c == m.commands.end())
            at.fail("VM_COMMAND: unknown command " + member);
        return {m.id, c->second.id, depth, 0, 0, 0};
    }
    auto p = m.properties.find(member);
    if (p == m.properties.end())
        at.fail("VM_PROPERTY: unknown property " + member);
    return {m.id, p->second.id, depth, p->second.tag, p->second.item, p->second.access};
}
uint32_t ViewCompiler::value(uint32_t tag, const std::string &text)
{
    uint64_t bits = 0;
    if (tag == 1)
        bits = text == "true";
    else if (tag == 2)
        bits = uint64_t(std::stoll(text));
    else if (tag == 3)
    {
        double v = std::stod(text);
        require(std::isfinite(v), "STATIC_TYPE: finite number");
        std::memcpy(&bits, &v, 8);
    }
    else if (tag == 4)
        bits = image.sid(text);
    else
        throw Error("STATIC_TYPE: scalar required");
    return image.add(VALU, {tag, 0, uint32_t(bits), uint32_t(bits >> 32), 0, 0});
}
uint32_t ViewCompiler::makeTemplate(Source &s, const Node &n, const std::vector<Scope> &scopes, const std::string &name)
{
    require(image.count(TMPL) < 100000, "LIMIT: templates");
    auto t = image.add(TMPL, {image.sid(name), none, scopes[0].model, s.scope, 0, 0, span(s, n), 0});
    auto root = lower(s, n, scopes, t, none, "0");
    image.records[TMPL][t][1] = root;
    // Nested bodies may have emitted outlets too; collect this owner's declarations contiguously.
    std::vector<uint32_t> owned;
    for (uint32_t i = 0; i < image.count(SLOT); ++i)
        if (image.records[SLOT][i][0] == t && image.records[SLOT][i][3] == 0)
            owned.push_back(i);
    if (!owned.empty())
    {
        // Outlets are emitted after child bodies; a template's outlets need not initially be adjacent.
        // Final canonicalization in compile() rebuilds the SLOT table and all aux indexes.
        image.records[TMPL][t][5] = uint32_t(owned.size());
    }
    return t;
}
Image ViewCompiler::compile(const fs::path &input, const fs::path &schema, const std::string &name)
{
    base = fs::weakly_canonical(input).parent_path();
    auto &root = load(input);
    require(!root.model.empty(), root.relative + ":1:1: SCHEMA_ROOT: explicit view-model required");
    image.schema = parseSchema(schema, root.model);
    dependencies.insert(dependencies.end(), image.schema.dependencies.begin(), image.schema.dependencies.end());
    active.insert(root.path.generic_string());
    image.root = makeTemplate(root, root.root, {{"", image.schema.root}}, name);
    active.clear();
    // Component indices are canonical by name, independent of traversal.
    auto previous = image.components;
    std::sort(image.components.begin(), image.components.end(), [](auto &a, auto &b) { return a.name < b.name; });
    for (auto &n : image.records[NODE])
        if (n[0] == 1)
        {
            auto name = previous[n[1]].name;
            n[1] = uint32_t(std::find_if(image.components.begin(), image.components.end(),
                                         [&](auto &c) { return c.name == name; }) -
                            image.components.begin());
        }
    auto old = image.records[SLOT];
    image.records[SLOT].clear();
    std::map<uint32_t, uint32_t> remap;
    for (auto kind : {TMPL, CALL})
        for (uint32_t owner = 0; owner < image.count(kind); ++owner)
        {
            auto start = image.count(SLOT);
            for (uint32_t i = 0; i < old.size(); ++i)
                if (old[i][0] == owner && old[i][3] == (kind == CALL ? 1u : 0u))
                {
                    remap[i] = image.count(SLOT);
                    image.add(SLOT, old[i]);
                }
            auto &r = image.records[kind][owner];
            r[kind == TMPL ? 4 : 2] = start;
            r[kind == TMPL ? 5 : 3] = image.count(SLOT) - start;
        }
    for (auto &n : image.records[NODE])
        if (n[0] == 4)
            n[9] = remap.at(n[9]);
    for (auto k : {BIND, EVNT, COND})
        std::stable_sort(image.records[k].begin(), image.records[k].end(),
                         [](auto &a, auto &b) { return a[0] < b[0]; });
    return image;
}
} // namespace oneview
