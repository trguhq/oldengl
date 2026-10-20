/* MIT licensed. Fragment-stage completion for XGL raster captures.
 * Coverage and primary RGB come from XGL. This module reconstructs the
 * post-TNL attributes XGL cannot export and uses Mesa's existing span
 * pipeline, preserving GL fragment ordering and buffer ownership.
 */
#include "xgl_fragments.h"
#include "swrast/s_context.h"
#include "swrast/s_span.h"
#include "swrast/s_texture.h"
#include "swrast/s_trispan.h"
#include "macros.h"
#include "mmath.h"
#include <float.h>
#include <math.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

struct fragment_workspace {
    GLint depth_x[MAX_HEIGHT];
    GLfixed depth_z[MAX_HEIGHT], depth_step[MAX_HEIGHT];
    GLboolean depth_valid[MAX_HEIGHT];
    GLdepth depth[MAX_WIDTH];
    GLfloat fog[MAX_WIDTH];
    GLchan rgba[MAX_WIDTH][4];
    GLchan specular[MAX_WIDTH][4];
    GLchan primary[MAX_WIDTH][4];
    GLfloat s[MAX_TEXTURE_UNITS][MAX_WIDTH];
    GLfloat t[MAX_TEXTURE_UNITS][MAX_WIDTH];
    GLfloat r[MAX_TEXTURE_UNITS][MAX_WIDTH];
    GLfloat lambda[MAX_TEXTURE_UNITS][MAX_WIDTH];
};

static void record_triangle_depth(GLcontext *ctx, const SWvertex *v0,
    const SWvertex *v1, const SWvertex *v2, struct fragment_workspace *work)
{
/* Reuse only Mesa's fixed-point Z interpolator. The callback records
 * one attribute anchor/step per row; it never writes pixels or decides
 * XGL coverage, and suppresses the template's occlusion side effect. */
#define INTERP_Z 1
#define DO_OCCLUSION_TEST 1
#define SETUP_CODE span.y = 0;
#define RENDER_SPAN(SPAN) do { \
    if ((SPAN).y >= 0 && (SPAN).y < MAX_HEIGHT && (SPAN).count) { \
        work->depth_x[(SPAN).y] = (SPAN).x; \
        work->depth_z[(SPAN).y] = (SPAN).z; \
        work->depth_step[(SPAN).y] = (SPAN).zStep; \
        work->depth_valid[(SPAN).y] = GL_TRUE; \
    } \
} while (0)
#include "swrast/s_tritemp.h"
}

static int finite_value(double value)
{
    return value >= -FLT_MAX && value <= FLT_MAX;
}

static GLchan channel(double value)
{
    if (value <= 0.0) return 0;
    if (value >= CHAN_MAXF) return CHAN_MAX;
    return (GLchan)value;
}

static double attribute(const struct oldengl_xgl_fragment_setup *setup,
                          const double weight[3], GLuint component, int kind)
{
    double value = 0;
    GLuint i;
    if (kind == 0 && setup->ctx->Light.ShadeModel == GL_FLAT)
        return setup->vertex[setup->count-1].color[component];
    if (kind == 1 && setup->ctx->Light.ShadeModel == GL_FLAT)
        return setup->vertex[setup->count-1].specular[component];
    for (i = 0; i < setup->count; ++i) {
        const SWvertex *v = setup->vertex+i;
        if (kind == 0) value += weight[i]*v->color[component];
        else if (kind == 1) value += weight[i]*v->specular[component];
        else if (kind == 2) value += weight[i]*v->win[2];
        else value += weight[i]*v->fog;
    }
    return value;
}

static GLdepth depth_value(const struct oldengl_xgl_fragment_setup *setup,
                            const double weight[3], double x, double y)
{
    double value = setup->mode == GL_TRIANGLES ?
        setup->depth_plane[0]*x+setup->depth_plane[1]*y+setup->depth_plane[2] :
        attribute(setup, weight, 0, 2);
    if (setup->mode == GL_TRIANGLES) {
        const struct fragment_workspace *work = (const struct fragment_workspace *)setup->workspace;
        GLint row = (GLint)y;
        if (row >= 0 && row < MAX_HEIGHT && work->depth_valid[row]) {
            value = work->depth_z[row] +
                ((GLint)x-work->depth_x[row])*(double)work->depth_step[row];
            if (setup->ctx->Visual.depthBits <= 16) value = floor(value/2048.0);
        } else if (setup->ctx->Visual.depthBits <= 16) value += 0.5;
    } else if (setup->mode == GL_POINTS || setup->ctx->Visual.depthBits <= 16)
        value += 0.5;
    if (value <= 0.0) return 0;
    if (value >= setup->ctx->DepthMax) return (GLdepth)setup->ctx->DepthMax;
    return (GLdepth)value;
}

static void texture_value(const struct oldengl_xgl_fragment_setup *setup,
                           const double weight[3], GLuint unit,
                           GLfloat out[3], GLfloat *lambda)
{
    double value[4] = {0,0,0,0}, inverse;
    GLuint i, component;
    if (setup->mode == GL_POINTS) {
        const GLfloat *coordinate = setup->vertex[0].texcoord[unit];
        double divisor = coordinate[3] == 0.0f ? 1.0 : coordinate[3];
        for (component = 0; component < 3; ++component)
            out[component] = (GLfloat)(coordinate[component]/divisor);
        *lambda = 0;
        return;
    }
    for (i = 0; i < setup->count; ++i) {
        double factor = weight[i] * setup->vertex[i].win[3];
        for (component = 0; component < 4; ++component)
            value[component] += factor * setup->vertex[i].texcoord[unit][component];
    }
    /* Match Mesa's defined implementation behavior for a zero texture q. */
    inverse = value[3] == 0.0 ? 1.0 : 1.0/value[3];
    for (component = 0; component < 3; ++component)
        out[component] = (GLfloat)(value[component]*inverse);
    *lambda = 0;
    if (setup->mode == GL_TRIANGLES) {
        /* Computing rho/q^2 directly can overflow even when the final
         * logarithmic LOD is finite. Work in log space instead. */
        if (setup->rho[unit] > 0.0)
            *lambda = (GLfloat)((log(setup->rho[unit]) +
                2.0*log(fabs(inverse))) * 0.7213475204444817);
        else *lambda = -1000.0f;
    }
}

GLboolean oldengl_xgl_fragments_prepare(GLcontext *ctx, GLenum mode,
    const SWvertex *v0, const SWvertex *v1, const SWvertex *v2,
    struct oldengl_xgl_fragment_setup *setup)
{
    const SWvertex *input[3];
    GLuint i, j, unit, count;
    double denominator;
    if (!setup) return GL_FALSE;
    memset(setup, 0, sizeof(*setup));
    count = mode == GL_POINTS ? 1 : mode == GL_LINES ? 2 : mode == GL_TRIANGLES ? 3 : 0;
    if (!ctx || !count || !ctx->Visual.rgbMode || !ctx->DrawBuffer ||
        !SWRAST_CONTEXT(ctx) || CHAN_BITS != 8 ||
        ctx->RenderMode != GL_RENDER || ctx->Const.MaxTextureUnits > MAX_TEXTURE_UNITS)
        return GL_FALSE;
    input[0] = v0; input[1] = v1; input[2] = v2;
    for (i = 0; i < count; ++i) {
        if (!input[i]) return GL_FALSE;
        for (j = 0; j < 4; ++j)
            if (!finite_value(input[i]->win[j])) return GL_FALSE;
        if (!finite_value(input[i]->fog) ||
            fabs(input[i]->win[0]) > 100000000.0 ||
            fabs(input[i]->win[1]) > 100000000.0) return GL_FALSE;
        for (unit = 0; unit < ctx->Const.MaxTextureUnits; ++unit) {
            if (!ctx->Texture.Unit[unit]._ReallyEnabled) continue;
            for (j = 0; j < 4; ++j)
                if (!finite_value(input[i]->texcoord[unit][j])) return GL_FALSE;
        }
        setup->vertex[i] = *input[i];
    }
    setup->ctx = ctx; setup->mode = mode; setup->count = count;
    if (count == 1) setup->weight[0][2] = 1;
    else if (count == 2) {
        GLint x0 = (GLint)v0->win[0], y0 = (GLint)v0->win[1];
        GLint dx = (GLint)v1->win[0]-x0, dy = (GLint)v1->win[1]-y0;
        GLuint major = abs(dx) > abs(dy) ? 0 : 1;
        denominator = major ? dy : dx;
        if (denominator == 0.0) return GL_FALSE;
        setup->weight[1][major] = 1.0/denominator;
        setup->weight[1][2] = -((major ? y0 : x0)+0.5)/denominator;
        setup->weight[0][major] = -setup->weight[1][major];
        setup->weight[0][2] = 1.0-setup->weight[1][2];
    } else {
        double x0 = v0->win[0], y0 = v0->win[1];
        double x1 = v1->win[0], y1 = v1->win[1];
        double x2 = v2->win[0], y2 = v2->win[1];
        denominator = (y1-y2)*(x0-x2)+(x2-x1)*(y0-y2);
        if (!finite_value(denominator) || fabs(denominator) < 1.0e-20)
            return GL_FALSE;
        setup->weight[0][0] = (y1-y2)/denominator;
        setup->weight[0][1] = (x2-x1)/denominator;
        setup->weight[0][2] = (x1*y2-x2*y1)/denominator;
        setup->weight[1][0] = (y2-y0)/denominator;
        setup->weight[1][1] = (x0-x2)/denominator;
        setup->weight[1][2] = (x2*y0-x0*y2)/denominator;
        for (j = 0; j < 3; ++j)
            setup->weight[2][j] = (j == 2 ? 1.0 : 0.0)-setup->weight[0][j]-setup->weight[1][j];
    }
    if (count == 3) {
        double sx[3], sy[3], depth_weight[3][3];
        GLuint low = 0;
        /* Mesa snaps triangle XY to a 1/16-pixel grid before computing
         * depth gradients. Keep that plane separate from the continuous
         * XGL RGB/texture interpolation, or mixed rendering can disagree
         * by thousands of depth units for a perspective triangle. */
        for (i = 0; i < 3; ++i) {
            double fixed_x = (GLfloat)((setup->vertex[i].win[0]+0.5f)*2048.0f);
            double fixed_y = (GLfloat)((setup->vertex[i].win[1]-0.5f)*2048.0f);
            fixed_x = fixed_x < 0.0 ? ceil(fixed_x-0.5) : floor(fixed_x+0.5);
            fixed_y = fixed_y < 0.0 ? ceil(fixed_y-0.5) : floor(fixed_y+0.5);
            sx[i] = floor(fixed_x/128.0)*0.0625-0.5;
            sy[i] = floor(fixed_y/128.0)*0.0625+0.5;
            if (sy[i] < sy[low]) low = i;
        }
        denominator = (sy[1]-sy[2])*(sx[0]-sx[2])+(sx[2]-sx[1])*(sy[0]-sy[2]);
        if (denominator == 0.0) return GL_FALSE;
        depth_weight[0][0] = (sy[1]-sy[2])/denominator;
        depth_weight[0][1] = (sx[2]-sx[1])/denominator;
        depth_weight[0][2] = (sx[1]*sy[2]-sx[2]*sy[1])/denominator;
        depth_weight[1][0] = (sy[2]-sy[0])/denominator;
        depth_weight[1][1] = (sx[0]-sx[2])/denominator;
        depth_weight[1][2] = (sx[2]*sy[0]-sx[0]*sy[2])/denominator;
        for (j = 0; j < 3; ++j) {
            depth_weight[2][j] = (j == 2 ? 1.0 : 0.0)-depth_weight[0][j]-depth_weight[1][j];
            for (i = 0; i < 3; ++i)
                setup->depth_plane[j] += depth_weight[i][j]*setup->vertex[i].win[2];
        }
        if (fabs(setup->depth_plane[0]) > ctx->DepthMaxF) {
            setup->depth_plane[0] = setup->depth_plane[1] = 0;
            setup->depth_plane[2] = setup->vertex[low].win[2];
        }
    }
    for (unit = 0; unit < ctx->Const.MaxTextureUnits; ++unit) {
        const struct gl_texture_object *object = ctx->Texture.Unit[unit]._Current;
        const struct gl_texture_image *image;
        double ds[2] = {0,0}, dt[2] = {0,0};
        double first_q = 0.0;
        int zero_q = 0;
        if (!ctx->Texture.Unit[unit]._ReallyEnabled) continue;
        if (!object || object->BaseLevel < 0 || object->BaseLevel >= MAX_TEXTURE_LEVELS)
            return GL_FALSE;
        image = object->Image[object->BaseLevel];
        if (!image) return GL_FALSE;
        for (i = 0; i < count; ++i) {
            double factor = mode == GL_POINTS ? 1.0 : setup->vertex[i].win[3];
            double q = factor * setup->vertex[i].texcoord[unit][3];
            if (q == 0.0) ++zero_q;
            else if (first_q == 0.0) first_q = q;
            else if ((first_q < 0.0) != (q < 0.0)) return GL_FALSE;
            for (j = 0; j < 3; ++j) {
                double numerator = factor * setup->vertex[i].texcoord[unit][j];
                if (!finite_value(q == 0.0 ? numerator : numerator/q))
                    return GL_FALSE;
            }
        }
        /* A projective pole inside a primitive cannot safely be handed
         * to float-based sampling after partial target-buffer writes.
         * An all-zero q primitive follows Mesa's invQ=1 convention. */
        if (zero_q && zero_q != (int)count) return GL_FALSE;
        for (i = 0; i < count; ++i) for (j = 0; j < 2; ++j) {
            double factor = setup->weight[i][j]*setup->vertex[i].win[3];
            ds[j] += factor*setup->vertex[i].texcoord[unit][0];
            dt[j] += factor*setup->vertex[i].texcoord[unit][1];
        }
        setup->rho[unit] =
            (ds[0]*ds[0]+ds[1]*ds[1])*image->Width*image->Width+
            (object->Target == GL_TEXTURE_1D ? 0.0 :
             (dt[0]*dt[0]+dt[1]*dt[1])*image->Height*image->Height);
    }
    if (mode == GL_TRIANGLES && (ctx->Depth.Test ||
        (ctx->Fog.Enabled && SWRAST_CONTEXT(ctx)->_PreferPixelFog))) {
        double minimum = v0->win[1], maximum = minimum;
        /* The historical interpolator uses signed fixed-point XY. Reject
         * conversions outside that range before it can overflow, and bound
         * the row walk even for malformed or extreme viewport inputs. */
        for (i = 0; i < 3; ++i) {
            if (fabs(setup->vertex[i].win[0])+2.0 > (double)INT_MAX/2048.0 ||
                fabs(setup->vertex[i].win[1])+2.0 > (double)INT_MAX/2048.0)
                return GL_FALSE;
            if (setup->vertex[i].win[1] < minimum) minimum = setup->vertex[i].win[1];
            if (setup->vertex[i].win[1] > maximum) maximum = setup->vertex[i].win[1];
        }
        if (maximum-minimum > MAX_HEIGHT*8.0) return GL_FALSE;
    }
    setup->workspace = malloc(sizeof(struct fragment_workspace));
    if (setup->workspace) {
        struct fragment_workspace *work = (struct fragment_workspace *)setup->workspace;
        memset(work->depth_valid, 0, sizeof(work->depth_valid));
        if (mode == GL_TRIANGLES && (ctx->Depth.Test ||
            (ctx->Fog.Enabled && SWRAST_CONTEXT(ctx)->_PreferPixelFog)))
            record_triangle_depth(ctx, v0, v1, v2, work);
    }
    return setup->workspace ? GL_TRUE : GL_FALSE;
}

void oldengl_xgl_fragments_emit_span(struct oldengl_xgl_fragment_setup *setup,
    GLint x, GLint y, GLuint count, const GLubyte rgb[][3])
{
    struct fragment_workspace *work;
    GLcontext *ctx;
    GLuint offset = 0;
    GLenum primitive;
    if (!setup || !setup->workspace || !rgb || !count) return;
    work = (struct fragment_workspace *)setup->workspace;
    ctx = setup->ctx;
    primitive = setup->mode == GL_TRIANGLES ? GL_POLYGON :
        setup->mode == GL_LINES ? GL_LINE : GL_POINT;
    while (offset < count) {
        GLuint n = count-offset, i, j, unit;
        if (n > MAX_WIDTH) n = MAX_WIDTH;
        for (i = 0; i < n; ++i) {
            double weight[3];
            double px = x+offset+i+0.5, py = y+0.5;
            for (j = 0; j < 3; ++j)
                weight[j] = setup->weight[j][0]*px+setup->weight[j][1]*py+setup->weight[j][2];
            work->depth[i] = depth_value(setup, weight, px, py);
            {
                double fog = attribute(setup, weight, 0, 3);
                work->fog[i] = (GLfloat)(fog < 0.0 ? 0.0 : fog > 1.0 ? 1.0 : fog);
            }
            for (j = 0; j < 3; ++j) {
                work->rgba[i][j] = rgb[offset+i][j];
                work->specular[i][j] = channel(attribute(setup, weight, j, 1));
            }
            work->rgba[i][3] = channel(attribute(setup, weight, 3, 0));
            work->specular[i][3] = 0;
            for (unit = 0; unit < ctx->Const.MaxTextureUnits; ++unit) {
                GLfloat coordinate[3];
                if (!ctx->Texture.Unit[unit]._ReallyEnabled) continue;
                texture_value(setup, weight, unit, coordinate, &work->lambda[unit][i]);
                work->s[unit][i] = coordinate[0];
                work->t[unit][i] = coordinate[1];
                work->r[unit][i] = coordinate[2];
            }
        }
        if (setup->texture_rgb_complete &&
            (ctx->Texture._ReallyEnabled == TEXTURE0_1D ||
             ctx->Texture._ReallyEnabled == TEXTURE0_2D)) {
            /* XGL supplies textured RGB, but its raster does not export
             * alpha. Sample the existing software texture pipeline for
             * alpha and retain the native RGB; no GL state is modified. */
            for (i = 0; i < n; ++i) {
                double weight[3];
                double px = x+offset+i+0.5, py = y+0.5;
                for (j = 0; j < 3; ++j)
                    weight[j] = setup->weight[j][0]*px+setup->weight[j][1]*py+setup->weight[j][2];
                for (j = 0; j < 4; ++j)
                    work->primary[i][j] = channel(attribute(setup, weight, j, 0));
                memcpy(work->rgba[i], work->primary[i], sizeof(work->rgba[i]));
            }
            _swrast_texture_fragments(ctx, 0, n, work->s[0], work->t[0],
                work->r[0], work->lambda[0], (const GLchan (*)[4])work->primary,
                work->rgba);
            for (i = 0; i < n; ++i) for (j = 0; j < 3; ++j) {
                GLuint value = rgb[offset+i][j];
                if (ctx->Fog.ColorSumEnabled || (ctx->Light.Enabled &&
                    ctx->Light.Model.ColorControl == GL_SEPARATE_SPECULAR_COLOR))
                    value += work->specular[i][j];
                work->rgba[i][j] = value > CHAN_MAX ? CHAN_MAX : (GLchan)value;
            }
            _mesa_write_rgba_span(ctx, n, x+offset, y, work->depth,
                work->fog, work->rgba, NULL, primitive);
        } else if (ctx->Texture._ReallyEnabled & ~TEXTURE0_ANY) {
            _mesa_write_multitexture_span(ctx, n, x+offset, y, work->depth,
                work->fog, (const GLfloat (*)[MAX_WIDTH])work->s,
                (const GLfloat (*)[MAX_WIDTH])work->t,
                (const GLfloat (*)[MAX_WIDTH])work->r, work->lambda,
                work->rgba, (const GLchan (*)[4])work->specular, NULL, primitive);
        } else if (ctx->Texture._ReallyEnabled) {
            _mesa_write_texture_span(ctx, n, x+offset, y, work->depth, work->fog,
                work->s[0], work->t[0], work->r[0], work->lambda[0],
                work->rgba, (const GLchan (*)[4])work->specular, NULL, primitive);
        } else {
            /* Without textures, swrast's separate-specular wrapper has
             * already combined primary/secondary colors before this hook. */
            _mesa_write_rgba_span(ctx, n, x+offset, y, work->depth,
                work->fog, work->rgba, NULL, primitive);
        }
        offset += n;
    }
}

void oldengl_xgl_fragments_release(struct oldengl_xgl_fragment_setup *setup)
{
    if (!setup) return;
    free(setup->workspace);
    memset(setup, 0, sizeof(*setup));
}
