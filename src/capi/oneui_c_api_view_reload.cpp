#include "oneui_c_api_view_internal.h"
using namespace oneui::capi::views;
OneUiViewResult oneui_view_reload(OneUiView *view,const uint8_t *bytes,size_t length) {
    return call(__func__,[&] {
        check(bytes && length && length<=oneview::max_file,ONEUI_VIEW_INVALID_ARGUMENT,"Reload IR required");
        auto value=runtime(view);
        value->reload(oneview::readImage(oneview::Bytes(bytes,bytes+length)));
        view->root.widget=value->root->element.widget;
        pruneHostHandles(view);
    });
}
