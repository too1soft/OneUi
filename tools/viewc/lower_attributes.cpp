#include "compiler.h"

namespace oneview
{
void ViewCompiler::attributes(Source &s, const Node &n, const std::vector<Scope> &scopes, uint32_t index,
                              const ComponentContract &c)
{
    auto staticStart = image.count(PROP);
    const auto ownerKind = image.records[NODE][index][0] == 3 ? 1u : 0u;
    if (ownerKind && c.name == "NativeHost" && n.attrs.count("host"))
        Location{s.relative, uint32_t(n.line), uint32_t(n.column)}.fail("HOST_NAME: CALL cannot rename host");
    std::set<uint32_t> written;
    auto claim = [&](uint32_t id, const Attribute &a) {
        if (!written.insert(id).second)
            Location{s.relative, uint32_t(a.line), uint32_t(a.column)}.fail(
                "PROPERTY_DUPLICATE: multiple sources for property");
    };
    if (!n.text.empty())
    {
        require(n.text.find("{{") == std::string::npos, "EXPRESSION: use :text, interpolation is unsupported");
        auto &p = c.property("text");
        validateStatic(c.name, p.name, n.text);
        written.insert(p.id);
        image.add(PROP, {index, p.id, value(p.tag, n.text), span(s, n), ownerKind});
    }
    if (n.attrs.count("min") && n.attrs.count("max"))
        require(std::stod(n.attrs.at("min").value) <= std::stod(n.attrs.at("max").value), "STATIC_RANGE: min > max");
    for (auto &entry : n.attrs)
    {
        auto key = entry.first;
        const auto &a = entry.second;
        Location at{s.relative, uint32_t(a.line), uint32_t(a.column)};
        try
        {
            if (key[0] == '@')
            {
                const auto &e = c.event(key.substr(1));
                auto r = resolve(s, a, scopes, true);
                auto &cmd = image.schema.model(r.model).command(r.id);
                require(e.args.size() == cmd.args.size(), "COMMAND_SIGNATURE: argument count");
                auto start = image.count(EARG);
                for (uint32_t i = 0; i < e.args.size(); ++i)
                {
                    require(e.args[i].tag == cmd.args[i].tag, "COMMAND_SIGNATURE: argument type");
                    image.add(EARG, {1, i, e.args[i].tag, span(s, a)});
                }
                image.add(EVNT, {index, e.id, cmd.id, r.depth, r.model, start, uint32_t(e.args.size()), span(s, a), ownerKind});
                continue;
            }
            bool model = key == "v-model", bound = model || key[0] == ':';
            if (model)
            {
                require(c.modelProperty != 0, "MODEL_TYPE: v-model unsupported");
                key = c.property(c.modelProperty).name;
            }
            else if (bound)
                key.erase(0, 1);
            auto &p = c.property(key);
            claim(p.id, a);
            if (bound)
            {
                require((p.flags & (model ? 4 : 2)) != 0, "BIND_STATIC: property is static");
                if ((a.value == "true" || a.value == "false") && !model)
                {
                    require(p.tag == 1, "BIND_TYPE: bool literal");
                    image.add(PROP, {index, p.id, value(1, a.value), span(s, a), ownerKind});
                    continue;
                }
                auto r = resolve(s, a, scopes);
                require(r.tag == p.tag, "BIND_TYPE: schema type does not match component property");
                if (model)
                    require(r.access == 3, "MODEL_READ_ONLY: v-model requires rw property");
                if (r.tag == 5)
                    require(image.schema.model(r.item).name == p.record, "BIND_RECORD: component requires " + p.record);
                image.add(BIND, {index, p.id, r.id, r.depth | ((model ? 2u : 1u) << 16), r.model, 0, span(s, a), 0, ownerKind});
            }
            else
            {
                validateStatic(c.name, key, a.value);
                image.add(PROP, {index, p.id, value(p.tag, a.value), span(s, a), ownerKind});
            }
        }
        catch (const std::exception &e)
        {
            at.fail(e.what());
        }
    }
    image.records[NODE][index][5] = staticStart;
    image.records[NODE][index][6] = image.count(PROP) - staticStart;
    if (c.name == "NativeHost" && !ownerKind)
    {
        require(scopes.size() == 1, "HOST_DYNAMIC: WP3 only supports static hosts");
        auto h = n.attrs.find("host");
        require(h != n.attrs.end() && !h->second.value.empty(), "HOST_NAME: static nonempty host required");
        require(hostNames.insert(h->second.value).second, "HOST_NAME: duplicate host");
        auto flags = n.attrs.count("themed") && n.attrs.at("themed").value == "true" ? 1u : 0u;
        image.records[NODE][index][9] = image.add(HOST, {index, image.sid(h->second.value), flags, span(s, h->second)});
    }
}
} // namespace oneview
