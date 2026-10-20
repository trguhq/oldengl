/* MIT licensed. Connect native XGL primitives to Mesa's software fallback. */
#include "xgl_accel.h"
#include "xmesaP.h"
#include "swrast/s_context.h"
#include "swrast/s_pb.h"
#include <stdlib.h>

struct accelerated_context {
    GLcontext *ctx;
    OldenGLXGLAccel *native;
    Display *display;
    Window window;
    GLsizei width, height;
    GLboolean attempted;
    void (*choose_point)(GLcontext *);
    void (*choose_line)(GLcontext *);
    void (*choose_triangle)(GLcontext *);
    swrast_point_func point;
    swrast_line_func line;
    swrast_tri_func triangle;
    void (*clear)(GLcontext *, GLbitfield, GLboolean, GLint, GLint, GLint, GLint);
    void (*copy_pixels)(GLcontext *, GLint, GLint, GLsizei, GLsizei,
                        GLint, GLint, GLenum);
    struct accelerated_context *next;
};

static struct accelerated_context *contexts;

static struct accelerated_context *
find(GLcontext *ctx)
{
    struct accelerated_context *n;
    for (n = contexts; n; n = n->next)
        if (n->ctx == ctx)
            return n;
    return NULL;
}

static OldenGLXGLAccel *
surface(struct accelerated_context *n)
{
    const XMesaContext xm = (XMesaContext)n->ctx->DriverCtx;
    XMesaBuffer b = xm->xm_buffer;
    const char *disabled = getenv("OLDENGL_NO_XGL");
    if ((disabled && *disabled && *disabled != '0') || !b ||
        b->pixmap_flag || b->pbuffer_flag || b->buffer != b->frontbuffer ||
        n->ctx->Color.DriverDrawBuffer != GL_FRONT_LEFT ||
        (xm->xm_visual->visinfo->class != TrueColor &&
         xm->xm_visual->visinfo->class != DirectColor))
        return NULL;
    if (n->display != xm->display || n->window != (Window)b->frontbuffer) {
        oldengl_xgl_accel_destroy(n->native);
        n->native = NULL;
        n->display = xm->display;
        n->window = (Window)b->frontbuffer;
        n->attempted = GL_FALSE;
    }
    if (!n->attempted) {
        n->attempted = GL_TRUE;
        n->native = oldengl_xgl_accel_create(n->display, n->window,
            xm->xm_visual->visinfo->screen, b->width, b->height);
        n->width = b->width;
        n->height = b->height;
    } else if (n->native &&
        (n->width != (GLsizei)b->width || n->height != (GLsizei)b->height)) {
        oldengl_xgl_accel_resize(n->native, b->width, b->height);
        n->width = b->width;
        n->height = b->height;
    }
    return n->native;
}

static OldenGLXGLAccel *
begin_native(struct accelerated_context *n)
{
    OldenGLXGLAccel *a = surface(n);
    if (a)
        _mesa_flush_pb(n->ctx);
    return a;
}

static void
point(GLcontext *ctx, const SWvertex *v)
{
    struct accelerated_context *n = find(ctx);
    if (!oldengl_xgl_accel_point(begin_native(n), ctx, v))
        n->point(ctx, v);
}

static void
line(GLcontext *ctx, const SWvertex *a, const SWvertex *b)
{
    struct accelerated_context *n = find(ctx);
    if (!oldengl_xgl_accel_line(begin_native(n), ctx, a, b))
        n->line(ctx, a, b);
}

static void
triangle(GLcontext *ctx, const SWvertex *a, const SWvertex *b,
         const SWvertex *c)
{
    struct accelerated_context *n = find(ctx);
    if (!oldengl_xgl_accel_triangle(begin_native(n), ctx, a, b, c))
        n->triangle(ctx, a, b, c);
}

static void
choose_point(GLcontext *ctx)
{
    struct accelerated_context *n = find(ctx);
    n->choose_point(ctx);
    n->point = SWRAST_CONTEXT(ctx)->Point;
    SWRAST_CONTEXT(ctx)->Point = point;
}

static void
choose_line(GLcontext *ctx)
{
    struct accelerated_context *n = find(ctx);
    n->choose_line(ctx);
    n->line = SWRAST_CONTEXT(ctx)->Line;
    SWRAST_CONTEXT(ctx)->Line = line;
}

static void
choose_triangle(GLcontext *ctx)
{
    struct accelerated_context *n = find(ctx);
    n->choose_triangle(ctx);
    n->triangle = SWRAST_CONTEXT(ctx)->Triangle;
    SWRAST_CONTEXT(ctx)->Triangle = triangle;
}

static void
clear(GLcontext *ctx, GLbitfield mask, GLboolean all,
       GLint x, GLint y, GLint width, GLint height)
{
    struct accelerated_context *n = find(ctx);
    if ((mask & DD_FRONT_LEFT_BIT) && all &&
        oldengl_xgl_accel_clear(begin_native(n), ctx))
        mask &= ~DD_FRONT_LEFT_BIT;
    if (mask)
        n->clear(ctx, mask, all, x, y, width, height);
}

static void
copy_pixels(GLcontext *ctx, GLint srcx, GLint srcy, GLsizei width,
             GLsizei height, GLint dstx, GLint dsty, GLenum type)
{
    struct accelerated_context *n = find(ctx);
    const XMesaContext xm = (XMesaContext)ctx->DriverCtx;
    if (type == GL_COLOR && xm->xm_buffer == xm->xm_read_buffer &&
        oldengl_xgl_accel_copy_pixels(begin_native(n), ctx, srcx, srcy,
            width, height, dstx, dsty))
        return;
    n->copy_pixels(ctx, srcx, srcy, width, height, dstx, dsty, type);
}

void
oldengl_xgl_xmesa_attach(GLcontext *ctx)
{
    struct accelerated_context *n;
    SWcontext *sw;
    if (!ctx || find(ctx))
        return;
    n = (struct accelerated_context *)calloc(1, sizeof(*n));
    if (!n)
        return;
    sw = SWRAST_CONTEXT(ctx);
    n->ctx = ctx;
    n->choose_point = sw->choose_point;
    n->choose_line = sw->choose_line;
    n->choose_triangle = sw->choose_triangle;
    n->clear = ctx->Driver.Clear;
    n->copy_pixels = ctx->Driver.CopyPixels;
    n->next = contexts;
    contexts = n;
    sw->choose_point = choose_point;
    sw->choose_line = choose_line;
    sw->choose_triangle = choose_triangle;
    ctx->Driver.Clear = clear;
    ctx->Driver.CopyPixels = copy_pixels;
    _swrast_InvalidateState(ctx, ~0U);
}

void
oldengl_xgl_xmesa_detach(GLcontext *ctx)
{
    struct accelerated_context **link, *n;
    for (link = &contexts; *link && (*link)->ctx != ctx; link = &(*link)->next)
        ;
    n = *link;
    if (!n)
        return;
    oldengl_xgl_accel_destroy(n->native);
    *link = n->next;
    free(n);
}
