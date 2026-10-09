//! Compatibility facade over the SDK's shared deterministic hit testing.
use super::workspace::*;
use crate::{sys, Rect};
use std::collections::{BTreeMap, BTreeSet};

#[derive(Clone, Copy, Debug)]
pub struct DockPoint {
    pub x: f32,
    pub y: f32,
}
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum PanePlacement {
    Edge(DockEdge),
    Swap,
}
#[derive(Clone, Debug)]
pub struct PaneDrop<K> {
    pub target: K,
    pub placement: PanePlacement,
    pub preview: Rect,
}
#[derive(Clone, Debug)]
pub struct DockDrop<K> {
    pub target: K,
    pub edge: DockEdge,
    pub preview: Rect,
}
#[derive(Clone, Debug)]
pub struct DockGuide<K> {
    pub target: K,
    pub bounds: Rect,
    pub drop: Option<DockDrop<K>>,
}
fn rectangle(r: sys::OneUiRect) -> Rect {
    Rect {
        x: r.x,
        y: r.y,
        width: r.width,
        height: r.height,
    }
}
fn edge(value: i32) -> Option<DockEdge> {
    match value {
        1 => Some(DockEdge::Left),
        2 => Some(DockEdge::Right),
        3 => Some(DockEdge::Top),
        4 => Some(DockEdge::Bottom),
        _ => None,
    }
}
fn hit<K: Clone + Ord>(
    source: &K,
    point: DockPoint,
    panes: &BTreeMap<K, Rect>,
    floating: Option<&BTreeSet<K>>,
    mode: i32,
) -> Option<(K, sys::OneUiDockHit)> {
    let keys: Vec<_> = panes.keys().collect();
    let source_id = keys
        .iter()
        .position(|key| *key == source)
        .map(|i| i as u64 + 1)
        .unwrap_or(0);
    let targets: Vec<_> = panes
        .iter()
        .enumerate()
        .map(|(i, (key, r))| sys::OneUiDockHitTarget {
            pane: i as u64 + 1,
            bounds: sys::OneUiRect {
                x: r.x,
                y: r.y,
                width: r.width,
                height: r.height,
            },
            floating: i32::from(floating.map(|f| f.contains(key)).unwrap_or(false)),
        })
        .collect();
    let mut result = sys::OneUiDockHit::default();
    let status = unsafe {
        sys::oneui_dock_hit_test(
            source_id,
            point.x,
            point.y,
            targets.as_ptr(),
            targets.len(),
            mode,
            &mut result,
        )
    };
    if status != 1 || result.target == 0 {
        return None;
    }
    Some(((*keys.get(result.target as usize - 1)?).clone(), result))
}
/// Leaf rearrangement: the inner 44% swaps; the nearest normalized edge splits.
/// Invalid geometry is ignored; no model or mounted widget is modified.
pub fn pane_drop<K: Clone + Ord>(
    source: &K,
    point: DockPoint,
    panes: &BTreeMap<K, Rect>,
) -> Option<PaneDrop<K>> {
    let (target, result) = hit(source, point, panes, None, 0)?;
    let placement = if result.placement == 5 {
        PanePlacement::Swap
    } else {
        PanePlacement::Edge(edge(result.placement)?)
    };
    Some(PaneDrop {
        target,
        placement,
        preview: rectangle(result.preview),
    })
}
/// Floating targets are excluded. Strict guides require a 42px target edge or
/// one of the four central direction buttons. The center may have no drop.
pub fn dock_guide<K: Clone + Ord>(
    source: &K,
    point: DockPoint,
    panes: &BTreeMap<K, Rect>,
    floating: &BTreeSet<K>,
    strict: bool,
) -> Option<DockGuide<K>> {
    let (target, result) = hit(
        source,
        point,
        panes,
        Some(floating),
        if strict { 1 } else { 2 },
    )?;
    let drop = edge(result.placement).map(|edge| DockDrop {
        target: target.clone(),
        edge,
        preview: rectangle(result.preview),
    });
    Some(DockGuide {
        target,
        bounds: rectangle(result.bounds),
        drop,
    })
}

pub struct DockDrag<K> {
    pub source: K,
    pub initial: DockSnapshot<K>,
    pub origin: DockPoint,
    pub shown: Rect,
}
impl<K: Clone + Ord> DockDrag<K> {
    pub fn moved_bounds(
        &self,
        pointer: DockPoint,
        viewport: Rect,
        minimum: PaneMinimum,
    ) -> Result<Rect, DockError> {
        clamp_floating(
            Rect {
                x: self.shown.x + pointer.x - self.origin.x,
                y: self.shown.y + pointer.y - self.origin.y,
                ..self.shown
            },
            viewport,
            minimum,
        )
    }
    /// Only release commits model changes. Escape/capture loss simply discards
    /// the checkpoint and redraws the model, so preview never loses a pane.
    pub fn commit(
        &self,
        model: &mut DockWorkspace<K>,
        drop: Option<&DockDrop<K>>,
        bounds: Rect,
    ) -> Result<(), DockError> {
        if let Some(drop) = drop {
            model.dock(&self.source, &drop.target, drop.edge)
        } else {
            model.float(&self.source, bounds)
        }
    }
}
