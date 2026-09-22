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

fn finite_rect(r: Rect) -> bool {
    [r.x, r.y, r.width, r.height].iter().all(|v| v.is_finite()) && r.width >= 0.0 && r.height >= 0.0
}
fn valid_ratio(r: f32) -> bool {
    r.is_finite() && r > 0.0 && r < 1.0
}

impl<K: Clone + Ord> DockNode<K> {
    pub fn split(axis: DockAxis, ratio: f32, first: Self, second: Self) -> Result<Self, DockError> {
        let node = Self::Split {
            axis,
            ratio,
            first: Box::new(first),
            second: Box::new(second),
        };
        node.validate(&mut BTreeSet::new())?;
        Ok(node)
    }
    fn validate(&self, seen: &mut BTreeSet<K>) -> Result<(), DockError> {
        match self {
            Self::Leaf(id) => {
                if seen.insert(id.clone()) {
                    Ok(())
                } else {
                    Err(DockError::DuplicateId)
                }
            }
            Self::Split {
                ratio,
                first,
                second,
                ..
            } => {
                if !valid_ratio(*ratio) {
                    return Err(DockError::InvalidGeometry);
                }
                first.validate(seen)?;
                second.validate(seen)
            }
        }
    }
    pub fn ids(&self) -> Vec<K> {
        match self {
            Self::Leaf(id) => vec![id.clone()],
            Self::Split { first, second, .. } => {
                let mut ids = first.ids();
                ids.extend(second.ids());
                ids
            }
        }
    }
    pub fn contains(&self, id: &K) -> bool {
        match self {
            Self::Leaf(key) => key == id,
            Self::Split { first, second, .. } => first.contains(id) || second.contains(id),
        }
    }
    pub fn without(self, id: &K) -> Option<Self> {
        match self {
            Self::Leaf(key) => (key != *id).then_some(Self::Leaf(key)),
            Self::Split {
                axis,
                ratio,
                first,
                second,
            } => match (first.without(id), second.without(id)) {
                (Some(a), Some(b)) => Some(Self::Split {
                    axis,
                    ratio,
                    first: Box::new(a),
                    second: Box::new(b),
                }),
                (a, b) => a.or(b),
            },
        }
    }
    fn replace(&mut self, id: &K, next: Self) -> bool {
        match self {
            Self::Leaf(key) if key == id => {
                *self = next;
                true
            }
            Self::Leaf(_) => false,
            Self::Split { first, second, .. } => {
                if first.contains(id) {
                    first.replace(id, next)
                } else {
                    second.replace(id, next)
                }
            }
        }
    }
    pub fn swap(&mut self, a: &K, b: &K) -> Result<(), DockError> {
        if !self.contains(a) || !self.contains(b) {
            return Err(DockError::MissingPane);
        }
        fn walk<K: Clone + Ord>(n: &mut DockNode<K>, a: &K, b: &K) {
            match n {
                DockNode::Leaf(id) => {
                    if id == a {
                        *id = b.clone()
                    } else if id == b {
                        *id = a.clone()
                    }
                }
                DockNode::Split { first, second, .. } => {
                    walk(first, a, b);
                    walk(second, a, b);
                }
            }
        }
        walk(self, a, b);
        Ok(())
    }
    pub fn balance(&mut self) {
        if let Self::Split {
            ratio,
            first,
            second,
            ..
        } = self
        {
            *ratio = 0.5;
            first.balance();
            second.balance();
        }
    }
    pub fn set_ratio(&mut self, path: &[bool], value: f32) -> Result<(), DockError> {
        if !valid_ratio(value) {
            return Err(DockError::InvalidGeometry);
        }
        match self {
            Self::Split {
                ratio,
                first,
                second,
                ..
            } => {
                if let Some((side, rest)) = path.split_first() {
                    if *side {
                        second.set_ratio(rest, value)
                    } else {
                        first.set_ratio(rest, value)
                    }
                } else {
                    *ratio = value;
                    Ok(())
                }
            }
            _ => Err(DockError::InvalidTarget),
        }
    }
    fn location(&self, id: &K) -> Option<(K, DockEdge, f32)> {
        match self {
            Self::Leaf(_) => None,
            Self::Split {
                axis,
                ratio,
                first,
                second,
            } => {
                let edge = match axis {
                    DockAxis::Horizontal => DockEdge::Left,
                    DockAxis::Vertical => DockEdge::Top,
                };
                if matches!(first.as_ref(),Self::Leaf(key) if key==id) {
                    Some((second.ids()[0].clone(), edge, *ratio))
                } else if matches!(second.as_ref(),Self::Leaf(key) if key==id) {
                    Some((
                        first.ids()[0].clone(),
                        match axis {
                            DockAxis::Horizontal => DockEdge::Right,
                            DockAxis::Vertical => DockEdge::Bottom,
                        },
                        *ratio,
                    ))
                } else {
                    first.location(id).or_else(|| second.location(id))
                }
            }
        }
    }
    /// Minimum content extent, useful for a scrolling workspace. Use the same
    /// finite nonnegative gap/policy supplied to `geometry`.
    pub fn minimum(&self, gap: f32, policy: &impl Fn(&K) -> PaneMinimum) -> PaneMinimum {
        match self {
            Self::Leaf(id) => policy(id),
            Self::Split {
                axis,
                first,
                second,
                ..
            } => {
                let a = first.minimum(gap, policy);
                let b = second.minimum(gap, policy);
                match axis {
                    DockAxis::Horizontal => PaneMinimum {
                        width: a.width + b.width + gap,
                        height: a.height.max(b.height),
                    },
                    DockAxis::Vertical => PaneMinimum {
                        width: a.width.max(b.width),
                        height: a.height + b.height + gap,
                    },
                }
            }
        }
    }
    pub fn geometry(
        &self,
        bounds: Rect,
        gap: f32,
        policy: impl Fn(&K) -> PaneMinimum,
    ) -> Result<DockGeometry<K>, DockError> {
        self.validate(&mut BTreeSet::new())?;
        if !finite_rect(bounds)
            || !gap.is_finite()
            || gap < 0.0
            || self.ids().iter().any(|id| {
                let m = policy(id);
                !m.width.is_finite() || !m.height.is_finite() || m.width < 0.0 || m.height < 0.0
            })
        {
            return Err(DockError::InvalidGeometry);
        }
        let mut result = DockGeometry {
            panes: BTreeMap::new(),
            dividers: Vec::new(),
        };
        fn walk<K: Clone + Ord>(
            n: &DockNode<K>,
            r: Rect,
            gap: f32,
            policy: &impl Fn(&K) -> PaneMinimum,
            path: Vec<bool>,
            out: &mut DockGeometry<K>,
        ) {
            match n {
                DockNode::Leaf(id) => {
                    out.panes.insert(id.clone(), r);
                }
                DockNode::Split {
                    axis,
                    ratio,
                    first,
                    second,
                } => {
                    let x = *axis == DockAxis::Horizontal;
                    let extent = if x { r.width } else { r.height };
                    let gap = gap.min(extent);
                    let available = extent - gap;
                    let a = first.minimum(gap, policy);
                    let b = second.minimum(gap, policy);
                    let (amin, bmin) = if x {
                        (a.width, b.width)
                    } else {
                        (a.height, b.height)
                    };
                    let amount = if amin + bmin <= available {
                        (available * ratio).clamp(amin, available - bmin)
                    } else {
                        available * ratio
                    };
                    let (mut ra, mut rb, mut divider) = (r, r, r);
                    if x {
                        ra.width = amount;
                        rb.x += amount + gap;
                        rb.width = available - amount;
                        divider.x += amount;
                        divider.width = gap;
                    } else {
                        ra.height = amount;
                        rb.y += amount + gap;
                        rb.height = available - amount;
                        divider.y += amount;
                        divider.height = gap;
                    }
                    out.dividers.push(DividerGeometry {
                        path: path.clone(),
                        axis: *axis,
                        bounds: divider,
                    });
                    let mut ap = path.clone();
                    ap.push(false);
                    let mut bp = path;
                    bp.push(true);
                    walk(first, ra, gap, policy, ap, out);
                    walk(second, rb, gap, policy, bp, out);
                }
            }
        }
        walk(self, bounds, gap, &policy, Vec::new(), &mut result);
        Ok(result)
    }
}

impl<K: Clone + Ord> DockWorkspace<K> {
    pub fn new(root: DockNode<K>) -> Result<Self, DockError> {
        Self::from_snapshot(DockSnapshot {
            root: Some(root),
            floating: BTreeMap::new(),
            hidden: BTreeMap::new(),
            maximized: None,
        })
    }
    pub fn from_snapshot(state: DockSnapshot<K>) -> Result<Self, DockError> {
        let mut seen = BTreeSet::new();
        if let Some(root) = &state.root {
            root.validate(&mut seen)?;
        }
        for (id, f) in &state.floating {
            if !seen.insert(id.clone()) {
                return Err(DockError::DuplicateId);
            }
            if !finite_rect(f.bounds) || f.bounds.width == 0.0 || f.bounds.height == 0.0 {
                return Err(DockError::InvalidGeometry);
            }
        }
        if state
            .maximized
            .as_ref()
            .is_some_and(|id| !seen.contains(id))
        {
            return Err(DockError::MissingPane);
        }
        for old in state.hidden.values() {
            if !valid_ratio(old.ratio) {
                return Err(DockError::InvalidGeometry);
            }
            for root in [&old.before, &old.after].into_iter().flatten() {
                root.validate(&mut BTreeSet::new())?;
            }
            if old.floating.as_ref().is_some_and(|f| {
                !finite_rect(f.bounds) || f.bounds.width == 0.0 || f.bounds.height == 0.0
            }) {
                return Err(DockError::InvalidGeometry);
            }
        }
        let sequence = state.floating.values().map(|f| f.order).max().unwrap_or(0);
        Ok(Self { state, sequence })
    }
    pub fn snapshot(&self) -> DockSnapshot<K> {
        self.state.clone()
    }
    /// Restores an edit checkpoint atomically (Escape), or validated persisted state.
    pub fn restore(&mut self, state: DockSnapshot<K>) -> Result<(), DockError> {
        *self = Self::from_snapshot(state)?;
        Ok(())
    }
    pub fn visible(&self, id: &K) -> bool {
        self.state.floating.contains_key(id)
            || self.state.root.as_ref().is_some_and(|n| n.contains(id))
    }
    fn remember(&mut self, id: &K) {
        if let Some(f) = self.state.floating.get(id) {
            let mut old = self.state.hidden.get(id).cloned().unwrap_or(DockRestore {
                before: None,
                after: None,
                target: None,
                edge: DockEdge::Right,
                ratio: 0.5,
                floating: None,
            });
            old.floating = Some(f.clone());
            self.state.hidden.insert(id.clone(), old);
            return;
        }
        if let Some(root) = self.state.root.as_ref().filter(|n| n.contains(id)) {
            let (target, edge, ratio) = root
                .location(id)
                .map(|(k, e, r)| (Some(k), e, r))
                .unwrap_or((None, DockEdge::Right, 0.5));
            self.state.hidden.insert(
                id.clone(),
                DockRestore {
                    before: Some(root.clone()),
                    after: root.clone().without(id),
                    target,
                    edge,
                    ratio,
                    floating: None,
                },
            );
        }
    }
    pub fn hide(&mut self, id: &K) -> Result<(), DockError> {
        if !self.visible(id) {
            return Err(DockError::MissingPane);
        }
        self.remember(id);
        self.state.root = self.state.root.take().and_then(|n| n.without(id));
        self.state.floating.remove(id);
        if self.state.maximized.as_ref() == Some(id) {
            self.state.maximized = None;
        }
        Ok(())
    }
    pub fn show(&mut self, id: K) -> Result<(), DockError> {
        if self.visible(&id) {
            return Ok(());
        }
        let old = self
            .state
            .hidden
            .get(&id)
            .cloned()
            .ok_or(DockError::MissingPane)?;
        if let Some(f) = old.floating {
            self.state.floating.insert(id.clone(), f);
            self.raise(&id)?;
        } else if self.state.root == old.after
            && old.before.as_ref().is_some_and(|n| {
                n.ids()
                    .iter()
                    .all(|key| key == &id || !self.state.floating.contains_key(key))
            })
        {
            self.state.root = old.before;
        } else if let Some(target) = old
            .target
            .filter(|key| self.state.root.as_ref().is_some_and(|n| n.contains(key)))
        {
            self.insert(id, &target, old.edge, old.ratio)?;
        } else {
            self.state.root = Some(match self.state.root.take() {
                Some(root) => DockNode::Split {
                    axis: DockAxis::Horizontal,
                    ratio: 0.7,
                    first: Box::new(root),
                    second: Box::new(DockNode::Leaf(id)),
                },
                None => DockNode::Leaf(id),
            });
        }
        self.state.maximized = None;
        Ok(())
    }
    fn insert(&mut self, id: K, target: &K, edge: DockEdge, ratio: f32) -> Result<(), DockError> {
        let root = self.state.root.as_mut().ok_or(DockError::InvalidTarget)?;
        if !root.contains(target) {
            return Err(DockError::InvalidTarget);
        }
        let (a, b) = if edge.before() {
            (id, target.clone())
        } else {
            (target.clone(), id)
        };
        root.replace(
            target,
            DockNode::Split {
                axis: edge.axis(),
                ratio,
                first: Box::new(DockNode::Leaf(a)),
                second: Box::new(DockNode::Leaf(b)),
            },
        );
        Ok(())
    }
    /// Split only the target, retaining all surrounding branches and floating panes.
    pub fn dock(&mut self, id: &K, target: &K, edge: DockEdge) -> Result<(), DockError> {
        if id == target
            || !self.visible(id)
            || !self.state.root.as_ref().is_some_and(|n| n.contains(target))
        {
            return Err(DockError::InvalidTarget);
        }
        self.state.root = self.state.root.take().and_then(|n| n.without(id));
        self.state.floating.remove(id);
        self.insert(id.clone(), target, edge, 0.5)?;
        self.state.hidden.remove(id);
        self.state.maximized = None;
        Ok(())
    }
    pub fn float(&mut self, id: &K, bounds: Rect) -> Result<(), DockError> {
        if !finite_rect(bounds) || bounds.width <= 0.0 || bounds.height <= 0.0 {
            return Err(DockError::InvalidGeometry);
        }
        if !self.visible(id) {
            return Err(DockError::MissingPane);
        }
        if !self.state.floating.contains_key(id) {
            self.remember(id);
            self.state.root = self.state.root.take().and_then(|n| n.without(id));
        }
        let pinned = self.state.floating.get(id).is_some_and(|f| f.pinned);
        self.state.floating.insert(
            id.clone(),
            FloatingPane {
                bounds,
                pinned,
                order: 0,
            },
        );
        self.raise(id)?;
        self.state.maximized = None;
        Ok(())
    }
    pub fn redock(&mut self, id: &K) -> Result<(), DockError> {
        if !self.state.floating.contains_key(id) {
            return Err(DockError::MissingPane);
        }
        if !self.state.hidden.contains_key(id) {
            self.state.hidden.insert(
                id.clone(),
                DockRestore {
                    before: None,
                    after: None,
                    target: self
                        .state
                        .root
                        .as_ref()
                        .and_then(|n| n.ids().first().cloned()),
                    edge: DockEdge::Right,
                    ratio: 0.5,
                    floating: None,
                },
            );
        }
        self.state.floating.remove(id);
        // Floating movement must not overwrite the remembered docking tree.
        self.state.hidden.get_mut(id).unwrap().floating = None;
        self.show(id.clone())
    }
    pub fn maximize(&mut self, id: &K) -> Result<(), DockError> {
        if !self.visible(id) {
            return Err(DockError::MissingPane);
        }
        self.state.maximized = if self.state.maximized.as_ref() == Some(id) {
            None
        } else {
            Some(id.clone())
        };
        Ok(())
    }
    pub fn set_pinned(&mut self, id: &K, pinned: bool) -> Result<(), DockError> {
        self.state
            .floating
            .get_mut(id)
            .ok_or(DockError::MissingPane)?
            .pinned = pinned;
        self.raise(id)
    }
    pub fn raise(&mut self, id: &K) -> Result<(), DockError> {
        if self.sequence == u64::MAX {
            let order = self.paint_order();
            for (i, key) in order.iter().enumerate() {
                self.state.floating.get_mut(key).unwrap().order = i as u64;
            }
            self.sequence = order.len() as u64;
        }
        self.sequence += 1;
        self.state
            .floating
            .get_mut(id)
            .ok_or(DockError::MissingPane)?
            .order = self.sequence;
        Ok(())
    }
    pub fn paint_order(&self) -> Vec<K> {
        let mut panes = self.state.floating.iter().collect::<Vec<_>>();
        panes.sort_by_key(|(_, f)| (f.pinned, f.order));
        panes.into_iter().map(|(id, _)| id.clone()).collect()
    }
    pub fn set_float_bounds(&mut self, id: &K, bounds: Rect) -> Result<(), DockError> {
        if !finite_rect(bounds) || bounds.width <= 0.0 || bounds.height <= 0.0 {
            return Err(DockError::InvalidGeometry);
        }
        self.state
            .floating
            .get_mut(id)
            .ok_or(DockError::MissingPane)?
            .bounds = bounds;
        Ok(())
    }
}

/// Constrain a floating window without persisting compact-window geometry.
pub fn clamp_floating(
    bounds: Rect,
    workspace: Rect,
    minimum: PaneMinimum,
) -> Result<Rect, DockError> {
    if !finite_rect(bounds)
        || !finite_rect(workspace)
        || ![minimum.width, minimum.height]
            .iter()
            .all(|v| v.is_finite() && *v >= 0.0)
    {
        return Err(DockError::InvalidGeometry);
    }
    let width = bounds.width.max(minimum.width).min(workspace.width);
    let height = bounds.height.max(minimum.height).min(workspace.height);
    Ok(Rect {
        x: bounds
            .x
            .clamp(workspace.x, workspace.x + workspace.width - width),
        y: bounds
            .y
            .clamp(workspace.y, workspace.y + workspace.height - height),
        width,
        height,
    })
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum FloatResizeEdge {
    North,
    South,
    East,
    West,
    NorthEast,
    NorthWest,
    SouthEast,
    SouthWest,
}
/// Compute one resize preview from the original pointer-down rectangle. Using
/// the original rectangle makes reversing a drag after hitting a limit stable.
pub fn resize_floating(
    start: Rect,
    delta: (f32, f32),
    edge: FloatResizeEdge,
    workspace: Rect,
    minimum: PaneMinimum,
) -> Result<Rect, DockError> {
    if !delta.0.is_finite() || !delta.1.is_finite() {
        return Err(DockError::InvalidGeometry);
    }
    let mut result = clamp_floating(start, workspace, minimum)?;
    let start = result;
    let w = minimum.width.min(workspace.width);
    let h = minimum.height.min(workspace.height);
    if matches!(
        edge,
        FloatResizeEdge::East | FloatResizeEdge::NorthEast | FloatResizeEdge::SouthEast
    ) {
        result.width = (start.width + delta.0).clamp(w, workspace.x + workspace.width - start.x);
    }
    if matches!(
        edge,
        FloatResizeEdge::South | FloatResizeEdge::SouthEast | FloatResizeEdge::SouthWest
    ) {
        result.height = (start.height + delta.1).clamp(h, workspace.y + workspace.height - start.y);
    }
    if matches!(
        edge,
        FloatResizeEdge::West | FloatResizeEdge::NorthWest | FloatResizeEdge::SouthWest
    ) {
        result.x = (start.x + delta.0).clamp(workspace.x, start.x + start.width - w);
        result.width = start.x + start.width - result.x;
    }
    if matches!(
        edge,
        FloatResizeEdge::North | FloatResizeEdge::NorthEast | FloatResizeEdge::NorthWest
    ) {
        result.y = (start.y + delta.1).clamp(workspace.y, start.y + start.height - h);
        result.height = start.y + start.height - result.y;
    }
    Ok(result)
}
