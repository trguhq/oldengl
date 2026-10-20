/* MIT licensed. Mesa fragment processing for XGL-captured raster spans. */
#ifndef OLDENGL_XGL_FRAGMENTS_H
#define OLDENGL_XGL_FRAGMENTS_H
#include "mtypes.h"
#include "swrast/swrast.h"

struct oldengl_xgl_fragment_setup {
    GLcontext *ctx;
    GLenum mode;
    GLuint count;
    GLboolean texture_rgb_complete; /* capture includes compatible unit-zero texturing */
    SWvertex vertex[3];
    double weight[3][3]; /* each barycentric weight: a*x+b*y+c */
    double depth_plane[3]; /* Mesa-snapped triangle window-Z plane */
    double rho[MAX_TEXTURE_UNITS];
    void *workspace;
};

/* Prepare before native capture or any target-buffer write. A successful
 * preparation owns temporary storage until release. No allocation occurs
 * while emitting spans. The RGB input is the untextured native primary
 * color unless texture_rgb_complete is set after prepare; remaining
 * attributes come from post-TNL SWvertices. */
GLboolean oldengl_xgl_fragments_prepare(GLcontext *, GLenum mode,
    const SWvertex *, const SWvertex *, const SWvertex *,
    struct oldengl_xgl_fragment_setup *);
void oldengl_xgl_fragments_emit_span(struct oldengl_xgl_fragment_setup *,
    GLint x, GLint y, GLuint count, const GLubyte rgb[][3]);
void oldengl_xgl_fragments_release(struct oldengl_xgl_fragment_setup *);
#endif
