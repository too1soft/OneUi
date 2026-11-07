#include "component_extensions.h"
#include "validation.h"
#include <cmath>
#include <charconv>
#include <fstream>
#include <sstream>

namespace oneview {
namespace {
bool name(const std::string &s, bool component = false) {
    return s.size() <= 128 && std::regex_match(s, std::regex(component ? "[A-Z][A-Za-z0-9]*_[A-Za-z0-9_]+" : "[A-Za-z_][A-Za-z0-9_-]*"));
}
uint32_t number(std::istringstream &fields) {
    std::string token; fields >> token; uint32_t value=0;
    auto parsed=std::from_chars(token.data(),token.data()+token.size(),value);
    require(!token.empty() && parsed.ec==std::errc{} && parsed.ptr==token.data()+token.size(), "COMPONENTS: unsigned 32-bit integer required");
    return value;
}
uint32_t tag(const std::string &type) {
    if (type == "bool") return 1;
    if (type == "i64") return 2;
    if (type == "f64") return 3;
    if (type == "utf8") return 4;
    throw Error("COMPONENTS: scalar type must be bool, i64, f64 or utf8");
}
void common(ComponentContract &c) {
    uint32_t id = 0;
    for (const auto &[key, type] : std::vector<std::pair<std::string,uint32_t>>{
             {"class",4},{"grow",3},{"basis",3},{"min",3},{"max",3},{"shrink",3},
             {"visible",1},{"disabled",1},{"name",4},{"description",4}})
        c.properties.push_back({++id, type, 3, key, ""});
}
}
bool externalComponent(const std::string &s) { return registry().count(s) == 0; }
ComponentContracts parseComponentExtensions(const std::string &text) {
    require(!text.empty() && text.size() <= 1024*1024 && utf8(text), "COMPONENTS: metadata limit/UTF-8");
    ComponentContracts result;
    std::istringstream input(text);
    std::string line;
    ComponentContract current;
    bool header = false, opened = false;
    std::set<std::string> properties, events;
    for (size_t row = 1; std::getline(input,line); ++row) {
        auto comment = line.find('#'); if (comment != line.npos) line.resize(comment);
        std::istringstream fields(line);
        std::string word; if (!(fields >> word)) continue;
        if (!header) {
            auto version = number(fields);
            require(word == "oneui-components" && version == 1, "COMPONENTS: expected oneui-components 1");
            header = true;
        } else if (word == "component") {
            require(!opened && result.size() < 256, "COMPONENTS: nested/excessive components");
            current = {}; properties.clear(); events.clear();
            fields >> current.name; current.version=number(fields); fields >> current.childRule;
            require(bool(fields) && name(current.name,true) &&
                    current.version > 0 && !registry().count(current.name) && !result.count(current.name), "COMPONENTS: invalid/duplicate namespaced component");
            require(std::set<std::string>{"leaf","any","one","optional-one","two"}.count(current.childRule), "COMPONENTS: unsupported child rule");
            common(current); for (const auto &p : current.properties) properties.insert(p.name);
            opened = true;
        } else if (word == "property") {
            ComponentProperty p{}; std::string type;
            p.id=number(fields); fields >> p.name >> type; p.flags=number(fields);
            require(opened && current.properties.size() < 256 && bool(fields) &&
                    p.id >= 1000 && name(p.name) && properties.insert(p.name).second && p.flags > 0 && p.flags <= 7 &&
                    (!(p.flags & 4) || (p.flags & 2)), "COMPONENTS: invalid property");
            p.tag = tag(type); current.properties.push_back(std::move(p));
        } else if (word == "model") {
            require(opened && current.modelProperty == 0,"COMPONENTS: invalid model statement");
            current.modelProperty=number(fields);
            require(current.modelProperty >= 1000, "COMPONENTS: invalid model property");
        } else if (word == "event") {
            ComponentEvent e{}; e.id=number(fields); fields >> e.name; auto count=number(fields);
            require(opened && current.events.size() < 64 && bool(fields) && e.id && name(e.name) &&
                    events.insert(e.name).second && count <= 16, "COMPONENTS: invalid event");
            std::set<std::string> args;
            for (size_t i = 0; i < count; ++i) {
                Argument a{}; std::string type;
                require(bool(fields >> a.name >> type) && name(a.name) && args.insert(a.name).second, "COMPONENTS: invalid event argument");
                a.tag = tag(type); e.args.push_back(std::move(a));
            }
            current.events.push_back(std::move(e));
        } else if (word == "end") {
            require(opened, "COMPONENTS: unmatched end");
            auto order = [](const auto &a,const auto &b){ return a.id < b.id; };
            std::sort(current.properties.begin(),current.properties.end(),order);
            std::sort(current.events.begin(),current.events.end(),order);
            for (size_t i = 1; i < current.properties.size(); ++i)
                require(current.properties[i-1].id != current.properties[i].id, "COMPONENTS: duplicate property ID");
            for (size_t i = 1; i < current.events.size(); ++i)
                require(current.events[i-1].id != current.events[i].id, "COMPONENTS: duplicate event ID");
            if (current.modelProperty) require(current.property(current.modelProperty).flags & 4, "COMPONENTS: model property flags");
            for (const auto &p : current.properties) require(!(p.flags & 4) || current.modelProperty == p.id, "COMPONENTS: inconsistent model property");
            result.emplace(current.name,std::move(current)); opened = false;
        } else throw Error("COMPONENTS: unknown metadata statement at line " + std::to_string(row));
        require(!(fields >> word), "COMPONENTS: trailing metadata fields");
    }
    require(header && !opened && !result.empty(), "COMPONENTS: incomplete/empty document");
    return result;
}
ComponentContracts loadComponentExtensions(const std::vector<fs::path> &files) {
    ComponentContracts result = registry();
    for (const auto &file : files) {
        std::ifstream input(file,std::ios::binary);
        require(bool(input), "COMPONENTS: cannot read " + file.generic_string());
        std::string data(1024*1024+1,'\0'); input.read(data.data(),std::streamsize(data.size())); data.resize(size_t(input.gcount()));
        for (auto &[name,c] : parseComponentExtensions(data))
            require(result.emplace(name,std::move(c)).second, "COMPONENTS: duplicate component across packages");
        require(result.size() <= registry().size()+256, "COMPONENTS: registry limit");
    }
    return result;
}
void validateComponentStatic(const ComponentContract &c,const std::string &key,const std::string &text) {
    const auto &p = c.property(key);
    if (!externalComponent(c.name) || p.id < 1000) {
        // Shared generic properties have the same validation as Surface.
        validateStatic(externalComponent(c.name) ? "Surface" : c.name,key,text);
        return;
    }
    require(p.flags & 1, "STATIC_TYPE: property requires binding");
    if (p.tag == 1) require(text == "true" || text == "false", "STATIC_TYPE: expected bool");
    else if (p.tag == 2) { size_t used = 0; std::stoll(text,&used); require(used == text.size(), "STATIC_TYPE: expected i64"); }
    else if (p.tag == 3) { size_t used = 0; auto value = std::stod(text,&used); require(used == text.size() && std::isfinite(value), "STATIC_TYPE: expected finite f64"); }
    else require(utf8(text), "STATIC_UTF8: invalid UTF-8");
}
}
