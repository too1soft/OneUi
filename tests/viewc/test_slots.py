"""Single-root contract tests, loaded by the existing diagnostics CTest entry."""
from support import *


class SlotTests(CompilerCase):
    def source(self, content='<Text slot="body" grow="1" basis="0"/>', marker=' root="single"', fallback=''):
        self.write('Child.one', '<template><Column><slot name="body"' + marker + '>' +
                   fallback + '</slot></Column></template>')
        return ('<import name="Child" src="Child.one"/>\n<template view-model="Basic">\n'
                '<Child>' + content + '</Child></template>')

    def slot_records(self, data):
        return list(struct.iter_unpack('<8I', section(data, 15)))

    def root_component(self, data, template):
        nodes, templates = section(data, 5), section(data, 4)
        root = u32(templates, template * 32 + 4)
        return u32(nodes, root * 48 + 4)

    def test_single_root_and_default_wrapper(self):
        single = self.compile(self.source(), raw=True)
        self.assertEqual(struct.unpack_from('<HH', single, 8), (1, 1))
        slots = self.slot_records(single)
        self.assertEqual([r[3] for r in slots], [2, 3])
        # Contracts sorted by name: Column=0, Text=1. No supplied Column.
        self.assertEqual(self.root_component(single, slots[1][2]), 1)
        wrapped = self.compile(self.source(marker=''), raw=True)
        self.assertEqual(struct.unpack_from('<HH', wrapped, 8), (1, 0))
        slots = self.slot_records(wrapped)
        self.assertEqual([r[3] for r in slots], [0, 1])
        self.assertEqual(self.root_component(wrapped, slots[1][2]), 0)

    def test_zero_two_roots_and_call_location(self):
        for content, count in [('', 0), ('<Text slot="body"/><Text slot="body"/>', 2)]:
            with self.subTest(count=count):
                result = self.compile(self.source(content), error='SLOT_ROOT:', raw=True)
                self.assertIn('Basic.one:3:1:', result.stderr)
                self.assertIn(f'got {count}', result.stderr)
        self.compile('<Column><slot root="single"/></Column>', error='SLOT_ROOT:')

    def test_fallback_contract(self):
        data = self.compile(self.source('', fallback='<Text grow="1" basis="0"/>'), raw=True)
        self.assertEqual(self.root_component(data, self.slot_records(data)[0][2]), 1)
        for content in ['', '<Text slot="body"/>']:
            result = self.compile(self.source(content, fallback='<Text/><Text/>'), error='SLOT_ROOT:', raw=True)
            self.assertIn('Basic.one:3:1:', result.stderr)
        self.compile(self.source('', marker='', fallback='<Text/><Text/>'), raw=True)

    def test_static_callee_only_marker(self):
        self.compile(self.source(marker=' root="multiple"'), error='SLOT_ROOT:', raw=True)
        self.compile(self.source(marker=' :root="title"'), error='SLOT_NAME:', raw=True)
        self.compile(self.source().replace('<Child>', '<Child root="single">'), error='COMPONENT_PROPERTY', raw=True)

    def test_slot_binary_flags_version_and_roundtrip(self):
        data = self.compile(self.source(), raw=True)
        start = directory(data)[15][2]
        for offset, value, error in [(start + 12, 4, 'slot flags'),
                                     (start + 32 + 12, 1, 'slot contract mismatch')]:
            corrupt = bytearray(data)
            struct.pack_into('<I', corrupt, offset, value)
            self.verify_bytes(corrupt, error)
        corrupt = bytearray(data)
        struct.pack_into('<H', corrupt, 10, 0)
        self.verify_bytes(corrupt, 'VERSION: single-root slot')
        copy = self.root / 'roundtrip.oneview'
        self.run_cli('--roundtrip', self.root / 'Basic.oneview', '--output', copy)
        self.assertEqual(data, copy.read_bytes())

    def test_cpp_and_rust_emit_identical_single_root_ir(self):
        expected = self.compile(self.source(), raw=True)
        for language in ['cpp', 'rust']:
            ir = self.root / f'{language}.oneview'
            self.run_cli('--emit', language, '--input', self.root / 'Basic.one',
                         '--schema', self.root / 'Basic.oneschema', '--name', 'Basic',
                         '--output', self.root / f'{language}.generated', '--ir-output', ir)
            self.assertEqual(expected, ir.read_bytes())
