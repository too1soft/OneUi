//! Native composition for [`DockWorkspace`](super::workspace::DockWorkspace).
//! Content widgets are registered once. Reparenting never reconstructs content.
use super::workspace::*;
use crate::{
    Color, Error, Insets, OverlayAlignment, OverlayHost, Panel, Rect, SplitOrientation, SplitView,
    Widget,
};
use std::{collections::BTreeMap, rc::Rc};

#[derive(Clone, Copy)]
pub struct DockSurfaceStyle {
    pub gap: f32,
    pub divider: Color,
    pub active_divider: Color,
}
/// Docked content and floating chrome can have different minimum dimensions.
#[derive(Clone, Copy)]
pub struct DockPaneMinimum {
    pub docked: PaneMinimum,
    pub floating: PaneMinimum,
}
impl From<PaneMinimum> for DockPaneMinimum {
    fn from(value: PaneMinimum) -> Self {
        Self {
            docked: value,
            floating: value,
        }
    }
}
impl Default for DockSurfaceStyle {
    fn default() -> Self {
        Self {
            gap: 5.0,
            divider: Color::rgba(0, 0, 0, 0),
            active_divider: Color::rgb(37, 99, 235),
        }
    }
}
/// Path, new ratio and whether input is committed. Preview callbacks also carry
/// rollback values when Escape cancels; callers persist only committed edits.
pub type DockRatioCallback = Rc<dyn Fn(Vec<bool>, f32, bool)>;
#[derive(Clone)]
enum Mount {
    Pane(Rc<Panel>),
    Split(Rc<SplitView>),
}
// Allocate the entire new tree before changing any content ownership links.
enum Prepared {
    Pane(Rc<Panel>),
    Split(Rc<SplitView>, Box<Prepared>, Box<Prepared>),
}
impl Prepared {
    fn attach(self, splits: &mut Vec<Rc<SplitView>>) -> Mount {
        match self {
            Self::Pane(p) => Mount::Pane(p),
            Self::Split(split, first, second) => {
                let first = first.attach(splits);
                let second = second.attach(splits);
                split.set_first(first.widget());
                split.set_second(second.widget());
                splits.push(Rc::clone(&split));
                Mount::Split(split)
            }
        }
    }
}
impl Mount {
    fn widget(&self) -> &Widget {
        match self {
            Self::Pane(p) => p.as_widget(),
            Self::Split(s) => s.as_widget(),
        }
    }
}

pub struct DockSurface<K> {
    host: OverlayHost,
    empty: Rc<Panel>,
    panes: BTreeMap<K, Rc<Panel>>,
    splits: Vec<Rc<SplitView>>,
    root: Option<Mount>,
    overlays: Vec<K>,
}
fn transparent_panel() -> Result<Rc<Panel>, Error> {
    let p = Rc::new(Panel::new()?);
    p.set_background(Color::rgba(0, 0, 0, 0));
    p.set_border(Color::rgba(0, 0, 0, 0), 0.0);
    Ok(p)
}
impl<K: Clone + Ord> DockSurface<K> {
    pub fn new() -> Result<Self, Error> {
        let host = OverlayHost::new()?;
        let empty = transparent_panel()?;
        host.set_content(empty.as_widget());
        Ok(Self {
            host,
            empty,
            panes: BTreeMap::new(),
            splits: Vec::new(),
            root: None,
            overlays: Vec::new(),
        })
    }
    pub fn as_widget(&self) -> &Widget {
        self.host.as_widget()
    }
    pub fn register(&mut self, id: K, content: &Widget) -> Result<(), Error> {
        let panel = if let Some(panel) = self.panes.get(&id) {
            Rc::clone(panel)
        } else {
            let p = transparent_panel()?;
            self.panes.insert(id, Rc::clone(&p));
            p
        };
        panel.set_content(content);
        Ok(())
    }
    pub fn pane_widget(&self, id: &K) -> Option<&Widget> {
        self.panes.get(id).map(|p| p.as_widget())
    }
    /// Call for topology/visibility changes. Use `update_floating` during a drag
    /// so the active input field keeps its existing native focus chain.
    pub fn mount(
        &mut self,
        state: &DockSnapshot<K>,
        viewport: Rect,
        compact: Option<&K>,
        style: DockSurfaceStyle,
        minimum: impl Fn(&K) -> DockPaneMinimum,
        callback: DockRatioCallback,
    ) -> Result<(), Error> {
        // Validate everything before detaching the current scene.
        let model =
            DockWorkspace::from_snapshot(state.clone()).map_err(|_| Error::InvalidLayout)?;
        if !style.gap.is_finite()
            || style.gap < 0.0
            || ![viewport.x, viewport.y, viewport.width, viewport.height]
                .iter()
                .all(|n| n.is_finite())
            || viewport.width < 0.0
            || viewport.height < 0.0
        {
            return Err(Error::InvalidLayout);
        }
        if compact.is_some_and(|id| !model.visible(id)) {
            return Err(Error::InvalidLayout);
        }
        let visible = if let Some(id) = compact.or(state.maximized.as_ref()) {
            vec![id.clone()]
        } else {
            let mut ids = state.root.as_ref().map(|n| n.ids()).unwrap_or_default();
            ids.extend(state.floating.keys().cloned());
            ids
        };
        if visible.iter().any(|id| !self.panes.contains_key(id)) {
            return Err(Error::WidgetDestroyed);
        }
        let minima: BTreeMap<_, _> = visible.iter().map(|id| (id.clone(), minimum(id))).collect();
        if minima
            .values()
            .flat_map(|m| [&m.docked, &m.floating])
            .any(|m| {
                !m.width.is_finite() || !m.height.is_finite() || m.width < 0.0 || m.height < 0.0
            })
        {
            return Err(Error::InvalidLayout);
        }
        let floats = if compact.is_none() && state.maximized.is_none() {
            model
                .paint_order()
                .into_iter()
                .map(|id| {
                    clamp_floating(state.floating[&id].bounds, viewport, minima[&id].floating)
                        .map(|r| (id, r))
                        .map_err(|_| Error::InvalidLayout)
                })
                .collect::<Result<Vec<_>, _>>()?
        } else {
            Vec::new()
        };
        let mut splits = Vec::new();
        fn build<K: Clone + Ord>(
            node: &DockNode<K>,
            panes: &BTreeMap<K, Rc<Panel>>,
            path: Vec<bool>,
            style: DockSurfaceStyle,
            min: &impl Fn(&K) -> PaneMinimum,
            callback: &DockRatioCallback,
        ) -> Result<Prepared, Error> {
            match node {
                DockNode::Leaf(id) => Ok(Prepared::Pane(Rc::clone(
                    panes.get(id).ok_or(Error::WidgetDestroyed)?,
                ))),
                DockNode::Split {
                    axis,
                    ratio,
                    first,
                    second,
                } => {
                    let mut ap = path.clone();
                    ap.push(false);
                    let mut bp = path.clone();
                    bp.push(true);
                    let a = build(first, panes, ap, style, min, callback)?;
                    let b = build(second, panes, bp, style, min, callback)?;
                    let mut split = SplitView::new(if *axis == DockAxis::Horizontal {
                        SplitOrientation::Horizontal
                    } else {
                        SplitOrientation::Vertical
                    })?;
                    split.set_gap(style.gap);
                    split.set_ratio(*ratio);
                    split.set_resizable(true);
                    split.set_divider_colors(style.divider, style.active_divider);
                    let amin = first.minimum(style.gap, min);
                    let bmin = second.minimum(style.gap, min);
                    split.set_minimum_pane_extent(
                        if *axis == DockAxis::Horizontal {
                            amin.width
                        } else {
                            amin.height
                        },
                        if *axis == DockAxis::Horizontal {
                            bmin.width
                        } else {
                            bmin.height
                        },
                    );
                    {
                        let cb = Rc::clone(callback);
                        let path = path.clone();
                        split.set_on_ratio_changed(move |ratio| cb(path.clone(), ratio, false));
                    }
                    {
                        let cb = Rc::clone(callback);
                        split.set_on_ratio_committed(move |ratio| cb(path.clone(), ratio, true));
                    }
                    let split = Rc::new(split);
                    Ok(Prepared::Split(split, Box::new(a), Box::new(b)))
                }
            }
        }
        let root = if let Some(id) = compact.or(state.maximized.as_ref()) {
            Some(Prepared::Pane(Rc::clone(self.panes.get(id).unwrap())))
        } else {
            state
                .root
                .as_ref()
                .map(|n| {
                    build(
                        n,
                        &self.panes,
                        Vec::new(),
                        style,
                        &|id| minima[id].docked,
                        &callback,
                    )
                })
                .transpose()?
        };
        let focus = self.host.as_widget().capture_focus();
        for id in &self.overlays {
            if let Some(panel) = self.panes.get(id) {
                self.host.remove_overlay(panel.as_widget());
            }
        }
        self.overlays.clear();
        self.host.set_content(self.empty.as_widget());
        // Drop old split owners before installing callbacks for the new tree.
        self.root = None;
        self.splits.clear();
        let root = root.map(|prepared| prepared.attach(&mut splits));
        for (id, panel) in &self.panes {
            panel.as_widget().set_visible(visible.contains(id));
        }
        self.host.set_content(
            root.as_ref()
                .map(Mount::widget)
                .unwrap_or(self.empty.as_widget()),
        );
        self.root = root;
        self.splits = splits;
        for (i, (id, r)) in floats.into_iter().enumerate() {
            self.host.add_anchored_overlay(
                self.panes[&id].as_widget(),
                20 + i as i32,
                r.width,
                r.height,
                Insets {
                    left: r.x - viewport.x,
                    top: r.y - viewport.y,
                    ..Default::default()
                },
                OverlayAlignment::Start,
                OverlayAlignment::Start,
            );
            self.overlays.push(id);
        }
        if let Some(focus) = focus {
            focus.restore(self.host.as_widget());
        }
        Ok(())
    }
    /// Uses display-only clamping; the caller's saved desktop bounds stay intact.
    pub fn update_floating(
        &self,
        state: &DockSnapshot<K>,
        viewport: Rect,
        minimum: impl Fn(&K) -> PaneMinimum,
    ) -> Result<(), Error> {
        let updates = self
            .overlays
            .iter()
            .map(|id| {
                let f = state.floating.get(id).ok_or(Error::InvalidLayout)?;
                let r = clamp_floating(f.bounds, viewport, minimum(id))
                    .map_err(|_| Error::InvalidLayout)?;
                Ok((id, r))
            })
            .collect::<Result<Vec<_>, Error>>()?;
        for (id, r) in updates {
            if !self.host.update_anchored_overlay(
                self.panes[id].as_widget(),
                r.width,
                r.height,
                Insets {
                    left: r.x - viewport.x,
                    top: r.y - viewport.y,
                    ..Default::default()
                },
                OverlayAlignment::Start,
                OverlayAlignment::Start,
            ) {
                return Err(Error::WidgetDestroyed);
            }
        }
        Ok(())
    }
}
