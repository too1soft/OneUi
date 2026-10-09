//! Emit typed C function-pointer assignments from the expanded Rust AST.
//! No function is called. Incompatible parameters, returns and callbacks are
//! compiler errors, including when Windows import thunks have unequal addresses.
use std::{collections::BTreeSet, fmt::Write, path::Path};
use syn::{FnArg, GenericArgument, Item, PathArguments, ReturnType, Type};

struct Generator {
    output: String,
    next_type: usize,
    functions: BTreeSet<String>,
    symbol_checks: Vec<String>,
}

impl Generator {
    fn alias(&mut self, declaration: impl FnOnce(&str) -> String) -> String {
        let name = format!("oneui_rust_type_{}", self.next_type);
        self.next_type += 1;
        writeln!(self.output, "typedef {};", declaration(&name)).unwrap();
        name
    }

    fn return_type(&mut self, output: &ReturnType) -> String {
        match output {
            ReturnType::Default => "void".into(),
            ReturnType::Type(_, ty) => self.ty(ty),
        }
    }

    fn ty(&mut self, ty: &Type) -> String {
        match ty {
            Type::Path(path) if path.qself.is_none() => {
                let segment = path.path.segments.last().unwrap();
                let name = segment.ident.to_string();
                if name == "Option" {
                    let PathArguments::AngleBracketed(args) = &segment.arguments else {
                        panic!("unsupported Option");
                    };
                    assert_eq!(args.args.len(), 1);
                    let Some(GenericArgument::Type(inner @ Type::BareFn(_))) = args.args.first()
                    else {
                        panic!("only nullable function pointers are supported");
                    };
                    return self.ty(inner);
                }
                assert!(
                    matches!(segment.arguments, PathArguments::None),
                    "unexpected generic ABI type {name}"
                );
                match name.as_str() {
                    "c_void" => "void",
                    "c_char" => "char",
                    "c_float" | "f32" => "float",
                    "c_double" | "f64" => "double",
                    "c_int" | "i32" => "int",
                    "c_uint" | "u32" => "unsigned int",
                    "c_ushort" | "u16" => "unsigned short",
                    "c_short" | "i16" => "short",
                    "c_uchar" | "u8" => "unsigned char",
                    "c_schar" | "i8" => "signed char",
                    "c_ulonglong" => "unsigned long long",
                    "c_longlong" => "long long",
                    "i64" => "int64_t",
                    "u64" => "uint64_t",
                    "usize" => "size_t",
                    "isize" => "intptr_t",
                    "WChar" => "wchar_t",
                    _ if name.starts_with("OneUi") => return name,
                    _ => panic!("unsupported ABI type {name}"),
                }
                .into()
            }
            Type::Ptr(pointer) => {
                let inner = self.ty(&pointer.elem);
                let qualifier = if pointer.const_token.is_some() {
                    "const "
                } else {
                    ""
                };
                self.alias(|name| format!("{qualifier}{inner} *{name}"))
            }
            Type::BareFn(function) => {
                assert_eq!(
                    function
                        .abi
                        .as_ref()
                        .and_then(|abi| abi.name.as_ref())
                        .map(|name| name.value())
                        .as_deref(),
                    Some("C")
                );
                assert!(
                    function.variadic.is_none(),
                    "variadic callbacks need an explicit ABI policy"
                );
                let result = self.return_type(&function.output);
                let args = function
                    .inputs
                    .iter()
                    .map(|arg| self.ty(&arg.ty))
                    .collect::<Vec<_>>();
                let args = if args.is_empty() {
                    "void".into()
                } else {
                    args.join(", ")
                };
                self.alias(|name| format!("{result} (*{name})({args})"))
            }
            Type::Tuple(tuple) if tuple.elems.is_empty() => "void".into(),
            _ => panic!("unsupported Rust type at the C ABI boundary"),
        }
    }

    fn items(&mut self, items: &[Item]) {
        for item in items {
            match item {
                Item::Type(alias) if alias.ident.to_string().starts_with("OneUi") => {
                    if let Type::Path(path) = alias.ty.as_ref() {
                        if path
                            .path
                            .segments
                            .last()
                            .is_some_and(|segment| segment.ident == "Option")
                        {
                            let expected = self.ty(&alias.ty);
                            let name = &alias.ident;
                            writeln!(self.output, "{expected} check_callback_{name}({name} value) {{ return value; }}").unwrap();
                        }
                    }
                }
                Item::Mod(module) => {
                    if let Some((_, items)) = &module.content {
                        self.items(items);
                    }
                }
                Item::ForeignMod(block) => {
                    assert_eq!(
                        block.abi.name.as_ref().map(|n| n.value()).as_deref(),
                        Some("C")
                    );
                    for item in &block.items {
                        let syn::ForeignItem::Fn(function) = item else {
                            panic!("unexpected foreign item");
                        };
                        let name = function.sig.ident.to_string();
                        assert!(
                            name.starts_with("oneui_") && self.functions.insert(name.clone()),
                            "unexpected or duplicated ABI function {name}"
                        );
                        assert!(function.sig.variadic.is_none());
                        let result = self.return_type(&function.sig.output);
                        let args = function
                            .sig
                            .inputs
                            .iter()
                            .map(|arg| {
                                let FnArg::Typed(arg) = arg else {
                                    panic!("ABI function cannot have self");
                                };
                                self.ty(&arg.ty)
                            })
                            .collect::<Vec<_>>();
                        let args = if args.is_empty() {
                            "void".into()
                        } else {
                            args.join(", ")
                        };
                        let function_type =
                            self.alias(|alias| format!("{result} (*{alias})({args})"));
                        writeln!(
                            self.output,
                            "{function_type} check_signature_{name}(void) {{ return &{name}; }}"
                        )
                        .unwrap();
                        // Keep every symbol referenced by the executed helper,
                        // even with section GC or release optimization. Volatile
                        // typed pointers avoid comparing Windows import thunks.
                        self.symbol_checks.push(format!(
                            "    {function_type} volatile symbol_{name} = check_signature_{name}();\n    if (!symbol_{name}) return 0;"
                        ));
                    }
                }
                _ => {}
            }
        }
    }
}

pub fn generate(source: &Path, output: &Path, manifest: &Path, target: &str) -> usize {
    let expanded =
        ctest::expand(source, &[], target.to_string()).expect("expand actual FFI declarations");
    let tree = syn::parse_file(&expanded).expect("parse expanded Rust FFI");
    let mut generator = Generator {
        output: "#include <stdint.h>\n#include <oneui/oneui_c_api.h>\n".into(),
        next_type: 0,
        functions: BTreeSet::new(),
        symbol_checks: Vec::new(),
    };
    generator.items(&tree.items);
    let expected = std::fs::read_to_string(manifest)
        .unwrap()
        .lines()
        .filter(|line| line.starts_with("oneui_"))
        .map(str::to_string)
        .collect::<BTreeSet<_>>();
    assert_eq!(
        generator.functions, expected,
        "every supported ABI symbol must have a typed check"
    );
    let count = generator.functions.len();
    writeln!(
        generator.output,
        "size_t oneui_abi_checked_signatures(void) {{\n{}\n    return {count};\n}}",
        generator.symbol_checks.join("\n")
    )
    .unwrap();
    std::fs::write(output, generator.output).unwrap();
    count
}
