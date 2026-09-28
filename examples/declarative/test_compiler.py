import pathlib, subprocess, sys, tempfile
compiler = sys.argv[1]
with tempfile.TemporaryDirectory() as directory:
    root = pathlib.Path(directory)
    cases = [
        ('<Workspace><TitleBar/><WorkspaceBody/></Workspace>', True, ''),
        ('<Workspace><WorkspaceBody/><WorkspaceBody/></Workspace>', False, 'Duplicate region'),
        ('<Workspace><Column/></Workspace>', False, 'Invalid region'),
        ('<Workspace><TitleBar/></Workspace>', False, 'requires WorkspaceBody'),
        ('<DockPanel><PanelHeader/><PanelBody/><PanelFooter/></DockPanel>', True, ''),
        ('<DockPanel><PanelBody/></DockPanel>', False, 'requires PanelHeader'),
        ('<ToolButton icon="search" @click="search" name="Search"/>', True, ''),
        ('<Tabs presentation="segmented"/>', True, ''),
        ('<Tabs presentation="bad"/>', False, 'presentation'),
        ('<Progress :value="progress"/>', True, ''),
        ('<SplitView :second-collapsed="closed" collapsed-extent="40"><Column/><Column/></SplitView>', True, ''),
        ('<Tabs :items="tabs" v-model="selected" closable="true" @close="closeTab"/>', True, ''),
        ('<SplitView orientation="vertical" v-model="ratio"><Column/><Column/></SplitView>', True, ''),
        ('<SplitView><Column/></SplitView>', False, 'exactly two children'),
        ('<SplitView orientation="diagonal"><Column/><Column/></SplitView>', False, 'orientation'),
        ('<SplitView ratio="1.2"><Column/><Column/></SplitView>', False, 'ratio'),
        ('<TimeSeriesChart :series="series"/>', True, ''),
        ('<TimeSeriesChart series="string"/>', False, 'bound ViewModel member'),
        ('<Button icon="folder">Files</Button>', True, ''),
        ('<Icon symbol="terminal" size="24"/>', True, ''),
        ('<Icon symbol="unknown"/>', False, 'icon'),
        ('<Button @close="closeTab"/>', False, 'Tabs'),
        ('<NativeHost host="input" themed="true"/>', True, ''),
        ('<NativeHost host="terminal" grow="1"/>', True, ''),
        ('<NativeHost/>', False, 'static nonempty host'),
        ('<NativeHost host=""/>', False, 'static nonempty host'),
        ('<NativeHost :host="name"/>', False, 'static nonempty host'),
        ('<NativeHost host="terminal"><Input/></NativeHost>', False, 'Leaf components'),
        ('<NativeHost host="terminal" @click="save"/>', False, 'only supported on Button'),
        ('<Row align="center" justify="space-between" wrap="false"><Text/></Row>', True, ''),
        ('<Column align="stretch" wrap="false"/>', True, ''),
        ('<Row align="baseline"/>', False, 'align must'),
        ('<Column justify="between"/>', False, 'justify must'),
        ('<Row wrap="nowrap"/>', False, 'true or false'),

        ('<SidebarLayout><Sidebar/><Column/></SidebarLayout>', True, ''),
        ('<MasterDetail detail-open="true"><Column/><Column/></MasterDetail>', True, ''),
        ('<SidebarLayout><Column/><Column/></SidebarLayout>', False, 'Sidebar'),
        ('<MasterDetail><Column/></MasterDetail>', False, 'two children'),
        ('<MasterDetail breakpoint="0"><Column/><Column/></MasterDetail>', False, 'positive'),
        ('<Surface appearance="glossy"/>', False, 'appearance'),
        ('<Button variant="glossy"/>', False, 'variant'),
        ('<Text>hello</Text>', True, ''),
        ('<Reveal :open="flag" preset="expand" reduced-motion="false"><Input v-model="name"/></Reveal>', True, ''),
        ('<Reveal/>', False, 'exactly one child'),
        ('<Reveal><Text/><Text/></Reveal>', False, 'exactly one child'),
        ('<Reveal preset="bounce"><Text/></Reveal>', False, 'fade or expand'),
        ('<Reveal open="yes"><Text/></Reveal>', False, 'true or false'),
        ('<Input ref="name" v-model="name"/>', True, ''),
        ('<Input ref="name.invalid"/>', False, 'static identifier'),
        ('<Column><Input ref="same"/><Input ref="same"/></Column>', False, 'Duplicate ref'),
        ('<Input v-for="item in items" :key="item.id" ref="input"/>', False, 'ref inside v-for'),
        ('<DataTable @activate="edit" @delete="remove"/>', True, ''),
        ('<Button @activate="edit"/>', False, 'require DataTable'),
        ('<DataTable @delete="remove()"/>', False, 'member references'),
        ('<SettingsPage title="Preferences"><Section/><ActionBar><Button>Save</Button></ActionBar></SettingsPage>', True, ''),
        ('<ListPage><LoadingState subtitle="Loading"/></ListPage>', True, ''),
        ('<DetailPage title="Details"><Status tone="success">Online</Status></DetailPage>', True, ''),
        ('<FormRow label="Name" :error="validation"><Input /></FormRow>', True, ''),
        ('<FormGrid min-column-width="320"><FormRow label="Name"><Input/></FormRow></FormGrid>', True, ''),
        ('<FormGrid><Input/></FormGrid>', False, 'FormRow children only'),
        ('<FormGrid><FormRow v-for="item in items" :key="item.id"><Input/></FormRow></FormGrid>', False, 'direct v-for'),
        ('<FormGrid min-column-width="0"/>', False, 'finite'),
        ('<FormGrid :min-column-width="width"/>', False, 'static'),
        ('<SettingsPage><ActionBar/><ActionBar/></SettingsPage>', False, 'at most one ActionBar'),
        ('<Status tone="red"/>', False, 'Status tone'),
        ('<Content><Text>body</Text></Content>', True, ''),
        ('<Content max-width="720" align="center"><Input /></Content>', True, ''),
        ('<Content max-width="0008" />', True, ''),
        ('<Content max-width="0" />', False, 'finite positive'),
        ('<Content max-width="880px" />', False, 'logical pixels'),
        ('<Content max-width="NaN" />', False, 'finite nonnegative'),
        ('<Content max-width="9999999999999999999999999999999999999999" />', False, 'float range'),
        ('<Content align="middle" />', False, 'start, center or end'),
        ('<Content :max-width="width" />', False, 'static'),
        ('<Content :align="alignment" />', False, 'static'),
        ('<Column max-width="720" />', False, 'Unknown property'),
        ('<Column min="200" max="100" />', False, 'max must be at least min'),
        ('<Input basis="-20" />', False, 'nonnegative'),
        ('<Input grow="inf" />', False, 'finite'),
        ('<Select selectedIndex="1.5" />', False, 'integer'),
        ('<Select selectedIndex="-1" />', True, ''),
        ('<Unknown />', False, 'Unknown component'),
        ('<Input typo="x" />', False, 'Unknown property'),
        ('<Text :text="count + 1" />', False, 'member references'),
        ('<Text v-for="row in rows" />', False, ':key'),
        ('<Column><Text v-else /></Column>', False, 'immediately follow'),
        ('<Text @click="go" />', False, 'click only'),
        ('<Input><Text>ignored</Text></Input>', False, 'Leaf components'),
        ('<FormRow />', False, 'exactly one child'),
        ('<Text>{{ count }}</Text>', False, ':text'),
        ('<Input disabled="yes" />', False, 'true or false'),
        ('<Button :text="saveText" @click="save" />', True, ''),
        ('<Column><Text v-if="visible" /><Text v-else /></Column>', True, ''),
        ('<Text v-for="item in items" :key="item.id" :text="item.text" />', True, ''),
    ]
    for index, (body, valid, diagnostic) in enumerate(cases):
        source = root / f'case{index}.one'
        source.write_text(f'<template view-model="VM">\n{body}\n</template>', encoding='utf-8')
        result = subprocess.run([compiler, '--input', str(source), '--output', str(root/'out.h'), '--name', 'Test'], capture_output=True, text=True)
        assert (result.returncode == 0) == valid, (body, result.stderr)
        if not valid:
            assert diagnostic in result.stderr, result.stderr
            assert '.one:2:' in result.stderr, result.stderr
        else:
            assert '#line 2' in (root/'out.h').read_text(encoding='utf-8')
            assert 'ui.locate(' in (root/'out.h').read_text(encoding='utf-8')
    (root/'Child.one').write_text('<template><Section><slot name="actions"/><slot/></Section></template><style scoped>Section { gap:8px; }</style>')
    (root/'Main.one').write_text('<import name="Child" src="Child.one"/><template><Child><Button slot="actions">Save</Button><Text>Body</Text></Child></template>')
    result = subprocess.run([compiler,'--input',str(root/'Main.one'),'--output',str(root/'main.h'),'--name','Main','--depfile',str(root/'main.d')],capture_output=True,text=True)
    assert result.returncode == 0, result.stderr
    generated = (root/'main.h').read_text()
    assert 'scope_Main_Child' in generated and 'slots' in generated and 'Child.one' in (root/'main.d').read_text()
    if len(sys.argv) > 3:
        cpp_compiler, includes = sys.argv[2:4]
        for body, valid in [('<Reveal :open="flag"><Input v-model="name"/></Reveal>',True),('<Reveal :open="name"><Text/></Reveal>',False),('<Content max-width="0008" align="center"><Input v-model="name"/></Content>',True),('<Input v-model="name"/>',True),('<Input v-model="flag"/>',False),('<Text :text="flag"/>',False),('<Text :text="missing"/>',False)]:
            (root/'Typed.one').write_text('<template view-model="VM">\n'+body+'\n</template>')
            subprocess.run([compiler,'--input',str(root/'Typed.one'),'--output',str(root/'typed.h'),'--name','Typed'],check=True)
            (root/'typed.cpp').write_text('#include "oneui/ui_declarative.h"\nstruct VM { oneui::State<std::wstring> name; oneui::State<bool> flag; };\n#include "typed.h"\nvoid check(VM& vm,oneui::ui::Mount& ui){build_Typed(vm,ui);}')
            result = subprocess.run([cpp_compiler,'/nologo','/Zs','/EHsc','/std:c++17','/utf-8','/I'+includes,str(root/'typed.cpp')],capture_output=True)
            assert (result.returncode==0)==valid, result.stdout.decode(errors='replace')
            if not valid: assert b'Typed.one(2)' in result.stdout, result.stdout.decode(errors='replace')
        compose_cases = [
            ('ui.nativeHost("terminal").grow();', True, ''),
            ('ui.row({}).align(L"center").justify(L"space-between").wrap(false);', True, ''),
            ('ui.text(L"Caption").wrap(false);', False, 'Compose property is not supported'),

            ('ui.sidebarLayout(ui.sidebar({ui.text(L"Navigation")}),ui.masterDetail(ui.listPage({}),ui.detailPage({})).detailOpen(flag).breakpoint(800).paneWidth(380));ui.surface({}).appearance(L"raised");',True,''),
            ('ui.sidebarLayout(ui.column({}),ui.column({}));',False,''),
            ('ui.text(L"Caption").appearance(L"raised");',False,'Compose property is not supported'),
            ('ui.reveal(ui.input(name)).open(flag).preset(L"fade").reducedMotion(false);',True,''),
            ('ui.reveal(ui.text(L"Hint")).open(name);',False,'Compose property and State/Computed value types do not match'),
            ('ui.settingsPage({ui.formGrid({ui.field(L"Name",ui.input(name)).hint(L"Help").error(error)}),ui.actions({ui.button(L"Save",save).primary()})});',True,''),
            ('ui.text(name).ref("caption"); ui.select(items,index); ui.toggle(flag);',True,''),
            ('ui.input(flag);',False,''),
            ('ui.text(flag);',False,'Compose property and State/Computed value types do not match'),
            ('ui.text(L"Caption").error(error);',False,'Compose property is not supported'),
            ('ui.text(L"Caption").primary();',False,'Compose primary requires Button'),
            ('ui.text(L"Caption").model(name);',False,'Compose model is not supported'),
            ('ui.input(name).disabled(1);',False,'Compose property value has the wrong type'),
            ('ui.text(oneui::State<std::wstring>{L"temporary"});',False,'persistent State/Computed lvalue'),
            ('ui.formGrid({ui.text(L"Not a field")});',False,''),
        ]
        for body,valid,diagnostic in compose_cases:
            (root/'compose.cpp').write_text('#include "oneui/ui_compose.h"\nvoid check(oneui::ui::Mount& mount){oneui::ui::Compose ui(mount); oneui::State<std::wstring> name,error; oneui::State<bool> flag; oneui::State<int> index; oneui::State<std::vector<std::wstring>> items; oneui::ui::VmCommand save;\n'+body+'\n}',encoding='utf-8')
            result=subprocess.run([cpp_compiler,'/nologo','/Zs','/EHsc','/std:c++17','/utf-8','/I'+includes,str(root/'compose.cpp')],capture_output=True)
            assert (result.returncode==0)==valid,result.stdout.decode(errors='replace')
            if diagnostic: assert diagnostic.encode() in result.stdout,result.stdout.decode(errors='replace')
        print(f'Compose: {sum(valid for _,valid,_ in compose_cases)} positive and {sum(not valid for _,valid,_ in compose_cases)} negative C++ type/structure/lifetime diagnostics passed')
print('Compiler positive, negative, source mapping, imports, slots and scoped CSS tests passed')
