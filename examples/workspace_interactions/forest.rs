use crate::rows::{Group, Node};
#[derive(Clone)]
pub struct Forest {
    pub nodes: Vec<Node>,
    pub groups: Vec<Group>,
    serial: usize,
}
impl Node {
    pub fn leaf(id: String, pane: String) -> Self {
        Self {
            id,
            kind: "leaf".into(),
            pane,
            ratio: 0.5,
            ..Self::default()
        }
    }
}
impl Forest {
    pub fn new() -> Self {
        let mut s = Self {
            nodes: vec![],
            groups: vec![],
            serial: 100,
        };
        s.preset(1);
        s
    }
    pub fn preset(&mut self, count: usize) {
        self.nodes.clear();
        self.groups.clear();
        let keys: Vec<_> = (0..16).map(|i| format!("session/{i}")).collect();
        let root = self.balanced(&keys[..count], 0);
        self.groups.push(Group {
            id: "main".into(),
            root,
        });
        for key in &keys[count..] {
            self.nodes.push(Node::leaf(key.clone(), key.clone()));
            self.groups.push(Group {
                id: key.clone(),
                root: key.clone(),
            });
        }
    }
    fn balanced(&mut self, keys: &[String], depth: usize) -> String {
        if keys.len() == 1 {
            self.nodes
                .push(Node::leaf(keys[0].clone(), keys[0].clone()));
            return keys[0].clone();
        }
        let middle = keys.len() / 2;
        let first = self.balanced(&keys[..middle], depth + 1);
        let second = self.balanced(&keys[middle..], depth + 1);
        let id = format!("split:{}:{}", keys[0], keys.last().unwrap());
        self.nodes.push(Node {
            id: id.clone(),
            kind: "split".into(),
            axis: if depth % 2 == 0 {
                "horizontal"
            } else {
                "vertical"
            }
            .into(),
            ratio: 0.5,
            first,
            second,
            pane: String::new(),
        });
        id
    }
    pub fn leaves(&self, group: &str) -> Vec<String> {
        let Some(group) = self.groups.iter().find(|g| g.id == group) else {
            return vec![];
        };
        let mut stack = vec![group.root.as_str()];
        let mut out = vec![];
        while let Some(id) = stack.pop() {
            if let Some(n) = self.nodes.iter().find(|n| n.id == id) {
                if n.kind == "leaf" {
                    out.push(n.pane.clone());
                } else {
                    stack.push(&n.second);
                    stack.push(&n.first);
                }
            }
        }
        out
    }
    pub fn group_of(&self, pane: &str) -> Option<String> {
        self.groups
            .iter()
            .find(|g| self.leaves(&g.id).iter().any(|p| p == pane))
            .map(|g| g.id.clone())
    }
    pub fn detach(&mut self, pane: &str) -> bool {
        let Some(leaf) = self.nodes.iter().find(|n| n.pane == pane).cloned() else {
            return false;
        };
        if let Some(parent) = self
            .nodes
            .iter()
            .find(|n| n.first == leaf.id || n.second == leaf.id)
            .cloned()
        {
            let sibling = if parent.first == leaf.id {
                parent.second.clone()
            } else {
                parent.first.clone()
            };
            for n in &mut self.nodes {
                if n.first == parent.id {
                    n.first = sibling.clone();
                }
                if n.second == parent.id {
                    n.second = sibling.clone();
                }
            }
            for g in &mut self.groups {
                if g.root == parent.id {
                    g.root = sibling.clone();
                }
            }
            self.nodes.retain(|n| n.id != parent.id);
        } else {
            self.groups.retain(|g| g.root != leaf.id);
        }
        self.nodes.retain(|n| n.id != leaf.id);
        true
    }
    pub fn split(&mut self, source: &str, target: &str, edge: &str) -> bool {
        if source == target || !self.nodes.iter().any(|n| n.pane == target) {
            return false;
        }
        self.detach(source);
        let leaf = self.nodes.iter_mut().find(|n| n.pane == target).unwrap();
        self.serial += 1;
        let old = Node::leaf(format!("node-{}", self.serial), target.into());
        self.serial += 1;
        let added = Node::leaf(format!("node-{}", self.serial), source.into());
        leaf.kind = "split".into();
        leaf.pane.clear();
        leaf.ratio = 0.5;
        leaf.axis = if edge == "left" || edge == "right" {
            "horizontal"
        } else {
            "vertical"
        }
        .into();
        let before = edge == "left" || edge == "top";
        leaf.first = if before {
            added.id.clone()
        } else {
            old.id.clone()
        };
        leaf.second = if before {
            old.id.clone()
        } else {
            added.id.clone()
        };
        self.nodes.push(old);
        self.nodes.push(added);
        true
    }
    pub fn tab(&mut self, pane: &str) {
        if self.detach(pane) {
            self.serial += 1;
            let root = format!("node-{}", self.serial);
            self.nodes.push(Node::leaf(root.clone(), pane.into()));
            self.groups.push(Group {
                id: format!("tab-{}", self.serial),
                root,
            });
        }
    }
    pub fn swap(&mut self, a: &str, b: &str) {
        for n in &mut self.nodes {
            if n.pane == a {
                n.pane = b.into();
            } else if n.pane == b {
                n.pane = a.into();
            }
        }
    }
}
