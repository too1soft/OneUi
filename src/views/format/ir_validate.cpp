#include "validation.h"
#include <cmath>

namespace oneview
{
void validateImage(const Image &im)
{
    auto count = [&](Section k) { return im.count(k); };
    auto index = [&](Section k, uint32_t i) { require(i < count(k), "BAD_IR: index in section " + std::to_string(k)); };
    auto sid = [&](uint32_t i) { require(i < im.strings.size(), "BAD_IR: string ID"); };
    auto span = [&](uint32_t i) {
        if (i != none)
            index(SMAP, i);
    };
    auto range = [&](Section k, uint32_t start, uint32_t n) {
        require(start <= count(k) && n <= count(k) - start, "BAD_IR: record range");
    };
    auto component = [&](uint32_t node) -> const ComponentContract & {
        index(NODE, node);
        auto &n = im.records[NODE][componentRoot(im, node)];
        require(n[0] == 1 && n[1] < im.components.size(), "BAD_IR: component node");
        return im.components[n[1]];
    };
    index(TMPL, im.root);
    require(im.records[TMPL][im.root][2] == im.schema.root, "BAD_IR: root model");
    for (auto &r : im.records[SMAP])
    {
        sid(r[0]);
        require(!im.strings[r[0]].empty() && im.strings[r[0]][0] != '/' &&
                    im.strings[r[0]].find(':') == std::string::npos && im.strings[r[0]].find('\\') == std::string::npos,
                "BAD_IR: source path");
        require(r[1] && r[2] && r[3] && r[4] && (r[3] > r[1] || (r[3] == r[1] && r[4] >= r[2])),
                "BAD_IR: source coordinates");
        span(r[5]);
    }
    for (auto &r : im.records[TMPL])
    {
        sid(r[0]);
        index(NODE, r[1]);
        im.schema.model(r[2]);
        require(r[3] && r[7] == 0, "BAD_IR: template flags/scope");
        range(SLOT, r[4], r[5]);
        span(r[6]);
    }
    std::set<uint32_t> usedComponents;
    for (auto &r : im.records[NODE])
    {
        require(r[0] >= 1 && r[0] <= 4 && r[8], "BAD_IR: node opcode/flags/scope");
        if (r[0] == 1)
        {
            require(r[1] < im.components.size(), "BAD_IR: component index");
            usedComponents.insert(r[1]);
        }
        else
            require(r[1] == none, "BAD_IR: non-component index");
        if (r[2] != none)
            index(NODE, r[2]);
        range(EDGE, r[3], r[4]);
        range(PROP, r[5], r[6]);
        sid(r[7]);
        sid(r[11]);
        require(!im.strings[r[11]].empty(), "BAD_IR: stable ID");
        span(r[10]);
        if (r[0] == 2)
            index(REPT, r[9]);
        else if (r[0] == 3)
            index(CALL, r[9]);
        else if (r[0] == 4)
            index(SLOT, r[9]);
        else if (im.components[r[1]].name == "NativeHost")
            index(HOST, r[9]);
        else
            require(r[9] == none, "BAD_IR: component aux");
        if (r[0] != 1 && r[0] != 3)
            require(r[4] == 0 && r[6] == 0, "BAD_IR: structural node has children/properties");
        if (r[0] == 3)
            require(r[4] == 0, "BAD_IR: call has physical children");
    }
    require(usedComponents.size() == im.components.size(), "BAD_IR: unused component contract");
    for (auto &r : im.records[EDGE])
    {
        index(NODE, r[0]);
        index(NODE, r[1]);
    }
    for (auto &r : im.records[VALU])
    {
        require(r[0] >= 1 && r[0] <= 4 && r[1] == 0 && r[4] == 0 && r[5] == 0, "BAD_IR: value tag/flags");
        uint64_t bits = uint64_t(r[2]) | (uint64_t(r[3]) << 32);
        if (r[0] == 1)
            require(bits <= 1, "BAD_IR: boolean");
        if (r[0] == 3)
        {
            double f;
            std::memcpy(&f, &bits, 8);
            require(std::isfinite(f), "BAD_IR: finite number");
        }
        if (r[0] == 4)
        {
            require(r[3] == 0, "BAD_IR: string value high bits");
            sid(r[2]);
        }
    }
    std::set<std::pair<uint32_t, uint32_t>> assigned;
    for (auto kind : {PROP, BIND, EVNT})
        for (auto &r : im.records[kind])
        {
            index(NODE, r[0]);
            require(r.back() <= 1 && im.records[NODE][r[0]][0] == (r.back() ? 3u : 1u),
                    "BAD_IR: property/event owner_kind");
            if (r.back() && kind == PROP && component(r[0]).name == "NativeHost")
                require(component(r[0]).property(r[1]).name != "host", "HOST_NAME: CALL cannot rename host");
        }
    for (auto &r : im.records[PROP])
    {
        auto &c = component(r[0]);
        auto &p = c.property(r[1]);
        index(VALU, r[2]);
        auto &v = im.records[VALU][r[2]];
        require(p.tag == v[0] && (p.flags & 1) && assigned.emplace(r[0], r[1]).second,
                "BAD_IR: static property type/duplicate");
        span(r[3]);
        std::string text;
        if (v[0] == 1)
            text = v[2] ? "true" : "false";
        if (v[0] == 2)
        {
            uint64_t bits = uint64_t(v[2]) | (uint64_t(v[3]) << 32);
            int64_t n;
            std::memcpy(&n, &bits, 8);
            text = std::to_string(n);
        }
        if (v[0] == 3)
        {
            uint64_t bits = uint64_t(v[2]) | (uint64_t(v[3]) << 32);
            double f;
            std::memcpy(&f, &bits, 8);
            require(f >= 0 && f <= std::numeric_limits<float>::max(), "BAD_IR: numeric range");
            oneui::ui::syntax::validateLayoutNumber(p.name, float(f));
        }
        if (v[0] == 4)
            text = im.strings[v[2]];
        if (v[0] != 3)
            validateStatic(c.name, p.name, text);
    }
    uint32_t last = 0;
    for (auto &r : im.records[BIND])
    {
        auto &c = component(r[0]);
        auto &p = c.property(r[1]);
        auto &v = im.schema.model(r[4]).property(r[2]);
        auto mode = r[3] >> 16;
        require(r[0] >= last && (mode == 1 || mode == 2) && (p.flags & (mode == 1 ? 2 : 4)) && p.tag == v.tag &&
                    (mode == 1 || v.access == 3) && r[5] == 0 && r[7] == 0,
                "BAD_IR: binding");
        last = r[0];
        if (v.tag == 5)
            validateRecord(im.schema, im.schema.model(v.item), p.record);
        require(assigned.emplace(r[0], r[1]).second, "BAD_IR: duplicate binding/property");
        span(r[6]);
    }
    std::set<std::pair<uint32_t, uint32_t>> events;
    last = 0;
    for (auto &r : im.records[EVNT])
    {
        auto &e = component(r[0]).event(r[1]);
        auto &cmd = im.schema.model(r[4]).command(r[2]);
        range(EARG, r[5], r[6]);
        require(r[0] >= last && (r[3] >> 16) == 0 && r[6] == cmd.args.size() && r[6] == e.args.size() &&
                    events.emplace(r[0], r[1]).second,
                "BAD_IR: event");
        last = r[0];
        span(r[7]);
        for (uint32_t i = 0; i < r[6]; ++i)
        {
            auto &a = im.records[EARG][r[5] + i];
            require(a[2] == cmd.args[i].tag && a[2] == e.args[i].tag, "BAD_IR: command argument type");
            if (a[0] == 1)
                require(a[1] < e.args.size() && e.args[a[1]].tag == a[2], "BAD_IR: event argument source");
            else
            {
                require(a[0] == 2, "BAD_IR: argument opcode");
                index(VALU, a[1]);
                require(im.records[VALU][a[1]][0] == a[2], "BAD_IR: constant argument");
            }
            span(a[3]);
        }
    }
    std::set<uint32_t> conditions;
    last = 0;
    for (auto &r : im.records[COND])
    {
        index(NODE, r[0]);
        require(im.records[NODE][r[0]][0] != 2, "BAD_IR: condition on repeat");
        auto mode = r[3] >> 16;
        require(r[0] >= last && mode <= 3 && r[5] == 0 && conditions.insert(r[0]).second, "BAD_IR: condition");
        last = r[0];
        span(r[4]);
        if (mode & 2)
        {
            index(VALU, r[1]);
            require(im.records[VALU][r[1]][0] == 1, "BAD_IR: bool constant");
        }
        else
            require(im.schema.model(r[2]).property(r[1]).tag == 1, "BAD_IR: bool condition");
    }
    for (auto &r : im.records[REPT])
    {
        index(NODE, r[0]);
        auto &p = im.schema.model(r[2]).property(r[1]);
        auto &m = im.schema.model(r[4]);
        index(TMPL, r[5]);
        require(p.tag == 5 && p.item == m.id && m.key == r[6] && ((r[3] >> 16) == 0 || ((r[3] >> 16) == 3 && im.count(WSC1))) && im.records[TMPL][r[5]][2] == m.id,
                "BAD_IR: repeat model/key");
        span(r[7]);
    }
    for (auto &r : im.records[CALL])
    {
        index(NODE, r[0]);
        index(TMPL, r[1]);
        range(SLOT, r[2], r[3]);
        span(r[4]);
        require(r[5] == 0 || (r[5] == 1 && im.count(WSC1)), "BAD_IR: call flags");
    }
    for (auto &r : im.records[SLOT])
    {
        require(!(r[3] & ~(slotSupplied | slotSingleRoot)) && r[4] == 0 && r[7] == 0, "BAD_IR: slot flags/capture");
        index((r[3] & slotSupplied) ? CALL : TMPL, r[0]);
        sid(r[1]);
        require(!im.strings[r[1]].empty(), "BAD_IR: slot name");
        if (r[2] != none)
            index(TMPL, r[2]);
        im.schema.model(r[5]);
        span(r[6]);
    }
    for (auto &r : im.records[CSS])
    {
        require(r[0] && r[5] == 0, "BAD_IR: CSS flags/scope");
        sid(r[1]);
        sid(r[2]);
        sid(r[3]);
        span(r[4]);
        auto file = im.strings[r[3]];
        Bytes scopeBytes(file.begin(), file.end());
        scopeBytes.push_back(0);
        auto name = std::filesystem::path(file).stem().generic_string();
        scopeBytes.insert(scopeBytes.end(), name.begin(), name.end());
        auto scope = "one-s-" + hex(sha256(scopeBytes)).substr(0, 16);
        require(oneui::ui::syntax::css(im.strings[r[1]], scope, file, 1) == im.strings[r[2]],
                "BAD_IR: lowered CSS/scope mismatch");
        if(im.strings[r[2]].find("@container")!=std::string::npos)
            require(im.count(WSC1)&&(im.records[WSC1][0][1]&workspaceQueries),"VERSION: container query capability");
        require(std::any_of(im.records[TMPL].begin(), im.records[TMPL].end(), [&](auto &t) { return t[3] == r[0]; }),
                "BAD_IR: unused CSS scope");
    }
    require(im.count(WSC1) <= 1, "VERSION: WSC1 count");
    if (im.count(WSC1)) {
        const auto &w = im.records[WSC1][0];
        require(w[0] == 1 && w[1] && !(w[1] & ~7u) && !w[2] && !w[3], "VERSION: WSC1 capabilities");
    }
    std::set<std::string> hosts;
    for (auto &r : im.records[HOST])
    {
        require(component(r[0]).name == "NativeHost" && r[2] <= 3, "BAD_IR: host");
        sid(r[1]);
        require(!im.strings[r[1]].empty() && ((r[2] & 2) || hosts.insert(im.strings[r[1]]).second), "BAD_IR: host name");
        span(r[3]);
    }
    Schema builtins;
    addBuiltins(builtins);
    for (auto &e : im.schema.models)
        if (e.first.rfind("oneui.v1.", 0) == 0)
        {
            auto &m = e.second;
            auto &known = builtins.model(e.first);
            require(m.keyName == known.keyName && m.properties.size() == known.properties.size() && m.commands.empty(),
                    "BAD_IR: builtin model");
            for (auto &p : m.properties)
            {
                auto k = known.properties.find(p.first);
                require(k != known.properties.end() && k->second.tag == p.second.tag && p.second.access == 1 &&
                            k->second.itemName == p.second.itemName,
                        "BAD_IR: builtin property");
            }
        }
}
} // namespace oneview
