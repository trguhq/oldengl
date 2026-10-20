/* MIT licensed. Native XGL rasterization with conservative GL state checks. */
#include "xgl_accel.h"
#include "xgl_session.h"
#include "xgl_accel_private.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static GLboolean
color_target(const GLcontext *ctx)
{
    return ctx && ctx->Visual.rgbMode && ctx->Visual.alphaBits == 0 &&
        ctx->Visual.redBits >= 8 && ctx->Visual.greenBits >= 8 &&
        ctx->Visual.blueBits >= 8 &&
        ctx->Color.DriverDrawBuffer == GL_FRONT_LEFT &&
        ctx->Color.DrawDestMask == FRONT_LEFT_BIT &&
        ctx->Color.ColorMask[0] && ctx->Color.ColorMask[1] &&
        ctx->Color.ColorMask[2];
}

GLboolean
oldengl_xgl_accel_eligible(const GLcontext *ctx, GLenum primitive)
{
    if (!color_target(ctx) || ctx->RenderMode != GL_RENDER ||
        ctx->Depth.Test || ctx->Depth.OcclusionTest || ctx->Stencil.Enabled ||
        ctx->Color.AlphaEnabled || ctx->Color.BlendEnabled ||
        ctx->Color.ColorLogicOpEnabled || ctx->Scissor.Enabled ||
        ctx->Fog.Enabled || ctx->Fog.ColorSumEnabled ||
        ctx->Texture._ReallyEnabled ||
        ctx->Light.Model.ColorControl == GL_SEPARATE_SPECULAR_COLOR)
        return GL_FALSE;
    if (primitive == GL_POINTS)
        return !ctx->Point.SmoothFlag && !ctx->Point._Attenuated &&
            ctx->Point._Size == 1.0f;
    if (primitive == GL_LINES)
        return !ctx->Line.SmoothFlag && !ctx->Line.StippleFlag &&
            ctx->Line.Width == 1.0f;
    if (primitive == GL_TRIANGLES)
        return !ctx->Polygon.SmoothFlag && !ctx->Polygon.StippleFlag &&
            ctx->Polygon.FrontMode == GL_FILL &&
            ctx->Polygon.BackMode == GL_FILL;
    return GL_FALSE;
}

OldenGLXGLAccel *
oldengl_xgl_accel_create(Display *display, Window window, int screen,
                         GLsizei width, GLsizei height)
{
    OldenGLXGLAccel *a;
    Xgl_X_window native;
    Xgl_obj_desc description;
    Xgl_sys_state system;
    unsigned long serial;

    if (!display || !window || width < 1 || height < 1)
        return NULL;
    a = (OldenGLXGLAccel *)calloc(1, sizeof(*a));
    if (!a)
        return NULL;
    system = oldengl_xgl_acquire();
    if (!system) {
        free(a);
        return NULL;
    }
    serial = oldengl_xgl_error_serial();
    a->display = display;
    a->width = width;
    a->height = height;
    memset(&native, 0, sizeof(native));
    memset(&description, 0, sizeof(description));
    native.X_display = display;
    native.X_window = (Xgl_usgn32)window;
    native.X_screen = screen;
    description.win_ras.type = XGL_WIN_X;
    description.win_ras.desc = &native;
    XSync(display, False);
    a->raster = xgl_object_create(system, XGL_WIN_RAS, &description,
        XGL_DEV_COLOR_TYPE, XGL_COLOR_RGB, 0);
    if (!a->raster || oldengl_xgl_error_serial() != serial)
        goto failed;
    a->context = xgl_object_create(system, XGL_3D_CTX, NULL,
        XGL_CTX_DEVICE, a->raster,
        XGL_CTX_DEFERRAL_MODE, XGL_DEFER_ASAP,
        XGL_CTX_VDC_MAP, XGL_VDC_MAP_DEVICE,
        XGL_CTX_VDC_ORIENTATION, XGL_Y_DOWN_Z_AWAY,
        XGL_3D_CTX_HLHSR_MODE, XGL_HLHSR_NONE,
        XGL_3D_CTX_SURF_FACE_CULL, XGL_CULL_OFF,
        XGL_CTX_ROP, XGL_ROP_COPY,
        XGL_CTX_SURF_FRONT_FILL_STYLE, XGL_SURF_FILL_SOLID,
        XGL_CTX_SURF_EDGE_FLAG, (Xgl_boolean)0,
        XGL_CTX_MARKER, xgl_marker_dot,
        XGL_CTX_MARKER_COLOR_SELECTOR, XGL_MARKER_COLOR_POINT,
        XGL_CTX_LINE_COLOR_SELECTOR, XGL_LINE_COLOR_VERTEX,
        XGL_CTX_LINE_STYLE, XGL_LINE_SOLID,
        XGL_CTX_SURF_FRONT_COLOR_SELECTOR, XGL_SURF_COLOR_VERTEX_ILLUM_DEP,
        XGL_3D_CTX_SURF_FRONT_ILLUMINATION, XGL_ILLUM_NONE_INTERP_COLOR,
        XGL_CTX_NEW_FRAME_ACTION, XGL_CTX_NEW_FRAME_CLEAR, 0);
    if (!a->context || oldengl_xgl_error_serial() != serial)
        goto failed;
    return a;

failed:
    oldengl_xgl_accel_destroy(a);
    return NULL;
}

void
oldengl_xgl_accel_destroy(OldenGLXGLAccel *a)
{
    if (!a)
        return;
    if (a->context) {
        xgl_context_flush(a->context, XGL_FLUSH_SYNCHRONIZE);
        xgl_object_destroy(a->context);
    }
    oldengl_xgl_batch_cleanup(a);
    if (a->raster)
        xgl_object_destroy(a->raster);
    oldengl_xgl_release();
    free(a);
}

GLboolean
oldengl_xgl_accel_resize(OldenGLXGLAccel *a, GLsizei width, GLsizei height)
{
    unsigned long serial;
    if (!a || a->failed || width < 1 || height < 1)
        return GL_FALSE;
    serial = oldengl_xgl_error_serial();
    oldengl_xgl_batch_resize(a);
    XSync(a->display, False);
    xgl_window_raster_resize(a->raster);
    if (oldengl_xgl_error_serial() != serial) {
        a->failed = GL_TRUE;
        return GL_FALSE;
    }
    a->width = width;
    a->height = height;
    return GL_TRUE;
}

static GLboolean
start(OldenGLXGLAccel *a, GLcontext *ctx, GLenum primitive)
{
    if (!a || a->failed)
        return GL_FALSE;
    if (!oldengl_xgl_accel_eligible(ctx, primitive)) {
        ++a->stats.fallbacks;
        return GL_FALSE;
    }
    if (!oldengl_xgl_batch_reset(a))
        return GL_FALSE;
    /* Drain Xlib writes before direct device access. The XMesa adapter also
     * drains the swrast pixel batch before calling this module. */
    XSync(a->display, False);
    return GL_TRUE;
}

static GLboolean
finish(OldenGLXGLAccel *a, unsigned long serial)
{
    xgl_context_flush(a->context, XGL_FLUSH_SYNCHRONIZE);
    if (oldengl_xgl_error_serial() != serial) {
        a->failed = GL_TRUE;
        ++a->stats.fallbacks;
        return GL_FALSE;
    }
    return GL_TRUE;
}

static void
vertex(OldenGLXGLAccel *a, Xgl_pt_color_f3d *native, const SWvertex *v,
       const GLchan *color)
{
    /* GL sample centers are at half-integers, XGL raster sample centers at
     * integers. Input SWvertices have already undergone GL clipping. */
    native->x = v->win[0] - 0.5f;
    native->y = (GLfloat)a->height - 0.5f - v->win[1];
    native->z = 0.0f; /* no native depth buffer is used */
    native->color.rgb.r = (float)color[0] / CHAN_MAXF;
    native->color.rgb.g = (float)color[1] / CHAN_MAXF;
    native->color.rgb.b = (float)color[2] / CHAN_MAXF;
}

static GLboolean
finite_vertex(const SWvertex *v)
{
    return v && v->win[0] >= -FLT_MAX && v->win[0] <= FLT_MAX &&
        v->win[1] >= -FLT_MAX && v->win[1] <= FLT_MAX;
}

static void
point_list(Xgl_pt_list *list, Xgl_pt_color_f3d *points, unsigned int count)
{
    memset(list, 0, sizeof(*list));
    list->pt_type = XGL_PT_COLOR_F3D;
    list->num_pts = count;
    list->pts.color_f3d = points;
}

GLboolean
oldengl_xgl_accel_point(OldenGLXGLAccel *a, GLcontext *ctx, const SWvertex *v)
{
    Xgl_pt_color_f3d native;
    Xgl_pt_list list;
    unsigned long serial;
    if (!finite_vertex(v) || !start(a, ctx, GL_POINTS))
        return GL_FALSE;
    serial = oldengl_xgl_error_serial();
    vertex(a, &native, v, v->color);
    native.x = (float)floor(v->win[0]);
    native.y = (float)a->height - 1.0f - (float)floor(v->win[1]);
    point_list(&list, &native, 1);
    xgl_multimarker(a->context, &list);
    if (!finish(a, serial))
        return GL_FALSE;
    ++a->stats.points;
    return GL_TRUE;
}

GLboolean
oldengl_xgl_accel_line(OldenGLXGLAccel *a, GLcontext *ctx,
                      const SWvertex *v0, const SWvertex *v1)
{
    Xgl_pt_color_f3d points[2];
    Xgl_pt_list list;
    unsigned long serial;
    if (!finite_vertex(v0) || !finite_vertex(v1) || !start(a, ctx, GL_LINES))
        return GL_FALSE;
    serial = oldengl_xgl_error_serial();
    vertex(a, points, v0, ctx->Light.ShadeModel == GL_FLAT ? v1->color : v0->color);
    vertex(a, points + 1, v1, v1->color);
    point_list(&list, points, 2);
    xgl_object_set(a->context, XGL_3D_CTX_LINE_COLOR_INTERP,
        (Xgl_boolean)(ctx->Light.ShadeModel == GL_SMOOTH), 0);
    xgl_multipolyline(a->context, NULL, 1, &list);
    if (!finish(a, serial))
        return GL_FALSE;
    ++a->stats.lines;
    return GL_TRUE;
}

GLboolean
oldengl_xgl_accel_triangle(OldenGLXGLAccel *a, GLcontext *ctx,
                          const SWvertex *v0, const SWvertex *v1,
                          const SWvertex *v2)
{
    Xgl_pt_color_f3d points[3];
    Xgl_pt_list list;
    const GLchan *flat;
    GLfloat area;
    GLboolean front;
    unsigned long serial;
    if (!finite_vertex(v0) || !finite_vertex(v1) || !finite_vertex(v2) ||
        !start(a, ctx, GL_TRIANGLES))
        return GL_FALSE;
    area = (v1->win[0] - v0->win[0]) * (v2->win[1] - v0->win[1]) -
        (v1->win[1] - v0->win[1]) * (v2->win[0] - v0->win[0]);
    if (area == 0.0f)
        return GL_TRUE;
    if (ctx->Polygon.CullFlag) {
        front = ctx->Polygon.FrontFace == GL_CCW ? area > 0.0f : area < 0.0f;
        if (ctx->Polygon.CullFaceMode == GL_FRONT_AND_BACK ||
            (ctx->Polygon.CullFaceMode == GL_FRONT && front) ||
            (ctx->Polygon.CullFaceMode == GL_BACK && !front))
            return GL_TRUE;
    }
    serial = oldengl_xgl_error_serial();
    flat = ctx->Light.ShadeModel == GL_FLAT ? v2->color : NULL;
    vertex(a, points, v0, flat ? flat : v0->color);
    vertex(a, points + 1, v1, flat ? flat : v1->color);
    vertex(a, points + 2, v2, v2->color);
    point_list(&list, points, 3);
    xgl_triangle_list(a->context, NULL, &list, XGL_TLIST_FLAG_TRI_RESTART);
    if (!finish(a, serial))
        return GL_FALSE;
    ++a->stats.triangles;
    return GL_TRUE;
}

GLboolean
oldengl_xgl_accel_clear(OldenGLXGLAccel *a, GLcontext *ctx)
{
    Xgl_color color;
    unsigned long serial;
    if (!a || a->failed || !color_target(ctx) || ctx->Scissor.Enabled)
        return GL_FALSE;
    if (!oldengl_xgl_batch_reset(a))
        return GL_FALSE;
    serial = oldengl_xgl_error_serial();
    XSync(a->display, False);
    color.rgb.r = (float)ctx->Color.ClearColor[0] / CHAN_MAXF;
    color.rgb.g = (float)ctx->Color.ClearColor[1] / CHAN_MAXF;
    color.rgb.b = (float)ctx->Color.ClearColor[2] / CHAN_MAXF;
    xgl_object_set(a->context, XGL_CTX_BACKGROUND_COLOR, &color, 0);
    xgl_context_new_frame(a->context);
    if (!finish(a, serial))
        return GL_FALSE;
    ++a->stats.clears;
    return GL_TRUE;
}

GLboolean
oldengl_xgl_accel_copy_pixels(OldenGLXGLAccel *a, GLcontext *ctx,
                             GLint srcx, GLint srcy, GLsizei width,
                             GLsizei height, GLint dstx, GLint dsty)
{
    Xgl_bounds_i2d bounds;
    Xgl_pt_i2d position;
    unsigned long serial;
    if (!a || a->failed || !oldengl_xgl_accel_eligible(ctx, GL_TRIANGLES) ||
        ctx->Pixel.DriverReadBuffer != GL_FRONT_LEFT ||
        ctx->Pixel.ZoomX != 1.0f || ctx->Pixel.ZoomY != 1.0f ||
        ctx->_ImageTransferState || width < 1 || height < 1 ||
        width > a->width || height > a->height ||
        srcx < 0 || srcy < 0 || dstx < 0 || dsty < 0 ||
        srcx > a->width - width || dstx > a->width - width ||
        srcy > a->height - height || dsty > a->height - height)
        return GL_FALSE;
    if (!oldengl_xgl_batch_reset(a))
        return GL_FALSE;
    serial = oldengl_xgl_error_serial();
    XSync(a->display, False);
    bounds.xmin = srcx;
    bounds.xmax = srcx + width - 1;
    bounds.ymin = a->height - srcy - height;
    bounds.ymax = a->height - srcy - 1;
    position.x = dstx;
    position.y = a->height - dsty - height;
    xgl_context_copy_buffer(a->context, &bounds, &position, a->raster);
    if (!finish(a, serial))
        return GL_FALSE;
    ++a->stats.copies;
    return GL_TRUE;
}

void
oldengl_xgl_accel_get_stats(const OldenGLXGLAccel *a,
                           struct oldengl_xgl_accel_stats *stats)
{
    if (stats) {
        if (a)
            *stats = a->stats;
        else
            memset(stats, 0, sizeof(*stats));
    }
}
