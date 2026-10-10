use crate::{broadcast::Broadcast, page::WorkspaceCommand as C, vm::Model};
#[test]
fn broadcast_policy_and_confirmation() {
    let mut b = Broadcast::default();
    let panes = vec!["a".into(), "b".into(), "c".into()];
    b.toggle("a", "a", &panes);
    assert_eq!(b.source.as_deref(), Some("a"));
    assert_eq!(b.receivers.len(), 2);
    b.input("a", "hello", false);
    assert_eq!(b.received["b"], "hello");
    b.input("a", "one\ntwo", true);
    assert!(b.pending.is_some());
    assert_eq!(b.received["b"], "hello");
    b.confirm(false);
    assert_eq!(b.received["b"], "hello");
    b.input("a", "one\ntwo", true);
    b.confirm(true);
    assert_eq!(b.received["b"], "helloone\ntwo");
    b.input("b", "own", false);
    assert!(b.paused);
    b.input("a", "private", false);
    assert!(!b.received["b"].contains("private"));
    b.toggle("b", "c", &panes);
    assert_eq!(b.source.as_deref(), Some("a"));
    b.toggle("c", "a", &panes);
    assert!(b.source.is_none());
    b.disconnected.insert("a".into());
    b.toggle("b", "a", &panes);
    assert!(b.source.is_none());
    b.toggle("b", "b", &panes);
    assert!(!b.receivers.contains("a"));
    b.disconnect("c");
    assert!(b.paused);
    b.toggle("b", "c", &panes);
    assert!(b.source.is_none());
}
#[test]
fn forest_move_keeps_unique_panes_and_stable_rows() {
    let mut m = Model::default();
    m.preset(4).unwrap();
    let rows: Vec<_> = m.panes.iter().map(|p| std::rc::Rc::as_ptr(p.1)).collect();
    for _ in 0..500 {
        m.forest.swap("session/0", "session/1");
        m.forest.tab("session/0");
        m.forest.split("session/0", "session/1", "left");
        m.publish().unwrap();
        let leaves: Vec<_> = m
            .forest
            .groups
            .iter()
            .flat_map(|g| m.forest.leaves(&g.id))
            .collect();
        assert_eq!(leaves.len(), 16);
        assert_eq!(
            leaves
                .iter()
                .collect::<std::collections::BTreeSet<_>>()
                .len(),
            16
        );
        assert_eq!(
            rows,
            m.panes
                .iter()
                .map(|p| std::rc::Rc::as_ptr(p.1))
                .collect::<Vec<_>>()
        );
    }
}
#[test]
fn dock_restore_anchors_roundtrip_and_reject_invalid() {
    let mut m = Model::default();
    let original = m.tools.snapshot();
    let key = "sftp".to_string();
    m.tools.hide(&key).unwrap();
    let hidden = m.tools.snapshot();
    m.tools.show(key.clone()).unwrap();
    assert_eq!(m.tools.snapshot().root, original.root);
    m.tools.restore(hidden).unwrap();
    m.tools.show(key.clone()).unwrap();
    assert_eq!(m.tools.snapshot().root, original.root);
    m.publish_tools().unwrap();
    assert_eq!(
        m.placements
            .iter()
            .find(|p| p.0 == key)
            .unwrap()
            .1
            .borrow()
            .mode,
        "docked"
    );
    m.command(C::ToolFloat).unwrap();
    m.command(C::ToolPin).unwrap();
    m.command(C::ToolSave).unwrap();
    let saved = m.tools.snapshot();
    m.command(C::ToolRedock).unwrap();
    m.command(C::ToolRestore).unwrap();
    assert_eq!(m.tools.snapshot(), saved);
    let mut invalid = saved;
    invalid.floating.get_mut(&key).unwrap().bounds.width = f32::NAN;
    assert!(m.tools.restore(invalid).is_err());
}
#[test]
fn commands_reject_stale_and_close_waits_for_settle() {
    let mut m = Model::default();
    m.preset(2).unwrap();
    let nodes = m.forest.nodes.clone();
    m.command(C::DropPane {
        source: "session/0".into(),
        target: "session/1".into(),
        edge: "center".into(),
        request: 1,
        revision: -1,
    })
    .unwrap();
    assert!(m.forest.nodes == nodes);
    m.command(C::Close {
        pane: "session/1".into(),
    })
    .unwrap();
    assert_eq!(m.panes.len(), 16);
    let request = m.panes.get(1).unwrap().borrow().closeRequest;
    m.command(C::Settled {
        pane: "session/1".into(),
        request: request - 1,
        revision: m.revision,
    })
    .unwrap();
    assert_eq!(m.panes.len(), 16);
    m.command(C::Settled {
        pane: "session/1".into(),
        request,
        revision: m.revision,
    })
    .unwrap();
    assert_eq!(m.panes.len(), 15);
    m.preset(2).unwrap();
    assert_eq!(m.panes.len(), 16);
}
#[test]
fn media_selection_and_clear_do_not_touch_files() {
    let mut m = Model::default();
    m.mediaPath = "not-a-file.png".into();
    m.command(C::Choose).unwrap();
    assert_eq!(m.asset, m.mediaPath);
    m.command(C::Clear).unwrap();
    assert!(m.asset.is_empty());
    assert_eq!(m.mediaPath, "not-a-file.png");
    m.command(C::SelectFont {
        key: "missing".into(),
    })
    .unwrap();
    assert_eq!(m.font, "Consolas");
    assert_eq!(m.themes.len(), 12);
}

#[test]
fn header_visibility_and_number_follow_vm_topology() {
    let mut m = Model::default();
    m.preset(1).unwrap();
    assert!(!m.panes.get(0).unwrap().borrow().headerVisible);
    m.preset(4).unwrap();
    for i in 0..4 {
        let p = m.panes.get(i).unwrap().borrow();
        assert!(p.headerVisible && p.canClose);
        assert_eq!(p.number, i as i64 + 1);
    }
    m.forest.swap("session/0", "session/1");
    m.publish().unwrap();
    assert_eq!(m.panes.get(0).unwrap().borrow().number, 2);
    m.command(C::Broadcast {
        pane: "session/0".into(),
    })
    .unwrap();
    m.preset(1).unwrap();
    assert!(m.panes.get(0).unwrap().borrow().headerVisible);
    assert!(!m.panes.get(0).unwrap().borrow().canClose);
}
