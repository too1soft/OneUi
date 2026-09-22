use oneui::layout::workspace::*;
use oneui::layout::workspace_gesture::{dock_guide, DockDrag, DockPoint};
use oneui::Rect;
use std::collections::BTreeMap;

#[test]
fn pane_drop_uses_relative_edges_and_a_center_swap_zone() {
    use oneui::layout::workspace_gesture::{pane_drop, PanePlacement};
    let panes = BTreeMap::from([
        ("a", rect(0., 0., 200., 400.)),
        ("b", rect(205., 0., 800., 400.)),
    ]);
    let hit = |x, y| pane_drop(&"a", DockPoint { x, y }, &panes);
    let center = hit(605., 200.).unwrap();
    assert_eq!(center.target, "b");
    assert_eq!(center.placement, PanePlacement::Swap);
    assert_eq!(center.preview, rect(205., 0., 800., 400.));
    for (x, y, edge, preview) in [
        (215., 200., DockEdge::Left, rect(205., 0., 400., 400.)),
        (995., 200., DockEdge::Right, rect(605., 0., 400., 400.)),
        (605., 10., DockEdge::Top, rect(205., 0., 800., 200.)),
        (605., 390., DockEdge::Bottom, rect(205., 200., 800., 200.)),
    ] {
        let target = hit(x, y).unwrap();
        assert_eq!(target.placement, PanePlacement::Edge(edge));
        assert_eq!(target.preview, preview);
    }
    assert!(hit(100., 200.).is_none());
    assert!(hit(f32::NAN, 200.).is_none());
    assert!(hit(1100., 200.).is_none());
}

#[test]
fn dock_guides_resolve_central_arrows_and_ignore_floating_targets() {
    let panes = BTreeMap::from([
        ("sftp", rect(0., 0., 400., 800.)),
        ("terminal", rect(405., 0., 800., 800.)),
    ]);
    let floats = Default::default();
    let guide = dock_guide(&"ai", DockPoint { x: 200., y: 440. }, &panes, &floats, true).unwrap();
    assert_eq!(guide.drop.unwrap().edge, DockEdge::Bottom);
    assert!(
        dock_guide(&"ai", DockPoint { x: 200., y: 300. }, &panes, &floats, true)
            .unwrap()
            .drop
            .is_none()
    );
    assert_eq!(
        dock_guide(&"ai", DockPoint { x: 15., y: 300. }, &panes, &floats, true)
            .unwrap()
            .drop
            .unwrap()
            .edge,
        DockEdge::Left
    );
    let floats = std::collections::BTreeSet::from(["sftp"]);
    assert!(dock_guide(
        &"ai",
        DockPoint { x: 200., y: 440. },
        &panes,
        &floats,
        false
    )
    .is_none());
    assert!(dock_guide(
        &"sftp",
        DockPoint { x: 200., y: 440. },
        &panes,
        &Default::default(),
        false
    )
    .is_none());
}

#[test]
fn drag_preview_is_non_destructive_until_release() {
    let mut model = sample();
    let initial = model.snapshot();
    let drag = DockDrag {
        source: "ai",
        initial: initial.clone(),
        origin: DockPoint { x: 100., y: 100. },
        shown: rect(80., 80., 300., 240.),
    };
    let moved = drag
        .moved_bounds(
            DockPoint { x: 230., y: 180. },
            rect(0., 0., 1400., 900.),
            PaneMinimum {
                width: 240.,
                height: 160.,
            },
        )
        .unwrap();
    assert_eq!(moved, rect(210., 160., 300., 240.));
    assert_eq!(
        model.snapshot(),
        initial,
        "cancel can discard preview without model rollback"
    );
    drag.commit(&mut model, None, moved).unwrap();
    assert_eq!(model.snapshot().floating["ai"].bounds, moved);
    assert!(!model.snapshot().root.unwrap().contains(&"ai"));
}
fn rect(x: f32, y: f32, width: f32, height: f32) -> Rect {
    Rect {
        x,
        y,
        width,
        height,
    }
}
fn leaf(id: &'static str) -> DockNode<&'static str> {
    DockNode::Leaf(id)
}
fn sample() -> DockWorkspace<&'static str> {
    DockWorkspace::new(
        DockNode::split(
            DockAxis::Horizontal,
            0.2,
            leaf("sftp"),
            DockNode::split(
                DockAxis::Horizontal,
                0.72,
                leaf("terminal"),
                DockNode::split(DockAxis::Vertical, 0.5, leaf("monitor"), leaf("ai")).unwrap(),
            )
            .unwrap(),
        )
        .unwrap(),
    )
    .unwrap()
}
fn geometry(root: &DockNode<&'static str>) -> DockGeometry<&'static str> {
    root.geometry(rect(0., 0., 1530., 868.), 5., |_| PaneMinimum {
        width: 100.,
        height: 100.,
    })
    .unwrap()
}
#[test]
fn relative_docking_preserves_surrounding_geometry_and_float_state() {
    let mut layout = sample();
    layout
        .float(&"monitor", rect(1000., 10., 380., 840.))
        .unwrap();
    layout.set_pinned(&"monitor", true).unwrap();
    layout.float(&"ai", rect(20., 360., 340., 480.)).unwrap();
    let before = layout.snapshot();
    let old = geometry(before.root.as_ref().unwrap());
    layout.dock(&"ai", &"sftp", DockEdge::Bottom).unwrap();
    let after = layout.snapshot();
    let new = geometry(after.root.as_ref().unwrap());
    assert_eq!(old.panes["terminal"], new.panes["terminal"]);
    assert_eq!(before.floating["monitor"], after.floating["monitor"]);
    assert!(!after.floating.contains_key("ai"));
    assert_eq!(new.panes["ai"].height, new.panes["sftp"].height);
    assert_eq!(new.panes["ai"].x, new.panes["sftp"].x);
    assert_eq!(
        DockWorkspace::from_snapshot(after.clone())
            .unwrap()
            .snapshot(),
        after
    );
}
#[test]
fn repeated_hide_restore_returns_exact_tree_and_floating_bounds() {
    let mut layout = sample();
    let before = layout.snapshot();
    for _ in 0..5 {
        layout.hide(&"sftp").unwrap();
        assert!(!layout.visible(&"sftp"));
        layout.show("sftp").unwrap();
        assert_eq!(layout.snapshot().root, before.root);
    }
    layout.float(&"sftp", rect(20., 30., 500., 300.)).unwrap();
    layout.set_pinned(&"sftp", true).unwrap();
    layout.hide(&"sftp").unwrap();
    layout.show("sftp").unwrap();
    let restored = layout.snapshot().floating["sftp"].clone();
    assert_eq!(restored.bounds, rect(20., 30., 500., 300.));
    assert!(restored.pinned);
    layout.redock(&"sftp").unwrap();
    assert_eq!(layout.snapshot().root, before.root);
}
#[test]
fn hidden_target_removed_uses_surviving_layout_without_resurrecting_panels() {
    let mut layout = sample();
    layout.hide(&"ai").unwrap();
    layout.hide(&"monitor").unwrap();
    layout.show("ai").unwrap();
    assert!(layout.visible(&"ai"));
    assert!(!layout.visible(&"monitor"));
    let mut ids = layout.snapshot().root.unwrap().ids();
    ids.sort();
    assert_eq!(ids, vec!["ai", "sftp", "terminal"]);
}
#[test]
fn invalid_edits_are_atomic() {
    let mut layout = sample();
    let before = layout.snapshot();
    for edge in [
        DockEdge::Left,
        DockEdge::Right,
        DockEdge::Top,
        DockEdge::Bottom,
    ] {
        assert!(layout.dock(&"sftp", &"missing", edge).is_err());
        assert!(layout.dock(&"sftp", &"sftp", edge).is_err());
        assert_eq!(layout.snapshot(), before);
    }
    assert!(layout.float(&"sftp", rect(0., 0., f32::NAN, 100.)).is_err());
    assert_eq!(layout.snapshot(), before);
    let bad = DockSnapshot {
        root: Some(DockNode::Split {
            axis: DockAxis::Horizontal,
            ratio: 0.5,
            first: Box::new(leaf("a")),
            second: Box::new(leaf("a")),
        }),
        floating: BTreeMap::new(),
        hidden: BTreeMap::new(),
        maximized: None,
    };
    assert_eq!(layout.restore(bad), Err(DockError::DuplicateId));
    assert_eq!(layout.snapshot(), before);
}
#[test]
fn swap_balance_and_local_ratio_do_not_recreate_or_lose_ids() {
    let mut root = sample().snapshot().root.unwrap();
    let original = root.ids();
    root.swap(&"ai", &"terminal").unwrap();
    assert_eq!(root.ids(), vec!["sftp", "ai", "monitor", "terminal"]);
    root.set_ratio(&[true, true], 0.7).unwrap();
    let g = geometry(&root);
    assert_eq!(g.panes.len(), 4);
    root.balance();
    root.swap(&"ai", &"terminal").unwrap();
    assert_eq!(root.ids(), original);
}
#[test]
fn tiny_geometry_is_finite_inside_bounds_and_gap_does_not_go_negative() {
    let root = sample().snapshot().root.unwrap();
    for (w, h) in [(0., 0.), (1., 1.), (200., 100.), (1600., 1000.)] {
        let bounds = rect(30., 40., w, h);
        let g = root
            .geometry(bounds, 5., |_| PaneMinimum {
                width: 240.,
                height: 160.,
            })
            .unwrap();
        for r in g.panes.values() {
            assert!(r.width >= 0. && r.height >= 0.);
            assert!(r.x >= 30. && r.y >= 40.);
            assert!(r.x + r.width <= 30. + w + 0.01 && r.y + r.height <= 40. + h + 0.01);
        }
    }
}
#[test]
fn floating_clamp_does_not_destroy_saved_desktop_bounds() {
    let original = rect(1200., 500., 400., 500.);
    let min = PaneMinimum {
        width: 240.,
        height: 160.,
    };
    let compact = clamp_floating(original, rect(0., 0., 300., 200.), min).unwrap();
    assert_eq!(compact, rect(0., 0., 300., 200.));
    assert_eq!(original, rect(1200., 500., 400., 500.));
    let small = clamp_floating(rect(-10., -20., 1., 1.), rect(0., 0., 1600., 1000.), min).unwrap();
    assert_eq!(small, rect(0., 0., 240., 160.));
}
#[test]
fn pin_order_and_escape_checkpoint() {
    let mut layout = sample();
    layout
        .float(&"monitor", rect(900., 0., 400., 700.))
        .unwrap();
    layout.float(&"ai", rect(50., 50., 320., 300.)).unwrap();
    layout.set_pinned(&"monitor", true).unwrap();
    layout.raise(&"ai").unwrap();
    assert_eq!(layout.paint_order(), vec!["ai", "monitor"]);
    let checkpoint = layout.snapshot();
    layout.dock(&"ai", &"sftp", DockEdge::Bottom).unwrap();
    layout.maximize(&"sftp").unwrap();
    layout.restore(checkpoint.clone()).unwrap();
    assert_eq!(layout.snapshot(), checkpoint);
}

#[test]
fn externally_restored_float_can_redock_without_losing_the_pane() {
    let mut state = sample().snapshot();
    state.root = state.root.unwrap().without(&"ai");
    state.floating.insert(
        "ai",
        FloatingPane {
            bounds: rect(0., 0., 300., 300.),
            pinned: false,
            order: 1,
        },
    );
    let mut layout = DockWorkspace::from_snapshot(state).unwrap();
    layout.redock(&"ai").unwrap();
    assert!(layout.visible(&"ai"));
    assert_eq!(layout.snapshot().root.unwrap().ids().len(), 4);
}
#[test]
fn all_resize_edges_keep_opposite_anchors_and_obey_minima() {
    use FloatResizeEdge::*;
    let start = rect(300., 200., 500., 400.);
    let area = rect(0., 0., 1600., 1000.);
    let min = PaneMinimum {
        width: 240.,
        height: 160.,
    };
    for edge in [
        North, South, East, West, NorthEast, NorthWest, SouthEast, SouthWest,
    ] {
        for delta in [(-2000., -2000.), (2000., 2000.), (30., 40.)] {
            let r = resize_floating(start, delta, edge, area, min).unwrap();
            assert!(r.width >= 240. && r.height >= 160.);
            assert!(r.x >= 0. && r.y >= 0. && r.x + r.width <= 1600. && r.y + r.height <= 1000.);
            if matches!(edge, West | NorthWest | SouthWest) {
                assert_eq!(r.x + r.width, 800.);
            }
            if matches!(edge, North | NorthWest | NorthEast) {
                assert_eq!(r.y + r.height, 600.);
            }
        }
        assert_eq!(
            resize_floating(start, (0., 0.), edge, area, min).unwrap(),
            start
        );
    }
    assert!(resize_floating(start, (f32::NAN, 0.), East, area, min).is_err());
}
