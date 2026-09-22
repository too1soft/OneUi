//! Deterministic drop hit testing and a non-destructive drag checkpoint.
use super::workspace::*;
use crate::Rect;
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
/// Leaf rearrangement: the inner 44% swaps; the nearest normalized edge
/// splits the target. Geometry is read-only until the application commits.
pub fn pane_drop<K: Clone + Ord>(
    source: &K,
    point: DockPoint,
    panes: &BTreeMap<K, Rect>,
) -> Option<PaneDrop<K>> {
    if !point.x.is_finite() || !point.y.is_finite() {
        return None;
    }
    for (target, r) in panes {
        if target == source || r.width <= 0.0 || r.height <= 0.0 || !contains(*r, point) {
            continue;
        }
        let x = (point.x - r.x) / r.width;
        let y = (point.y - r.y) / r.height;
        if x > 0.28 && x < 0.72 && y > 0.28 && y < 0.72 {
            return Some(PaneDrop {
                target: target.clone(),
                placement: PanePlacement::Swap,
                preview: *r,
            });
        }
        let edge = [
            (DockEdge::Left, x),
            (DockEdge::Right, 1.0 - x),
            (DockEdge::Top, y),
            (DockEdge::Bottom, 1.0 - y),
        ]
        .into_iter()
        .min_by(|a, b| a.1.total_cmp(&b.1))?
        .0;
        let mut preview = *r;
        match edge {
            DockEdge::Left => preview.width *= 0.5,
            DockEdge::Right => {
                preview.width *= 0.5;
                preview.x += preview.width;
            }
            DockEdge::Top => preview.height *= 0.5,
            DockEdge::Bottom => {
                preview.height *= 0.5;
                preview.y += preview.height;
            }
        }
        return Some(PaneDrop {
            target: target.clone(),
            placement: PanePlacement::Edge(edge),
            preview,
        });
    }
    None
}
use std::collections::{BTreeMap, BTreeSet};

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
fn contains(r: Rect, p: DockPoint) -> bool {
    p.x >= r.x && p.y >= r.y && p.x <= r.x + r.width && p.y <= r.y + r.height
}
/// Floating targets are excluded. Strict mode activates only a 42px target
/// edge or one of the four central direction buttons.
pub fn dock_guide<K: Clone + Ord>(
    source: &K,
    point: DockPoint,
    panes: &BTreeMap<K, Rect>,
    floating: &BTreeSet<K>,
    strict: bool,
) -> Option<DockGuide<K>> {
    for (target, r) in panes {
        if target == source || floating.contains(target) || !contains(*r, point) {
            continue;
        }
        let cx = r.x + r.width * 0.5;
        let cy = r.y + r.height * 0.5;
        let zones = [
            (
                DockEdge::Left,
                Rect {
                    x: cx - 68.0,
                    y: cy - 18.0,
                    width: 42.0,
                    height: 36.0,
                },
            ),
            (
                DockEdge::Right,
                Rect {
                    x: cx + 26.0,
                    y: cy - 18.0,
                    width: 42.0,
                    height: 36.0,
                },
            ),
            (
                DockEdge::Top,
                Rect {
                    x: cx - 21.0,
                    y: cy - 58.0,
                    width: 42.0,
                    height: 36.0,
                },
            ),
            (
                DockEdge::Bottom,
                Rect {
                    x: cx - 21.0,
                    y: cy + 22.0,
                    width: 42.0,
                    height: 36.0,
                },
            ),
        ];
        let edge = zones
            .iter()
            .find(|(_, z)| contains(*z, point))
            .map(|(e, _)| *e)
            .or_else(|| {
                let distances = [
                    (DockEdge::Left, point.x - r.x),
                    (DockEdge::Right, r.x + r.width - point.x),
                    (DockEdge::Top, point.y - r.y),
                    (DockEdge::Bottom, r.y + r.height - point.y),
                ];
                distances
                    .into_iter()
                    .min_by(|a, b| a.1.total_cmp(&b.1))
                    .and_then(|(e, d)| (!strict || d <= 42.0).then_some(e))
            });
        let drop = edge.map(|edge| {
            let mut preview = *r;
            match edge {
                DockEdge::Left => preview.width *= 0.5,
                DockEdge::Right => {
                    preview.x += r.width * 0.5;
                    preview.width *= 0.5;
                }
                DockEdge::Top => preview.height *= 0.5,
                DockEdge::Bottom => {
                    preview.y += r.height * 0.5;
                    preview.height *= 0.5;
                }
            }
            DockDrop {
                target: target.clone(),
                edge,
                preview,
            }
        });
        return Some(DockGuide {
            target: target.clone(),
            bounds: *r,
            drop,
        });
    }
    None
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
