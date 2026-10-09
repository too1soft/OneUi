#include <oneui/dock_session_c.h>
#include <stdlib.h>
#define UTF8(s) ((OneUiUtf8String){s, sizeof(s) - 1})

typedef struct ExampleDocument {
    OneUiWidget *editor;
} ExampleDocument;
static void destroy_document(void *data) {
    ExampleDocument *document = (ExampleDocument *)data;
    oneui_widget_destroy(document->editor);
    free(document);
}
int example_c_dock_pane(OneUiDockSession *session, unsigned long long host) {
    ExampleDocument *document = (ExampleDocument *)calloc(1, sizeof(*document));
    if (!document)
        return 7;
    document->editor = oneui_text_field_create_utf8(UTF8("文档"));
    if (!document->editor) {
        free(document);
        return 7;
    }
    oneui_text_field_set_text_utf8(document->editor, UTF8("迁移保留原控件与所有者。"));
    OneUiDockDestination destination = {host, 0, 0, -1};
    /* Ownership of document always transfers, including on rejection.
       Do not free it again. Destroy the session before its Application. */
    return oneui_dock_session_add_pane_utf8(session, 42, UTF8("文档"), document->editor, 120, 80,
                                            destination, NULL, document, destroy_document);
}

/* The session retains the sheet; the caller may destroy its sheet handle. */
int example_c_dock_theme(OneUiDockSession *session, OneUiStyleSheet *sheet) {
    return oneui_dock_session_set_style_sheet(session, sheet);
}
