/* MIT licensed.  XGL window-system binding for the bundled Mesa renderer. */
#ifndef XGLMESA_H
#define XGLMESA_H

#include <X11/Xlib.h>
#include <GL/gl.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct xglmesa_context *XGLMesaContext;

/*
 * The caller owns display and window; they must remain valid until destruction.
 * The window must be InputOutput with a TrueColor or DirectColor visual.
 * Contexts have a single RGBA color buffer, a software depth/stencil buffer,
 * and an accumulation buffer.  share may be NULL.  Creation does not bind.
 * This binding and its XGL session are single-threaded.
 */
XGLMesaContext XGLMesaCreateContext(Display *display, Window window,
                                   XGLMesaContext share);
void XGLMesaDestroyContext(XGLMesaContext context);

/* Flushes the old context. NULL releases it. Invalid inside glBegin/glEnd. */
GLboolean XGLMesaMakeCurrent(XGLMesaContext context);
XGLMesaContext XGLMesaGetCurrentContext(void);

/*
 * Resize after ConfigureNotify.  Contents become undefined; viewport and
 * scissor state are preserved.  Width and height must be positive.
 */
GLboolean XGLMesaResizeBuffers(XGLMesaContext context,
                             GLsizei width, GLsizei height);

/*
 * Present the current context.  This is a synchronized copy, not an exchange
 * of front/back buffers.  glFlush and glFinish also present this single buffer.
 * Call again after Expose to repair the window from the retained software image.
 */
GLboolean XGLMesaSwapBuffers(XGLMesaContext context);

/* Read-only RGBA bytes, bottom row first; invalidated by resize/destruction. */
GLboolean XGLMesaGetColorBuffer(XGLMesaContext context, GLsizei *width,
                              GLsizei *height, const GLubyte **pixels);

/* Latest binding/XGL error, or ""; independent of glGetError. Never free it. */
const char *XGLMesaGetLastError(void);

#ifdef __cplusplus
}
#endif
#endif
