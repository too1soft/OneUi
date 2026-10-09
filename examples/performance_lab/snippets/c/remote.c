#include <oneui/oneui_c_api.h>
#include <stdlib.h>

typedef struct ExampleRemote {
    OneUiWidget *host, *frame, *input;
} ExampleRemote;

void example_c_remote_destroy(ExampleRemote *remote) {
    if (remote->input) {
        oneui_remote_input_region_release_all_inputs(remote->input);
        oneui_remote_input_region_set_on_text_input_utf8(remote->input, NULL, NULL);
    }
    oneui_widget_destroy(remote->host);
    oneui_widget_destroy(remote->input);
    oneui_widget_destroy(remote->frame);
    *remote = (ExampleRemote){0};
}
/* Retain all three handles and callback user_data until the session is closed. */
ExampleRemote example_c_remote(OneUiUtf8TextCallback text, void *user_data) {
    ExampleRemote remote = {oneui_overlay_host_create(), oneui_realtime_frame_view_create(),
                            oneui_remote_input_region_create()};
    if (!remote.host || !remote.frame || !remote.input) {
        example_c_remote_destroy(&remote);
        return remote;
    }
    unsigned char pixels[2 * 2 * 4] = {196, 237, 135, 255, 48,  98,  78,  255,
                                       48,  98,  78,  255, 196, 237, 135, 255};
    oneui_realtime_frame_view_submit_frame(remote.frame, pixels, 2, 2, 8, OneUiPixelFormatRgba8888,
                                           1, 0);
    oneui_remote_input_region_set_remote_size(remote.input, 2, 2);
    oneui_realtime_frame_view_set_scale_mode(remote.frame, OneUiVideoScaleModeFit);
    oneui_remote_input_region_set_scale_mode(remote.input, OneUiVideoScaleModeFit);
    oneui_remote_input_region_set_on_text_input_utf8(remote.input, text, user_data);
    oneui_overlay_host_set_content(remote.host, remote.frame);
    oneui_overlay_host_add_overlay(remote.host, remote.input, 0);
    return remote;
}
static void release_pixels(const void *pixels, void *user_data) {
    (void)user_data;
    free((void *)pixels);
}
int example_c_owned_frame(OneUiWidget *frame) {
    unsigned char *pixel = (unsigned char *)malloc(4);
    if (!pixel)
        return 0;
    pixel[0] = 196;
    pixel[1] = 237;
    pixel[2] = 135;
    pixel[3] = 255;
    /* Ownership transfers even on rejection: never free pixel after this call.
       Restore matching input dimensions if used with an input region. */
    return oneui_realtime_frame_view_submit_frame_owned(
        frame, pixel, 4, 1, 1, 4, OneUiPixelFormatRgba8888, 2, 0, release_pixels, NULL);
}
