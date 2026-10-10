#include "validation.h"
#include <functional>

namespace oneview
{
void validateGraph(const Image &im)
{
    auto &rs = im.records;
    std::vector<uint32_t> owners(im.count(NODE), none), edges(im.count(EDGE)), props(im.count(PROP)),
        args(im.count(EARG)), auxiliary[4];
    auxiliary[0].resize(im.count(REPT));
    auxiliary[1].resize(im.count(CALL));
    auxiliary[2].resize(im.count(SLOT));
    auxiliary[3].resize(im.count(HOST));
    std::map<uint32_t, std::set<std::string>> references;
    uint32_t expandedNodes = 0;
    std::vector<bool> workspaceScope{false};
    std::vector<bool> explicitCalls;
    std::vector<std::set<std::string>> scopeHosts(1);
    std::vector<uint32_t> hostInstances(im.count(HOST));
    auto claim = [](std::vector<uint32_t> &counts, uint32_t index, bool fresh) {
        return !fresh || ++counts[index] == 1;
    };
    std::vector<std::vector<const Record *>> bindings(im.count(NODE)), conditions(im.count(NODE)),
        staticProps(im.count(NODE));
    for (auto k : {BIND, EVNT})
        for (auto &r : rs[k])
            bindings[r[0]].push_back(&r);
    for (auto &r : rs[COND])
        conditions[r[0]].push_back(&r);
    for (auto &r : rs[PROP])
        staticProps[r[0]].push_back(&r);
    std::vector<bool> usedTemplates(im.count(TMPL));
    std::set<uint32_t> activeTemplates;
    std::function<void(uint32_t, const std::vector<uint32_t> &, unsigned)> visitTemplate;
    std::function<void(uint32_t, uint32_t, uint32_t, const std::vector<uint32_t> &, unsigned, std::set<std::string> &)>
        visitNode;
    auto lexical = [&](const std::vector<uint32_t> &scopes, uint32_t packed, uint32_t model) {
        auto d = packed & 0xffff;
        require(d < scopes.size() && scopes[d] == model, "BAD_IR: lexical model/depth");
    };
    visitNode = [&](uint32_t node, uint32_t tmpl, uint32_t parent, const std::vector<uint32_t> &scopes, unsigned depth,
                    std::set<std::string> &ids) {
        bool fresh = owners[node] == none;
        require(depth <= 64 && (fresh || owners[node] == tmpl) && ++expandedNodes <= 100000,
                "BAD_IR: node cycle/shared tree or expanded node limit");
        owners[node] = tmpl;
        auto &n = rs[NODE][node];
        require(n[2] == parent && n[8] == rs[TMPL][tmpl][3] && ids.insert(im.strings[n[11]]).second,
                "BAD_IR: parent/scope/stable ID");
        auto ref = im.strings[n[7]];
        require(ref.empty() || (identifier(ref) && ref.find('.') == std::string::npos && scopes.size() == 1 &&
                                n[0] != 2 && references[tmpl].insert(ref).second),
                "BAD_IR: invalid/duplicate ref");
        for (auto r : bindings[node])
            lexical(scopes, (*r)[3], (*r)[4]);
        for (auto r : conditions[node])
            lexical(scopes, (*r)[3], (*r)[2]);
        for (uint32_t i = 0; i < n[6]; ++i)
        {
            auto ix = n[5] + i;
            require(rs[PROP][ix][0] == node && claim(props, ix, fresh), "BAD_IR: property ownership");
        }
        if (n[0] == 1)
        {
            auto &c = im.components[n[1]];
            double minimum = 0, maximum = 0;
            bool hasMinimum = false, hasMaximum = false;
            for (auto p : staticProps[node])
            {
                auto name = c.property((*p)[1]).name;
                if (name == "min" || name == "max")
                {
                    auto &v = rs[VALU][(*p)[2]];
                    uint64_t bits = uint64_t(v[2]) | (uint64_t(v[3]) << 32);
                    double number;
                    std::memcpy(&number, &bits, 8);
                    if (name == "min")
                    {
                        minimum = number;
                        hasMinimum = true;
                    }
                    else
                    {
                        maximum = number;
                        hasMaximum = true;
                    }
                }
            }
            require(!hasMinimum || !hasMaximum || minimum <= maximum, "BAD_IR: min > max");
            Node logical;
            logical.tag = c.name;
            for (uint32_t i = 0; i < n[4]; ++i)
            {
                auto ix = n[3] + i;
                auto &e = rs[EDGE][ix];
                require(e[0] == node && claim(edges, ix, fresh), "BAD_IR: edge ownership");
                auto &ch = rs[NODE][e[1]];
                Node child;
                child.tag = ch[0] == 1 ? im.components[ch[1]].name : "<structural>";
                if (ch[0] == 2 && (rs[REPT][ch[9]][3] & repeatTransparent)) {
                    auto body = rs[TMPL][rs[REPT][ch[9]][5]][1];
                    require(rs[NODE][body][0] == 1, "BAD_IR: workspace item root");
                    child.tag = im.components[rs[NODE][body][1]].name;
                }
                if (ch[0] == 2)
                    child.attrs["v-for"] = {};
                logical.children.push_back(child);
                visitNode(e[1], tmpl, node, scopes, depth + 1, ids);
            }
            validateChildren(logical, c, {"<IR>", 1, 1});
            if (c.name == "NativeHost")
            {
                const bool dynamic = scopes.size() > 1;
                require(!dynamic || (std::all_of(explicitCalls.begin(), explicitCalls.end(), [](bool v) { return v; }) && workspaceScope.back() && im.count(WSC1) &&
                            (rs[WSC1][0][1] & workspaceHosts)), "BAD_IR: dynamic host scope/capability");
                require(bool(rs[HOST][n[9]][2] & 2) == dynamic &&
                            scopeHosts.back().insert(im.strings[rs[HOST][n[9]][1]]).second &&
                            rs[HOST][n[9]][0] == node && claim(auxiliary[3], n[9], fresh) &&
                            ++hostInstances[n[9]] == 1,
                        "BAD_IR: dynamic/duplicate host");
                bool matched = false;
                for (auto p : staticProps[node])
                    if (c.property((*p)[1]).name == "host")
                        matched = im.strings[rs[VALU][(*p)[2]][2]] == im.strings[rs[HOST][n[9]][1]];
                require(matched, "BAD_IR: host registration/property mismatch");
            }
        }
        else if (n[0] == 2)
        {
            auto &r = rs[REPT][n[9]];
            require(r[0] == node && claim(auxiliary[0], n[9], fresh), "BAD_IR: repeat ownership");
            lexical(scopes, r[3], r[2]);
            auto nested = scopes;
            nested.insert(nested.begin(), r[4]);
            const bool transparent = r[3] & repeatTransparent;
            if (transparent) {
                require(im.strings[n[11]].find('/') == std::string::npos,
                    "BAD_IR: workspace view-id contains slash");
                require(std::all_of(explicitCalls.begin(), explicitCalls.end(), [](bool v) { return v; }),
                    "BAD_IR: workspace import requires explicit view-id");
                require(im.count(WSC1) && (rs[WSC1][0][1] & workspaceCollections) && parent != none &&
                    rs[NODE][parent][0] == 1, "BAD_IR: transparent collection parent");
                const auto &rule = im.components[rs[NODE][parent][1]].childRule;
                require(rule == "pane-collection" || rule == "dock-collection", "BAD_IR: transparent collection rule");
            }
            workspaceScope.push_back(transparent);
            scopeHosts.emplace_back();
            visitTemplate(r[5], nested, depth + 1);
            scopeHosts.pop_back();
            workspaceScope.pop_back();
        }
        else if (n[0] == 3)
        {
            auto &r = rs[CALL][n[9]];
            require(r[0] == node && claim(auxiliary[1], n[9], fresh), "BAD_IR: call ownership");
            require(!r[5] || im.strings[n[11]].find('/') == std::string::npos,
                "BAD_IR: workspace call-id contains slash");
            explicitCalls.push_back(r[5] == 1);
            visitTemplate(r[1], scopes, depth + 1);
            std::set<std::string> supplied;
            for (uint32_t i = 0; i < r[3]; ++i)
            {
                auto ix = r[2] + i;
                auto &slot = rs[SLOT][ix];
                require(slot[0] == n[9] && slot[3] == 1 && slot[5] == scopes[0] &&
                            supplied.insert(im.strings[slot[1]]).second && claim(auxiliary[2], ix, fresh),
                        "BAD_IR: supplied slot");
                auto &target = rs[TMPL][r[1]];
                bool found = false;
                for (uint32_t j = 0; j < target[5]; ++j)
                    if (im.strings[rs[SLOT][target[4] + j][1]] == im.strings[slot[1]])
                        found = true;
                require(found && slot[2] != none, "BAD_IR: unknown/empty supplied slot");
                visitTemplate(slot[2], scopes, depth + 1);
            }
            explicitCalls.pop_back();
        }
        else
        {
            auto &r = rs[SLOT][n[9]];
            auto &t = rs[TMPL][tmpl];
            require(r[0] == tmpl && r[3] == 0 && r[5] == scopes[0] && n[9] >= t[4] && n[9] - t[4] < t[5] &&
                        claim(auxiliary[2], n[9], fresh),
                    "BAD_IR: outlet ownership");
            if (r[2] != none)
                visitTemplate(r[2], scopes, depth + 1);
        }
    };
    visitTemplate = [&](uint32_t t, const std::vector<uint32_t> &scopes, unsigned depth) {
        require(depth <= 64 && !activeTemplates.count(t), "BAD_IR: template cycle/depth");
        require(rs[TMPL][t][2] == scopes[0], "BAD_IR: template model");
        references[t].clear();
        usedTemplates[t] = true;
        activeTemplates.insert(t);
        std::set<std::string> ids;
        std::set<std::string> outletNames;
        for (uint32_t i = 0; i < rs[TMPL][t][5]; ++i)
        {
            auto &r = rs[SLOT][rs[TMPL][t][4] + i];
            require(r[0] == t && r[3] == 0 && outletNames.insert(im.strings[r[1]]).second, "BAD_IR: duplicate outlet");
        }
        visitNode(rs[TMPL][t][1], t, none, scopes, depth, ids);
        activeTemplates.erase(t);
    };
    visitTemplate(im.root, {im.schema.root}, 1);
    require(std::all_of(usedTemplates.begin(), usedTemplates.end(), [](bool x) { return x; }),
            "BAD_IR: unreachable template");
    require(std::all_of(owners.begin(), owners.end(), [](auto x) { return x != none; }), "BAD_IR: unreachable node");
    for (auto &r : rs[EVNT])
        for (uint32_t i = 0; i < r[6]; ++i)
            require(++args[r[5] + i] == 1, "BAD_IR: argument ownership");
    for (auto *counts : {&edges, &props, &args, &auxiliary[0], &auxiliary[1], &auxiliary[2], &auxiliary[3]})
        require(std::all_of(counts->begin(), counts->end(), [](auto x) { return x == 1; }), "BAD_IR: unowned record");
    for (uint32_t i = 0; i < im.count(SMAP); ++i)
    {
        auto parent = rs[SMAP][i][5];
        unsigned depth = 0;
        std::set<uint32_t> seen;
        while (parent != none)
        {
            require(++depth <= 64 && seen.insert(parent).second, "BAD_IR: source map cycle");
            parent = rs[SMAP][parent][5];
        }
    }
}
} // namespace oneview
