from support import *
from golden_reference import basic_image


class BinaryTests(CompilerCase):
    def test_independent_golden_bytes(self):
        frozen = bytes.fromhex((FIXTURES / 'Basic.oneview.hex').read_text())
        self.assertEqual(frozen, basic_image(), 'reference changed: review format before changing golden')
        actual = self.compile((FIXTURES / 'Basic.one').read_text().strip(),
                              (FIXTURES / 'Basic.oneschema').read_text(), raw=True)
        self.assertEqual(actual, frozen)
        self.verify_bytes(frozen, error=None)

    def test_reproducible_across_absolute_paths(self):
        first = self.compile()
        previous = self.root
        self.root = self.root / 'different build root'
        self.root.mkdir()
        second = self.compile()
        self.assertEqual(first, second)
        self.assertNotIn(str(previous).encode(), second)

    def test_header_rejections(self):
        original = bytearray(self.compile())
        for offset, fmt, value, code in [
            (0, '<B', 0, 'magic'), (8, '<H', 2, 'VERSION'),
            (12, '<I', 127, 'header'), (16, '<Q', 1, 'length'),
            (24, '<Q', 0xfffffffffffffff0, 'directory'), (32, '<I', 0xffffffff, 'directory'),
            (36, '<I', 1, 'VERSION'), (40, '<I', 9, 'VERSION'),
            (44, '<I', 0xffffffff, 'index'), (112, '<I', 2, 'VERSION'),
            (116, '<I', 9, 'root'), (120, '<Q', 1, 'reserved'),
        ]:
            with self.subTest(offset=offset):
                data = original.copy()
                struct.pack_into(fmt, data, offset, value)
                self.verify_bytes(data, code)

    def test_truncation_at_every_boundary(self):
        original = self.compile()
        positions = {0, 7, 127, 128, len(original) - 1}
        for _, _, start, size, _, _ in directory(original).values():
            positions.update((start, start + size))
        for length in sorted(positions - {len(original)}):
            with self.subTest(length=length):
                self.verify_bytes(original[:length], 'BAD_IR')

    def test_directory_and_strings(self):
        original = bytearray(self.compile())
        cases = [
            (128, '<I', 99, 'required'), (160, '<I', 1, 'duplicate'),
            (128 + 4, '<I', 2, 'flags'), (128 + 8, '<Q', 0, 'range'),
            (128 + 8, '<Q', 705, 'alignment'), (128 + 16, '<Q', 0xffffffffffffffff, 'range'),
            (128 + 3 * 32 + 28, '<I', 36, 'stride'),
            (128 + 4 * 32 + 24, '<I', 0xffffffff, 'count'),
        ]
        for at, fmt, val, error in cases:
            with self.subTest(at=at, val=val):
                data = original.copy()
                struct.pack_into(fmt, data, at, val)
                self.verify_bytes(data, error)
        start = directory(original)[1][2]
        for at, val in [(start, 1), (start + 8, 1), (start + 16, 0xffffffffffffffff)]:
            data = original.copy()
            struct.pack_into('<Q', data, at, val)
            self.verify_bytes(data, 'BAD_IR')
        data = original.copy()
        data[start + 40] = 0xff
        self.verify_bytes(data, 'UTF-8')

    def test_hash_and_semantic_corruption(self):
        original = bytearray(self.compile())
        data = original.copy()
        data[directory(data)[2][2] + 30] ^= 1
        self.verify_bytes(data, 'hash')
        mutations = [
            (4, 4, 9999, 'index'), (4, 8, 99, 'model'),
            (5, 0, 5, 'opcode'), (5, 8, 0, 'parent'),
            (5, 44, 9999, 'string'), (9, 4, 99, 'property'),
            (9, 8, 99, 'property'), (9, 12, (1 << 16) | 1, 'lexical'),
            (9, 12, 3 << 16, 'binding'), (9, 20, 1, 'binding'),
            (17, 4, 0, 'coordinates'), (17, 20, 0, 'cycle'),
        ]
        for kind, offset, val, error in mutations:
            with self.subTest(kind=kind, offset=offset):
                data = original.copy()
                struct.pack_into('<I', data, directory(data)[kind][2] + offset, val)
                self.verify_bytes(data, error)

    def test_schema_validation_after_rehash(self):
        data = bytearray(self.compile())
        at = directory(data)[2][2]
        struct.pack_into('<I', data, at + 16, 2)  # forged model ID
        data[48:80] = hashlib.sha256(section(data, 2)).digest()
        self.verify_bytes(data, 'model')
        data = bytearray(self.compile())
        at = directory(data)[3][2]
        data[at + 20] ^= 1
        data[80:112] = hashlib.sha256(section(data, 3)).digest()
        self.verify_bytes(data, 'contract')

    def test_optional_section_forward_compatibility(self):
        original = bytearray(self.compile())
        # Change SMAP into an unknown optional extension and clear every span.
        for kind, columns in [(4, [6]), (5, [10]), (9, [6])]:
            _, _, start, _, count, stride = directory(original)[kind]
            for i in range(count):
                for col in columns:
                    struct.pack_into('<I', original, start + i * stride + col * 4, 0xffffffff)
        struct.pack_into('<I', original, 128 + 16 * 32, 1000)
        struct.pack_into('<H', original, 10, 9)
        self.verify_bytes(original, error=None)
        struct.pack_into('<I', original, 128 + 16 * 32 + 4, 1)
        self.verify_bytes(original, 'required')

    def test_tree_cycle_and_unowned_edge(self):
        original = bytearray(self.compile('<Column><Text/></Column>'))
        at = directory(original)[6][2]
        struct.pack_into('<I', original, at + 4, 0)
        self.verify_bytes(original, 'BAD_IR')

    def test_every_record_family_rejects_invalid_fields(self):
        from test_diagnostics import SCHEMA
        self.write('Child.one', '<template><Column><slot><Text text="fallback"/></slot></Column></template>')
        source = '''<import name="Child" src="Child.one"/>
<template view-model="Basic"><Column>
<Switch checked="true"/><Progress value="0.5"/>
<Text :text="title" v-if="flag"/><Tabs :items="tabs" @close="close"/>
<Text v-for="item in rows" :key="item.id" :text="item.text"/>
<Child><Text text="supplied"/></Child><NativeHost host="terminal"/>
</Column></template><style scoped>Text.x {color:#ffffff;}</style>'''
        original = bytearray(self.compile(source, SCHEMA, raw=True))
        # Mutate independently encoded fields without touching contract hashes.
        for kind, offset, value in [
            (4, 28, 1), (5, 0, 0x10001), (6, 0, 0xffffffff),
            (7, 4, 1), (8, 8, 0xffffffff), (9, 28, 1),
            (10, 8, 0xffffffff), (11, 0, 9), (12, 20, 1),
            (13, 12, 1 << 16), (14, 4, 0), (15, 16, 1),
            (16, 20, 1), (17, 16, 0), (18, 8, 2),
        ]:
            with self.subTest(section=kind, field=offset):
                mutated = original.copy()
                struct.pack_into('<I', mutated, directory(mutated)[kind][2] + offset, value)
                self.verify_bytes(mutated)

    def test_duplicate_property_ids_after_rehash(self):
        data = bytearray(self.compile(schema='model Basic {r title:utf8 r unused:bool}'))
        start = directory(data)[2][2]
        record = struct.pack('<II', 2, 6) + b'unused'
        at = data.index(record, start)
        struct.pack_into('<I', data, at, 1)
        data[48:80] = hashlib.sha256(section(data, 2)).digest()
        self.verify_bytes(data, 'BAD_IR')


if __name__ == '__main__':
    unittest.main()
