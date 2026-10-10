use crate::{
    delta::sync,
    page::Workspace_properties as P,
    rows::{Node, Placement},
    vm::Model,
};
use oneui::{
    layout::workspace::{DockAxis, DockEdge, DockNode, DockWorkspace},
    view::VmResult,
};
pub fn initial() -> DockWorkspace<String> {
    let leaf = |s: &str| DockNode::Leaf(s.into());
    let tools = DockNode::split(
        DockAxis::Vertical,
        0.55,
        leaf("sftp"),
        DockNode::split(DockAxis::Vertical, 0.6, leaf("monitor"), leaf("notes")).unwrap(),
    )
    .unwrap();
    DockWorkspace::new(
        DockNode::split(DockAxis::Horizontal, 0.74, leaf("terminal"), tools).unwrap(),
    )
    .unwrap()
}
pub fn edge(value: &str) -> DockEdge {
    match value {
        "left" => DockEdge::Left,
        "top" => DockEdge::Top,
        "bottom" => DockEdge::Bottom,
        _ => DockEdge::Right,
    }
}
impl Model {
    pub fn publish_tools(&mut self) -> VmResult {
        let state = self.tools.snapshot();
        let mut nodes = vec![];
        let mut work = vec![];
        self.toolRoot = if state.root.is_some() {
            "d".into()
        } else {
            String::new()
        };
        if let Some(root) = state.root.as_ref() {
            work.push(("d".to_owned(), root));
        }
        while let Some((id, node)) = work.pop() {
            match node {
                DockNode::Leaf(pane) => nodes.push((id.clone(), Node::leaf(id, pane.clone()))),
                DockNode::Split {
                    axis,
                    ratio,
                    first,
                    second,
                } => {
                    let a = format!("{id}0");
                    let b = format!("{id}1");
                    nodes.push((
                        id.clone(),
                        Node {
                            id,
                            kind: "split".into(),
                            axis: if *axis == DockAxis::Horizontal {
                                "horizontal"
                            } else {
                                "vertical"
                            }
                            .into(),
                            ratio: *ratio as f64,
                            first: a.clone(),
                            second: b.clone(),
                            pane: String::new(),
                        },
                    ));
                    work.push((b, second));
                    work.push((a, first));
                }
            }
        }
        sync(&mut self.toolNodes, nodes, &self.cx, P::TOOLNODES)?;
        let placements = ["terminal", "sftp", "monitor", "notes"]
            .iter()
            .map(|id| {
                // Restore records survive showing/redocking: they are anchors, not a
                // visibility bit. Geometry membership is authoritative.
                let f = state.floating.get(*id);
                let docked = state
                    .root
                    .as_ref()
                    .is_some_and(|n| n.contains(&(*id).to_string()));
                let value = Placement {
                    id: (*id).into(),
                    mode: if f.is_some() {
                        "floating"
                    } else if docked {
                        "docked"
                    } else {
                        "hidden"
                    }
                    .into(),
                    x: f.map_or(0., |f| f.bounds.x as f64),
                    y: f.map_or(0., |f| f.bounds.y as f64),
                    width: f.map_or(400., |f| f.bounds.width as f64),
                    height: f.map_or(360., |f| f.bounds.height as f64),
                    pinned: f.is_some_and(|f| f.pinned),
                    order: f.map_or(0, |f| f.order as i64),
                };
                ((*id).into(), value)
            })
            .collect();
        sync(&mut self.placements, placements, &self.cx, P::PLACEMENTS)?;
        self.toolMax = state.maximized.unwrap_or_default();
        self.toolRevision += 1;
        for p in [P::TOOLROOT, P::TOOLREVISION, P::TOOLMAX, P::COMPACT] {
            self.cx.notify(p)?;
        }
        Ok(())
    }
    pub fn tool_ratio(&mut self, split: &str, ratio: f64) {
        let mut state = self.tools.snapshot();
        if let Some(root) = state.root.as_mut() {
            let path: Vec<_> = split.chars().skip(1).map(|c| c == '1').collect();
            if root.set_ratio(&path, ratio as f32).is_ok() {
                self.tools.restore(state).unwrap();
            }
        }
    }
}
