#pragma once
#include "schema.h"

namespace oneview
{
struct ComponentProperty
{
    uint32_t id, tag, flags;
    std::string name, record;
};
struct ComponentEvent
{
    uint32_t id;
    std::string name;
    std::vector<Argument> args;
};
struct ComponentContract
{
    std::string name, childRule;
    uint32_t version = 1, modelProperty = 0;
    std::vector<ComponentProperty> properties;
    std::vector<ComponentEvent> events;
    const ComponentProperty &property(const std::string &name) const;
    const ComponentProperty &property(uint32_t id) const;
    const ComponentEvent &event(const std::string &name) const;
    const ComponentEvent &event(uint32_t id) const;
};
const std::map<std::string, ComponentContract> &registry();
Bytes encodeComponents(const std::vector<ComponentContract> &components);
std::vector<ComponentContract> decodeComponents(const Bytes &bytes);
void addBuiltins(Schema &schema);
} // namespace oneview
