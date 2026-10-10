#pragma once
#include "oneui/ui_template_support.h"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>

namespace fs = std::filesystem;
using namespace oneui::ui::syntax;
struct Attribute
{
    std::string value;
    int line = 1, column = 1, endLine = 1, endColumn = 1;
};
struct Node
{
    std::string tag, text;
    std::map<std::string, Attribute> attrs;
    std::vector<Node> children;
    int line = 1, column = 1, endLine = 1, endColumn = 1;
};
struct Parser
{
    std::string source, file;
    std::size_t pos = 0;
    unsigned depth = 0, count = 0;
    int line() const
    {
        return 1 + int(std::count(source.begin(), source.begin() + pos, '\n'));
    }
    int column() const
    {
        auto p = source.rfind('\n', pos ? pos - 1 : 0);
        return int(p == std::string::npos ? pos + 1 : pos - p);
    }
    [[noreturn]] void fail(std::string message) const
    {
        throw std::runtime_error(file + ":" + std::to_string(line()) + ":" + std::to_string(column()) + ": " + message);
    }
    bool starts(std::string s) const
    {
        return source.compare(pos, s.size(), s) == 0;
    }
    void space()
    {
        while (pos < source.size() && std::isspace((unsigned char)source[pos]))
            ++pos;
    }
    std::string name()
    {
        auto begin = pos;
        while (pos < source.size() && (std::isalnum((unsigned char)source[pos]) ||
                                       std::string("_-:@.").find(source[pos]) != std::string::npos))
            ++pos;
        if (begin == pos)
            fail("Expected name");
        return source.substr(begin, pos - begin);
    }
    Node node()
    {
        if (++depth > 64 || ++count > 100000)
            fail("LIMIT: template tree");
        space();
        auto startColumn = column();
        if (!starts("<"))
            fail("Expected opening tag");
        ++pos;
        Node n;
        n.line = line();
        n.column = startColumn;
        n.tag = name();
        space();
        while (!starts(">") && !starts("/>"))
        {
            Attribute a;
            a.line = line();
            a.column = column();
            auto key = name();
            space();
            if (starts("="))
            {
                ++pos;
                space();
                if (!starts("\"") && !starts("'"))
                    fail("Attribute needs quoted value");
                char quote = source[pos++];
                auto begin = pos;
                while (pos < source.size() && source[pos] != quote)
                    ++pos;
                if (pos == source.size())
                    fail("Unclosed attribute");
                a.value = source.substr(begin, pos - begin);
                ++pos;
            }
            else if (key != "scoped" && key != "v-else")
                fail("Attribute needs a value");
            a.endLine = line();
            a.endColumn = column();
            if (!n.attrs.emplace(key, a).second)
                fail("Duplicate attribute: " + key);
            space();
        }
        if (starts("/>"))
        {
            pos += 2;
            n.endLine = line();
            n.endColumn = column();
            --depth;
            return n;
        }
        ++pos;
        if (n.tag == "style")
        {
            auto end = source.find("</style>", pos);
            if (end == source.npos)
                fail("Unclosed style");
            n.text = source.substr(pos, end - pos);
            pos = end + 8;
            n.endLine = line();
            n.endColumn = column();
            --depth;
            return n;
        }
        while (!starts("</"))
        {
            if (pos >= source.size())
                fail("Unclosed " + n.tag);
            if (starts("<!--"))
            {
                auto end = source.find("-->", pos);
                if (end == source.npos)
                    fail("Unclosed comment");
                pos = end + 3;
            }
            else if (starts("<"))
                n.children.push_back(node());
            else
                n.text += source[pos++];
        }
        pos += 2;
        auto close = name();
        space();
        if (close != n.tag || !starts(">"))
            fail("Mismatched closing tag");
        ++pos;
        n.text = trim(n.text);
        n.endLine = line();
        n.endColumn = column();
        --depth;
        return n;
    }
    std::vector<Node> all()
    {
        std::vector<Node> nodes;
        if (starts("\xEF\xBB\xBF"))
            pos = 3;
        space();
        while (pos < source.size())
        {
            if (starts("<!--"))
            {
                auto end = source.find("-->", pos);
                if (end == source.npos)
                    fail("Unclosed comment");
                pos = end + 3;
            }
            else
                nodes.push_back(node());
            space();
        }
        return nodes;
    }
};
inline std::string read(fs::path path)
{
    std::error_code ioError;
    auto length = fs::file_size(path, ioError);
    if (ioError)
        throw std::runtime_error(path.generic_string() + ":1:1: Cannot read source");
    if (length > 64ull * 1024 * 1024)
        throw std::runtime_error(path.generic_string() + ":1:1: LIMIT: file");
    std::ifstream f(path, std::ios::binary);
    if (!f)
        throw std::runtime_error("Cannot read " + path.string());
    return {std::istreambuf_iterator<char>(f), {}};
}
inline std::string quote(const std::string &s)
{
    std::ostringstream out;
    out << std::quoted(s);
    auto result = out.str();
    std::string escaped;
    for (char c : result)
    {
        if (c == '\n')
            escaped += "\\n";
        else if (c == '\r')
            escaped += "\\r";
        else
            escaped += c;
    }
    return escaped;
}
