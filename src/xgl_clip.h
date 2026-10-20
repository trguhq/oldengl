/* MIT licensed. OpenGL eye-space user clip planes mapped to XGL world planes. */
#ifndef OLDENGL_XGL_CLIP_H
#define OLDENGL_XGL_CLIP_H
#include <xgl/xgl.h>
#include "mtypes.h"

GLboolean oldengl_xgl_clip_eligible(const GLcontext *);
GLboolean oldengl_xgl_clip_configure(Xgl_ctx, const GLcontext *);

#endif
