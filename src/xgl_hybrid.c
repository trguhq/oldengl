/* MIT licensed. Native rasterization with ordered software fragment stages.
 * A nondisplayed XGL window buffer retains primary RGB. Temporary native Z
 * distinguishes covered fragments, including black fragments, from untouched
 * pixels. No application color, alpha, depth or stencil is committed until
 * both captures have succeeded. One primitive is processed at a time.
 */
#include "xgl_accel_private.h"
#include "xgl_fragments.h"
#include <float.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

static int finite_value(double value)
{
    return value >= -FLT_MAX && value <= FLT_MAX;
}

static GLboolean eligible(const GLcontext *ctx, GLenum mode)
{
    if (!ctx || ctx->RenderMode != GL_RENDER || !ctx->Visual.rgbMode ||
        ctx->Visual.redBits < 8 || ctx->Visual.greenBits < 8 ||
        ctx->Visual.blueBits < 8 || !ctx->DrawBuffer ||
        ctx->DrawBuffer->Width > MAX_WIDTH || ctx->DrawBuffer->Height > MAX_HEIGHT)
        return GL_FALSE;
    if (mode == GL_POINTS)
        return !ctx->Point.SmoothFlag && !ctx->Point._Attenuated && ctx->Point._Size == 1;
    if (mode == GL_LINES)
        return !ctx->Line.SmoothFlag && !ctx->Line.StippleFlag && ctx->Line.Width == 1;
    /* Filled triangles reach this callback after swsetup has handled
     * unfilled modes, two-sided materials and polygon depth offsets. */
    return mode == GL_TRIANGLES && !ctx->Polygon.SmoothFlag;
}

static GLboolean culled(const GLcontext *ctx, const SWvertex *a,
                         const SWvertex *b, const SWvertex *c)
{
    double area = ((double)b->win[0]-a->win[0])*((double)c->win[1]-a->win[1])-
        ((double)b->win[1]-a->win[1])*((double)c->win[0]-a->win[0]);
    int front;
    if (area == 0) return GL_TRUE;
    if (!ctx->Polygon.CullFlag) return GL_FALSE;
    if (ctx->Polygon.CullFaceMode == GL_FRONT_AND_BACK) return GL_TRUE;
    front = ctx->Polygon.FrontFace == GL_CCW ? area > 0 : area < 0;
    return (front && ctx->Polygon.CullFaceMode == GL_FRONT) ||
        (!front && ctx->Polygon.CullFaceMode == GL_BACK);
}

/* Clamp before converting to int; even very large legal viewports must not
 * overflow an integer while determining the capture rectangle. */
static GLint bound(double value, GLint maximum, int upper)
{
    if (value <= 0) return 0;
    if (value >= maximum) return maximum;
    return (GLint)(upper ? ceil(value) : floor(value));
}

/* Preserve clip W for XGL's perspective-correct texture interpolation.
 * The input is (screen_x*W,screen_y*W,W), and the view swaps Z and W.
 * After division it recovers screen XY and positive reciprocal W as Z.
 * This transform is invertible, and covered Z stays below the sentinel. */
static GLboolean texture_points(const GLcontext *ctx, const SWvertex *input[3],
    const Xgl_pt_color_f3d original[3], Xgl_usgn32 sentinel, char *packed)
{
    size_t stride = sizeof(Xgl_pt_color_data_f3d)+sizeof(GLfloat);
    GLuint i;
    GLfloat q;
    if (!ctx->Texture._ReallyEnabled || !oldengl_xgl_texture_rgb_eligible(ctx)) return GL_FALSE;
    q = input[0]->texcoord[0][3];
    if (!finite_value(q) || q == 0) return GL_FALSE;
    for (i = 0; i < 3; ++i) {
        Xgl_pt_color_data_f3d *point = (Xgl_pt_color_data_f3d *)(packed+i*stride);
        double inverse = input[i]->win[3], w;
        GLfloat uv[2];
        if (!(inverse > 0) || inverse >= sentinel*0.25 || input[i]->texcoord[0][3] != q)
            return GL_FALSE;
        w = 1.0/inverse;
        if (!finite_value(w) || !finite_value(original[i].x*w) ||
            !finite_value(original[i].y*w) ||
            !finite_value((double)input[i]->texcoord[0][0]/q) ||
            !finite_value((double)input[i]->texcoord[0][1]/q)) return GL_FALSE;
        point->x = (GLfloat)(original[i].x*w);
        point->y = (GLfloat)(original[i].y*w);
        point->z = (GLfloat)w; point->color = original[i].color;
        uv[0] = input[i]->texcoord[0][0]/q;
        uv[1] = input[i]->texcoord[0][1]/q;
        memcpy(packed+i*stride+offsetof(Xgl_pt_color_data_f3d, data), uv, sizeof(uv));
    }
    return GL_TRUE;
}

GLboolean oldengl_xgl_accel_hybrid(OldenGLXGLAccel *a, GLcontext *ctx, GLenum mode,
    const SWvertex *v0, const SWvertex *v1, const SWvertex *v2)
{
    struct oldengl_xgl_fragment_setup setup;
    const SWvertex *input[3];
    Xgl_pt_color_f3d point[3];
    union {
        Xgl_pt_color_data_f3d alignment;
        char bytes[3*(sizeof(Xgl_pt_color_data_f3d)+sizeof(GLfloat))];
    } textured;
    static const GLfloat perspective[16] = {
        1,0,0,0, 0,1,0,0, 0,0,0,1, 0,0,1,0
    };
    Xgl_pt_list list;
    Xgl_usgn32 sentinel;
    GLubyte rgb[MAX_WIDTH][3];
    GLboolean success = GL_FALSE;
    unsigned long serial;
    GLuint count, i, j;
    size_t pixels, offset;
    GLint x, y, first, xmin, xmax, ymin, ymax;
    double minx, maxx, miny, maxy;
    if (!a || a->failed || !eligible(ctx, mode) ||
        a->width != (GLsizei)ctx->DrawBuffer->Width ||
        a->height != (GLsizei)ctx->DrawBuffer->Height) return GL_FALSE;
    if (!oldengl_xgl_fragments_prepare(ctx, mode, v0, v1, v2, &setup)) return GL_FALSE;
    if (mode == GL_TRIANGLES && culled(ctx, v0, v1, v2)) {
        oldengl_xgl_fragments_release(&setup); return GL_TRUE;
    }
    input[0] = v0; input[1] = v1; input[2] = v2;
    count = setup.count;
    minx = maxx = v0->win[0]; miny = maxy = v0->win[1];
    memset(point, 0, sizeof(point));
    memset(&list, 0, sizeof(list));
    for (i = 0; i < count; ++i) {
        const GLchan *color = ctx->Light.ShadeModel == GL_FLAT ? input[count-1]->color : input[i]->color;
        if (input[i]->win[0] < minx) minx = input[i]->win[0];
        if (input[i]->win[0] > maxx) maxx = input[i]->win[0];
        if (input[i]->win[1] < miny) miny = input[i]->win[1];
        if (input[i]->win[1] > maxy) maxy = input[i]->win[1];
        point[i].x = input[i]->win[0]-0.5f;
        point[i].y = a->height-0.5f-input[i]->win[1];
        point[i].color.rgb.r = color[0]/CHAN_MAXF;
        point[i].color.rgb.g = color[1]/CHAN_MAXF;
        point[i].color.rgb.b = color[2]/CHAN_MAXF;
    }
    if (mode == GL_POINTS) {
        point[0].x = (GLfloat)floor(v0->win[0]);
        point[0].y = a->height-1.0f-(GLfloat)floor(v0->win[1]);
    }
    xmin = bound(minx-1, a->width, 0); xmax = bound(maxx+1, a->width, 1);
    ymin = bound(miny-1, a->height, 0); ymax = bound(maxy+1, a->height, 1);
    if (xmin == xmax || ymin == ymax) {
        oldengl_xgl_fragments_release(&setup); return GL_TRUE;
    }
    serial = oldengl_xgl_error_serial();
    if (!oldengl_xgl_batch_memory(a) || !oldengl_xgl_batch_reset(a) ||
        !oldengl_xgl_batch_target(a, GL_TRUE)) goto done;
    XSync(a->display, False);
    xgl_object_set(a->memory_context, XGL_3D_CTX_HLHSR_MODE, XGL_HLHSR_Z_BUFFER,
        XGL_3D_CTX_Z_BUFFER_COMP_METHOD, XGL_Z_COMP_ALWAYS,
        XGL_3D_CTX_Z_BUFFER_WRITE_MASK, (Xgl_usgn32)0xffffffffUL,
        XGL_CTX_RENDER_BUFFER, XGL_RENDER_Z_BUFFER, 0);
    xgl_object_get(a->memory, XGL_MEM_RAS_Z_BUFFER_ADDR, &a->memory_z);
    if (!a->memory_z || oldengl_xgl_error_serial() != serial) goto done;
    pixels = (size_t)a->width*a->height;
    for (offset = 0; offset < pixels; ++offset) a->memory_z[offset] = 0xffffffU;
    xgl_object_set(a->memory, XGL_RAS_SOURCE_BUFFER, XGL_BUFFER_SEL_Z, 0);
    xgl_object_set(a->context, XGL_3D_CTX_HLHSR_MODE, XGL_HLHSR_Z_BUFFER,
        XGL_3D_CTX_Z_BUFFER_COMP_METHOD, XGL_Z_COMP_ALWAYS,
        XGL_3D_CTX_Z_BUFFER_WRITE_MASK, (Xgl_usgn32)0xffffffffUL,
        XGL_CTX_RENDER_BUFFER, XGL_RENDER_Z_BUFFER, 0);
    xgl_context_copy_buffer(a->context, NULL, NULL, a->memory);
    xgl_context_flush(a->context, XGL_FLUSH_SYNCHRONIZE);
    /* Read the actual device sentinel: native Z may have fewer than 24
     * bits. A zero sentinel cannot distinguish our covered Z=0 pixels. */
    xgl_object_set(a->raster, XGL_RAS_SOURCE_BUFFER, XGL_BUFFER_SEL_Z, 0);
    xgl_context_copy_buffer(a->memory_context, NULL, NULL, a->raster);
    xgl_context_flush(a->memory_context, XGL_FLUSH_SYNCHRONIZE);
    sentinel = a->memory_z[0];
    if (!sentinel || oldengl_xgl_error_serial() != serial) goto done;
    list.pt_type = XGL_PT_COLOR_F3D; list.num_pts = count; list.pts.color_f3d = point;
    if (mode == GL_TRIANGLES && texture_points(ctx, input, point, sentinel, textured.bytes)) {
        if (!a->view) a->view = xgl_object_create(oldengl_xgl_system(), XGL_TRANS, NULL,
            XGL_TRANS_DATA_TYPE, XGL_DATA_FLT, XGL_TRANS_DIMENSION, XGL_TRANS_3D, 0);
        if (!a->texture) a->texture = oldengl_xgl_texture_create(oldengl_xgl_system());
        if (!a->view || !a->texture ||
            !oldengl_xgl_texture_configure(a->texture, a->context, ctx)) goto done;
        xgl_transform_write_specific(a->view, (void *)perspective, 0);
        xgl_object_set(a->context, XGL_CTX_VIEW_TRANS, a->view, 0);
        list.pt_type = XGL_PT_COLOR_DATA_F3D; list.num_data_values = 2;
        list.pts.color_data_f3d = (Xgl_pt_color_data_f3d *)textured.bytes;
        setup.texture_rgb_complete = GL_TRUE;
    }
    /* Geometry writes its Z through HLHSR, while RENDER_DRAW_BUFFER
     * selects the color target. Z-only rendering is for raster copies. */
    xgl_object_set(a->context, XGL_CTX_RENDER_BUFFER, XGL_RENDER_DRAW_BUFFER,
        XGL_3D_CTX_LINE_COLOR_INTERP, (Xgl_boolean)(ctx->Light.ShadeModel == GL_SMOOTH), 0);
    if (mode == GL_POINTS) xgl_multimarker(a->context, &list);
    else if (mode == GL_LINES) xgl_multipolyline(a->context, NULL, 1, &list);
    else xgl_triangle_list(a->context, NULL, &list, XGL_TLIST_FLAG_TRI_RESTART);
    xgl_context_flush(a->context, XGL_FLUSH_SYNCHRONIZE);
    xgl_object_set(a->raster, XGL_RAS_SOURCE_BUFFER, (Xgl_usgn32)1, 0);
    xgl_object_set(a->memory_context, XGL_3D_CTX_HLHSR_MODE, XGL_HLHSR_NONE,
        XGL_CTX_RENDER_BUFFER, XGL_RENDER_DRAW_BUFFER, 0);
    xgl_context_copy_buffer(a->memory_context, NULL, NULL, a->raster);
    xgl_context_flush(a->memory_context, XGL_FLUSH_SYNCHRONIZE);
    xgl_object_set(a->raster, XGL_RAS_SOURCE_BUFFER, XGL_BUFFER_SEL_Z, 0);
    xgl_object_set(a->memory_context, XGL_3D_CTX_HLHSR_MODE, XGL_HLHSR_Z_BUFFER,
        XGL_CTX_RENDER_BUFFER, XGL_RENDER_Z_BUFFER, 0);
    xgl_context_copy_buffer(a->memory_context, NULL, NULL, a->raster);
    xgl_context_flush(a->memory_context, XGL_FLUSH_SYNCHRONIZE);
    if (oldengl_xgl_error_serial() != serial || !oldengl_xgl_batch_reset(a)) goto done;
    /* All fallible native work is finished. Commit spans in GL primitive
     * order; never merge overlapping primitives into one scratch image. */
    for (y = ymin; y < ymax; ++y) {
        x = xmin;
        while (x < xmax) {
            offset = (size_t)(a->height-1-y)*a->width+x;
            if (a->memory_z[offset] == sentinel) { ++x; continue; }
            first = x;
            while (x < xmax && a->memory_z[(size_t)(a->height-1-y)*a->width+x] != sentinel) {
                Xgl_usgn32 word = a->memory_rgb[(size_t)(a->height-1-y)*a->width+x];
                j = (GLuint)(x-first);
                rgb[j][0] = (GLubyte)(word >> a->red_shift);
                rgb[j][1] = (GLubyte)(word >> a->green_shift);
                rgb[j][2] = (GLubyte)(word >> a->blue_shift);
                ++x;
            }
            oldengl_xgl_fragments_emit_span(&setup, first, y, (GLuint)(x-first),
                (const GLubyte (*)[3])rgb);
            a->stats.hybrid_fragments += (unsigned long)(x-first);
        }
    }
    ++a->stats.hybrid_primitives;
    if (setup.texture_rgb_complete) ++a->stats.hybrid_textured;
    if (mode == GL_POINTS) ++a->stats.points;
    else if (mode == GL_LINES) ++a->stats.lines;
    else ++a->stats.triangles;
    success = GL_TRUE;
done:
    if (!success) {
        /* The scratch target is never displayed. CPU color/depth/stencil
         * are still untouched, so the ordinary rasterizer can replay. */
        (void)oldengl_xgl_batch_reset(a);
        if (oldengl_xgl_error_serial() != serial) a->failed = GL_TRUE;
    }
    oldengl_xgl_fragments_release(&setup);
    return success;
}
