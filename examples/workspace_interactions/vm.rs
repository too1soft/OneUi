use crate::{broadcast::Broadcast, forest::Forest, page::*, rows::*};
use oneui::layout::workspace::{DockSnapshot, DockWorkspace};
use oneui::view::{StableList, VmContext, VmResult};
use std::{cell::RefCell, rc::Rc};
pub struct Model {
    pub nodes: StableList<Node>,
    pub groups: StableList<Group>,
    pub panes: StableList<PaneModel>,
    pub tabs: StableList<TabModel>,
    pub tabSources: StableList<TabModel>,
    pub activeGroup: String,
    pub activePane: String,
    pub maximized: String,
    pub revision: i64,
    pub ack: i64,
    pub navigationSerial: i64,
    pub navigationEdge: String,
    pub navigationMode: String,
    pub motion: bool,
    pub selectedTab: String,
    pub status: String,
    pub settings: bool,
    pub showPickers: bool,
    pub menu: bool,
    pub confirmPaste: bool,
    pub toolNodes: StableList<Node>,
    pub placements: StableList<Placement>,
    pub toolRoot: String,
    pub toolRevision: i64,
    pub toolMax: String,
    pub compact: String,
    pub fonts: StableList<Font>,
    pub themes: StableList<Theme>,
    pub font: String,
    pub asset: String,
    pub error: String,
    pub loading: bool,
    pub mediaPath: String,
    pub level: f64,
    pub treeSelected: bool,
    pub browserSelected: bool,
    pub detailsSelected: bool,
    pub cx: VmContext,
    pub forest: Forest,
    pub broadcast_state: Broadcast,
    pub tools: DockWorkspace<String>,
    pub saved_tools: Option<DockSnapshot<String>>,
    pub resize_count: usize,
    pub commands: usize,
}
impl Default for Model {
    fn default() -> Self {
        let mut value = Self {
            nodes: Default::default(),
            groups: Default::default(),
            panes: Default::default(),
            tabs: Default::default(),
            tabSources: Default::default(),
            activeGroup: Default::default(),
            activePane: Default::default(),
            maximized: Default::default(),
            revision: Default::default(),
            ack: Default::default(),
            navigationSerial: Default::default(),
            navigationEdge: Default::default(),
            navigationMode: Default::default(),
            motion: Default::default(),
            selectedTab: Default::default(),
            status: Default::default(),
            settings: Default::default(),
            showPickers: true,
            menu: Default::default(),
            confirmPaste: Default::default(),
            toolNodes: Default::default(),
            placements: Default::default(),
            toolRoot: Default::default(),
            toolRevision: Default::default(),
            toolMax: Default::default(),
            compact: Default::default(),
            fonts: Default::default(),
            themes: Default::default(),
            font: Default::default(),
            asset: Default::default(),
            error: Default::default(),
            loading: Default::default(),
            mediaPath: Default::default(),
            level: Default::default(),
            treeSelected: Default::default(),
            browserSelected: Default::default(),
            detailsSelected: Default::default(),
            cx: VmContext::default(),
            forest: Forest::new(),
            broadcast_state: Broadcast::default(),
            tools: crate::tools::initial(),
            saved_tools: None,
            resize_count: 0,
            commands: 0,
        };
        value.motion = true;
        value.level = 1.;
        value.font = "Consolas".into();
        value.browserSelected = true;
        value.navigationMode = "focus".into();
        value.navigationEdge = "right".into();
        value.status = "就绪 · 终端只在本地回显".into();
        crate::fixtures::populate(&mut value);
        value.publish().unwrap();
        value.publish_tools().unwrap();
        value
    }
}
impl WorkspaceVm for Model {
    type NodesItem = Node;
    fn nodes(&self) -> &StableList<Node> {
        &self.nodes
    }
    type GroupsItem = Group;
    fn groups(&self) -> &StableList<Group> {
        &self.groups
    }
    type PanesItem = PaneModel;
    fn panes(&self) -> &StableList<PaneModel> {
        &self.panes
    }
    type TabsItem = TabModel;
    fn tabs(&self) -> &StableList<TabModel> {
        &self.tabs
    }
    type TabSourcesItem = TabModel;
    fn tabSources(&self) -> &StableList<TabModel> {
        &self.tabSources
    }
    fn activeGroup(&self) -> &str {
        &self.activeGroup
    }
    fn activePane(&self) -> &str {
        &self.activePane
    }
    fn maximized(&self) -> &str {
        &self.maximized
    }
    fn revision(&self) -> i64 {
        self.revision
    }
    fn ack(&self) -> i64 {
        self.ack
    }
    fn navigationSerial(&self) -> i64 {
        self.navigationSerial
    }
    fn navigationEdge(&self) -> &str {
        &self.navigationEdge
    }
    fn navigationMode(&self) -> &str {
        &self.navigationMode
    }
    fn motion(&self) -> bool {
        self.motion
    }
    fn selectedTab(&self) -> &str {
        &self.selectedTab
    }
    fn set_selectedTab(&mut self, value: String, _: &VmContext) -> VmResult {
        if self.forest.groups.iter().any(|g| g.id == value) {
            self.activeGroup = value;
            self.maximized.clear();
            self.publish()?;
        }
        Ok(())
    }
    fn status(&self) -> &str {
        &self.status
    }
    fn settings(&self) -> bool {
        self.settings
    }
    fn showPickers(&self) -> bool {
        self.showPickers
    }
    fn menu(&self) -> bool {
        self.menu
    }
    fn confirmPaste(&self) -> bool {
        self.confirmPaste
    }
    type ToolNodesItem = Node;
    fn toolNodes(&self) -> &StableList<Node> {
        &self.toolNodes
    }
    type PlacementsItem = Placement;
    fn placements(&self) -> &StableList<Placement> {
        &self.placements
    }
    fn toolRoot(&self) -> &str {
        &self.toolRoot
    }
    fn toolRevision(&self) -> i64 {
        self.toolRevision
    }
    fn toolMax(&self) -> &str {
        &self.toolMax
    }
    fn compact(&self) -> &str {
        &self.compact
    }
    type FontsItem = Font;
    fn fonts(&self) -> &StableList<Font> {
        &self.fonts
    }
    type ThemesItem = Theme;
    fn themes(&self) -> &StableList<Theme> {
        &self.themes
    }
    fn font(&self) -> &str {
        &self.font
    }
    fn asset(&self) -> &str {
        &self.asset
    }
    fn error(&self) -> &str {
        &self.error
    }
    fn loading(&self) -> bool {
        self.loading
    }
    fn mediaPath(&self) -> &str {
        &self.mediaPath
    }
    fn set_mediaPath(&mut self, value: String, cx: &VmContext) -> VmResult {
        self.mediaPath = value;
        cx.notify(Workspace_properties::MEDIAPATH)
    }
    fn level(&self) -> f64 {
        self.level
    }
    fn set_level(&mut self, value: f64, cx: &VmContext) -> VmResult {
        self.level = value;
        cx.notify(Workspace_properties::LEVEL)
    }
    fn treeSelected(&self) -> bool {
        self.treeSelected
    }
    fn browserSelected(&self) -> bool {
        self.browserSelected
    }
    fn detailsSelected(&self) -> bool {
        self.detailsSelected
    }
    fn invoke(&mut self, command: WorkspaceCommand, _: &VmContext) -> VmResult {
        self.command(command)
    }
    fn on_view_connect(&mut self, cx: &VmContext) -> VmResult {
        self.cx = cx.clone();
        Ok(())
    }
    fn on_view_disconnect(&mut self, _: &VmContext) {
        self.cx = VmContext::default();
    }
}
