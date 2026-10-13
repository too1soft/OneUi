#include "emit_rust.h"
#include <cctype>
#include <sstream>
#include <set>

namespace oneview {
namespace {
enum : uint32_t { Bool = 1, I64 = 2, F64 = 3, Utf8 = 4, List = 5 };
std::string symbol(std::string name) {
    for (auto &c : name)
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') c = '_';
    if (name == "self" || name == "Self" || name == "super" || name == "crate") name = "field_" + name;
    return name;
}
std::string upper(std::string name) {
    name = symbol(name);
    for (auto &c : name) c = char(std::toupper(static_cast<unsigned char>(c)));
    return name;
}
std::string pascal(std::string name) {
    std::string result;
    bool first = true;
    for (auto c : name) {
        if (!std::isalnum(static_cast<unsigned char>(c))) { first = true; continue; }
        result += first ? char(std::toupper(static_cast<unsigned char>(c))) : c;
        first = false;
    }
    return result;
}
std::string type(uint32_t tag, bool getter = false) {
    switch (tag) {
    case Bool: return "bool";
    case I64: return "i64";
    case F64: return "f64";
    case Utf8: return getter ? "&str" : "String";
    default: throw Error("Rust emitter: scalar type expected");
    }
}
std::string variant(uint32_t tag) {
    return tag == Bool ? "Bool" : tag == I64 ? "I64" : tag == F64 ? "F64" : "Utf8";
}
std::string decode(uint32_t tag, const std::string &value) {
    return value + (tag == Bool ? ".boolean()?" : tag == I64 ? ".integer()?" :
                    tag == F64 ? ".number()?" : ".text()?.to_owned()");
}
std::string trait(const Model &model) { return symbol(model.name) + "Vm"; }
std::string commands(const Model &model) { return symbol(model.name) + "Command"; }
void emitTrait(std::ostream &out, const Schema &schema, const Model &model) {
    const auto entity = model.name.find('.') == std::string::npos ? symbol(model.name) : pascal(model.name);
    out << "#[derive(Clone, Debug, PartialEq)]\npub enum " << entity << "Command {\n";
    for (const auto &[_, command] : model.commands) {
        out << "    " << pascal(command.name);
        if (!command.args.empty()) {
            out << " { ";
            for (auto &arg : command.args) out << "r#" << symbol(arg.name) << ": " << type(arg.tag) << ", ";
            out << "}";
        }
        out << ",\n";
    }
    out << "}\n";
    if (entity != symbol(model.name))
        out << "pub use self::" << entity << "Command as " << commands(model) << ";\n"
            << "pub use self::" << entity << "Vm as " << trait(model) << ";\n";
    out << "#[allow(non_snake_case)]\npub trait " << entity << "Vm: 'static {\n";
    for (auto &[_, p] : model.properties) {
        if (p.tag == List) {
            out << "    type " << pascal(p.name) << "Item: " << trait(schema.model(p.item)) << ";\n"
                << "    fn r#" << symbol(p.name) << "(&self) -> &oneui::view::StableList<Self::"
                << pascal(p.name) << "Item>;\n";
        } else {
            out << "    fn r#" << symbol(p.name) << "(&self) -> " << type(p.tag, true) << ";\n";
            if (p.access == 3)
                out << "    fn set_" << symbol(p.name) << "(&mut self, value: " << type(p.tag)
                    << ", cx: &oneui::view::VmContext) -> oneui::view::VmResult;\n";
        }
    }
    if (!model.commands.empty())
        out << "    fn invoke(&mut self, command: " << commands(model)
            << ", cx: &oneui::view::VmContext) -> oneui::view::VmResult;\n";
    out << "    fn on_view_connect(&mut self, _cx: &oneui::view::VmContext) -> oneui::view::VmResult { Ok(()) }\n"
        << "    fn on_view_disconnect(&mut self, _cx: &oneui::view::VmContext) {}\n}\n"
        << "#[allow(non_snake_case)]\npub mod " << symbol(model.name) << "_properties {\n";
    for (auto &[_, p] : model.properties)
        out << "    pub const " << upper(p.name) << ": oneui::view::PropertyId = oneui::view::PropertyId::new("
            << p.id << ");\n";
    out << "}\n";
}
void emitModel(std::ostream &out, const Schema &schema, const Model &model) {
    out << "#[allow(non_snake_case, unused_variables)]\nfn model_" << symbol(model.name)
        << "<V: " << trait(model) << ">() -> oneui::view::ModelSpec<V> {\n"
        << "    use oneui::view::{Value, ViewError};\n    oneui::view::ModelSpec {\n"
        << "        schema: SCHEMA_HASH, model: " << model.id << ",\n"
        << "        get: |vm, id| match oneui::view::PropertyId::new(id) {\n";
    for (auto &[_, p] : model.properties)
        if (p.tag != List)
            out << "            " << symbol(model.name) << "_properties::" << upper(p.name) << " => Ok(Value::" << variant(p.tag) << "(vm.r#" << symbol(p.name)
                << "()" << (p.tag == Utf8 ? ".to_owned()" : "") << ")),\n";
    out << "            _ => Err(ViewError::UNKNOWN_ID),\n        },\n"
        << "        set: |vm, id, value, cx| match oneui::view::PropertyId::new(id) {\n";
    for (auto &[_, p] : model.properties) {
        out << "            " << symbol(model.name) << "_properties::" << upper(p.name) << " => ";
        if (p.access == 3 && p.tag != List)
            out << "vm.set_" << symbol(p.name) << "(" << decode(p.tag, "value") << ", cx),\n";
        else out << "Err(ViewError::READ_ONLY),\n";
    }
    out << "            _ => Err(ViewError::UNKNOWN_ID),\n        },\n"
        << "        invoke: |vm, id, args, cx| {\n"
        << "            let command: Result<" << commands(model) << ", ViewError> = match id {\n";
    for (auto &[_, c] : model.commands) {
        out << "            " << c.id << " => {\n                if args.len() != " << c.args.size()
            << " { return Err(ViewError::TYPE); }\n                Ok(" << commands(model)
            << "::" << pascal(c.name);
        if (!c.args.empty()) {
            out << " { ";
            for (size_t i = 0; i < c.args.size(); ++i)
                out << "r#" << symbol(c.args[i].name) << ": " << decode(c.args[i].tag, "args[" + std::to_string(i) + "]") << ", ";
            out << "}";
        }
        out << ")\n            },\n";
    }
    out << "            _ => Err(ViewError::UNKNOWN_ID),\n            };\n";
    if (model.commands.empty())
        out << "            match command { Ok(command) => match command {}, Err(error) => Err(error) }\n";
    else
        out << "            vm.invoke(command?, cx)\n";
    out << "        },\n";
    for (const auto &operation : {"info", "key", "item"}) {
        const bool info = std::string(operation) == "info", key = std::string(operation) == "key";
        out << "        list_" << operation << ": |vm, id" << (info ? "" : ", index") << "| match oneui::view::PropertyId::new(id) {\n";
        for (auto &[_, p] : model.properties) {
            if (p.tag != List) continue;
            out << "            " << symbol(model.name) << "_properties::" << upper(p.name) << " => ";
            if (info) out << "Ok(vm.r#" << symbol(p.name) << "().info()),\n";
            else if (key) out << "vm.r#" << symbol(p.name) << "().key(index).map(str::to_owned).ok_or(ViewError::BAD_KEY),\n";
            else out << "Ok(model_" << symbol(schema.model(p.item).name) << "::<V::" << pascal(p.name)
                << "Item>().bind(vm.r#" << symbol(p.name) << "().get(index).ok_or(ViewError::BAD_KEY)?.clone())),\n";
        }
        out << "            _ => Err(ViewError::UNKNOWN_ID),\n        },\n";
    }
    out << "        connect: V::on_view_connect, disconnect: V::on_view_disconnect,\n    }\n}\n";
}
} // namespace
std::string emitRust(const Image &image, const Bytes &bytes) {
    std::ostringstream out;
    out << "// Generated by oneui-viewc. UI construction is exclusively in the IR runtime.\n"
        << "pub const SCHEMA_HASH: [u8; 32] = [";
    for (auto value : sha256(image.schema.encode())) out << unsigned(value) << ',';
    out << "];\npub const IR: &[u8] = &[\n";
    for (size_t i = 0; i < bytes.size(); ++i) {
        if (i % 24 == 0) out << "    ";
        out << unsigned(bytes[i]) << ',';
        if (i % 24 == 23) out << '\n';
    }
    out << "\n];\n";
    std::set<std::string> names;
    for (auto &[_, model] : image.schema.models) {
        if (!names.insert(symbol(model.name)).second) throw Error("Rust model names collide after qualification");
        if (model.name.find('.') != std::string::npos &&
            pascal(model.name) != symbol(model.name) && !names.insert(pascal(model.name)).second)
            throw Error("Rust model names collide after qualification");
        std::set<std::string> methods{"on_view_connect", "on_view_disconnect"};
        if (!model.commands.empty()) methods.insert("invoke");
        std::set<std::string> constants, variants;
        for (const auto &[name, property] : model.properties) {
            if (!methods.insert(symbol(name)).second || !constants.insert(upper(name)).second ||
                (property.access == 3 && !methods.insert("set_" + symbol(name)).second))
                property.location.fail("RUST_NAME: property names collide in generated trait");
        }
        for (const auto &[name, command] : model.commands) {
            if (!variants.insert(pascal(name)).second)
                command.location.fail("RUST_NAME: command variants collide");
            std::set<std::string> arguments;
            for (const auto &arg : command.args)
                if (!arguments.insert(symbol(arg.name)).second)
                    command.location.fail("RUST_NAME: command arguments collide");
        }
        emitTrait(out, image.schema, model);
    }
    for (auto &[_, model] : image.schema.models) emitModel(out, image.schema, model);
    const auto &root = image.schema.model(image.schema.root);
    out << "pub use " << symbol(root.name) << "_properties::*;\n"
        << "oneui::view::define_view_adapter! {\n    pub fn spec<V: " << trait(root) << ">() {\n"
        << "        ir = IR; model = model_" << symbol(root.name) << "::<V>();\n    }\n}\n";
    return out.str();
}
} // namespace oneview
