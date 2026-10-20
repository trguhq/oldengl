/* MIT licensed. XGL Reference pp. 86-88 defines model clip planes as a
 * world-coordinate point and unit normal pointing toward the kept half-space.
 * The native model transform is GL's model/view, so XGL world is GL eye space.
 */
#include "xgl_clip.h"
#include "xgl_session.h"
#include <float.h>
#include <math.h>

static GLboolean build_planes(const GLcontext *ctx,
    Xgl_plane planes[MAX_CLIP_PLANES], Xgl_usgn32 *count)
{
    GLuint i, j;
    if (!ctx) return GL_FALSE;
    *count = 0;
    for (i = 0; i < MAX_CLIP_PLANES; ++i) {
        const GLfloat *equation;
        double length_squared, inverse_length, position_scale;
        Xgl_plane *plane;
        if (!ctx->Transform.ClipEnabled[i]) continue;
        equation = ctx->Transform.EyeUserPlane[i];
        for (j = 0; j < 4; ++j)
            if (!(equation[j] >= -FLT_MAX && equation[j] <= FLT_MAX))
                return GL_FALSE;
        length_squared = (double)equation[0]*equation[0] +
            (double)equation[1]*equation[1] + (double)equation[2]*equation[2];
        /* A constant equation has no point/normal representation. Reject
         * before native work, allowing Mesa to handle either kept half-space. */
        if (!(length_squared > 0.0)) return GL_FALSE;
        inverse_length = 1.0/sqrt(length_squared);
        position_scale = -(double)equation[3]/length_squared;
        plane = planes + (*count)++;
        plane->normal.x = equation[0]*inverse_length;
        plane->normal.y = equation[1]*inverse_length;
        plane->normal.z = equation[2]*inverse_length;
        plane->pt.x = equation[0]*position_scale;
        plane->pt.y = equation[1]*position_scale;
        plane->pt.z = equation[2]*position_scale;
    }
    /* XGL specifies ordinary WC planes. Keep projective model/view
     * transforms on Mesa until their model-clipping semantics are verified. */
    return !*count || (!ctx->ModelView.m[3] && !ctx->ModelView.m[7] &&
        !ctx->ModelView.m[11] && ctx->ModelView.m[15] == 1.0f);
}

GLboolean oldengl_xgl_clip_eligible(const GLcontext *ctx)
{
    Xgl_plane planes[MAX_CLIP_PLANES];
    Xgl_usgn32 count;
    return build_planes(ctx, planes, &count);
}

GLboolean oldengl_xgl_clip_configure(Xgl_ctx native, const GLcontext *ctx)
{
    Xgl_plane planes[MAX_CLIP_PLANES];
    Xgl_usgn32 count;
    unsigned long serial;
    if (!native || !build_planes(ctx, planes, &count)) return GL_FALSE;
    serial = oldengl_xgl_error_serial();
    /* The count must be set before the plane list, including on growth.
     * Setting zero also prevents previous user clips leaking into later draws. */
    xgl_object_set(native, XGL_3D_CTX_MODEL_CLIP_PLANE_NUM, count, 0);
    if (oldengl_xgl_error_serial() != serial) return GL_FALSE;
    if (count) xgl_object_set(native, XGL_3D_CTX_MODEL_CLIP_PLANES, planes, 0);
    return oldengl_xgl_error_serial() == serial;
}
