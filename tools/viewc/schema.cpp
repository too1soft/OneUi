#include "schema.h"
#include <functional>
#include <set>

namespace oneview
{
bool identifier(const std::string &value)
{
    bool start = true;
    for (auto c : value)
    {
        if (c == '.')
        {
            if (start)
                return false;
            start = true;
        }
        else if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || (!start && c >= '0' && c <= '9'))
            start = false;
        else
            return false;
    }
    return !start;
}
const Model &Schema::model(const std::string &name) const
{
    auto i = models.find(name);
    require(i != models.end(), "SCHEMA_MODEL: unknown model " + name);
    return i->second;
}
const Model &Schema::model(uint32_t id) const
{
    if (modelNames.empty())
        for (auto &e : models)
            require(e.second.id && modelNames.emplace(e.second.id, e.first).second, "BAD_IR: duplicate/zero model ID");
    auto found = modelNames.find(id);
    require(found != modelNames.end(), "BAD_IR: unknown model ID");
    return models.at(found->second);
}
const Property &Model::property(uint32_t id) const
{
    if (propertyNames.empty())
        for (auto &e : properties)
            require(e.second.id && propertyNames.emplace(e.second.id, e.first).second,
                    "BAD_IR: duplicate/zero property ID");
    auto found = propertyNames.find(id);
    require(found != propertyNames.end(), "BAD_IR: unknown property ID");
    return properties.at(found->second);
}
const Command &Model::command(uint32_t id) const
{
    if (commandNames.empty())
        for (auto &e : commands)
            require(e.second.id && commandNames.emplace(e.second.id, e.first).second,
                    "BAD_IR: duplicate/zero command ID");
    auto found = commandNames.find(id);
    require(found != commandNames.end(), "BAD_IR: unknown command ID");
    return commands.at(found->second);
}
void resolveSchema(Schema &s, const std::string &root, bool prune)
{
    s.modelNames.clear();
    for (auto &entry : s.models)
    {
        entry.second.propertyNames.clear();
        entry.second.commandNames.clear();
    }
    s.model(root);
    std::set<std::string> visited, active;
    std::function<void(const std::string &, unsigned)> visit = [&](const std::string &name, unsigned depth) {
        const auto &m = s.model(name);
        if (active.count(name))
            m.location.fail("SCHEMA_CYCLE: recursive list model");
        if (visited.count(name))
            return;
        if (depth > 64)
            m.location.fail("LIMIT: model depth");
        active.insert(name);
        for (const auto &entry : m.properties)
            if (entry.second.tag == 5)
            {
                const auto &p = entry.second;
                if (p.access != 1)
                    p.location.fail("SCHEMA_LIST: list must be read only");
                if (!s.models.count(p.itemName))
                    p.location.fail("SCHEMA_TYPE: unknown item model " + p.itemName);
                if (s.model(p.itemName).keyName.empty())
                    p.location.fail("SCHEMA_KEY: list item requires key");
                visit(p.itemName, depth + 1);
            }
        active.erase(name);
        visited.insert(name);
    };
    // Validate even unused declarations; only encoding prunes unrelated roots.
    for (const auto &e : s.models)
        visit(e.first, 1);
    uint32_t id = 0;
    for (auto &e : s.models)
        e.second.id = ++id;
    for (auto &e : s.models)
    {
        auto &m = e.second;
        id = 0;
        for (auto &q : m.properties)
        {
            q.second.id = ++id;
            q.second.item = q.second.tag == 5 ? s.model(q.second.itemName).id : 0;
        }
        m.key = 0;
        if (!m.keyName.empty())
        {
            auto k = m.properties.find(m.keyName);
            if (k == m.properties.end() || k->second.tag != 4 || k->second.access != 1)
                m.location.fail("SCHEMA_KEY: key must name a read-only utf8 property");
            m.key = k->second.id;
        }
        id = 0;
        for (auto &q : m.commands)
        {
            auto &c = q.second;
            c.id = ++id;
            auto gate = [&](const std::string &n) {
                if (n.empty())
                    return 0u;
                auto p = m.properties.find(n);
                if (p == m.properties.end() || p->second.tag != 1 || p->second.access != 1)
                    c.location.fail("SCHEMA_GATE: when/busy requires read-only bool");
                return p->second.id;
            };
            c.when = gate(c.whenName);
            c.busy = gate(c.busyName);
        }
    }
    if (prune)
    {
        visited.clear();
        visit(root, 1);
        for (auto i = s.models.begin(); i != s.models.end();)
        {
            if (!visited.count(i->first))
                i = s.models.erase(i);
            else
                ++i;
        }
    }
    if (prune)
    {
        resolveSchema(s, root, false);
        return;
    }
    s.root = s.model(root).id;
}
Bytes Schema::encode() const
{
    Bytes b{'O', 'V', 'S', 'C', 'H', 'M', '1', 0};
    words(b, {root, uint32_t(models.size())});
    for (const auto &e : models)
    {
        const auto &m = e.second;
        put(b, m.id);
        string(b, m.name);
        words(b, {m.key, uint32_t(m.properties.size()), uint32_t(m.commands.size())});
        for (const auto &q : m.properties)
        {
            const auto &p = q.second;
            put(b, p.id);
            string(b, p.name);
            words(b, {p.tag, p.access, p.item});
        }
        for (const auto &q : m.commands)
        {
            const auto &c = q.second;
            put(b, c.id);
            string(b, c.name);
            words(b, {0, c.when, c.busy, uint32_t(c.args.size())});
            for (const auto &a : c.args)
            {
                string(b, a.name);
                words(b, {a.tag, 0});
            }
        }
    }
    return b;
}
Schema decodeSchema(const Bytes &b)
{
    Cursor c(b);
    require(c.get(8) == 0x00314d484353564full, "BAD_IR: SCHM magic");
    Schema s;
    s.root = c.u32();
    auto count = c.u32();
    require(count <= 100000, "LIMIT: models");
    for (uint32_t i = 0; i < count; ++i)
    {
        Model m;
        m.id = c.u32();
        m.name = c.str();
        m.key = c.u32();
        auto np = c.u32(), nc = c.u32();
        require(identifier(m.name) && np <= 100000 && nc <= 100000, "BAD_IR: model");
        for (uint32_t j = 0; j < np; ++j)
        {
            Property p;
            p.id = c.u32();
            p.name = c.str();
            p.tag = c.u32();
            p.access = c.u32();
            p.item = c.u32();
            require(identifier(p.name) && p.tag >= 1 && p.tag <= 5 && (p.access == 1 || p.access == 3) &&
                        (p.tag == 5 || p.item == 0),
                    "BAD_IR: property");
            require(m.properties.emplace(p.name, p).second, "BAD_IR: duplicate property");
        }
        for (uint32_t j = 0; j < nc; ++j)
        {
            Command cmd;
            cmd.id = c.u32();
            cmd.name = c.str();
            require(c.u32() == 0, "BAD_IR: result type");
            cmd.when = c.u32();
            cmd.busy = c.u32();
            auto n = c.u32();
            require(n <= 100000 && identifier(cmd.name), "BAD_IR: command");
            std::set<std::string> names;
            for (uint32_t k = 0; k < n; ++k)
            {
                Argument a{c.str(), c.u32()};
                require(c.u32() == 0 && a.tag >= 1 && a.tag <= 4 && identifier(a.name) && names.insert(a.name).second,
                        "BAD_IR: argument");
                cmd.args.push_back(a);
            }
            require(m.commands.emplace(cmd.name, cmd).second, "BAD_IR: duplicate command");
        }
        require(s.models.emplace(m.name, m).second, "BAD_IR: duplicate model");
    }
    c.done();
    auto root = s.model(s.root).name;
    for (auto &e : s.models)
    {
        auto &m = e.second;
        if (m.key)
            m.keyName = m.property(m.key).name;
        for (auto &p : m.properties)
            if (p.second.tag == 5)
                p.second.itemName = s.model(p.second.item).name;
        for (auto &q : m.commands)
        {
            if (q.second.when)
                q.second.whenName = m.property(q.second.when).name;
            if (q.second.busy)
                q.second.busyName = m.property(q.second.busy).name;
        }
    }
    resolveSchema(s, root, true);
    require(s.encode() == b, "BAD_IR: noncanonical schema or unreachable model");
    return s;
}
} // namespace oneview
