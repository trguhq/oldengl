/* MIT licensed. GL-compatible subset of XGL ambient/diffuse lighting. */
#ifndef OLDENGL_XGL_LIGHTING_H
#define OLDENGL_XGL_LIGHTING_H

#include <xgl/xgl.h>
#include "mtypes.h"

typedef struct oldengl_xgl_lighting OldenGLXGLLighting;

OldenGLXGLLighting *oldengl_xgl_lighting_create(Xgl_sys_state);
void oldengl_xgl_lighting_destroy(OldenGLXGLLighting *);
GLboolean oldengl_xgl_lighting_eligible(const GLcontext *);
GLboolean oldengl_xgl_lighting_configure(OldenGLXGLLighting *, Xgl_ctx,
                                       const GLcontext *);

/* The caller must separately guarantee unit-length transformed normals
 * (or GL_NORMALIZE semantics), and an affine GL modelview transform.
 * XGL owns the light objects attached to its context; destroy the context
 * through the normal XGL API. This helper owns only its handle storage.
 */
#endif
