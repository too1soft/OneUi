#include <oneui/oneui_c_api.h>

OneUiWidget *example_c_image(void) {
    const unsigned char rgba[] = {196, 237, 135, 255, 48, 98, 78, 255};
    OneUiWidget *image = oneui_image_view_create();
    if (!image)
        return NULL;
    if (oneui_image_view_set_rgba(image, rgba, sizeof(rgba), 2, 1, 8) != 1) {
        oneui_widget_destroy(image);
        return NULL;
    }
    oneui_image_view_set_content_mode(image, 0); /* contain */
    oneui_image_view_set_corner_radius(image, 12);
    /* Pixels have been copied. The caller owns the returned handle. */
    return image;
}
