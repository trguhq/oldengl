/* MIT licensed. Object-space XGL transform/clip/light/depth rendering.
 * Color and Z staging make native draws visible to every software fallback.
 */
#include "xgl_accel_private.h"
#include "xgl_tnl.h"
#include "xgl_pixels.h"
#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static const GLfloat identity[16] = {
    1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1
};

static int finite_number(double n) { return n >= -FLT_MAX && n <= FLT_MAX; }
static GLfloat unit_color(GLfloat n) { return n < 0 ? 0 : n > 1 ? 1 : n; }

void oldengl_xgl_batch_resize(OldenGLXGLAccel *a)
{
    if (a->memory_context) xgl_object_destroy(a->memory_context);
    if (a->memory) xgl_object_destroy(a->memory);
    a->memory_context = a->memory = 0;
    a->memory_rgb = a->memory_z = NULL;
    memset(a->depth_rounding, 0, sizeof(a->depth_rounding));
    a->capabilities_queried = a->back_checked = a->back_available = GL_FALSE;
}

void oldengl_xgl_batch_cleanup(OldenGLXGLAccel *a)
{
    oldengl_xgl_batch_resize(a);
    if (a->model) xgl_object_destroy(a->model);
    if (a->view) xgl_object_destroy(a->view);
    oldengl_xgl_lighting_destroy(a->lighting);
    oldengl_xgl_texture_destroy(a->texture);
    a->model = a->view = 0;
    a->lighting = NULL;
    a->texture = NULL;
}

GLboolean oldengl_xgl_batch_reset(OldenGLXGLAccel *a)
{
    unsigned long serial;
    if (!a || a->failed) return GL_FALSE;
    if (!a->model && !a->back_checked) return GL_TRUE;
    serial = oldengl_xgl_error_serial();
    if (a->model) xgl_transform_write_specific(a->model, (void *)identity, 0);
    if (a->view) xgl_transform_write_specific(a->view, (void *)identity, 0);
    xgl_object_set(a->raster, XGL_WIN_RAS_BUF_DRAW, (Xgl_usgn32)0,
        XGL_RAS_SOURCE_BUFFER, XGL_BUFFER_SEL_DRAW, 0);
    xgl_object_set(a->context,
        XGL_3D_CTX_SURF_FRONT_TMAP_NUM, (Xgl_usgn32)0,
        XGL_3D_CTX_SURF_BACK_TMAP_NUM, (Xgl_usgn32)0, XGL_CTX_CLIP_PLANES, 0,
        XGL_3D_CTX_HLHSR_MODE, XGL_HLHSR_NONE,
        XGL_CTX_RENDER_BUFFER, XGL_RENDER_DRAW_BUFFER,
        XGL_CTX_SURF_FRONT_COLOR_SELECTOR, XGL_SURF_COLOR_VERTEX_ILLUM_DEP,
        XGL_3D_CTX_SURF_FRONT_ILLUMINATION, XGL_ILLUM_NONE_INTERP_COLOR, 0);
    return oldengl_xgl_error_serial() == serial;
}

static GLboolean common_state(const GLcontext *c)
{
    return c && c->Visual.rgbMode && !c->Visual.alphaBits &&
        c->Visual.redBits >= 8 && c->Visual.greenBits >= 8 && c->Visual.blueBits >= 8 &&
        c->RenderMode == GL_RENDER && oldengl_xgl_accel_fragment_eligible(c) &&
        !c->Transform._AnyClip &&
        c->Color.ColorMask[0] && c->Color.ColorMask[1] && c->Color.ColorMask[2] &&
        ((c->Color.DriverDrawBuffer == GL_FRONT_LEFT && c->Color.DrawDestMask == FRONT_LEFT_BIT) ||
         (c->Color.DriverDrawBuffer == GL_BACK_LEFT && c->Color.DrawDestMask == BACK_LEFT_BIT)) &&
        c->Viewport.Width > 0 && c->Viewport.Height > 0 &&
        c->Viewport.Near != c->Viewport.Far;
}

/* A window-space winding check is needed because XGL facet-normal culling
 * is not guaranteed to be GL's post-projection winding convention.
 */
static int projected(const GLcontext *c, const struct oldengl_xgl_vertex *v,
                     double *x, double *y)
{
    double eye[4], clip[4];
    unsigned int r, k;
    for (r = 0; r < 4; ++r) {
        eye[r] = 0;
        for (k = 0; k < 4; ++k) eye[r] += c->ModelView.m[k*4+r] * v->position[k];
    }
    for (r = 0; r < 4; ++r) {
        clip[r] = 0;
        for (k = 0; k < 4; ++k) clip[r] += c->ProjectionMatrix.m[k*4+r] * eye[k];
    }
    if (!(clip[3] > 0) || !finite_number(clip[3])) return 0;
    *x = clip[0] / clip[3]; *y = clip[1] / clip[3];
    return finite_number(*x) && finite_number(*y);
}

static int visible_triangle(const GLcontext *c, const struct oldengl_xgl_vertex *v)
{
    double x[3], y[3], area;
    int i, front;
    if (!c->Polygon.CullFlag) return 1;
    if (c->Polygon.CullFaceMode == GL_FRONT_AND_BACK) return 0;
    for (i = 0; i < 3; ++i) if (!projected(c, v+i, x+i, y+i)) return -1;
    area = (x[1]-x[0])*(y[2]-y[0]) - (y[1]-y[0])*(x[2]-x[0]);
    if (!finite_number(area)) return -1;
    if (area == 0) return 0;
    front = c->Polygon.FrontFace == GL_CCW ? area > 0 : area < 0;
    return !((front && c->Polygon.CullFaceMode == GL_FRONT) ||
             (!front && c->Polygon.CullFaceMode == GL_BACK));
}

static double normal_scale(const GLcontext *c, const struct oldengl_xgl_vertex *v)
{
    double n[3], length;
    unsigned int r, k;
    if (!c->ModelView.inv) return 0;
    for (r = 0; r < 3; ++r) {
        n[r] = 0;
        for (k = 0; k < 3; ++k) n[r] += c->ModelView.inv[r*4+k] * v->normal[k];
    }
    length = sqrt(n[0]*n[0] + n[1]*n[1] + n[2]*n[2]);
    if (!(length > 0) || !finite_number(length)) return 0;
    if (c->Transform.Normalize) return 1.0 / length;
    /* With normalization disabled, XGL's unit-normal contract is usable
     * only if GL's transformed normal is already unit length.
     */
    if (fabs(length-1.0) > 0.00001) return 0;
    return 1.0;
}

static GLboolean preflight(const GLcontext *c, const struct oldengl_xgl_batch *b)
{
    GLuint i, j;
    GLfloat texture_q = 0.0f;
    if (!common_state(c) || !b || !b->vertices || !b->commands || !b->vertex_count ||
        !b->command_count || !oldengl_xgl_lighting_eligible(c) ||
        !oldengl_xgl_texture_eligible(c)) return GL_FALSE;
    for (i = 0; i < 16; ++i)
        if (!finite_number(c->ModelView.m[i]) || !finite_number(c->ProjectionMatrix.m[i]))
            return GL_FALSE;
    if (c->Light.Enabled) {
        if (c->Transform.RescaleNormals && !c->Transform.Normalize) return GL_FALSE;
        if (c->ModelView.m[3] || c->ModelView.m[7] ||
            c->ModelView.m[11] || c->ModelView.m[15] != 1) return GL_FALSE;
        for (i = 0; i < b->vertex_count; ++i)
            if (!normal_scale(c, b->vertices+i)) return GL_FALSE;
    }
    if (c->Texture._ReallyEnabled) for (i = 0; i < b->vertex_count; ++i) {
        GLfloat stq[3];
        if (!oldengl_xgl_texture_coordinates(c, b->vertices[i].texcoord,
                b->vertices[i].position, stq) || stq[2] == 0.0f) return GL_FALSE;
        if (i && stq[2] != texture_q) return GL_FALSE;
        texture_q = stq[2];
        if (!finite_number((double)stq[0]/texture_q) ||
            !finite_number((double)stq[1]/texture_q)) return GL_FALSE;
    }
    for (i = 0; i < b->command_count; ++i) {
        const struct oldengl_xgl_command *d = b->commands+i;
        if (d->first > b->vertex_count || d->count > b->vertex_count-d->first) return GL_FALSE;
        if (d->mode == GL_POINTS) {
            if (c->Light.Enabled || c->Texture._ReallyEnabled) return GL_FALSE;
            if (c->Point.SmoothFlag || c->Point._Attenuated || c->Point._Size != 1) return GL_FALSE;
        } else if (d->mode == GL_LINES) {
            if (c->Light.Enabled || c->Texture._ReallyEnabled) return GL_FALSE;
            if (d->count%2 || c->Line.SmoothFlag || c->Line.StippleFlag || c->Line.Width != 1) return GL_FALSE;
        } else if (d->mode == GL_TRIANGLES) {
            if (d->count%3 || c->Polygon.SmoothFlag || c->Polygon.StippleFlag ||
                c->Polygon.OffsetFill || c->Polygon.FrontMode != GL_FILL ||
                c->Polygon.BackMode != GL_FILL) return GL_FALSE;
            for (j = 0; j < d->count; j += 3)
                if (visible_triangle(c, b->vertices+d->first+j) < 0) return GL_FALSE;
        } else return GL_FALSE;
    }
    return GL_TRUE;
}

static GLboolean transforms(OldenGLXGLAccel *a)
{
    unsigned long serial = oldengl_xgl_error_serial();
    if (!a->capabilities_queried) {
        Xgl_pt_d3d maximum;
        memset(&maximum, 0, sizeof(maximum));
        xgl_object_get(a->raster, XGL_DEV_MAXIMUM_COORDINATES, &maximum);
        if (oldengl_xgl_error_serial() != serial) return GL_FALSE;
        a->maximum_z = maximum.z;
        a->capabilities_queried = GL_TRUE;
    }
    if (!a->model) a->model = xgl_object_create(oldengl_xgl_system(), XGL_TRANS, NULL,
        XGL_TRANS_DATA_TYPE, XGL_DATA_FLT, XGL_TRANS_DIMENSION, XGL_TRANS_3D, 0);
    if (!a->view) a->view = xgl_object_create(oldengl_xgl_system(), XGL_TRANS, NULL,
        XGL_TRANS_DATA_TYPE, XGL_DATA_FLT, XGL_TRANS_DIMENSION, XGL_TRANS_3D, 0);
    if (!a->lighting) a->lighting = oldengl_xgl_lighting_create(oldengl_xgl_system());
    if (!a->texture) a->texture = oldengl_xgl_texture_create(oldengl_xgl_system());
    return a->model && a->view && a->lighting && a->texture &&
        oldengl_xgl_error_serial() == serial;
}

GLboolean oldengl_xgl_batch_memory(OldenGLXGLAccel *a)
{
    Xgl_object calibration;
    Xgl_color color;
    Xgl_pt_i2d position;
    unsigned int component, shift, shifts[3];
    unsigned long serial = oldengl_xgl_error_serial();
    if (a->memory && a->memory_context && a->memory_rgb) return GL_TRUE;
    if (a->memory_context) xgl_object_destroy(a->memory_context);
    if (a->memory) xgl_object_destroy(a->memory);
    a->memory_context = a->memory = 0;
    a->memory_rgb = a->memory_z = NULL;
    a->memory = xgl_object_create(oldengl_xgl_system(), XGL_MEM_RAS, NULL,
        XGL_DEV_COLOR_TYPE, XGL_COLOR_RGB, XGL_RAS_DEPTH, 32,
        XGL_RAS_WIDTH, a->width, XGL_RAS_HEIGHT, a->height, 0);
    if (!a->memory || oldengl_xgl_error_serial() != serial) return GL_FALSE;
    xgl_object_get(a->memory, XGL_MEM_RAS_IMAGE_BUFFER_ADDR, &a->memory_rgb);
    if (!a->memory_rgb || oldengl_xgl_error_serial() != serial) return GL_FALSE;
    calibration = xgl_object_create(oldengl_xgl_system(), XGL_2D_CTX, NULL,
        XGL_CTX_DEVICE, a->memory, XGL_CTX_DEFERRAL_MODE, XGL_DEFER_ASAP, 0);
    if (!calibration) return GL_FALSE;
    position.x = position.y = 0;
    for (component = 0; component < 3; ++component) {
        a->memory_rgb[0] = 0;
        color.rgb.r = component == 0 ? 1.0f : 0.0f;
        color.rgb.g = component == 1 ? 1.0f : 0.0f;
        color.rgb.b = component == 2 ? 1.0f : 0.0f;
        xgl_context_set_pixel(calibration, &position, &color);
        xgl_context_flush(calibration, XGL_FLUSH_SYNCHRONIZE);
        for (shift = 0; shift <= 24; shift += 8)
            if (a->memory_rgb[0] == ((Xgl_usgn32)255 << shift)) break;
        if (shift > 24 || oldengl_xgl_error_serial() != serial) {
            xgl_object_destroy(calibration); return GL_FALSE;
        }
        shifts[component] = shift;
    }
    xgl_object_destroy(calibration);
    if (shifts[0] == shifts[1] || shifts[0] == shifts[2] || shifts[1] == shifts[2]) return GL_FALSE;
    a->red_shift = shifts[0]; a->green_shift = shifts[1]; a->blue_shift = shifts[2];
    a->memory_context = xgl_object_create(oldengl_xgl_system(), XGL_3D_CTX, NULL,
        XGL_CTX_DEVICE, a->memory, XGL_CTX_DEFERRAL_MODE, XGL_DEFER_ASAP,
        XGL_CTX_ROP, XGL_ROP_COPY, XGL_CTX_RASTER_FILL_STYLE, XGL_RAS_FILL_COPY, 0);
    return a->memory_context && oldengl_xgl_error_serial() == serial;
}

GLboolean oldengl_xgl_batch_target(OldenGLXGLAccel *a, GLboolean back)
{
    unsigned long serial = oldengl_xgl_error_serial();
    if (back && !a->back_checked) {
        Xgl_usgn32 allocated = 0;
        xgl_object_set(a->raster, XGL_WIN_RAS_BUFFERS_REQUESTED, (Xgl_usgn32)2, 0);
        xgl_object_get(a->raster, XGL_WIN_RAS_BUFFERS_ALLOCATED, &allocated);
        a->back_checked = GL_TRUE;
        a->back_available = allocated >= 2 && oldengl_xgl_error_serial() == serial;
    }
    if (back && !a->back_available) return GL_FALSE;
    xgl_object_set(a->raster, XGL_WIN_RAS_BUF_DISPLAY, (Xgl_usgn32)0,
        XGL_WIN_RAS_BUF_DRAW, (Xgl_usgn32)(back ? 1 : 0), 0);
    return oldengl_xgl_error_serial() == serial;
}

static int depth_function(GLenum function)
{
    switch (function) {
    case GL_NEVER: return XGL_Z_COMP_NEVER;
    case GL_LESS: return XGL_Z_COMP_LESS_THAN;
    case GL_EQUAL: return XGL_Z_COMP_EQUAL;
    case GL_LEQUAL: return XGL_Z_COMP_LESS_THAN_OR_EQUAL;
    case GL_GREATER: return XGL_Z_COMP_GREATER_THAN;
    case GL_NOTEQUAL: return XGL_Z_COMP_NOT_EQUAL;
    case GL_GEQUAL: return XGL_Z_COMP_GREATER_THAN_OR_EQUAL;
    default: return XGL_Z_COMP_ALWAYS;
    }
}

static GLboolean depth_upload(OldenGLXGLAccel *a, const GLcontext *c)
{
    GLuint x, y, width = (GLuint)a->width, height = (GLuint)a->height;
    unsigned long serial = oldengl_xgl_error_serial();
    if (!c->Depth.Test) {
        xgl_object_set(a->context, XGL_3D_CTX_HLHSR_MODE, XGL_HLHSR_NONE, 0);
        return oldengl_xgl_error_serial() == serial;
    }
    xgl_object_set(a->memory_context, XGL_3D_CTX_HLHSR_MODE, XGL_HLHSR_Z_BUFFER,
        XGL_3D_CTX_Z_BUFFER_COMP_METHOD, XGL_Z_COMP_ALWAYS,
        XGL_3D_CTX_Z_BUFFER_WRITE_MASK, (Xgl_usgn32)0xffffffffUL, 0);
    xgl_object_get(a->memory, XGL_MEM_RAS_Z_BUFFER_ADDR, &a->memory_z);
    if (!a->memory_z || oldengl_xgl_error_serial() != serial) return GL_FALSE;
    for (y = 0; y < height; ++y) for (x = 0; x < width; ++x) {
        size_t source = (size_t)y*width+x;
        GLuint z = c->Visual.depthBits <= 16 ?
            ((GLushort *)c->DrawBuffer->DepthBuffer)[source] :
            ((GLuint *)c->DrawBuffer->DepthBuffer)[source];
        a->memory_z[(size_t)(height-y-1)*width+x] = (Xgl_usgn32)z;
    }
    xgl_object_set(a->context, XGL_3D_CTX_HLHSR_MODE, XGL_HLHSR_Z_BUFFER,
        XGL_3D_CTX_Z_BUFFER_COMP_METHOD, XGL_Z_COMP_ALWAYS,
        XGL_3D_CTX_Z_BUFFER_WRITE_MASK, (Xgl_usgn32)0xffffffffUL,
        XGL_CTX_RENDER_BUFFER, XGL_RENDER_Z_BUFFER, 0);
    xgl_object_set(a->memory, XGL_RAS_SOURCE_BUFFER, XGL_BUFFER_SEL_Z, 0);
    xgl_context_copy_buffer(a->context, NULL, NULL, a->memory);
    xgl_context_flush(a->context, XGL_FLUSH_SYNCHRONIZE);
    xgl_object_set(a->context,
        XGL_3D_CTX_Z_BUFFER_COMP_METHOD, depth_function(c->Depth.Func),
        XGL_3D_CTX_Z_BUFFER_WRITE_MASK, (Xgl_usgn32)(c->Depth.Mask ? 0xffffffffUL : 0),
        XGL_CTX_RENDER_BUFFER, XGL_RENDER_DRAW_BUFFER, 0);
    return oldengl_xgl_error_serial() == serial;
}

static GLboolean depth_download(OldenGLXGLAccel *a, GLcontext *c)
{
    GLuint x, y, width = (GLuint)a->width, height = (GLuint)a->height;
    unsigned long serial = oldengl_xgl_error_serial();
    if (!c->Depth.Test || !c->Depth.Mask) return GL_TRUE;
    xgl_object_set(a->raster, XGL_RAS_SOURCE_BUFFER, XGL_BUFFER_SEL_Z, 0);
    xgl_object_set(a->memory_context, XGL_CTX_RENDER_BUFFER, XGL_RENDER_Z_BUFFER,
        XGL_3D_CTX_HLHSR_MODE, XGL_HLHSR_Z_BUFFER,
        XGL_3D_CTX_Z_BUFFER_COMP_METHOD, XGL_Z_COMP_ALWAYS,
        XGL_3D_CTX_Z_BUFFER_WRITE_MASK, (Xgl_usgn32)0xffffffffUL, 0);
    xgl_context_copy_buffer(a->memory_context, NULL, NULL, a->raster);
    xgl_context_flush(a->memory_context, XGL_FLUSH_SYNCHRONIZE);
    if (oldengl_xgl_error_serial() != serial) return GL_FALSE;
    for (y = 0; y < height; ++y) for (x = 0; x < width; ++x) {
        size_t destination = (size_t)y*width+x;
        GLuint z = (GLuint)a->memory_z[(size_t)(height-y-1)*width+x];
        if (c->Visual.depthBits <= 16) ((GLushort *)c->DrawBuffer->DepthBuffer)[destination] = (GLushort)z;
        else ((GLuint *)c->DrawBuffer->DepthBuffer)[destination] = z;
    }
    return GL_TRUE;
}

/* XGL does not specify integer Z rounding. Probe the actual raster path
 * in a nondisplayed window buffer. XGL_RENDER_Z_BUFFER is restricted to
 * raster operators: geometry primitives require XGL_RENDER_DRAW_BUFFER.
 * Mesa's authoritative Z image is uploaded after these scratch probes.
 */
static GLboolean depth_mapping(OldenGLXGLAccel *a, const GLcontext *c,
                               const struct oldengl_xgl_batch *batch,
                               double *bias)
{
    static const GLfloat samples[6] = {0.25f,0.5f,0.75f,1.25f,1.5f,1.75f};
    static const Xgl_usgn32 truncated[6] = {0,0,0,1,1,1};
    static const Xgl_usgn32 nearest[6] = {0,1,1,1,2,2};
    Xgl_pt_color_f3d vertex[3];
    Xgl_pt_list list;
    Xgl_usgn32 saved_draw = 0;
    GLuint command, sample, i;
    unsigned long serial = oldengl_xgl_error_serial();
    int kind, is_truncated, is_nearest, first = 1;
    GLboolean scratch_active = GL_FALSE, success = GL_FALSE;
    double selected;
    *bias = 0;
    if (!c->Depth.Test) return GL_TRUE;
    xgl_object_set(a->memory_context, XGL_3D_CTX_HLHSR_MODE, XGL_HLHSR_Z_BUFFER,
        XGL_3D_CTX_Z_BUFFER_COMP_METHOD, XGL_Z_COMP_ALWAYS,
        XGL_3D_CTX_Z_BUFFER_WRITE_MASK, (Xgl_usgn32)0xffffffffUL,
        XGL_CTX_RENDER_BUFFER, XGL_RENDER_Z_BUFFER, 0);
    xgl_object_get(a->memory, XGL_MEM_RAS_Z_BUFFER_ADDR, &a->memory_z);
    if (!a->memory_z || oldengl_xgl_error_serial() != serial) goto done;
    for (command = 0; command < batch->command_count; ++command) {
        GLenum mode = batch->commands[command].mode;
        kind = mode == GL_POINTS ? 0 : mode == GL_LINES ? 1 : 2;
        if (!a->depth_rounding[kind]) {
            if (!scratch_active) {
                if (!a->back_checked) {
                    Xgl_usgn32 allocated = 0;
                    xgl_object_set(a->raster, XGL_WIN_RAS_BUFFERS_REQUESTED,
                        (Xgl_usgn32)2, 0);
                    xgl_object_get(a->raster, XGL_WIN_RAS_BUFFERS_ALLOCATED, &allocated);
                    a->back_checked = GL_TRUE;
                    a->back_available = allocated >= 2 &&
                        oldengl_xgl_error_serial() == serial;
                }
                if (!a->back_available) goto done;
                xgl_object_get(a->raster, XGL_WIN_RAS_BUF_DRAW, &saved_draw);
                if (oldengl_xgl_error_serial() != serial) goto done;
                scratch_active = GL_TRUE;
                xgl_object_set(a->raster, XGL_WIN_RAS_BUF_DISPLAY, (Xgl_usgn32)0,
                    XGL_WIN_RAS_BUF_DRAW, (Xgl_usgn32)1, 0);
                if (oldengl_xgl_error_serial() != serial) goto done;
            }
            memset(vertex, 0, sizeof(vertex));
            memset(&list, 0, sizeof(list));
            list.pt_type = XGL_PT_COLOR_F3D;
            list.num_pts = (Xgl_usgn32)(kind+1);
            list.pts.color_f3d = vertex;
            if (kind == 1) vertex[1].x = 2;
            if (kind == 2) {
                vertex[0].x = vertex[0].y = -1;
                vertex[1].x = 2; vertex[1].y = -1;
                vertex[2].x = -1; vertex[2].y = 2;
            }
            xgl_object_set(a->context, XGL_3D_CTX_HLHSR_MODE, XGL_HLHSR_Z_BUFFER,
                XGL_3D_CTX_Z_BUFFER_COMP_METHOD, XGL_Z_COMP_ALWAYS,
                XGL_3D_CTX_Z_BUFFER_WRITE_MASK, (Xgl_usgn32)0xffffffffUL,
                XGL_CTX_RENDER_BUFFER, XGL_RENDER_DRAW_BUFFER, 0);
            is_truncated = is_nearest = 1;
            for (sample = 0; sample < 6; ++sample) {
                for (i = 0; i < list.num_pts; ++i) vertex[i].z = samples[sample];
                if (kind == 0) xgl_multimarker(a->context, &list);
                else if (kind == 1) xgl_multipolyline(a->context, NULL, 1, &list);
                else xgl_triangle_list(a->context, NULL, &list, XGL_TLIST_FLAG_TRI_RESTART);
                xgl_context_flush(a->context, XGL_FLUSH_SYNCHRONIZE);
                xgl_object_set(a->raster, XGL_RAS_SOURCE_BUFFER, XGL_BUFFER_SEL_Z, 0);
                xgl_context_copy_buffer(a->memory_context, NULL, NULL, a->raster);
                xgl_context_flush(a->memory_context, XGL_FLUSH_SYNCHRONIZE);
                if (oldengl_xgl_error_serial() != serial) goto done;
                if (a->memory_z[0] != truncated[sample]) is_truncated = 0;
                if (a->memory_z[0] != nearest[sample]) is_nearest = 0;
            }
            a->depth_rounding[kind] = is_truncated ? 1 : is_nearest ? 2 : -1;
        }
        if (a->depth_rounding[kind] < 0) goto done;
        /* Mesa rounds points and 16-bit lines/triangles, but truncates
         * 24-bit line/triangle Z. Align native conversion at that boundary. */
        selected = (kind == 0 || c->Visual.depthBits <= 16 ? 0.5 : 0.0) -
            (a->depth_rounding[kind] == 2 ? 0.5 : 0.0);
        if (!first && *bias != selected) goto done;
        *bias = selected; first = 0;
    }
    success = GL_TRUE;
done:
    if (scratch_active)
        xgl_object_set(a->raster, XGL_WIN_RAS_BUF_DRAW, saved_draw, 0);
    return success && oldengl_xgl_error_serial() == serial;
}

static GLboolean configure_view(OldenGLXGLAccel *a, const GLcontext *c, double depth_bias)
{
    GLfloat view[16];
    double sx = c->Viewport.Width*0.5, sy = c->Viewport.Height*0.5;
    double tx = c->Viewport.X+sx-0.5, ty = (double)a->height-c->Viewport.Y-sy-0.5;
    double maximum = c->DepthMaxF > 0 ? c->DepthMaxF : 1.0;
    double sz = maximum*(c->Viewport.Far-c->Viewport.Near)*0.5;
    double tz = maximum*(c->Viewport.Far+c->Viewport.Near)*0.5 + depth_bias;
    Xgl_bounds_d3d clip;
    unsigned int column;
    const GLfloat *p = c->ProjectionMatrix.m;
    for (column = 0; column < 4; ++column) {
        view[column*4] = (GLfloat)(sx*p[column*4] + tx*p[column*4+3]);
        view[column*4+1] = (GLfloat)(-sy*p[column*4+1] + ty*p[column*4+3]);
        view[column*4+2] = (GLfloat)(sz*p[column*4+2] + tz*p[column*4+3]);
        view[column*4+3] = p[column*4+3];
    }
    for (column = 0; column < 16; ++column) if (!finite_number(view[column])) return GL_FALSE;
    clip.xmin = c->Viewport.X-0.5; clip.xmax = (double)c->Viewport.X+c->Viewport.Width-0.5;
    clip.ymin = (double)a->height-c->Viewport.Y-c->Viewport.Height-0.5;
    clip.ymax = (double)a->height-c->Viewport.Y-0.5;
    clip.zmin = depth_bias + maximum*(c->Viewport.Near < c->Viewport.Far ? c->Viewport.Near : c->Viewport.Far);
    clip.zmax = depth_bias + maximum*(c->Viewport.Near > c->Viewport.Far ? c->Viewport.Near : c->Viewport.Far);
    /* GL column-major storage is the transpose required by XGL row vectors. */
    xgl_transform_write_specific(a->model, (void *)c->ModelView.m, 0);
    xgl_transform_write_specific(a->view, view, 0);
    xgl_object_set(a->context, XGL_CTX_LOCAL_MODEL_TRANS, a->model,
        XGL_CTX_VIEW_TRANS, a->view, XGL_CTX_VIEW_CLIP_BOUNDS, &clip,
        XGL_CTX_CLIP_PLANES, XGL_CLIP_XMIN | XGL_CLIP_XMAX | XGL_CLIP_YMIN |
            XGL_CLIP_YMAX | XGL_CLIP_ZMIN | XGL_CLIP_ZMAX, 0);
    return GL_TRUE;
}

GLboolean oldengl_xgl_accel_draw_batch(OldenGLXGLAccel *a, GLcontext *c,
    const struct oldengl_xgl_batch *b, GLubyte *back_rgba)
{
    GLboolean back, textured, lighting, success = GL_FALSE;
    char *vertices = NULL;
    size_t stride;
    double depth_bias;
    Xgl_pt_list list;
    GLuint command, i, count;
    unsigned long serial;
    if (!a || a->failed || !preflight(c, b)) return GL_FALSE;
    back = c->Color.DriverDrawBuffer == GL_BACK_LEFT;
    textured = c->Texture._ReallyEnabled != 0;
    lighting = c->Light.Enabled;
    if (back && !back_rgba) return GL_FALSE;
    if (!transforms(a)) return GL_FALSE;
    if (c->Depth.Test && (!c->DrawBuffer || !c->DrawBuffer->DepthBuffer ||
        c->DrawBuffer->Width != (GLuint)a->width || c->DrawBuffer->Height != (GLuint)a->height ||
        (c->Visual.depthBits != 16 && c->Visual.depthBits != 24) ||
        a->maximum_z < c->DepthMaxF || a->maximum_z > 16777215.0)) return GL_FALSE;
    if ((back || c->Depth.Test) && !oldengl_xgl_batch_memory(a)) return GL_FALSE;
    stride = textured ? (lighting ? sizeof(Xgl_pt_color_normal_data_f3d) :
        sizeof(Xgl_pt_color_data_f3d)) + sizeof(float) :
        lighting ? sizeof(Xgl_pt_color_normal_f3d) : sizeof(Xgl_pt_color_f3d);
    if (b->vertex_count > ((size_t)-1)/stride) return GL_FALSE;
    vertices = (char *)calloc(b->vertex_count, stride);
    if (!vertices) return GL_FALSE;
    for (i = 0; i < b->vertex_count; ++i) {
        const struct oldengl_xgl_vertex *v = b->vertices+i;
        Xgl_pt_color_f3d *p = (Xgl_pt_color_f3d *)(vertices+i*stride);
        p->x = v->position[0]; p->y = v->position[1]; p->z = v->position[2];
        p->color.rgb.r = unit_color(v->color[0]);
        p->color.rgb.g = unit_color(v->color[1]); p->color.rgb.b = unit_color(v->color[2]);
        if (lighting) {
            double scale = normal_scale(c, v);
            Xgl_pt_color_normal_f3d *n = (Xgl_pt_color_normal_f3d *)p;
            n->normal.x = (GLfloat)(v->normal[0]*scale);
            n->normal.y = (GLfloat)(v->normal[1]*scale);
            n->normal.z = (GLfloat)(v->normal[2]*scale);
        }
        if (textured) {
            GLfloat stq[3], uv[2];
            size_t offset = lighting ? offsetof(Xgl_pt_color_normal_data_f3d, data) :
                offsetof(Xgl_pt_color_data_f3d, data);
            oldengl_xgl_texture_coordinates(c, v->texcoord, v->position, stq);
            uv[0] = stq[0]/stq[2]; uv[1] = stq[1]/stq[2];
            memcpy(vertices+i*stride+offset, uv, sizeof(uv));
        }
    }
    /* Preserve flat provoking colors after decomposition, before any draw. */
    if (!lighting && c->Light.ShadeModel == GL_FLAT) for (command = 0; command < b->command_count; ++command) {
        const struct oldengl_xgl_command *d = b->commands+command;
        GLuint group = d->mode == GL_LINES ? 2 : d->mode == GL_TRIANGLES ? 3 : 1;
        for (i = 0; i < d->count; i += group) {
            GLuint j;
            Xgl_color flat = ((Xgl_pt_color_f3d *)(vertices+(d->first+i+group-1)*stride))->color;
            for (j = 0; j < group; ++j)
                ((Xgl_pt_color_f3d *)(vertices+(d->first+i+j)*stride))->color = flat;
        }
    }
    serial = oldengl_xgl_error_serial();
    XSync(a->display, False);
    if (!oldengl_xgl_batch_reset(a) || !oldengl_xgl_batch_target(a, back) ||
        !depth_mapping(a, c, b, &depth_bias)) goto done;
    if (back) {
        if (!xglmesa_pack_rgba(a->memory_rgb, back_rgba, a->width, a->height,
                a->red_shift, a->green_shift, a->blue_shift)) goto done;
        xgl_object_set(a->context, XGL_3D_CTX_HLHSR_MODE, XGL_HLHSR_NONE,
            XGL_CTX_RENDER_BUFFER, XGL_RENDER_DRAW_BUFFER, 0);
        xgl_object_set(a->memory, XGL_RAS_SOURCE_BUFFER, XGL_BUFFER_SEL_DRAW, 0);
        xgl_context_copy_buffer(a->context, NULL, NULL, a->memory);
        xgl_context_flush(a->context, XGL_FLUSH_SYNCHRONIZE);
    }
    if (!depth_upload(a, c) || !configure_view(a, c, depth_bias) ||
        !oldengl_xgl_lighting_configure(a->lighting, a->context, c) ||
        !oldengl_xgl_texture_configure(a->texture, a->context, c) ||
        oldengl_xgl_error_serial() != serial) goto done;
    xgl_object_set(a->context, XGL_CTX_RENDER_BUFFER, XGL_RENDER_DRAW_BUFFER,
        XGL_3D_CTX_LINE_COLOR_INTERP, (Xgl_boolean)(c->Light.ShadeModel == GL_SMOOTH), 0);
    for (command = 0; command < b->command_count; ++command) {
        const struct oldengl_xgl_command *d = b->commands+command;
        GLuint group = d->mode == GL_TRIANGLES ? 3 : d->mode == GL_LINES ? 2 : 1;
        memset(&list, 0, sizeof(list));
        list.pt_type = textured ? (lighting ? XGL_PT_COLOR_NORMAL_DATA_F3D : XGL_PT_COLOR_DATA_F3D) :
            lighting ? XGL_PT_COLOR_NORMAL_F3D : XGL_PT_COLOR_F3D;
        list.num_data_values = textured ? 2 : 0;
        for (i = 0; i < d->count; i += count) {
            count = d->mode == GL_POINTS || (d->mode == GL_TRIANGLES && !c->Polygon.CullFlag) ?
                d->count : group;
            if (d->mode == GL_TRIANGLES && !visible_triangle(c, b->vertices+d->first+i)) continue;
            list.num_pts = count;
            if (textured && lighting) list.pts.color_normal_data_f3d =
                (Xgl_pt_color_normal_data_f3d *)(vertices+(d->first+i)*stride);
            else if (textured) list.pts.color_data_f3d =
                (Xgl_pt_color_data_f3d *)(vertices+(d->first+i)*stride);
            else if (lighting) list.pts.color_normal_f3d =
                (Xgl_pt_color_normal_f3d *)(vertices+(d->first+i)*stride);
            else list.pts.color_f3d = (Xgl_pt_color_f3d *)(vertices+(d->first+i)*stride);
            if (d->mode == GL_POINTS) { xgl_multimarker(a->context, &list); a->stats.points += count; }
            else if (d->mode == GL_LINES) { xgl_multipolyline(a->context, NULL, 1, &list); ++a->stats.lines; }
            else { xgl_triangle_list(a->context, NULL, &list, XGL_TLIST_FLAG_TRI_RESTART); a->stats.triangles += count/3; }
        }
    }
    xgl_context_flush(a->context, XGL_FLUSH_SYNCHRONIZE);
    if (oldengl_xgl_error_serial() != serial) goto done;
    if (back) {
        GLuint x, y;
        xgl_object_set(a->raster, XGL_RAS_SOURCE_BUFFER, (Xgl_usgn32)1, 0);
        xgl_object_set(a->memory_context, XGL_CTX_RENDER_BUFFER, XGL_RENDER_DRAW_BUFFER,
            XGL_3D_CTX_HLHSR_MODE, XGL_HLHSR_NONE, 0);
        xgl_context_copy_buffer(a->memory_context, NULL, NULL, a->raster);
        xgl_context_flush(a->memory_context, XGL_FLUSH_SYNCHRONIZE);
        if (oldengl_xgl_error_serial() != serial) goto done;
        for (y = 0; y < (GLuint)a->height; ++y) for (x = 0; x < (GLuint)a->width; ++x) {
            Xgl_usgn32 word = a->memory_rgb[(size_t)(a->height-y-1)*a->width+x];
            GLubyte *p = back_rgba+((size_t)y*a->width+x)*4;
            p[0] = (GLubyte)(word >> a->red_shift); p[1] = (GLubyte)(word >> a->green_shift);
            p[2] = (GLubyte)(word >> a->blue_shift); p[3] = 255;
        }
        ++a->stats.back_batches;
    }
    /* Commit the authoritative Mesa depth image only after every native
     * color readback succeeds, so failure can replay against original Z. */
    if (!depth_download(a, c)) goto done;
    ++a->stats.object_batches; a->stats.transformed_vertices += b->vertex_count;
    if (c->Light.Enabled) a->stats.lit_vertices += b->vertex_count;
    if (c->Depth.Test) ++a->stats.depth_batches;
    if (textured) a->stats.textured_vertices += b->vertex_count;
    success = GL_TRUE;
done:
    if (oldengl_xgl_error_serial() != serial) a->failed = GL_TRUE;
    if (!success) ++a->stats.fallbacks;
    free(vertices);
    return success;
}
