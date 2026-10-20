/* MIT licensed. Connect native XGL primitives to Mesa's software fallback. */
#include "xgl_accel.h"
#include "xgl_tnl.h"
#include "xmesaP.h"
#include "swrast/s_context.h"
#include "swrast/s_pb.h"
#include <stdlib.h>
#include <X11/Xutil.h>

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
surface(struct accelerated_context *n, GLboolean object_space)
{
    const XMesaContext xm = (XMesaContext)n->ctx->DriverCtx;
    XMesaBuffer b = xm->xm_buffer;
    const char *disabled = getenv("OLDENGL_NO_XGL");
    if ((disabled && *disabled && *disabled != '0') || !b ||
        b->pixmap_flag || b->pbuffer_flag ||
        (n->ctx->Color.DriverDrawBuffer != GL_FRONT_LEFT &&
         !(object_space && n->ctx->Color.DriverDrawBuffer == GL_BACK_LEFT && b->db_state)) ||
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
    OldenGLXGLAccel *a = surface(n, GL_FALSE);
    if (a)
        _mesa_flush_pb(n->ctx);
    return a;
}

static unsigned int color_component(unsigned long pixel, unsigned long mask)
{
    unsigned int shift = 0;
    if (!mask) return 0;
    while (!(mask & 1UL)) { mask >>= 1; ++shift; }
    return (unsigned int)((((pixel >> shift) & mask)*255UL + mask/2)/mask);
}

static unsigned long component_bits(unsigned int component, unsigned long mask)
{
    unsigned int shift = 0;
    unsigned long maximum = mask;
    if (!mask) return 0;
    while (!(maximum & 1UL)) { maximum >>= 1; ++shift; }
    return (((unsigned long)component*maximum + 127UL)/255UL << shift) & mask;
}

GLboolean
oldengl_xgl_xmesa_draw_object_batch(GLcontext *ctx,
                                    const struct oldengl_xgl_batch *batch)
{
    struct accelerated_context *n = find(ctx);
    XMesaContext xm = (XMesaContext)ctx->DriverCtx;
    XMesaBuffer b = xm->xm_buffer;
    OldenGLXGLAccel *native;
    XImage *image = NULL;
    GLubyte *rgba = NULL;
    GLboolean owned = GL_FALSE, rendered;
    size_t x, y, width, height;
    unsigned long red, green, blue;
    if (!n || !b || !ctx->Visual.rgbMode || ctx->Visual.alphaBits ||
        ctx->RenderMode != GL_RENDER) return GL_FALSE;
    native = surface(n, GL_TRUE);
    if (!native) return GL_FALSE;
    _mesa_flush_pb(ctx);
    width = b->width; height = b->height;
    red = xm->xm_visual->visinfo->red_mask;
    green = xm->xm_visual->visinfo->green_mask;
    blue = xm->xm_visual->visinfo->blue_mask;
    if (ctx->Color.DriverDrawBuffer == GL_BACK_LEFT) {
        if (!width || !height || width > ((size_t)-1)/height/4) return GL_FALSE;
        image = b->backimage;
        if (!image && b->backpixmap) {
            image = XGetImage(xm->display, b->backpixmap, 0, 0,
                (unsigned int)width, (unsigned int)height, AllPlanes, ZPixmap);
            owned = GL_TRUE;
        }
        if (!image) return GL_FALSE;
        rgba = (GLubyte *)malloc(width*height*4);
        if (!rgba) { if (owned) XDestroyImage(image); return GL_FALSE; }
        for (y = 0; y < height; ++y) for (x = 0; x < width; ++x) {
            unsigned long pixel = XGetPixel(image, (int)x, (int)(height-y-1));
            GLubyte *p = rgba+(y*width+x)*4;
            p[0] = (GLubyte)color_component(pixel, red);
            p[1] = (GLubyte)color_component(pixel, green);
            p[2] = (GLubyte)color_component(pixel, blue); p[3] = 255;
        }
    }
    rendered = oldengl_xgl_accel_draw_batch(native, ctx, batch, rgba);
    if (rendered && image) {
        for (y = 0; y < height; ++y) for (x = 0; x < width; ++x) {
            const GLubyte *p = rgba+(y*width+x)*4;
            unsigned long pixel = component_bits(p[0], red) |
                component_bits(p[1], green) | component_bits(p[2], blue);
            XPutPixel(image, (int)x, (int)(height-y-1), pixel);
        }
        if (owned) XPutImage(xm->display, b->backpixmap, b->swapgc, image,
                             0, 0, 0, 0, (unsigned int)width, (unsigned int)height);
    }
    free(rgba);
    if (owned) XDestroyImage(image);
    return rendered;
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
