import struct
import unittest
from support import CompilerCase, directory, section, FIXTURES

SCHEMA = 'model Basic {r rows:list<Item>} model Item key id {r id:utf8;r text:utf8}'
ITEM = '<Pane view-id="panes" v-for="p in rows" :key="p.id" :pane-key="p.id"><NativeHost host="terminal"/></Pane>'

class WorkspaceTests(CompilerCase):
    def test_picker_projections_slots_and_slider_contract(self):
        root=FIXTURES.parent/'workspace'
        view=(root/'Pickers.one').read_text(encoding='utf-8')
        schema=(root/'Pickers.oneschema').read_text(encoding='utf-8')
        self.compile(view,schema,raw=True)
        self.compile(view,schema.replace('r rgba:i64','r rgba:f64'),raw=True,error='BIND_RECORD')
        self.compile(view,schema.replace('phase:i64','phase:utf8'),raw=True,error='COMMAND_SIGNATURE')
        self.compile(view.replace('slot="empty"','slot="oops"'),schema,raw=True,error='MEDIA_SLOT')
        self.compile('<template view-model="Root"><Card><Text/><Text/></Card></template>',schema,raw=True,error='CHILDREN')

    def test_dock_projection_and_event_contract(self):
        root=FIXTURES.parent/'workspace'
        view=(root/'DockWorkspace.one').read_text(encoding='utf-8')
        schema=(root/'DockWorkspace.oneschema').read_text(encoding='utf-8')
        self.compile(view,schema,raw=True)
        self.compile(view,schema.replace('Placement','AppPlacement'),raw=True)
        self.compile(view,schema.replace('r pinned: bool','r pinned: utf8'),raw=True,error='BIND_RECORD')
        self.compile(view,schema.replace('width:f64','width:i64'),raw=True,error='COMMAND_SIGNATURE')

    def test_container_query_required_capability_and_syntax(self):
        source='<template view-model="Basic"><Column class="shell"><ContainerPage :selected="selected"><Text/></ContainerPage></Column></template><style scoped>.shell {container-name:sftp;container-type:size;} @container sftp (width >= 860px) and (aspect-ratio >= 8/5) {ContainerPage {display:flex;}}</style>'
        schema='model Basic {r selected:bool}'
        data=self.compile(source,schema,raw=True)
        self.assertEqual(struct.unpack('<IIII',section(data,19)),(1,4,0,0))
        self.compile(source.replace('8/5','8/0'),schema,raw=True,error='CONTAINER_SYNTAX')
        self.compile(source.replace('display:flex','display:grid'),schema,raw=True,error='CSS_LAYOUT')
        self.compile(source.replace('display:flex','container-name:cycle'),schema,raw=True,error='CONTAINER_DEPENDENCY')

    def test_header_slots_and_semantic_signatures(self):
        schema='model Basic {r id:utf8;command split(pane:utf8,edge:utf8);command moved(source:utf8,phase:i64)}'
        self.compile('<PaneHeader :pane-key="id" @splitRequested="split"><Text slot="identity" text="Title"/><Button slot="actions" text="More"/></PaneHeader>',schema)
        self.compile('<DragHandle :source="id" threshold="4" @dragStateChanged="moved"><Text text="Drag"/></DragHandle>',schema)
        self.compile('<DropSurface view-id="strip" domain="terminal"><Tabs/></DropSurface>',schema)
        self.compile('<PaneHeader><Text slot="unknown"/></PaneHeader>',schema,error='HEADER_SLOT')
        self.compile('<PaneHeader @splitRequested="split"/>',schema.replace('edge:utf8','edge:f64'),error='COMMAND_SIGNATURE')

    def test_required_capability_and_host_flag(self):
        data = self.compile('<PaneGrid>' + ITEM + '</PaneGrid>', SCHEMA)
        self.assertEqual(struct.unpack('<IIII', section(data, 19)), (1,3,0,0))
        self.assertEqual(struct.unpack('<IIII', section(data,18))[2],2)
        bad = bytearray(data)
        struct.pack_into('<I',bad,directory(data)[19][2]+4,0x80)
        self.verify_bytes(bad,'VERSION: WSC1')

    def test_reject_non_item_and_missing_id(self):
        self.compile('<PaneGrid><Column/></PaneGrid>',SCHEMA,error='WORKSPACE_CHILDREN')
        self.compile('<PaneGrid>'+ITEM.replace('view-id="panes"','')+'</PaneGrid>',SCHEMA,error='WORKSPACE_SCOPE')

    def test_host_whitelist_and_duplicates(self):
        self.compile('<Column><Column view-id="rows" v-for="p in rows" :key="p.id"><NativeHost host="terminal"/></Column></Column>',SCHEMA,error='HOST_DYNAMIC')
        self.compile('<PaneGrid>'+ITEM.replace('</Pane>','<NativeHost host="terminal"/></Pane>')+'</PaneGrid>',SCHEMA,error='duplicate host')

    def test_same_name_in_different_collections(self):
        self.compile('<Column><PaneGrid>'+ITEM+'</PaneGrid><PaneGrid>'+ITEM.replace('"panes"','"other"')+'</PaneGrid></Column>',SCHEMA)

    def test_ordinary_repeat_has_no_capability(self):
        data=self.compile('<Column><Text v-for="p in rows" :key="p.id" :text="p.text"/></Column>',SCHEMA)
        self.assertNotIn(19,directory(data))

    def test_import_identity_and_forged_call_flag(self):
        self.write('Pool.one','<template view-model="Basic"><PaneGrid>'+ITEM+'</PaneGrid></template>')
        source='<import name="Pool" src="Pool.one"/><template view-model="Basic"><Pool view-id="pool"/></template>'
        data=self.compile(source,SCHEMA,raw=True)
        bad=bytearray(data)
        struct.pack_into('<I',bad,directory(data)[14][2]+20,0)
        self.verify_bytes(bad,'explicit view-id')
        self.compile(source.replace(' view-id="pool"',''),SCHEMA,raw=True,error='WORKSPACE_SCOPE')
        self.compile(source.replace('"pool"','"ambiguous/pool"'),SCHEMA,raw=True,error='WORKSPACE_SCOPE')

    def test_supplied_slot_and_dock_collection(self):
        self.write('Frame.one','<template view-model="Basic"><Column><slot/></Column></template>')
        source='<import name="Frame" src="Frame.one"/><template view-model="Basic"><Frame view-id="frame"><PaneGrid>'+ITEM+'</PaneGrid></Frame></template>'
        self.compile(source,SCHEMA,raw=True)
        self.compile(source.replace(' view-id="frame"',''),SCHEMA,raw=True,error='WORKSPACE_SCOPE')
        dock=ITEM.replace('Pane','DockItem').replace('<NativeHost host="terminal"/>',
            '<PanelHeader/><PanelBody><NativeHost host="terminal"/></PanelBody>')
        self.compile('<DockWorkspace>'+dock+'</DockWorkspace>',SCHEMA)
        self.compile('<PaneGrid>'+ITEM.replace('"panes"','"a/b"')+'</PaneGrid>',SCHEMA,error='WORKSPACE_SCOPE')

    def test_reserved_event_flags_remain_rejected(self):
        data=self.compile('<Column><PaneGrid>'+ITEM+'</PaneGrid><Button @click="go"/></Column>',
            SCHEMA.replace('r rows:list<Item>','r rows:list<Item>;command go()'))
        bad=bytearray(data)
        struct.pack_into('<I',bad,directory(data)[10][2]+12,3<<16)
        self.verify_bytes(bad,'BAD_IR: event')

    def test_flat_tree_schema_contract(self):
        root=FIXTURES.parent/'workspace'
        self.compile((root/'Tree.one').read_text(encoding='utf-8'),
            (root/'Tree.oneschema').read_text(encoding='utf-8'),raw=True)

    def test_pane_projection_is_structural_and_events_are_typed(self):
        root=FIXTURES.parent/'workspace'
        view=(root/'PaneGrid.one').read_text(encoding='utf-8')
        schema=(root/'PaneGrid.oneschema').read_text(encoding='utf-8')
        self.compile(view,schema,raw=True)
        self.compile(view,schema.replace('ApplicationNode','UnrelatedBusinessName'),raw=True)
        self.compile(view,schema.replace('r ratio: f64','r ratio: utf8'),raw=True,error='BIND_RECORD')
        self.compile(view,schema.replace('cols:i64','cols:f64'),raw=True,error='COMMAND_SIGNATURE')

if __name__ == '__main__': unittest.main()
