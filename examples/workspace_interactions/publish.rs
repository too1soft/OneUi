use crate::{delta::sync, page::Workspace_properties as P, rows::TabModel, vm::Model};
use oneui::view::VmResult;
impl Model {
    pub fn publish(&mut self) -> VmResult {
        if !self.forest.groups.iter().any(|g| g.id == self.activeGroup) {
            self.activeGroup = self
                .forest
                .groups
                .first()
                .map(|g| g.id.clone())
                .unwrap_or_default();
        }
        let leaves = self.forest.leaves(&self.activeGroup);
        if !leaves.contains(&self.activePane) {
            self.activePane = leaves.first().cloned().unwrap_or_default();
        }
        self.selectedTab = self.activeGroup.clone();
        sync(
            &mut self.nodes,
            self.forest
                .nodes
                .iter()
                .map(|n| (n.id.clone(), n.clone()))
                .collect(),
            &self.cx,
            P::NODES,
        )?;
        sync(
            &mut self.groups,
            self.forest
                .groups
                .iter()
                .map(|g| (g.id.clone(), g.clone()))
                .collect(),
            &self.cx,
            P::GROUPS,
        )?;
        let tabs: Vec<_> = self
            .forest
            .groups
            .iter()
            .enumerate()
            .map(|(i, g)| {
                let panes = self.forest.leaves(&g.id);
                (
                    g.id.clone(),
                    TabModel {
                        id: g.id.clone(),
                        title: format!("工作区 {} · {}", i + 1, panes.len()),
                        pane: panes.first().cloned().unwrap_or_default(),
                        enabled: panes.len() == 1,
                    },
                )
            })
            .collect();
        sync(&mut self.tabs, tabs.clone(), &self.cx, P::TABS)?;
        sync(&mut self.tabSources, tabs, &self.cx, P::TABSOURCES)?;
        for i in 0..self.panes.len() {
            let changed = {
                let mut p = self.panes.get(i).unwrap().borrow_mut();
                let old = p.clone();
                p.active = p.id == self.activePane;
                p.headerVisible = leaves.len() > 1 || self.broadcast_state.source.is_some();
                p.number = leaves
                    .iter()
                    .position(|k| k == &p.id)
                    .map(|i| i as i64 + 1)
                    .unwrap_or(1);
                p.canClose = leaves.len() > 1;
                p.role = self.broadcast_state.role(&p.id).into();
                p.hint = self.broadcast_state.hint(&p.id, &self.activePane).into();
                *p != old
            };
            if changed {
                let change = self.panes.update(i, &[])?;
                self.cx.notify_list(P::PANES, &change)?;
            }
        }
        self.confirmPaste = self.broadcast_state.pending.is_some();
        self.status = format!(
            "{} 个可见窗格 · {} 次尺寸结算 · 广播{}",
            leaves.len(),
            self.resize_count,
            if self.broadcast_state.source.is_none() {
                "关闭"
            } else if self.broadcast_state.paused {
                "暂停"
            } else {
                "运行中"
            }
        );
        for p in [
            P::ACTIVEGROUP,
            P::ACTIVEPANE,
            P::SELECTEDTAB,
            P::MAXIMIZED,
            P::REVISION,
            P::ACK,
            P::NAVIGATIONSERIAL,
            P::NAVIGATIONEDGE,
            P::NAVIGATIONMODE,
            P::MOTION,
            P::STATUS,
            P::SETTINGS,
            P::MENU,
            P::CONFIRMPASTE,
            P::BROWSERSELECTED,
            P::TREESELECTED,
            P::DETAILSSELECTED,
            P::FONT,
            P::ASSET,
            P::ERROR,
            P::LOADING,
        ] {
            self.cx.notify(p)?;
        }
        Ok(())
    }
    pub fn preset(&mut self, count: usize) -> VmResult {
        for i in 0..16 {
            let id = format!("session/{i}");
            if !self.panes.iter().any(|p| p.0 == id) {
                let row = crate::rows::PaneModel {
                    id: id.clone(),
                    title: format!("本地 {:02}", i + 1),
                    path: "~/workspace".into(),
                    ..Default::default()
                };
                let change = self.panes.insert(
                    self.panes.len(),
                    id,
                    std::rc::Rc::new(std::cell::RefCell::new(row)),
                )?;
                self.cx.notify_list(P::PANES, &change)?;
            }
        }
        self.forest.preset(count);
        self.activeGroup = "main".into();
        self.maximized.clear();
        self.revision += 1;
        self.publish()
    }
    pub fn input(&mut self, pane: &str, text: &str, paste: bool) -> VmResult {
        self.broadcast_state.input(pane, text, paste);
        self.publish()
    }
}
