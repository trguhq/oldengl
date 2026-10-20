/* MIT licensed. Private native XGL primitive/raster dispatch interface. */
#ifndef OLDENGL_XGL_ACCEL_H
#define OLDENGL_XGL_ACCEL_H

#include <X11/Xlib.h>
#include "mtypes.h"
#include "swrast/swrast.h"

typedef struct oldengl_xgl_accel OldenGLXGLAccel;
struct oldengl_xgl_accel_stats {
    unsigned long points, lines, triangles, clears, copies, fallbacks;
    unsigned long object_batches, transformed_vertices, lit_vertices;
    unsigned long depth_batches, back_batches, textured_vertices;
};

struct oldengl_xgl_batch;
/* back_rgba is a bottom-up, packed RGBA staging image for GL_BACK, or NULL
 * for GL_FRONT. Successful back draws update it before returning. */
GLboolean oldengl_xgl_accel_draw_batch(OldenGLXGLAccel *, GLcontext *,
    const struct oldengl_xgl_batch *, GLubyte *back_rgba);

/* Uses the existing native X window as its authoritative color buffer. */
OldenGLXGLAccel *oldengl_xgl_accel_create(Display *, Window, int screen,
                                         GLsizei width, GLsizei height);
void oldengl_xgl_accel_destroy(OldenGLXGLAccel *);
GLboolean oldengl_xgl_accel_resize(OldenGLXGLAccel *, GLsizei, GLsizei);
/* Fragment effects only; native batch depth handling is checked separately.
 * Callers must separately require an RGB target without stored alpha. */
GLboolean oldengl_xgl_accel_fragment_eligible(const GLcontext *);
GLboolean oldengl_xgl_accel_eligible(const GLcontext *, GLenum primitive);
GLboolean oldengl_xgl_accel_point(OldenGLXGLAccel *, GLcontext *,
                                 const SWvertex *);
GLboolean oldengl_xgl_accel_line(OldenGLXGLAccel *, GLcontext *,
                                const SWvertex *, const SWvertex *);
GLboolean oldengl_xgl_accel_triangle(OldenGLXGLAccel *, GLcontext *,
                                    const SWvertex *, const SWvertex *,
                                    const SWvertex *);
GLboolean oldengl_xgl_accel_clear(OldenGLXGLAccel *, GLcontext *);
GLboolean oldengl_xgl_accel_copy_pixels(OldenGLXGLAccel *, GLcontext *,
                                       GLint, GLint, GLsizei, GLsizei,
                                       GLint, GLint);
void oldengl_xgl_accel_get_stats(const OldenGLXGLAccel *,
                                struct oldengl_xgl_accel_stats *);

/* XMesa integration; these are called only in OLDENGL_XGL_ACCEL builds. */
void oldengl_xgl_xmesa_attach(GLcontext *);
void oldengl_xgl_xmesa_detach(GLcontext *);

#endif
