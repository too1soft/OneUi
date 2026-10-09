use oneui::{Application, Error, QuitPolicy, Window, WindowOptions};

pub fn application() -> Result<i32, Error> {
    let app = Application::new("org.example.oneui", "示例")?;
    if !app.is_primary_instance() {
        return Ok(0);
    }
    let window = Window::new(&WindowOptions {
        title: "主窗口".into(),
        width: 800,
        height: 600,
        ..Default::default()
    })?;
    if !app.add_window(&window, true) {
        return Err(Error::ApplicationOperationFailed);
    }
    app.set_quit_policy(QuitPolicy::LastWindowClosed);
    app.set_close_guard(&window, |decision| {
        // Move into an async UI callback if needed; dropping it cancels closing.
        decision.resolve(true);
    });
    app.set_on_activation(|event| println!("activation: {:?}", event));
    window.show();
    // Locals drop in reverse order: the window is destroyed before the app.
    Ok(app.run())
}
