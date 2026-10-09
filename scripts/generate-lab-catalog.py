#!/usr/bin/env python3
"""Check public capability ownership and embed the exact compiled example sources."""
import argparse
import json
from pathlib import Path
import re


def validate(root, catalog):
    pages = set(re.findall(r'\{"([a-z]+/[a-z]+)"',
                           (root / 'examples/performance_lab/demo_catalog.cpp').read_text(encoding='utf-8')))
    groups = catalog['groups']
    ids = [group['page'] for group in groups]
    if len(ids) != len(set(ids)) or set(ids) != pages:
        raise ValueError('Capability pages differ from the actual navigation catalog')
    assigned = set()
    for group in groups:
        if not group['interactions'] or not group['tests'] or not group['note']:
            raise ValueError('Missing behavior, test or limitation: ' + group['page'])
        if set(group['languages']) != {'cpp', 'c', 'rust', 'one'}:
            raise ValueError('Missing language coverage: ' + group['page'])
        for path in group['headers'] + group['tests']:
            if not (root / path).is_file():
                raise ValueError('Referenced file is missing: ' + path)
        for header in group['headers']:
            if header in assigned:
                raise ValueError('Ambiguous primary ownership: ' + header)
            assigned.add(header)
        languages = set()
        for example in group['examples']:
            language, path = example['language'], root / example['path']
            if language in languages or language not in {'cpp', 'c', 'rust', 'one'}:
                raise ValueError('Invalid/duplicate example language: ' + group['page'])
            languages.add(language)
            if not path.is_file() or not path.read_text(encoding='utf-8').strip():
                raise ValueError('Missing example: ' + str(path))
            if language == 'rust':
                rust_lib = root / 'bindings/rust/oneui-lab-examples/src/lib.rs'
                if example['path'] not in rust_lib.read_text(encoding='utf-8'):
                    raise ValueError('Rust example is not in the compiled crate: ' + str(path))
            elif language in {'cpp', 'c'} and '/snippets/' + language + '/' not in str(example['path']):
                raise ValueError('Example is outside the compiled source directory')
            elif language == 'one':
                template_test = root / 'examples/performance_lab/snippets/template_check.cpp'
                if 'build_' + path.stem + '(' not in template_test.read_text(encoding='utf-8'):
                    raise ValueError('Template is not instantiated by the compilation test: ' + str(path))
    public = {path.relative_to(root).as_posix() for path in (root / 'include/oneui').rglob('*.h')}
    if assigned != public:
        raise ValueError('Unassigned public headers: ' + ', '.join(sorted(public - assigned)) +
                         '; unknown headers: ' + ', '.join(sorted(assigned - public)))
    return groups


def generate(root, groups, output):
    lines = ['// Generated: edit capabilities.json or the compiled snippet source instead.',
             '#include "code_examples.hpp"', 'namespace lab {',
             'std::vector<CodeExample> compiledExamples(const std::string &page) {']
    names = {'cpp': 'C++', 'c': 'C · UTF-8', 'rust': 'Rust', 'one': '.one'}
    for group in groups:
        if not group['examples']:
            continue
        lines.append('if (page == ' + json.dumps(group['page']) + ') return {')
        for example in group['examples']:
            source = (root / example['path']).read_text(encoding='utf-8')
            if ')OneUiSample"' in source:
                raise ValueError('Example contains the reserved raw literal delimiter')
            lines.append('{L"' + names[example['language']] + '", LR"OneUiSample(' +
                         source + ')OneUiSample"},')
        lines.append('};')
    lines += ['return {};', '}', '} // namespace lab', '']
    text = '\n'.join(lines)
    output.parent.mkdir(parents=True, exist_ok=True)
    if not output.exists() or output.read_text(encoding='utf-8') != text:
        output.write_bytes(text.encode('utf-8'))


def markdown(groups, output):
    lines = ['# OneUI 试验台能力目录', '',
             '本表由 `examples/performance_lab/capabilities.json` 生成。'
             '头文件归属和示例编译通过不等于所有交互、语言入口及原生平台已验收。', '',
             '构建试验台会检查 C++、UTF-8 C、Rust 示例，并实例化 `.one` 模板。'
             '每页仍有迁移验收项；`generate-lab-catalog.py --strict` 会拒绝将当前目录视为发布完成。', '',
             '| 页面 | 能力组 | C++ | C · UTF-8 | Rust | .one | 迁移 |',
             '| --- | --- | --- | --- | --- | --- | --- |']
    for group in groups:
        examples = {value['language']: value['path'] for value in group['examples']}
        cells = ['[源码](../' + examples[lang] + ')' if lang in examples else '待补 / 见限制'
                 for lang in ['cpp', 'c', 'rust', 'one']]
        lines.append('| `' + group['page'] + '` | ' + group['name'] + ' | ' + ' | '.join(cells) +
                     ' | ' + ('完成' if group['migration'] == 'complete' else '迁移中') + ' |')
    for group in groups:
        lines += ['', '## ' + group['name'] + ' · `' + group['page'] + '`', '',
                  '关键行为：' + '；'.join(group['interactions']) + '。', '', group['note'], '',
                  '测试入口：' + '、'.join('[' + Path(p).name + '](../' + p + ')' for p in group['tests']) + '。', '',
                  '公开头文件：' + '、'.join('[' + p.removeprefix('include/oneui/') + '](../' + p + ')'
                                             for p in group['headers']) + '。']
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(('\n'.join(lines) + '\n').encode('utf-8'))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--output', type=Path)
    parser.add_argument('--markdown', type=Path, help='Update the repository capability matrix')
    parser.add_argument('--strict', action='store_true', help='Release gate: reject incomplete migrations')
    args = parser.parse_args()
    root = args.root.resolve()
    catalog = json.loads((root / 'examples/performance_lab/capabilities.json').read_text(encoding='utf-8'))
    groups = validate(root, catalog)
    if args.output:
        generate(root, groups, args.output)
    if args.markdown:
        markdown(groups, args.markdown)
    pending = [group['page'] for group in groups
               if group['migration'] != 'complete' or not group['examples']]
    print(f'{len(groups)} pages, {sum(len(g["headers"]) for g in groups)} public headers assigned; '
          f'{sum(len(g["examples"]) for g in groups)} source examples; {len(pending)} incomplete pages')
    if args.strict and pending:
        raise SystemExit('Release coverage is incomplete: ' + ', '.join(pending))


if __name__ == '__main__':
    main()
