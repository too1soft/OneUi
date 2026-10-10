from support import *


SCHEMA = '''model Basic {
 r title:utf8 rw input:utf8 r flag:bool rw enabled:bool rw index:i64
 rw ratio:f64 r rows:list<Item> r tabs:list<oneui.v1.TabItem>
 command click() command close(key:utf8) command wrong(key:i64)
} model Item key id { r id:utf8 r text:utf8 r children:list<Child> command select() }
model Child key id { r id:utf8 r name:utf8 }'''


class DiagnosticTests(CompilerCase):
    def test_binding_and_command_errors(self):
        cases = [
            ('<Unknown/>', 'COMPONENT:'), ('<Text :text="missing"/>', 'VM_PROPERTY'),
            ('<Text :text="flag"/>', 'BIND_TYPE'), ('<Input v-model="title"/>', 'MODEL_READ_ONLY'),
            ('<Switch v-model="input"/>', 'BIND_TYPE'), ('<Text v-model="input"/>', 'MODEL_TYPE'),
            ('<Text @click="click"/>', 'COMPONENT_EVENT'), ('<Button @click="close"/>', 'COMMAND_SIGNATURE'),
            ('<Tabs @close="wrong"/>', 'COMMAND_SIGNATURE'), ('<Button @click="missing"/>', 'VM_COMMAND'),
            ('<Text :text="input + title"/>', 'EXPRESSION'), ('<Text :class="title"/>', 'BIND_STATIC'),
            ('<Text text="a" :text="title"/>', 'PROPERTY_DUPLICATE'), ('<Text text="a">b</Text>', 'PROPERTY_DUPLICATE'),
            ('<Input text="a" v-model="input"/>', 'PROPERTY_DUPLICATE'),
            ('<Text visible="yes"/>', 'STATIC_TYPE'), ('<Text grow="-1"/>', 'STATIC_TYPE'),
            ('<Text min="2" max="1"/>', 'STATIC_RANGE'), ('<Select selectedIndex="2147483648"/>', 'STATIC_TYPE'),
            ('<Icon symbol="not-an-icon"/>', 'STATIC_ICON'), ('<Button variant="wat"/>', 'STATIC_ENUM'),
            ('<Text v-if="title"/>', 'CONDITION_TYPE'), ('<Text v-else/>', 'CONDITION:'),
            ('<Column><Text v-if="flag"/><Text/><Text v-else/></Column>', 'CONDITION:'),
            ('<Column><Text v-if="flag" v-else/></Column>', 'CONDITION:'),
            ('<Text><Text/></Text>', 'CHILDREN'), ('<Scroll/>', 'CHILDREN'),
            ('<Workspace><TitleBar/></Workspace>', 'CHILDREN'),
            ('<NativeHost/>', 'HOST_NAME'), ('<Column><NativeHost host="x"/><NativeHost host="x"/></Column>', 'HOST_NAME'),
            ('<Column><Text ref="same"/><Text view-id="same"/></Column>', 'NODE_ID'),
            ('<Tabs :items="rows"/>', 'BIND_RECORD'), ('<Text>{{title}}</Text>', 'EXPRESSION'),
        ]
        for one, code in cases:
            with self.subTest(one=one):
                self.compile(one, SCHEMA, error=code)

    def test_list_errors(self):
        cases = [
            ('<Text v-for="item in rows"/>', 'REPEAT:'),
            ('<Text v-for="item in title" :key="item.id"/>', 'REPEAT_TYPE'),
            ('<Text v-for="item in rows" :key="item.text"/>', 'REPEAT_KEY'),
            ('<Text v-for="item in rows" :key="item.id" v-if="flag"/>', 'REPEAT:'),
            ('<Text :key="title"/>', 'REPEAT_KEY'),
            ('<Text v-for="item in rows" :key="item.id" ref="row"/>', 'REF:'),
            ('<NativeHost v-for="item in rows" :key="item.id" host="x"/>', 'HOST_DYNAMIC'),
            ('<Column v-for="item in rows" :key="item.id"><Text ref="row"/></Column>', 'REF:'),
        ]
        for one, code in cases:
            with self.subTest(one=one):
                self.compile(one, SCHEMA, error=code)

    def test_slots_import_and_css_errors(self):
        self.write('Child.one', '<template><Column><slot name="body"/></Column></template>')
        prefix = '<import name="Child" src="Child.one"/><template view-model="Basic">'
        self.compile(prefix + '<Child><Text slot="wrong"/></Child></template>', SCHEMA, error='SLOT_NAME', raw=True)
        self.compile(prefix + '<Child class="x"/></template>', SCHEMA, raw=True)
        self.write('Child.one', '<import name="Root" src="Basic.one"/><template><Root/></template>')
        self.compile(prefix + '<Child/></template>', SCHEMA, error='TEMPLATE_CYCLE', raw=True)
        self.compile('<Column><slot/><slot/></Column>', SCHEMA, error='SLOT_NAME')
        self.compile('<template view-model="Basic"><Text/></template><style>Text.x { color:red; }</style>', SCHEMA, error='CSS_SCOPE', raw=True)
        self.compile('<template view-model="Basic"><Text/></template><style scoped>Text.x { imaginary:1; }</style>', SCHEMA, error='Property is not consumed', raw=True)

    def test_all_segments_and_nested_lexical_bindings(self):
        self.write('Child.one', '<template><Column><slot name="body"><Text text="fallback"/></slot></Column></template>')
        one = '''<import name="Child" src="Child.one"/>
<template view-model="Basic"><Column view-id="root">
 <Input v-model="input"/><Switch v-model="enabled"/><Select v-model="index"/>
 <Tabs :items="tabs" @close="close"/><Button @click="click"/>
 <Text v-if="flag" :text="title"/><Text v-else text="hidden"/>
 <Column v-for="item in rows" :key="item.id">
  <Button :text="item.text" @click="item.select"/>
  <Column v-for="child in item.children" :key="child.id">
   <Text :text="child.name"/><Text :text="item.text"/><Text :text="title"/>
  </Column>
 </Column>
 <Child><Text slot="body" :text="title"/></Child><NativeHost host="terminal"/>
</Column></template><style scoped>Text.x { color: #ffffff; }</style>'''
        data = self.compile(one, SCHEMA, raw=True)
        for kind in range(1, 19):
            with self.subTest(section=kind):
                self.assertGreater(directory(data)[kind][4], 0)
        copy = self.root / 'roundtrip.oneview'
        self.run_cli('--roundtrip', self.root / 'Basic.oneview', '--output', copy)
        self.assertEqual(data, copy.read_bytes())
        spans = section(data, 17)
        self.assertTrue(any(u32(spans, at+20) != 0xffffffff for at in range(0,len(spans),24)))
        binds = section(data, 9)
        depths = [struct.unpack_from('<H', binds, at + 12)[0] for at in range(0, len(binds), 36)]
        self.assertIn(2, depths)
        self.assertIn(1, depths)

    def test_constants_and_composite_builtins(self):
        self.compile('<Column><Text :visible="true" v-if="false"/><DataTable :items="rows" :columns="columns"/><TimeSeriesChart :series="series"/></Column>',
                     'model Basic {r title:utf8 r rows:list<oneui.v1.Row> r columns:list<oneui.v1.Column> r series:list<oneui.v1.Series>}')

    def test_import_in_repeat_uses_item_vm_and_slot_captures_caller(self):
        self.write('Child.one', '''<template view-model="Item"><Column>
<Button :text="text" @click="select"/><slot name="body"><Text :text="text"/></slot>
</Column></template>''')
        source = '''<import name="Child" src="Child.one"/>
<template view-model="Basic"><Column>
<Child v-for="item in rows" :key="item.id"><Text slot="body" :text="title"/></Child>
</Column></template>'''
        data = self.compile(source, SCHEMA, raw=True)
        records = section(data, 9)
        depths = [struct.unpack_from('<H', records, at+12)[0] for at in range(0,len(records),36)]
        self.assertEqual(sorted(depths), [0,0,1])


if __name__ == '__main__':
    unittest.main()
