/* Test fixture only. */
#ifndef OLDENGL_TEST_XUTIL_H
#define OLDENGL_TEST_XUTIL_H
#include "Xlib.h"
typedef struct {
    Visual *visual;
    VisualID visualid;
    int screen, depth, class;
    unsigned long red_mask, green_mask, blue_mask;
    int colormap_size, bits_per_rgb;
} XVisualInfo;
#endif
