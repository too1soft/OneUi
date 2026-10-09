//! Lossless conversion between generic Rust IDs and the shared SDK snapshot ABI.
use super::*;
use crate::sys;

pub(super) struct Encoded<K> {
    pub keys: Vec<K>,
    nodes: Vec<sys::OneUiDockTreeNode>,
    floating: Vec<sys::OneUiDockFloating>,
    hidden: Vec<sys::OneUiDockRestore>,
    root: i32,
    maximized: u64,
}
pub(super) fn rect(r: Rect) -> sys::OneUiRect {
    sys::OneUiRect {
        x: r.x,
        y: r.y,
        width: r.width,
        height: r.height,
    }
}
pub(super) fn rectangle(r: sys::OneUiRect) -> Rect {
    Rect {
        x: r.x,
        y: r.y,
        width: r.width,
        height: r.height,
    }
}
impl<K: Clone + Ord> Encoded<K> {
    pub fn new(state: &DockSnapshot<K>) -> Self {
        let mut seen = BTreeSet::new();
        if let Some(root) = &state.root {
            seen.extend(root.ids());
        }
        seen.extend(state.floating.keys().cloned());
        seen.extend(state.hidden.keys().cloned());
        if let Some(id) = &state.maximized {
            seen.insert(id.clone());
        }
        for old in state.hidden.values() {
            if let Some(target) = &old.target {
                seen.insert(target.clone());
            }
            for root in [&old.before, &old.after].into_iter().flatten() {
                seen.extend(root.ids());
            }
        }
        let mut value = Self {
            keys: seen.into_iter().collect(),
            nodes: Vec::new(),
            floating: Vec::new(),
            hidden: Vec::new(),
            root: -1,
            maximized: 0,
        };
        value.root = value.node(state.root.as_ref());
        value.maximized = state.maximized.as_ref().map(|id| value.id(id)).unwrap_or(0);
        for (id, pane) in &state.floating {
            value.floating.push(value.float(id, pane));
        }
        for (id, old) in &state.hidden {
            let before_root = value.node(old.before.as_ref());
            let after_root = value.node(old.after.as_ref());
            let floating = old
                .floating
                .as_ref()
                .map(|pane| value.float(id, pane))
                .unwrap_or(sys::OneUiDockFloating {
                    pane: 0,
                    bounds: rect(Rect {
                        x: 0.,
                        y: 0.,
                        width: 0.,
                        height: 0.,
                    }),
                    pinned: 0,
                    order: 0,
                });
            value.hidden.push(sys::OneUiDockRestore {
                pane: value.id(id),
                before_root,
                after_root,
                target: old.target.as_ref().map(|id| value.id(id)).unwrap_or(0),
                edge: old.edge as i32,
                ratio: old.ratio,
                has_floating: i32::from(old.floating.is_some()),
                floating,
            });
        }
        value
    }
    pub fn id(&self, id: &K) -> u64 {
        self.keys
            .binary_search(id)
            .map(|i| i as u64 + 1)
            .unwrap_or(0)
    }
    fn float(&self, id: &K, pane: &FloatingPane) -> sys::OneUiDockFloating {
        sys::OneUiDockFloating {
            pane: self.id(id),
            bounds: rect(pane.bounds),
            pinned: i32::from(pane.pinned),
            order: pane.order,
        }
    }
    fn node(&mut self, root: Option<&DockNode<K>>) -> i32 {
        let Some(root) = root else {
            return -1;
        };
        let index = self.nodes.len();
        self.nodes.push(sys::OneUiDockTreeNode {
            kind: 0,
            pane: 0,
            axis: 0,
            ratio: 0.5,
            first: -1,
            second: -1,
        });
        self.nodes[index] = match root {
            DockNode::Leaf(id) => sys::OneUiDockTreeNode {
                kind: 0,
                pane: self.id(id),
                axis: 0,
                ratio: 0.5,
                first: -1,
                second: -1,
            },
            DockNode::Split {
                axis,
                ratio,
                first,
                second,
            } => {
                let first = self.node(Some(first));
                let second = self.node(Some(second));
                sys::OneUiDockTreeNode {
                    kind: 1,
                    pane: 0,
                    axis: *axis as i32,
                    ratio: *ratio,
                    first,
                    second,
                }
            }
        };
        index as i32
    }
    pub fn raw(&self) -> sys::OneUiDockSnapshot {
        sys::OneUiDockSnapshot {
            nodes: self.nodes.as_ptr(),
            node_count: self.nodes.len(),
            root: self.root,
            floating: self.floating.as_ptr(),
            floating_count: self.floating.len(),
            hidden: self.hidden.as_ptr(),
            hidden_count: self.hidden.len(),
            maximized: self.maximized,
        }
    }
    pub unsafe fn decode(&self, source: &sys::OneUiDockSnapshot) -> DockSnapshot<K> {
        unsafe fn node<K: Clone>(
            source: &sys::OneUiDockSnapshot,
            keys: &[K],
            index: i32,
        ) -> Option<DockNode<K>> {
            if index < 0 {
                return None;
            }
            let value = &*source.nodes.add(index as usize);
            Some(if value.kind == 0 {
                DockNode::Leaf(keys[value.pane as usize - 1].clone())
            } else {
                DockNode::Split {
                    axis: if value.axis == 0 {
                        DockAxis::Horizontal
                    } else {
                        DockAxis::Vertical
                    },
                    ratio: value.ratio,
                    first: Box::new(node(source, keys, value.first).unwrap()),
                    second: Box::new(node(source, keys, value.second).unwrap()),
                }
            })
        }
        fn floating(value: sys::OneUiDockFloating) -> FloatingPane {
            FloatingPane {
                bounds: rectangle(value.bounds),
                pinned: value.pinned != 0,
                order: value.order,
            }
        }
        let mut result = DockSnapshot {
            root: node(source, &self.keys, source.root),
            floating: BTreeMap::new(),
            hidden: BTreeMap::new(),
            maximized: (source.maximized != 0)
                .then(|| self.keys[source.maximized as usize - 1].clone()),
        };
        for i in 0..source.floating_count {
            let value = *source.floating.add(i);
            result
                .floating
                .insert(self.keys[value.pane as usize - 1].clone(), floating(value));
        }
        for i in 0..source.hidden_count {
            let value = &*source.hidden.add(i);
            result.hidden.insert(
                self.keys[value.pane as usize - 1].clone(),
                DockRestore {
                    before: node(source, &self.keys, value.before_root),
                    after: node(source, &self.keys, value.after_root),
                    target: (value.target != 0)
                        .then(|| self.keys[value.target as usize - 1].clone()),
                    edge: match value.edge {
                        0 => DockEdge::Left,
                        1 => DockEdge::Right,
                        2 => DockEdge::Top,
                        _ => DockEdge::Bottom,
                    },
                    ratio: value.ratio,
                    floating: (value.has_floating != 0).then(|| floating(value.floating)),
                },
            );
        }
        result
    }
}
