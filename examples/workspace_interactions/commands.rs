use crate::{
    page::{WorkspaceCommand as C, Workspace_properties as P},
    tools::edge,
    vm::Model,
};
use oneui::{view::VmResult, Rect};
impl Model {
    pub fn command(&mut self, c: C) -> VmResult {
        self.commands += 1;
        match c {
            C::One => return self.preset(1),
            C::Two => return self.preset(2),
            C::Four => return self.preset(4),
            C::Sixteen => return self.preset(16),
            C::Balance => {
                for n in &mut self.forest.nodes {
                    n.ratio = 0.5;
                }
                self.revision += 1;
            }
            C::Restore => self.maximized.clear(),
            C::Activate { pane } => self.activePane = pane,
            C::Maximize { pane } => {
                self.maximized = if self.maximized == pane {
                    String::new()
                } else {
                    pane
                }
            }
            C::Split { pane, edge } => {
                let visible = self.forest.leaves(&self.activeGroup);
                if let Some(source) = self
                    .panes
                    .iter()
                    .map(|v| v.0.to_owned())
                    .find(|p| !visible.contains(p))
                {
                    self.forest.split(&source, &pane, &edge);
                    self.activePane = source;
                    self.revision += 1;
                }
            }
            C::Close { pane } => {
                if self.forest.leaves(&self.activeGroup).len() > 1 {
                    let index = self.panes.iter().position(|v| v.0 == pane);
                    if let Some(i) = index {
                        let mut row = self.panes.get(i).unwrap().borrow_mut();
                        row.closing = true;
                        row.closeRequest = self.commands as i64;
                        drop(row);
                        let change = self.panes.update(i, &[])?;
                        self.cx.notify_list(P::PANES, &change)?;
                    }
                }
            }
            C::Settled { pane, request, .. } => {
                let index = self.panes.iter().position(|v| v.0 == pane);
                if let Some(i) = index {
                    if self.panes.get(i).unwrap().borrow().closeRequest == request {
                        self.forest.detach(&pane);
                        let change = self.panes.remove(i)?;
                        self.cx.notify_list(P::PANES, &change)?;
                        self.revision += 1;
                    }
                }
            }
            C::Ratio {
                split,
                ratio,
                request,
                revision,
            } => {
                self.ack = request;
                if revision == self.revision {
                    if let Some(n) = self.forest.nodes.iter_mut().find(|n| n.id == split) {
                        n.ratio = ratio.clamp(0.001, 0.999);
                        self.revision += 1;
                    }
                }
            }
            C::DropPane {
                source,
                target,
                edge,
                request,
                revision,
            } => {
                self.ack = request;
                if revision == self.revision {
                    if edge == "tab-strip" {
                        self.forest.tab(&source);
                        self.activeGroup = self.forest.group_of(&source).unwrap();
                    } else if edge == "center" {
                        self.forest.swap(&source, &target);
                    } else {
                        let group = self.forest.group_of(&target);
                        if self.forest.split(&source, &target, &edge) {
                            self.activeGroup = group.unwrap();
                        }
                    }
                    self.activePane = source;
                    self.revision += 1;
                }
            }
            C::Focus {
                target,
                request,
                revision,
                ..
            } => {
                self.ack = request;
                if revision == self.revision {
                    self.activePane = target;
                }
            }
            C::Swap {
                source,
                target,
                request,
                revision,
            } => {
                self.ack = request;
                if revision == self.revision {
                    self.forest.swap(&source, &target);
                    self.activePane = source;
                    self.revision += 1;
                }
            }
            C::Left | C::Right | C::Up | C::Down | C::SwapRight => {
                self.navigationEdge = match c {
                    C::Left => "left",
                    C::Up => "top",
                    C::Down => "bottom",
                    _ => "right",
                }
                .into();
                self.navigationMode = if matches!(c, C::SwapRight) {
                    "swap"
                } else {
                    "focus"
                }
                .into();
                self.navigationSerial += 1;
            }
            C::Reorder { source, before } => {
                if let Some(i) = self.forest.groups.iter().position(|g| g.id == source) {
                    let group = self.forest.groups.remove(i);
                    let to = self
                        .forest
                        .groups
                        .iter()
                        .position(|g| g.id == before)
                        .unwrap_or(self.forest.groups.len());
                    self.forest.groups.insert(to, group);
                }
            }
            C::Resize { .. } => {
                self.resize_count += 1;
                return Ok(());
            }
            C::Broadcast { pane } => self.broadcast_state.toggle(
                &pane,
                &self.activePane,
                &self.forest.leaves(&self.activeGroup),
            ),
            C::Disconnect => self.broadcast_state.disconnect(&self.activePane),
            C::Paste => {
                self.broadcast_state
                    .input(&self.activePane, "echo first\necho second\n", true);
            }
            C::AcceptPaste => self.broadcast_state.confirm(true),
            C::CancelPaste => self.broadcast_state.confirm(false),
            C::MenuOpen { pane, .. } => {
                self.activePane = pane;
                self.menu = !self.menu;
            }
            C::History | C::Live { .. } => {
                let key = if let C::Live { ref pane } = c {
                    pane.clone()
                } else {
                    self.activePane.clone()
                };
                let index = self.panes.iter().position(|v| v.0 == key);
                if let Some(i) = index {
                    self.panes.get(i).unwrap().borrow_mut().history = matches!(c, C::History);
                    let change = self.panes.update(i, &[])?;
                    self.cx.notify_list(P::PANES, &change)?;
                }
            }
            C::SettingsToggle => self.settings = !self.settings,
            C::SftpTree | C::SftpBrowser | C::SftpDetails => {
                self.treeSelected = matches!(c, C::SftpTree);
                self.browserSelected = matches!(c, C::SftpBrowser);
                self.detailsSelected = matches!(c, C::SftpDetails);
            }
            C::SelectFont { key } => {
                if self
                    .fonts
                    .iter()
                    .any(|v| v.0 == key && v.1.borrow().available)
                {
                    self.font = key;
                }
            }
            C::RetryFont => self.error.clear(),
            C::Choose => {
                self.asset = self.mediaPath.clone();
                self.error.clear();
            }
            C::Clear => {
                self.asset.clear();
                self.error.clear();
            }
            C::Cancel => self.loading = false,
            C::Failed {
                source, message, ..
            } => {
                if self.asset == source {
                    self.error = message;
                }
            }
            C::Adjust { .. } => return Ok(()),
            other => return self.tool_command(other),
        }
        self.publish()
    }
    fn tool_command(&mut self, c: C) -> VmResult {
        let sftp = "sftp".to_string();
        match c {
            C::Dock {
                source,
                target,
                edge: side,
                revision,
                ..
            } if revision == self.toolRevision => {
                let _ = self.tools.dock(&source, &target, edge(&side));
            }
            C::FloatTool {
                pane,
                x,
                y,
                width,
                height,
                revision,
                ..
            } if revision == self.toolRevision => {
                let _ = self.tools.float(
                    &pane,
                    Rect {
                        x: x as f32,
                        y: y as f32,
                        width: width as f32,
                        height: height as f32,
                    },
                );
            }
            C::Bounds {
                pane,
                x,
                y,
                width,
                height,
                revision,
                ..
            } if revision == self.toolRevision => {
                let _ = self.tools.set_float_bounds(
                    &pane,
                    Rect {
                        x: x as f32,
                        y: y as f32,
                        width: width as f32,
                        height: height as f32,
                    },
                );
            }
            C::Raise { pane, revision, .. } if revision == self.toolRevision => {
                let _ = self.tools.raise(&pane);
            }
            C::ToolRatio {
                split,
                ratio,
                revision,
                ..
            } if revision == self.toolRevision => self.tool_ratio(&split, ratio),
            C::ToolFloat => {
                let _ = self.tools.float(
                    &sftp,
                    Rect {
                        x: 180.,
                        y: 100.,
                        width: 560.,
                        height: 440.,
                    },
                );
            }
            C::ToolPin => {
                let pinned = self
                    .tools
                    .snapshot()
                    .floating
                    .get(&sftp)
                    .is_some_and(|f| f.pinned);
                let _ = self.tools.set_pinned(&sftp, !pinned);
            }
            C::ToolHide => {
                let _ = self.tools.hide(&sftp);
            }
            C::ToolShow => {
                let _ = self.tools.show(sftp);
            }
            C::ToolRedock => {
                let _ = self.tools.redock(&sftp);
            }
            C::ToolMaximize => {
                let _ = self.tools.maximize(&sftp);
            }
            C::ToolSave => self.saved_tools = Some(self.tools.snapshot()),
            C::ToolRestore => {
                if let Some(saved) = self.saved_tools.clone() {
                    self.tools.restore(saved).unwrap();
                }
            }
            C::CompactToggle => {
                self.compact = if self.compact.is_empty() {
                    "terminal".into()
                } else {
                    String::new()
                }
            }
            _ => return Ok(()),
        }
        self.publish_tools()
    }
}
