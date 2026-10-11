from support import *


SCHEMA = '''model Basic {
 r title:utf8 rw input:utf8 r flag:bool rw enabled:bool rw index:i64
 rw ratio:f64 r rows:list<Item> r tabs:list<oneui.v1.TabItem>
 command click() command close(key:utf8) command wrong(key:i64)
} model Item key id { r id:utf8 r text:utf8 r children:list<Child> command select() }
model Child key id { r id:utf8 r name:utf8 }'''


class DiagnosticTests(CompilerCase):
    def test_state_view_contract(self):
        self.compile('<template view-model="Basic"><StateView title="Empty" action="Open"/></template><style scoped>StateView {background: #112233; padding:12px;} state-view-part.state-view-action:hover {background:#334455;} state-view-part.state-view-action:pressed {background:#556677;}</style>', SCHEMA, raw=True)
        self.compile('<StateView :title="title" :message="input" :action="title" icon="server" @action="click"/>', SCHEMA)
        self.compile('<StateView><Text/></StateView>', SCHEMA, error='CHILDREN')
        self.compile('<StateView @action="close"/>', SCHEMA, error='COMMAND_SIGNATURE')

    def test_menu_record_contracts_remain_distinct(self):
        schema = ('model Basic {r native:list<Native> r data:list<Data>}'
                  'model Native key id {r id:utf8 r kind:utf8 r title:utf8 r detail:utf8 '
                  'r icon:utf8 r danger:bool r disabled:bool}'
                  'model Data key id {r id:utf8 r parent:utf8 r text:utf8 r icon:utf8 '
                  'r shortcut:utf8 r disabled:bool r checked:bool r danger:bool r separator:bool}')
        self.compile('<Column><Menu :items="native"/><DataMenu :items="data"/></Column>', schema)
        self.compile('<Menu :items="data"/>', schema, error='BIND_RECORD')
        self.compile('<DataMenu :items="native"/>', schema, error='BIND_RECORD')

    def test_pane_header_action_labels(self):
        self.compile('<PaneHeader :split-label="title" :maximize-label="title" :close-label="title" :live-label="title" :menu-label="title"/>', SCHEMA)

    def test_native_server_icon(self):
        self.compile('<Column><Icon symbol="server-native"/><StateView icon="server-native"/></Column>', SCHEMA)

    def test_play_icon(self):
        self.compile('<Column><Icon symbol="play"/><Button icon="play" @click="click"/></Column>', SCHEMA)
    def test_reorderable_grid_contract(self):
        self.compile('<template view-model="Basic"><ReorderableGrid/></template><style scoped>ReorderableGrid {grid-min-column-width:280px;gap:8px;padding:0px;outline-width:2px;outline-color:#123456;}</style>', SCHEMA, raw=True)
        self.compile('<template view-model="Basic"><Button/></template><style scoped>Button {opacity:0.58;}</style>', SCHEMA, raw=True)
        schema = SCHEMA.replace('command click()', 'command reorder(source:utf8,target:i64) command drag(source:utf8,phase:i64,x:f64,y:f64) command click()')
        self.compile('<ReorderableGrid column-count="3" @reorderRequested="reorder" @itemDrag="drag"><GridItem view-id="rows" v-for="row in rows" :key="row.id" :grid-key="row.id"><Text :text="row.text"/></GridItem></ReorderableGrid>', schema)
        self.compile('<ReorderableGrid><Text/></ReorderableGrid>', schema, error='WORKSPACE_CHILDREN')
        self.compile('<GridItem grid-key="a"><Text/><Text/></GridItem>', schema, error='CHILDREN')
        self.compile('<ReorderableGrid><GridItem v-for="row in rows" :key="row.id" :grid-key="row.id"><Text/></GridItem></ReorderableGrid>', schema, error='WORKSPACE_SCOPE')
        self.compile('<ReorderableGrid @reorderRequested="click"/>', schema, error='COMMAND_SIGNATURE')
        self.compile('<ReorderableGrid :column-count="flag"/>', schema, error='BIND_TYPE')

    def test_overlay_and_optional_icons(self):
        self.compile('<Overlay overlay-width="34" overlay-height="18" inset-right="10" horizontal-align="end" vertical-align="center" :overlay-visible="flag"><Input v-model="input" prefix-icon="search-outline"/><Button trailing-icon="down-outline"/></Overlay>', SCHEMA)
        for children in ['', '<Text/>', '<Text/><Text/><Text/>']:
            self.compile('<Overlay>' + children + '</Overlay>', SCHEMA, error='CHILDREN')
        self.compile('<Overlay :overlay-visible="title"><Text/><Text/></Overlay>', SCHEMA, error='BIND_TYPE')
        self.compile('<SearchInput :prefix-icon="flag"/>', SCHEMA, error='BIND_TYPE')
        self.compile('<Button :trailing-icon="flag"/>', SCHEMA, error='BIND_TYPE')
        self.compile('<template view-model="Basic"><Button class="compact"/></template><style scoped>Button.compact {icon-size:12px;}</style>', SCHEMA, raw=True)

    def test_text_font_family_style(self):
        self.compile('<template view-model="Basic"><Text class="address"/></template><style scoped>Text.address {font-family:Cascadia Mono;}</style>', SCHEMA, raw=True)

    def test_editor_control_contracts(self):
        self.compile('<TextArea v-model="input" line-height="23" :read-only="flag"/>', SCHEMA)
        self.compile('<Checkbox v-model="enabled" text="Selected"/>', SCHEMA)
        self.compile('<Input v-model="input" password="true" password-mask="●"/>', SCHEMA)
        self.compile('<TextArea v-model="title"/>', SCHEMA, error='MODEL_READ_ONLY')
        self.compile('<Checkbox v-model="input"/>', SCHEMA, error='BIND_TYPE')
        self.compile('<TextArea><Text/></TextArea>', SCHEMA, error='CHILDREN')

    def test_dialog_contract(self):
        self.compile('<Dialog :title="title" :close-visible="flag" @close="click"><Column/><Row/></Dialog>', SCHEMA)
        for children in ['', '<Column/>', '<Column/><Row/><Text/>']:
            self.compile('<Dialog>' + children + '</Dialog>', SCHEMA, error='CHILDREN')
        self.compile('<Dialog @close="close"><Column/><Row/></Dialog>', SCHEMA, error='COMMAND_SIGNATURE')

    def test_template_table_and_row_pointer(self):
        schema = 'model Basic {r title:utf8 command pointer(x:f64,y:f64,control:bool,shift:bool,click_count:i64)}'
        self.compile('<TemplateTable minimum-width="820" content-height="1000"><Text/><Column/></TemplateTable>', schema)
        for children in ['', '<Column/>', '<Column/><Row/><Text/>']:
            self.compile('<TemplateTable>' + children + '</TemplateTable>', schema, error='CHILDREN')
        self.compile('<InteractiveSurface @pointer="pointer"><Text/></InteractiveSurface>', schema)
        self.compile('<InteractiveSurface @pointer="click"><Text/></InteractiveSurface>', SCHEMA, error='COMMAND_SIGNATURE')
        self.compile('<Button :class="title" trailing-text="3"/>', schema)
        self.compile('<Button :class="flag"/>', SCHEMA, error='BIND_TYPE')

    def test_binding_and_command_errors(self):
        cases = [
            ('<Unknown/>', 'COMPONENT:'), ('<Text :text="missing"/>', 'VM_PROPERTY'),
            ('<Text :text="flag"/>', 'BIND_TYPE'), ('<Input v-model="title"/>', 'MODEL_READ_ONLY'),
            ('<Switch v-model="input"/>', 'BIND_TYPE'), ('<Text v-model="input"/>', 'MODEL_TYPE'),
            ('<Text @click="click"/>', 'COMPONENT_EVENT'), ('<Button @click="close"/>', 'COMMAND_SIGNATURE'),
            ('<Tabs @close="wrong"/>', 'COMMAND_SIGNATURE'), ('<Button @click="missing"/>', 'VM_COMMAND'),
              ('<Text :text="input + title"/>', 'EXPRESSION'), ('<Icon :class="title"/>', 'BIND_STATIC'),
            ('<Text :class="flag"/>', 'BIND_TYPE'),
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

    def test_presentation_class_bindings(self):
        for component in ('Text', 'Surface', 'Column'):
            with self.subTest(component=component):
                data = self.compile(f'<{component} :class="title"/>', SCHEMA)
                self.assertEqual(len(section(data, 9)), 36)

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

    def test_pane_header_parts(self):
        self.compile('<template view-model="Basic"><PaneHeader/></template><style scoped>pane-header-part.pane-header-path {font-family:Consolas;font-size:13px;color:#123456;} pane-header-part.pane-header-button:hover {background:#abcdef;} pane-grid-part.pane-grid-divider:hover {background:#445566;}</style>',
                     'model Basic {r title:utf8}', raw=True)
    def test_fill_only_material_contract(self):
        self.compile('<template view-model="Basic"><Column><Button/><SearchInput/><Select/></Column></template>'
                     '<style scoped>Button {background-opacity:0.28;} SearchInput {background-opacity:0.28;} '
                     'Select {background-opacity:0.28;content-background-opacity:0.96;}</style>', SCHEMA, raw=True)
        self.compile('<template view-model="Basic"><Icon/></template>'
                     '<style scoped>Icon {background-opacity:0.28;}</style>', SCHEMA,
                     error='not consumed', raw=True)

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


from test_slots import SlotTests

if __name__ == '__main__':
    unittest.main()
