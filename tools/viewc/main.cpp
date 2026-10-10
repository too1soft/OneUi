#include "compiler.h"
#include "emit_rust.h"
#include "emit_cpp.h"

namespace
{
void output(const fs::path &path, const std::string &bytes)
{
    if (!path.parent_path().empty())
        fs::create_directories(path.parent_path());
    std::ofstream f(path, std::ios::binary);
    f.write(bytes.data(), std::streamsize(bytes.size()));
    if (!f)
        throw std::runtime_error("Cannot write output: " + path.generic_string());
}
std::string escapeDep(const std::string &text)
{
    std::string s;
    for (char c : text)
    {
        if (c == '$')
            s += '$';
        if (c == ' ' || c == '#' || c == ':')
            s += '\\';
        s += c;
    }
    return s;
}
} // namespace
int main(int argc, char **argv)
{
    try
    {
        std::map<std::string, std::string> args;
        const std::set<std::string> options = {"--input", "--output", "--name",   "--vm-header", "--depfile",
                                               "--emit",  "--schema", "--verify", "--roundtrip", "--ir-output"};
        for (int i = 1; i < argc; i += 2)
        {
            if (i + 1 >= argc || !options.count(argv[i]) || !args.emplace(argv[i], argv[i + 1]).second)
                throw std::runtime_error("CLI: unknown, duplicate or unpaired option");
        }
        if (args.count("--roundtrip"))
        {
            if (args.size() != 2 || !args.count("--output"))
                throw std::runtime_error("CLI: --roundtrip FILE --output FILE");
            auto data = read(args.at("--roundtrip"));
            auto bytes = oneview::writeImage(oneview::readImage(oneview::Bytes(data.begin(), data.end())));
            output(args.at("--output"), std::string(bytes.begin(), bytes.end()));
            return 0;
        }
        if (args.count("--verify"))
        {
            if (args.size() != 1)
                throw std::runtime_error("CLI: --verify FILE is standalone");
            auto data = read(args.at("--verify"));
            oneview::Bytes bytes(data.begin(), data.end());
            auto image = oneview::readImage(bytes);
            std::cout << "OneView 1.0 nodes=" << image.count(oneview::NODE)
                      << " templates=" << image.count(oneview::TMPL)
                      << " schema=" << oneview::hex(oneview::sha256(image.schema.encode())) << "\n";
            return 0;
        }
        if (!args.count("--input") || !args.count("--output"))
            throw std::runtime_error("Usage: oneui-viewc --input page.one --output page.oneview --emit ir --schema "
                                     "page.oneschema [--name Page]");
        auto name = args.count("--name") ? args.at("--name") : fs::path(args.at("--input")).stem().string();
        if (!std::regex_match(name, std::regex("[A-Za-z_][A-Za-z0-9_]*")))
            throw std::runtime_error("Invalid generated name");
        auto emit = args.count("--emit") ? args.at("--emit") : "cpp";
        std::vector<fs::path> dependencies;
        std::string generated;
        if (emit == "ir" || emit == "rust" || emit == "cpp")
        {
            if (!args.count("--schema") || (emit != "cpp" && args.count("--vm-header")))
                throw std::runtime_error("CLI: ir requires --schema and does not accept --vm-header");
            oneview::ViewCompiler compiler;
            auto image = compiler.compile(args.at("--input"), args.at("--schema"), name);
            auto bytes = oneview::writeImage(image);
            if (emit == "cpp") generated = oneview::emitCpp(image, bytes, name, args["--vm-header"],
                fs::weakly_canonical(args.at("--input")).parent_path());
            else if (emit == "rust") generated = oneview::emitRust(image, bytes);
            else generated.assign(bytes.begin(), bytes.end());
            if (args.count("--ir-output"))
                output(args.at("--ir-output"), std::string(bytes.begin(), bytes.end()));
            dependencies = compiler.dependencies;
        }
        else
            throw std::runtime_error("CLI: supported emitters: ir, rust, cpp");
        output(args.at("--output"), generated);
        if (args.count("--depfile"))
        {
            std::string dep = escapeDep(fs::path(args.at("--output")).generic_string()) + ":";
            std::set<std::string> seen;
            for (auto &path : dependencies)
                if (seen.insert(path.generic_string()).second)
                    dep += " " + escapeDep(path.generic_string());
            output(args.at("--depfile"), dep + "\n");
        }
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
