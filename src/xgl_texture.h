/* MIT licensed. GL texture state mapped to XGL surface texture maps. */
#ifndef OLDENGL_XGL_TEXTURE_H
#define OLDENGL_XGL_TEXTURE_H

#include <xgl/xgl.h>
#include "mtypes.h"

typedef struct oldengl_xgl_texture OldenGLXGLTexture;

OldenGLXGLTexture *oldengl_xgl_texture_create(Xgl_sys_state);
void oldengl_xgl_texture_destroy(OldenGLXGLTexture *);
GLboolean oldengl_xgl_texture_eligible(const GLcontext *);
/* RGB-only capture can retain framebuffer alpha in Mesa separately. */
GLboolean oldengl_xgl_texture_rgb_eligible(const GLcontext *);
GLboolean oldengl_xgl_texture_configure(OldenGLXGLTexture *, Xgl_ctx,
                                      const GLcontext *);
GLboolean oldengl_xgl_texture_coordinates(const GLcontext *,
                                        const GLfloat texcoord[4],
                                        const GLfloat position[4],
                                        GLfloat stq[3]);

/* Coordinates include linear texgen and the GL texture matrix. The caller
 * must check that q is the same nonzero value at every vertex of a batch,
 * then supply (s/q,t/q) as two XGL vertex data values. GL 1D uses a one-row
 * native image with t/q fixed at 0.5 after applying the texture matrix.
 * XGL's texture API has
 * no projective texture-q input. Use only surface (triangle) primitives.
 * Destroy the native context before this helper: the context owns its TMAP
 * object and this helper owns the MIPMAP_TEXTURE referenced by that TMAP.
 */
#endif
