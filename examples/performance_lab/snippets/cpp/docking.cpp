#include <oneui/controls/text_field.h>
#include <oneui/layout/dock_session.h>

struct ExampleDocument {
    std::shared_ptr<oneui::TextField> editor = std::make_shared<oneui::TextField>();
    bool dirty = false;
};
oneui::DockSessionResult example_dock_pane(oneui::DockSession &session, oneui::DockId host) {
    auto document = std::make_shared<ExampleDocument>();
    document->editor->setText(L"拆分、合并后保留原控件和控制器。");
    std::weak_ptr<ExampleDocument> weak = document;
    document->editor->setOnChanged([weak](const std::wstring &) {
        if (auto owner = weak.lock())
            owner->dirty = true;
    });
    // Register an empty Panel with attachHost(window, panel) to obtain host.
    // The session retains BOTH the widget and the owning controller.
    return session.addPane({42, L"文档", document->editor, document}, {host});
}
oneui::DockSessionResult example_move(oneui::DockSession &session, oneui::DockId targetHost) {
    // movePane commits a transaction; failure restores the source host.
    return session.movePane(42, {targetHost});
}

// Applies to existing hosts and future floating windows without replacing panes.
oneui::DockSessionResult example_dock_theme(oneui::DockSession &session,
                                            std::shared_ptr<oneui::StyleSheet> sheet) {
    return session.setStyleSheet(std::move(sheet));
}
