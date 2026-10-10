#pragma once
#include "binary.h"
#include <filesystem>
#include <map>

namespace oneview
{
struct Location
{
    std::string file;
    uint32_t line = 1, column = 1;
    [[noreturn]] void fail(const std::string &text) const
    {
        throw Error(file + ":" + std::to_string(line) + ":" + std::to_string(column) + ": " + text);
    }
};
struct Property
{
    uint32_t id = 0, tag = 0, access = 1, item = 0;
    std::string name, itemName;
    Location location;
};
struct Argument
{
    std::string name;
    uint32_t tag = 0;
};
struct Command
{
    uint32_t id = 0, when = 0, busy = 0;
    std::string name, whenName, busyName;
    std::vector<Argument> args;
    Location location;
};
struct Model
{
    uint32_t id = 0, key = 0;
    std::string name, keyName;
    Location location;
    std::map<std::string, Property> properties;
    std::map<std::string, Command> commands;
    mutable std::map<uint32_t, std::string> propertyNames, commandNames;
    const Property &property(uint32_t id) const;
    const Command &command(uint32_t id) const;
};
struct Schema
{
    std::map<std::string, Model> models;
    mutable std::map<uint32_t, std::string> modelNames;
    std::vector<std::filesystem::path> dependencies;
    uint32_t root = 0;
    const Model &model(uint32_t id) const;
    const Model &model(const std::string &name) const;
    Bytes encode() const;
};
Schema parseSchema(const std::filesystem::path &path, const std::string &root);
Schema decodeSchema(const Bytes &data);
void resolveSchema(Schema &schema, const std::string &root, bool prune);
bool identifier(const std::string &value);
} // namespace oneview
