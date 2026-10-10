#include "component_contract.h"
#include <fstream>
#include <set>

namespace oneview
{
namespace
{
struct Token
{
    std::string text;
    Location location;
    bool quoted = false;
};
struct Lexer
{
    std::string source, file;
    size_t pos = 0;
    uint32_t line = 1, column = 1;
    void step()
    {
        if (source[pos++] == '\n')
        {
            ++line;
            column = 1;
        }
        else
            ++column;
    }
    Token next()
    {
        for (;;)
        {
            while (pos < source.size() && std::isspace(uint8_t(source[pos])))
                step();
            if (source.compare(pos, 2, "//") != 0)
                break;
            while (pos < source.size() && source[pos] != '\n')
                step();
        }
        Token t{{}, {file, line, column}};
        if (pos == source.size())
            return t;
        char c = source[pos];
        if (c == '"')
        {
            t.quoted = true;
            step();
            while (pos < source.size() && source[pos] != '"')
            {
                if (source[pos] == '\n' || source[pos] == '\\')
                    t.location.fail("SCHEMA_SYNTAX: import uses an unescaped relative path with / separators");
                t.text += source[pos];
                step();
            }
            if (pos == source.size())
                t.location.fail("SCHEMA_SYNTAX: unclosed string");
            step();
            return t;
        }
        if (std::isalpha(uint8_t(c)) || c == '_')
        {
            do
            {
                t.text += source[pos];
                step();
            } while (pos < source.size() &&
                     (std::isalnum(uint8_t(source[pos])) || source[pos] == '_' || source[pos] == '.'));
        }
        else
        {
            t.text = c;
            step();
        }
        return t;
    }
};
struct Parser
{
    Lexer lexer;
    Token token;
    Schema &schema;
    std::set<std::string> &active;
    std::set<std::string> &loaded;
    void advance()
    {
        token = lexer.next();
    }
    bool take(const std::string &s)
    {
        if (!token.quoted && token.text == s)
        {
            advance();
            return true;
        }
        return false;
    }
    void expect(const std::string &s)
    {
        if (!take(s))
            token.location.fail("SCHEMA_SYNTAX: expected '" + s + "'");
    }
    std::string name()
    {
        auto t = token;
        if (t.quoted || !identifier(t.text))
            t.location.fail("SCHEMA_SYNTAX: expected identifier");
        advance();
        return t.text;
    }
    uint32_t type(std::string &item)
    {
        auto at = token.location;
        auto n = name();
        if (n == "bool")
            return 1;
        if (n == "i64")
            return 2;
        if (n == "f64")
            return 3;
        if (n == "utf8")
            return 4;
        if (n == "list")
        {
            expect("<");
            item = name();
            expect(">");
            return 5;
        }
        at.fail("SCHEMA_TYPE: unknown type " + n);
    }
    void model()
    {
        Model m;
        m.location = token.location;
        m.name = name();
        if (take("key"))
            m.keyName = name();
        expect("{");
        while (!take("}"))
        {
            auto at = token.location;
            if (token.text.empty())
                at.fail("SCHEMA_SYNTAX: unclosed model");
            if (take("command"))
            {
                Command c;
                c.location = at;
                c.name = name();
                expect("(");
                std::set<std::string> args;
                if (!take(")"))
                    for (;;)
                    {
                        Argument a;
                        a.name = name();
                        expect(":");
                        std::string item;
                        a.tag = type(item);
                        if (a.tag == 5 || !args.insert(a.name).second)
                            at.fail("SCHEMA_COMMAND: scalar, uniquely named arguments required");
                        c.args.push_back(a);
                        if (take(")"))
                            break;
                        expect(",");
                    }
                if (take("when"))
                    c.whenName = name();
                if (take("busy"))
                    c.busyName = name();
                if (!m.commands.emplace(c.name, c).second)
                    at.fail("SCHEMA_DUPLICATE: command " + c.name);
            }
            else
            {
                Property p;
                p.location = at;
                if (take("rw"))
                    p.access = 3;
                else
                    expect("r");
                p.name = name();
                expect(":");
                p.tag = type(p.itemName);
                if (!m.properties.emplace(p.name, p).second)
                    at.fail("SCHEMA_DUPLICATE: property " + p.name);
            }
            take(";");
        }
        if (!schema.models.emplace(m.name, m).second)
            m.location.fail("SCHEMA_DUPLICATE: model " + m.name);
    }
    void file(const std::filesystem::path &path, unsigned depth)
    {
        auto canonical = std::filesystem::weakly_canonical(path);
        auto key = canonical.generic_string();
        require(depth <= 64, "LIMIT: schema import depth");
        if (active.count(key))
            throw Error(key + ":1:1: SCHEMA_CYCLE: cyclic import");
        if (loaded.count(key))
            return; // A shared dependency in a diamond is one declaration.
        std::error_code ioError;
        auto length = std::filesystem::file_size(canonical, ioError);
        require(!ioError, key + ":1:1: SCHEMA_IO: cannot read");
        require(length <= max_file, key + ":1:1: LIMIT: schema file");
        active.insert(key);
        std::ifstream f(canonical, std::ios::binary);
        require(bool(f), key + ":1:1: SCHEMA_IO: cannot read");
        lexer = {{std::istreambuf_iterator<char>(f), {}}, key};
        require(lexer.source.size() <= max_file && utf8(lexer.source), key + ":1:1: SCHEMA_UTF8: invalid source");
        if (lexer.source.compare(0, 3, "\xef\xbb\xbf") == 0)
            lexer.pos = 3;
        schema.dependencies.push_back(canonical);
        advance();
        while (!token.text.empty())
        {
            if (take("import"))
            {
                auto t = token;
                if (!t.quoted || t.text.empty() || std::filesystem::path(t.text).is_absolute() ||
                    t.text.find(":") != std::string::npos)
                    t.location.fail("SCHEMA_IMPORT: expected relative path");
                advance();
                take(";");
                Parser child{{}, {}, schema, active, loaded};
                child.file(canonical.parent_path() / t.text, depth + 1);
            }
            else
            {
                expect("model");
                model();
                take(";");
            }
        }
        active.erase(key);
        loaded.insert(key);
    }
};
} // namespace
Schema parseSchema(const std::filesystem::path &path, const std::string &root)
{
    Schema s;
    std::set<std::string> active, loaded;
    Parser p{{}, {}, s, active, loaded};
    p.file(path, 1);
    addBuiltins(s);
    if (!s.models.count(root))
        Location{path.generic_string(), 1, 1}.fail("SCHEMA_MODEL: unknown root model " + root);
    resolveSchema(s, root, true);
    return s;
}
} // namespace oneview
