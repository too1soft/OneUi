//! Toolkit-owned workspace geometry. IDs identify mounted widgets, never sessions.
//! Layout edits move IDs and preserve the application's widget/controller objects.
use crate::Rect;
use std::collections::{BTreeMap, BTreeSet};

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum DockAxis {
    Horizontal,
    Vertical,
}
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum DockEdge {
    Left,
    Right,
    Top,
    Bottom,
}
impl DockEdge {
    pub fn axis(self) -> DockAxis {
        match self {
            Self::Left | Self::Right => DockAxis::Horizontal,
            _ => DockAxis::Vertical,
        }
    }
    pub fn before(self) -> bool {
        matches!(self, Self::Left | Self::Top)
    }
}
#[derive(Clone, Debug, PartialEq)]
pub enum DockNode<K> {
    Leaf(K),
    Split {
        axis: DockAxis,
        ratio: f32,
        first: Box<Self>,
        second: Box<Self>,
    },
}
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct PaneMinimum {
    pub width: f32,
    pub height: f32,
}
#[derive(Clone, Debug, PartialEq)]
pub struct FloatingPane {
    pub bounds: Rect,
    pub pinned: bool,
    pub order: u64,
}
#[derive(Clone, Debug, PartialEq)]
pub struct DockRestore<K> {
    pub before: Option<DockNode<K>>,
    pub after: Option<DockNode<K>>,
    pub target: Option<K>,
    pub edge: DockEdge,
    pub ratio: f32,
    pub floating: Option<FloatingPane>,
}
#[derive(Clone, Debug, PartialEq)]
pub struct DockSnapshot<K> {
    pub root: Option<DockNode<K>>,
    pub floating: BTreeMap<K, FloatingPane>,
    pub hidden: BTreeMap<K, DockRestore<K>>,
    pub maximized: Option<K>,
}
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum DockError {
    InvalidGeometry,
    DuplicateId,
    MissingPane,
    InvalidTarget,
}
#[derive(Clone, Debug)]
pub struct DividerGeometry {
    pub path: Vec<bool>,
    pub axis: DockAxis,
    pub bounds: Rect,
}
#[derive(Clone, Debug)]
pub struct DockGeometry<K> {
    pub panes: BTreeMap<K, Rect>,
    pub dividers: Vec<DividerGeometry>,
}
#[derive(Clone, Debug)]
pub struct DockWorkspace<K> {
    state: DockSnapshot<K>,
    sequence: u64,
}

#[path = "workspace_floating.rs"]
mod floating;
#[path = "workspace_native.rs"]
mod native;
#[path = "workspace_node.rs"]
mod node;
#[path = "workspace_snapshot.rs"]
mod snapshot;
pub use floating::{clamp_floating, resize_floating, FloatResizeEdge};
fn empty_rect() -> Rect {
    Rect {
        x: 0.,
        y: 0.,
        width: 0.,
        height: 0.,
    }
}

impl<K: Clone + Ord> DockWorkspace<K> {
    pub fn new(root: DockNode<K>) -> Result<Self, DockError> {
        Self::from_snapshot(native::root_state(root))
    }
    pub fn from_snapshot(state: DockSnapshot<K>) -> Result<Self, DockError> {
        let sequence = native::validate(&state)?;
        Ok(Self { state, sequence })
    }
    pub fn snapshot(&self) -> DockSnapshot<K> {
        self.state.clone()
    }
    pub fn restore(&mut self, state: DockSnapshot<K>) -> Result<(), DockError> {
        *self = Self::from_snapshot(state)?;
        Ok(())
    }
    pub fn visible(&self, id: &K) -> bool {
        self.state.floating.contains_key(id)
            || self
                .state
                .root
                .as_ref()
                .is_some_and(|node| node.contains(id))
    }
    fn edit(
        &mut self,
        command: i32,
        id: Option<&K>,
        target: Option<&K>,
        bounds: Rect,
        value: f32,
    ) -> Result<(), DockError> {
        let (state, sequence) = native::edit(
            &self.state,
            self.sequence,
            command,
            id,
            target,
            bounds,
            value,
            &[],
        )?;
        self.state = state;
        self.sequence = sequence;
        Ok(())
    }
    pub fn hide(&mut self, id: &K) -> Result<(), DockError> {
        self.edit(0, Some(id), None, empty_rect(), 0.)
    }
    pub fn show(&mut self, id: K) -> Result<(), DockError> {
        self.edit(1, Some(&id), None, empty_rect(), 0.)
    }
    pub fn dock(&mut self, id: &K, target: &K, edge: DockEdge) -> Result<(), DockError> {
        self.edit(2 + edge as i32, Some(id), Some(target), empty_rect(), 0.)
    }
    pub fn float(&mut self, id: &K, bounds: Rect) -> Result<(), DockError> {
        self.edit(6, Some(id), None, bounds, 0.)
    }
    pub fn redock(&mut self, id: &K) -> Result<(), DockError> {
        self.edit(7, Some(id), None, empty_rect(), 0.)
    }
    pub fn maximize(&mut self, id: &K) -> Result<(), DockError> {
        self.edit(8, Some(id), None, empty_rect(), 0.)
    }
    pub fn set_pinned(&mut self, id: &K, pinned: bool) -> Result<(), DockError> {
        self.edit(
            9,
            Some(id),
            None,
            empty_rect(),
            if pinned { 1. } else { 0. },
        )
    }
    pub fn raise(&mut self, id: &K) -> Result<(), DockError> {
        self.edit(10, Some(id), None, empty_rect(), 0.)
    }
    pub fn set_float_bounds(&mut self, id: &K, bounds: Rect) -> Result<(), DockError> {
        self.edit(11, Some(id), None, bounds, 0.)
    }
    pub fn paint_order(&self) -> Vec<K> {
        let mut panes: Vec<_> = self.state.floating.iter().collect();
        panes.sort_by_key(|(_, value)| (value.pinned, value.order));
        panes.into_iter().map(|(id, _)| id.clone()).collect()
    }
}
