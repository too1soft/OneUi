#include "compiler.h"

namespace oneview
{
uint32_t ViewCompiler::lower(Source &s, Node n, const std::vector<Scope> &scopes, uint32_t tmpl, uint32_t parent,
                             const std::string &path)
{
    Location at{s.relative, uint32_t(n.line), uint32_t(n.column)};
    auto sourceSpan = span(s, n);
    std::string stable = s.relative + ":" + path;
    if (n.attrs.count("view-id"))
        stable = n.attrs.at("view-id").value;
    else if (n.attrs.count("ref"))
        stable = n.attrs.at("ref").value;
    if (stable.empty() || !stableIds[tmpl].insert(stable).second)
        at.fail("NODE_ID: empty/duplicate stable ID");
    auto ref = n.attrs.count("ref") ? n.attrs.at("ref").value : "";
    if (!ref.empty() && (!identifier(ref) || ref.find('.') != std::string::npos || scopes.size() > 1 ||
                         n.attrs.count("v-for") || !references[tmpl].insert(ref).second))
        at.fail("REF: static identifier outside repeat required");
    auto index =
        image.add(NODE, {1, none, parent, 0, 0, 0, 0, image.sid(ref), s.scope, none, sourceSpan, image.sid(stable)});
    const bool explicitId = n.attrs.count("view-id") != 0;
    n.attrs.erase("ref");
    n.attrs.erase("view-id");
    if (n.attrs.count("v-for"))
    {
        if (n.attrs.count("v-if") || n.attrs.count("v-else"))
            at.fail("REPEAT: put conditions inside v-for");
        auto attr = n.attrs.at("v-for");
        std::smatch match;
        if (!std::regex_match(attr.value, match, std::regex("([A-Za-z_][A-Za-z0-9_]*) in ([A-Za-z_][A-Za-z0-9_.]*)")) ||
            !n.attrs.count(":key"))
            at.fail("REPEAT: item in member and :key required");
        auto variable = match[1].str();
        attr.value = match[2].str();
        auto list = resolve(s, attr, scopes);
        if (list.tag != 5)
            at.fail("REPEAT_TYPE: list required");
        auto &item = image.schema.model(list.item);
        auto keyAttr = n.attrs.at(":key");
        if (keyAttr.value != variable + "." + item.property(item.key).name)
            Location{s.relative, uint32_t(keyAttr.line), uint32_t(keyAttr.column)}.fail(
                "REPEAT_KEY: use the schema key property");
        if (scopes.size() >= 64)
            at.fail("LIMIT: lexical depth");
        for (auto &scope : scopes)
            if (scope.variable == variable)
                at.fail("REPEAT_SCOPE: shadowed item variable");
        bool transparent = false;
        if (parent != none && image.records[NODE][parent][0] == 1) {
            const auto &rule = image.components[image.records[NODE][parent][1]].childRule;
            transparent = rule == "pane-collection" || rule == "dock-collection";
        }
        if (transparent && !explicitId) at.fail("WORKSPACE_SCOPE: explicit view-id required");
        if (transparent && stable.find('/') != std::string::npos)
            at.fail("WORKSPACE_SCOPE: view-id must not contain '/'");
        auto childScopes = scopes;
        childScopes.insert(childScopes.begin(), {variable, item.id, transparent});
        n.attrs.erase("v-for");
        n.attrs.erase(":key");
        rowHostNames.emplace_back();
        auto body = makeTemplate(s, n, childScopes, s.relative + ":" + path + ":repeat");
        rowHostNames.pop_back();
        auto aux = image.add(REPT, {index, list.id, list.model, list.depth | (transparent ? repeatTransparent | repeatExplicit : 0u), item.id, body, item.key, span(s, keyAttr)});
        if (transparent) requireWorkspace(image, workspaceCollections);
        image.records[NODE][index][0] = 2;
        image.records[NODE][index][9] = aux;
        return index;
    }
    if (n.attrs.count(":key"))
        at.fail("REPEAT_KEY: key without v-for");
    if (n.attrs.count("v-if"))
    {
        auto a = n.attrs.at("v-if");
        auto inv = n.attrs.count("v-else") ? 1u : 0u;
        if (a.value == "true" || a.value == "false")
            image.add(COND, {index, value(1, a.value), scopes[0].model, (2u | inv) << 16, span(s, a), 0});
        else
        {
            auto p = resolve(s, a, scopes);
            if (p.tag != 1)
                at.fail("CONDITION_TYPE: bool required");
            image.add(COND, {index, p.id, p.model, p.depth | (inv << 16), span(s, a), 0});
        }
        n.attrs.erase("v-if");
        n.attrs.erase("v-else");
    }
    else if (n.attrs.count("v-else"))
        at.fail("CONDITION: v-else must immediately follow v-if");
    if (s.imports.count(n.tag))
    {
        if (!n.text.empty())
            at.fail("CALL_TEXT: use a supplied slot for text content");
        auto child = load(s.imports.at(n.tag), sourceSpan);
        child.importSpan = sourceSpan;
        auto key = child.path.generic_string();
        if (active.size() >= 64 || !active.insert(key).second)
            at.fail("TEMPLATE_CYCLE/LIMIT: recursive template import");
        if (!child.model.empty() && image.schema.model(child.model).id != scopes[0].model)
            at.fail("CALL_MODEL: imported template must share current model");
        auto calleeScopes = scopes;
        calleeScopes.front().variable.clear();
        const auto beginNodes = image.count(NODE);
        auto target = makeTemplate(child, child.root, calleeScopes, child.relative + ":" + path);
        active.erase(key);
        const bool workspaceCall = scopes.front().workspace || std::any_of(
            image.records[NODE].begin() + beginNodes, image.records[NODE].end(), [&](const auto &node) {
                return node[0] == 2 && (image.records[REPT][node[9]][3] & repeatTransparent);
            });
        if (workspaceCall && !explicitId) at.fail("WORKSPACE_SCOPE: import requires explicit view-id");
        auto call = image.add(CALL, {index, target, 0, 0, sourceSpan, workspaceCall ? 1u : 0u});
        std::map<std::string, std::vector<Node>> groups;
        for (auto ch : n.children)
        {
            auto name = ch.attrs.count("slot") ? ch.attrs.at("slot").value : "default";
            ch.attrs.erase("slot");
            groups[name].push_back(ch);
        }
        for (auto &group : groups)
        {
            if (!slots[target].count(group.first))
                at.fail("SLOT_NAME: unknown supplied slot " + group.first);
            Node wrapper;
            wrapper.tag = "Column";
            wrapper.children = group.second;
            wrapper.line = n.line;
            wrapper.column = n.column;
            wrapper.endLine = n.endLine;
            wrapper.endColumn = n.endColumn;
            auto body = makeTemplate(s, wrapper, scopes, s.relative + ":" + path + ":slot:" + group.first);
            image.add(SLOT, {call, image.sid(group.first), body, 1, 0, scopes[0].model, sourceSpan, 0});
        }
        image.records[NODE][index][0] = 3;
        image.records[NODE][index][9] = call;
        // Supplied slots can introduce workspace collections as well as the callee.
        const bool slotWorkspace = std::any_of(image.records[NODE].begin() + beginNodes,
            image.records[NODE].end(), [&](const auto &node) {
                return node[0] == 2 && (image.records[REPT][node[9]][3] & repeatTransparent);
            });
        if (workspaceCall || slotWorkspace) {
            if (!explicitId || stable.find('/') != std::string::npos)
                at.fail("WORKSPACE_SCOPE: explicit slash-free import view-id required");
            image.records[CALL][call][5] = 1;
        }
        if (!n.attrs.empty())
        {
            try {
                const auto rootNode = componentRoot(image, index);
                const auto contract = image.components.at(image.records[NODE][rootNode][1]);
                attributes(s, n, scopes, index, contract);
            } catch (const Error& e) { at.fail(e.what()); }
        }
        return index;
    }
    if (n.tag == "slot")
    {
        auto name = n.attrs.count("name") ? n.attrs.at("name").value : "default";
        if (n.attrs.size() > (n.attrs.count("name") ? 1u : 0u) || !n.text.empty() || name.empty() ||
            !slots[tmpl].insert(name).second)
            at.fail("SLOT_NAME: invalid/duplicate outlet");
        uint32_t body = none;
        if (!n.children.empty())
        {
            n.tag = "Column";
            n.attrs.clear();
            body = makeTemplate(s, n, scopes, s.relative + ":" + path + ":fallback");
        }
        auto slot = image.add(SLOT, {tmpl, image.sid(name), body, 0, 0, scopes[0].model, sourceSpan, 0});
        image.records[NODE][index][0] = 4;
        image.records[NODE][index][9] = slot;
        return index;
    }
    auto entry = registry().find(n.tag);
    if (entry == registry().end())
        at.fail("COMPONENT: unknown component " + n.tag);
    const auto &c = entry->second;
    validateChildren(n, c, at);
    auto ci = std::find_if(image.components.begin(), image.components.end(), [&](auto &v) { return v.name == c.name; });
    if (ci == image.components.end())
    {
        image.components.push_back(c);
        ci = image.components.end() - 1;
    }
    image.records[NODE][index][1] = uint32_t(ci - image.components.begin());
    std::vector<uint32_t> children;
    Attribute previous;
    bool hasPrevious = false;
    unsigned ordinal = 0;
    for (auto ch : n.children)
    {
        bool isElse = ch.attrs.count("v-else") != 0;
        auto current = ch.attrs.find("v-if");
        if (isElse)
        {
            if (!hasPrevious || current != ch.attrs.end())
                at.fail("CONDITION: invalid v-else");
            ch.attrs["v-if"] = previous;
        }
        auto next = current != ch.attrs.end() ? current->second : Attribute{};
        bool nextExists = current != ch.attrs.end() && !isElse;
        children.push_back(lower(s, ch, scopes, tmpl, index, path + "." + std::to_string(ordinal++)));
        previous = next;
        hasPrevious = nextExists;
    }
    image.records[NODE][index][3] = image.count(EDGE);
    image.records[NODE][index][4] = uint32_t(children.size());
    for (auto child : children)
        image.add(EDGE, {index, child});
    try
    {
        attributes(s, n, scopes, index, c);
    }
    catch (const Error &e)
    {
        at.fail(e.what());
    }
    return index;
}
} // namespace oneview
