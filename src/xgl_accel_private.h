/* MIT licensed. Private state shared by raster and object-space dispatch. */
#ifndef OLDENGL_XGL_ACCEL_PRIVATE_H
#define OLDENGL_XGL_ACCEL_PRIVATE_H
#include "xgl_accel.h"
#include "xgl_session.h"
#include "xgl_lighting.h"
#include "xgl_texture.h"

struct oldengl_xgl_accel {
    Display *display;
    Xgl_object raster, context;
    GLsizei width, height;
    GLboolean failed;
    struct oldengl_xgl_accel_stats stats;
    Xgl_trans model, view;
    OldenGLXGLLighting *lighting;
    OldenGLXGLTexture *texture;
    Xgl_object memory, memory_context;
    Xgl_usgn32 *memory_rgb, *memory_z;
    unsigned int red_shift, green_shift, blue_shift;
    double maximum_z;
    int depth_rounding[3]; /* per point/line/triangle: unknown, truncate, nearest */
    GLboolean capabilities_queried, back_checked, back_available;
};

void oldengl_xgl_batch_cleanup(OldenGLXGLAccel *);
void oldengl_xgl_batch_resize(OldenGLXGLAccel *);
GLboolean oldengl_xgl_batch_reset(OldenGLXGLAccel *);
#endif
