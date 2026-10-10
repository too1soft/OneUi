use super::*;
use crate::page::WorkspaceVm;
pub fn run(d: &mut Driver) {
    let app = d.app.clone();
    eprintln!("acceptance: root drag");
    // The initial four-pane state came from a real root-routed toolbar click.
    assert_eq!(app.borrow().vm.borrow().forest.leaves("main").len(), 4);
    let a = rect("session/0", 1);
    let b = rect("session/1", 2);
    let before = app.borrow().vm.borrow().forest.nodes.clone();
    pointer(0, a[0] + 55., a[1] + 14., 1);
    pointer(1, a[0] + 65., a[1] + 14., 1);
    pointer(2, b[0] + b[2] * 0.5, b[1] + b[3] * 0.5, 1);
    app.borrow_mut().flush();
    assert!(
        app.borrow().vm.borrow().forest.nodes != before,
        "root drag must commit a layout intent, not only activation"
    );
    // A root text event reaches the existing terminal callback owner.
    let body = rect("session/0", 2);
    pointer(0, body[0] + 70., body[1] + 75., 1);
    pointer(2, body[0] + 70., body[1] + 75., 1);
    unsafe {
        workspace_acceptance_text(1);
        workspace_acceptance_text(0);
    }
    {
        let mut a = app.borrow_mut();
        a.vm.borrow_mut().motion = false;
        a.vm.borrow_mut().publish().unwrap();
        a.flush();
    }
    let identities = d.baseline.clone();
    eprintln!("acceptance: identity cycles");
    for cycle in 0..500 {
        {
            let a = app.borrow();
            let mut m = a.vm.borrow_mut();
            m.preset(if cycle % 2 == 0 { 16 } else { 4 }).unwrap();
        }
        if cycle == 0 {
            eprintln!("identity: preset");
        }
        app.borrow_mut().flush();
        {
            let a = app.borrow();
            let mut m = a.vm.borrow_mut();
            m.forest.swap("session/0", "session/1");
            m.forest.tab("session/0");
            m.forest.split("session/0", "session/1", "right");
            m.revision += 1;
            m.publish().unwrap();
        }
        if cycle == 0 {
            eprintln!("identity: swap/tab/split");
        }
        app.borrow_mut().flush();
        {
            let a = app.borrow();
            let mut m = a.vm.borrow_mut();
            m.maximized = "session/0".into();
            m.publish().unwrap();
        }
        if cycle == 0 {
            eprintln!("identity: maximize");
        }
        app.borrow_mut().flush();
        {
            let a = app.borrow();
            let mut m = a.vm.borrow_mut();
            m.maximized.clear();
            m.publish().unwrap();
            m.command(C::ToolHide).unwrap();
        }
        if cycle == 0 {
            eprintln!("identity: hide");
        }
        app.borrow_mut().flush();
        app.borrow().vm.borrow_mut().command(C::ToolShow).unwrap();
        if cycle == 0 {
            eprintln!("identity: show");
        }
        app.borrow_mut().flush();
        let a = app.borrow();
        for (key, handle, owner) in &identities {
            let content = &a.content.sessions[key];
            assert_eq!(content.address(), *handle);
            assert_eq!(Rc::as_ptr(content) as usize, *owner);
            assert_eq!(
                Rc::strong_count(content),
                2,
                "one pool owner + one native host lease"
            );
            let scope = a
                .view
                .resolve_scope(&[oneui::view::ScopeSegment {
                    instance_id: "pool",
                    key,
                }])
                .unwrap();
            a.view.find_host_in(&scope, "terminal").unwrap();
        }
        assert_eq!(a.content.counts.created.get(), 16);
        assert_eq!(a.content.counts.attached.get(), 16);
        assert_eq!(a.content.counts.dropped.get(), 0);
    }
    let mut lines = String::from("key,opaque_handle,native_widget,owner,strong_count\n");
    {
        let a = app.borrow();
        for (key, session) in &a.content.sessions {
            let native = unsafe {
                session
                    .widget()
                    .with_native_handle(|w| workspace_acceptance_address(w.cast()))
            };
            lines.push_str(&format!(
                "{key},0x{:x},0x{native:x},0x{:x},{}\n",
                session.address(),
                Rc::as_ptr(session) as usize,
                Rc::strong_count(session)
            ));
        }
    }
    std::fs::write(d.output.with_extension("identity.csv"), lines).unwrap();
    println!("PASS root pointer/drag/text + 500 split/swap/tab/maximize/hide cycles; created=16 attach=16 dropped=0 strong_count=2");
}
