/* MIT licensed. Native per-pixel XGL surface texture mapping.
 *
 * Reference: XGL 3.2 Programmer's Guide, chapter 17, and reference entries
 * XGL_TMAP_DESCRIPTOR, XGL_MIPMAP_TEXTURE_MEM_RAS_LIST, and data point types.
 * GL owns the authoritative images. Compare freshly fetched texels on every
 * batch so TexSubImage, shared objects, and display-list uploads cannot leave
 * a stale native texture. XGL receives the application's actual mip levels.
 */
#include "xgl_texture.h"
#include "xgl_session.h"
#include "colormac.h"
#include <float.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

struct oldengl_xgl_texture_retired {
    Xgl_object mipmap;
    struct oldengl_xgl_texture_retired *next;
};

struct oldengl_xgl_texture {
    Xgl_sys_state system;
    Xgl_object mipmap;
    Xgl_usgn32 *texels;
    size_t texel_count;
    GLuint width, height, levels;
    struct oldengl_xgl_texture_retired *retired;
};

static GLboolean finite_number(double value)
{
    return value >= -FLT_MAX && value <= FLT_MAX;
}

static GLboolean supported_format(GLenum format)
{
    return format == GL_ALPHA || format == GL_LUMINANCE ||
        format == GL_LUMINANCE_ALPHA || format == GL_INTENSITY ||
        format == GL_RGB || format == GL_RGBA;
}

static GLboolean power_of_two(GLuint value)
{
    return value && !(value & (value - 1));
}

static GLboolean mipmapped(GLenum filter)
{
    return filter != GL_NEAREST && filter != GL_LINEAR;
}

static GLboolean filter_method(GLenum filter, Xgl_texture_interp_method *method)
{
    switch (filter) {
    case GL_NEAREST: *method = XGL_TEXTURE_INTERP_POINT; break;
    case GL_LINEAR: *method = XGL_TEXTURE_INTERP_BILINEAR; break;
    case GL_NEAREST_MIPMAP_NEAREST: *method = XGL_TEXTURE_INTERP_MIPMAP_POINT; break;
    case GL_LINEAR_MIPMAP_NEAREST: *method = XGL_TEXTURE_INTERP_MIPMAP_BILINEAR; break;
    case GL_NEAREST_MIPMAP_LINEAR: *method = XGL_TEXTURE_INTERP_MIPMAP_PT_LINEAR; break;
    case GL_LINEAR_MIPMAP_LINEAR: *method = XGL_TEXTURE_INTERP_MIPMAP_TRILINEAR; break;
    default: return GL_FALSE;
    }
    return GL_TRUE;
}

static GLboolean boundary_method(GLenum wrap, Xgl_texture_boundary *method)
{
    if (wrap == GL_REPEAT) *method = XGL_TEXTURE_BOUNDARY_WRAP;
    else if (wrap == GL_CLAMP_TO_EDGE) *method = XGL_TEXTURE_BOUNDARY_CLAMP_BOUNDARY;
    /* XGL CLAMP selects a constant outside the image. GL_CLAMP first
     * clamps the coordinate and may blend edge and border texels. */
    else return GL_FALSE;
    return GL_TRUE;
}

static GLuint level_count(const struct gl_texture_object *object)
{
    if (!mipmapped(object->MinFilter)) return 1;
    return (GLuint)(object->_MaxLevel - object->BaseLevel + 1);
}

static GLboolean linear_generator(GLenum mode, const GLfloat plane[4])
{
    unsigned int i;
    if (mode != GL_OBJECT_LINEAR && mode != GL_EYE_LINEAR) return GL_FALSE;
    for (i = 0; i < 4; ++i) if (!finite_number(plane[i])) return GL_FALSE;
    return GL_TRUE;
}

GLboolean oldengl_xgl_texture_eligible(const GLcontext *ctx)
{
    const struct gl_texture_unit *unit;
    const struct gl_texture_object *object;
    const struct gl_texture_image *base, *image;
    Xgl_texture_interp_method filter;
    Xgl_texture_boundary boundary;
    GLuint i, levels, width, height;
    if (!ctx) return GL_FALSE;
    if (!ctx->Texture._ReallyEnabled) return GL_TRUE;
    /* This native path does not preserve framebuffer alpha, test alpha,
     * or blend alpha. Its caller separately rejects fragment operations. */
    if (!ctx->Visual.rgbMode || ctx->Visual.alphaBits || CHAN_BITS != 8 ||
        ctx->Texture._ReallyEnabled != TEXTURE0_2D) return GL_FALSE;
    unit = ctx->Texture.Unit;
    object = unit->_Current;
    if (!object || !object->Complete || object->Target != GL_TEXTURE_2D ||
        object->BaseLevel < 0 || object->BaseLevel >= MAX_TEXTURE_LEVELS ||
        object->CompareFlag || object->MaxAnisotropy != 1.0f ||
        unit->LodBias != 0.0f ||
        !filter_method(object->MinFilter, &filter) ||
        (object->MagFilter != GL_NEAREST && object->MagFilter != GL_LINEAR) ||
        !boundary_method(object->WrapS, &boundary) ||
        !boundary_method(object->WrapT, &boundary)) return GL_FALSE;
    /* GL's two nearest-mipmap modes with linear magnification switch at
     * lambda 0.5. XGL switches min/mag filters at a one-texel footprint. */
    if (object->MagFilter == GL_LINEAR &&
        (object->MinFilter == GL_NEAREST_MIPMAP_NEAREST ||
         object->MinFilter == GL_NEAREST_MIPMAP_LINEAR)) return GL_FALSE;
    base = object->Image[object->BaseLevel];
    if (!base || !supported_format(base->Format)) return GL_FALSE;
    if (unit->EnvMode != GL_MODULATE && unit->EnvMode != GL_REPLACE &&
        unit->EnvMode != GL_BLEND && unit->EnvMode != GL_DECAL) return GL_FALSE;
    /* GL 1.1 leaves DECAL on alpha/luminance/intensity images undefined.
     * Keep those combinations in the existing software implementation. */
    if (unit->EnvMode == GL_DECAL && base->Format != GL_RGB &&
        base->Format != GL_RGBA) return GL_FALSE;
    for (i = 0; i < 4; ++i)
        if (!finite_number(unit->EnvColor[i]) || unit->EnvColor[i] < 0.0f ||
            unit->EnvColor[i] > 1.0f) return GL_FALSE;
    if ((unit->TexGenEnabled & S_BIT) &&
        !linear_generator(unit->GenModeS, unit->GenModeS == GL_EYE_LINEAR ?
                          unit->EyePlaneS : unit->ObjectPlaneS)) return GL_FALSE;
    if ((unit->TexGenEnabled & T_BIT) &&
        !linear_generator(unit->GenModeT, unit->GenModeT == GL_EYE_LINEAR ?
                          unit->EyePlaneT : unit->ObjectPlaneT)) return GL_FALSE;
    if ((unit->TexGenEnabled & R_BIT) &&
        !linear_generator(unit->GenModeR, unit->GenModeR == GL_EYE_LINEAR ?
                          unit->EyePlaneR : unit->ObjectPlaneR)) return GL_FALSE;
    if ((unit->TexGenEnabled & Q_BIT) &&
        !linear_generator(unit->GenModeQ, unit->GenModeQ == GL_EYE_LINEAR ?
                          unit->EyePlaneQ : unit->ObjectPlaneQ)) return GL_FALSE;
    for (i = 0; i < 16; ++i)
        if (!finite_number(ctx->TextureMatrix[0].m[i])) return GL_FALSE;
    if (mipmapped(object->MinFilter)) {
        /* XGL documents pyramids halving both dimensions, ending when
         * the smaller dimension reaches one. Rectangular GL mip chains
         * have an additional one-dimensional tail, so use software. */
        if (base->Width != base->Height || object->_MaxLevel < object->BaseLevel ||
            object->_MaxLevel >= MAX_TEXTURE_LEVELS) return GL_FALSE;
    }
    levels = level_count(object);
    if (mipmapped(object->MinFilter) &&
        (!finite_number(object->MinLod) || !finite_number(object->MaxLod) ||
         object->MinLod > 0.0f || object->MaxLod < (GLfloat)(levels-1))) return GL_FALSE;
    width = base->Width;
    height = base->Height;
    if (!power_of_two(width) || !power_of_two(height) ||
        width > INT_MAX || height > INT_MAX) return GL_FALSE;
    for (i = 0; i < levels; ++i) {
        image = object->Image[object->BaseLevel + (GLint)i];
        if (!image || !image->Data || !image->FetchTexel || image->IsCompressed ||
            image->Border || image->Depth != 1 || image->Width != width ||
            image->Height != height || image->Format != base->Format) return GL_FALSE;
        if (width > 1) width /= 2;
        if (height > 1) height /= 2;
    }
    return GL_TRUE;
}

OldenGLXGLTexture *oldengl_xgl_texture_create(Xgl_sys_state system)
{
    OldenGLXGLTexture *state;
    if (!system) return NULL;
    state = (OldenGLXGLTexture *)calloc(1, sizeof(*state));
    if (state) state->system = system;
    return state;
}

void oldengl_xgl_texture_destroy(OldenGLXGLTexture *state)
{
    struct oldengl_xgl_texture_retired *retired, *next;
    if (!state) return;
    if (state->mipmap) xgl_object_destroy(state->mipmap);
    for (retired = state->retired; retired; retired = next) {
        next = retired->next;
        xgl_object_destroy(retired->mipmap);
        free(retired);
    }
    free(state->texels);
    free(state);
}

static Xgl_usgn32 *fetch_images(const struct gl_texture_object *object,
                               size_t *count)
{
    Xgl_usgn32 *pixels;
    const struct gl_texture_image *image;
    size_t total = 0, size, offset = 0;
    GLuint i, x, y, levels = level_count(object);
    GLchan rgba[4];
    for (i = 0; i < levels; ++i) {
        image = object->Image[object->BaseLevel + (GLint)i];
        if ((size_t)image->Width > ((size_t)-1) / image->Height) return NULL;
        size = (size_t)image->Width * image->Height;
        if (size > ((size_t)-1) - total) return NULL;
        total += size;
    }
    if (total > ((size_t)-1) / sizeof(*pixels)) return NULL;
    pixels = (Xgl_usgn32 *)malloc(total * sizeof(*pixels));
    if (!pixels) return NULL;
    for (i = 0; i < levels; ++i) {
        image = object->Image[object->BaseLevel + (GLint)i];
        for (y = 0; y < image->Height; ++y) {
            for (x = 0; x < image->Width; ++x) {
                image->FetchTexel(image, (GLint)x, (GLint)y, 0, rgba);
                /* GL stores its bottom row first; XGL memory raster DCs
                 * start at the top. Write words, as required on SPARC. */
                pixels[offset + (size_t)(image->Height-1-y)*image->Width+x] =
                    (Xgl_usgn32)CHAN_TO_UBYTE(rgba[0]) |
                    ((Xgl_usgn32)CHAN_TO_UBYTE(rgba[1]) << 8) |
                    ((Xgl_usgn32)CHAN_TO_UBYTE(rgba[2]) << 16) |
                    ((Xgl_usgn32)CHAN_TO_UBYTE(rgba[3]) << 24);
            }
        }
        offset += (size_t)image->Width * image->Height;
    }
    *count = total;
    return pixels;
}

static Xgl_object upload_images(OldenGLXGLTexture *state,
                                const struct gl_texture_object *object,
                                const Xgl_usgn32 *pixels)
{
    Xgl_object mipmap = 0;
    Xgl_mem_ras rasters[MAX_TEXTURE_LEVELS];
    Xgl_usgn32 *address;
    GLuint i, levels = level_count(object);
    size_t size, offset = 0;
    const struct gl_texture_image *image;
    unsigned long serial = oldengl_xgl_error_serial();
    memset(rasters, 0, sizeof(rasters));
    for (i = 0; i < levels; ++i) {
        image = object->Image[object->BaseLevel + (GLint)i];
        rasters[i] = xgl_object_create(state->system, XGL_MEM_RAS, NULL,
            XGL_DEV_COLOR_TYPE, XGL_COLOR_RGB, XGL_RAS_DEPTH, 32,
            XGL_RAS_WIDTH, image->Width, XGL_RAS_HEIGHT, image->Height, 0);
        if (!rasters[i] || oldengl_xgl_error_serial() != serial) goto done;
        address = NULL;
        xgl_object_get(rasters[i], XGL_MEM_RAS_IMAGE_BUFFER_ADDR, &address);
        if (!address || oldengl_xgl_error_serial() != serial) goto done;
        size = (size_t)image->Width * image->Height;
        memcpy(address, pixels + offset, size * sizeof(*pixels));
        offset += size;
    }
    mipmap = xgl_object_create(state->system, XGL_MIPMAP_TEXTURE, NULL, 0);
    if (mipmap) xgl_object_set(mipmap, XGL_MIPMAP_TEXTURE_LEVELS,
        (Xgl_usgn32)levels, XGL_MIPMAP_TEXTURE_MEM_RAS_LIST, rasters, 0);
done:
    for (i = 0; i < levels; ++i) if (rasters[i]) xgl_object_destroy(rasters[i]);
    if (oldengl_xgl_error_serial() != serial) {
        if (mipmap) xgl_object_destroy(mipmap);
        mipmap = 0;
    }
    return mipmap;
}

GLboolean oldengl_xgl_texture_configure(OldenGLXGLTexture *state,
                                       Xgl_ctx native, const GLcontext *ctx)
{
    const struct gl_texture_unit *unit;
    const struct gl_texture_object *object;
    const struct gl_texture_image *base;
    Xgl_texture_general_desc descriptor;
    Xgl_texture_general_mipmap_desc *mipmap_desc;
    Xgl_render_component_desc *component;
    Xgl_tmap texture = 0;
    Xgl_object mipmap, old_mipmap;
    Xgl_usgn32 *pixels;
    Xgl_boolean enabled = 1;
    struct oldengl_xgl_texture_retired *retired = NULL;
    size_t count;
    GLuint levels;
    GLboolean changed;
    unsigned long serial;
    if (!state || !native || !oldengl_xgl_texture_eligible(ctx)) return GL_FALSE;
    serial = oldengl_xgl_error_serial();
    if (!ctx->Texture._ReallyEnabled) {
        xgl_object_set(native, XGL_3D_CTX_SURF_FRONT_TMAP_NUM, (Xgl_usgn32)0,
            XGL_3D_CTX_SURF_BACK_TMAP_NUM, (Xgl_usgn32)0, 0);
        return oldengl_xgl_error_serial() == serial;
    }
    unit = ctx->Texture.Unit;
    object = unit->_Current;
    base = object->Image[object->BaseLevel];
    levels = level_count(object);
    pixels = fetch_images(object, &count);
    if (!pixels) return GL_FALSE;
    changed = !state->mipmap || state->width != base->Width ||
        state->height != base->Height || state->levels != levels ||
        state->texel_count != count ||
        memcmp(state->texels, pixels, count * sizeof(*pixels)) != 0;
    mipmap = changed ? upload_images(state, object, pixels) : state->mipmap;
    if (!mipmap) { free(pixels); return GL_FALSE; }
    if (changed && state->mipmap) {
        retired = (struct oldengl_xgl_texture_retired *)malloc(sizeof(*retired));
        if (!retired) {
            xgl_object_destroy(mipmap);
            free(pixels);
            return GL_FALSE;
        }
    }
    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.texture_type = XGL_TEXTURE_TYPE_MIPMAP;
    descriptor.comp_info.color_info.num_render_comp_desc = 1;
    descriptor.comp_info.color_info.num_channels[0] = 3;
    component = descriptor.comp_info.color_info.render_component_desc;
    component->comp = XGL_RENDER_COMP_FINAL_COLOR;
    if (base->Format == GL_ALPHA) component->texture_op = XGL_TEXTURE_OP_NONE;
    else if (unit->EnvMode == GL_MODULATE) component->texture_op = XGL_TEXTURE_OP_MODULATE;
    else if (unit->EnvMode == GL_REPLACE ||
             (unit->EnvMode == GL_DECAL && base->Format == GL_RGB))
        component->texture_op = XGL_TEXTURE_OP_REPLACE;
    else if (unit->EnvMode == GL_DECAL) component->texture_op = XGL_TEXTURE_OP_DECAL_INTRINSIC;
    else {
        component->texture_op = XGL_TEXTURE_OP_BLEND_INTRINSIC;
        component->op.blend_int.rgb.c.r = unit->EnvColor[0];
        component->op.blend_int.rgb.c.g = unit->EnvColor[1];
        component->op.blend_int.rgb.c.b = unit->EnvColor[2];
    }
    mipmap_desc = &descriptor.texture_info.mipmap;
    mipmap_desc->texture_map = mipmap;
    mipmap_desc->max_u_freq = mipmap_desc->max_v_freq = 1.0f;
    boundary_method(object->WrapS, &mipmap_desc->u_boundary);
    boundary_method(object->WrapT, &mipmap_desc->v_boundary);
    filter_method(object->MinFilter, &mipmap_desc->interp_info.filter1);
    filter_method(object->MagFilter, &mipmap_desc->interp_info.filter2);
    mipmap_desc->orientation_matrix[0][0] = 1.0f;
    mipmap_desc->orientation_matrix[1][1] = 1.0f;
    /* Context-owned TMAP handles may be destroyed when reset disables
     * texturing, so obtain the current handle each time. */
    xgl_object_set(native, XGL_3D_CTX_SURF_FRONT_TMAP_NUM, (Xgl_usgn32)1,
        XGL_3D_CTX_SURF_BACK_TMAP_NUM, (Xgl_usgn32)0, 0);
    xgl_object_get(native, XGL_3D_CTX_SURF_FRONT_TMAP, &texture);
    if (!texture || oldengl_xgl_error_serial() != serial) {
        if (changed) xgl_object_destroy(mipmap);
        free(retired);
        free(pixels);
        return GL_FALSE;
    }
    xgl_object_set(texture, XGL_TMAP_COORD_SOURCE, XGL_TEXTURE_COORD_DATA,
        XGL_TMAP_PARAM_TYPE, XGL_TEXTURE_PARAM_EXPLICIT,
        XGL_TMAP_T0_INDEX, (Xgl_usgn32)0, XGL_TMAP_T1_INDEX, (Xgl_usgn32)1,
        XGL_TMAP_DESCRIPTOR, &descriptor, 0);
    xgl_object_set(native, XGL_3D_CTX_SURF_FRONT_TMAP_SWITCHES, &enabled,
        XGL_3D_CTX_SURF_TMAP_PERSP_CORRECTION, XGL_TEXTURE_PERSP_PIXEL, 0);
    if (changed) {
        /* Keep the new mipmap alive even after an XGL error: the context
         * may already hold its descriptor. Destruction follows context
         * destruction, including on the failure path. */
        old_mipmap = state->mipmap;
        state->mipmap = mipmap;
        free(state->texels);
        state->texels = pixels;
        state->texel_count = count;
        state->width = base->Width;
        state->height = base->Height;
        state->levels = levels;
        if (old_mipmap) {
            if (oldengl_xgl_error_serial() == serial) {
                xgl_object_destroy(old_mipmap);
                free(retired);
            } else {
                /* An error may leave either descriptor installed. Hold
                 * both resources until their native context is gone. */
                retired->mipmap = old_mipmap;
                retired->next = state->retired;
                state->retired = retired;
            }
        }
    } else free(pixels);
    return oldengl_xgl_error_serial() == serial;
}

GLboolean oldengl_xgl_texture_coordinates(const GLcontext *ctx,
                                         const GLfloat texcoord[4],
                                         const GLfloat position[4],
                                         GLfloat stq[3])
{
    const struct gl_texture_unit *unit;
    const GLfloat *plane;
    GLenum mode;
    double generated[4], eye[4], transformed[4];
    unsigned int component, k;
    if (!ctx || !texcoord || !position || !stq) return GL_FALSE;
    unit = ctx->Texture.Unit;
    for (component = 0; component < 4; ++component) {
        generated[component] = texcoord[component];
        eye[component] = 0;
        for (k = 0; k < 4; ++k)
            eye[component] += ctx->ModelView.m[k*4+component] * position[k];
    }
    for (component = 0; component < 4; ++component) {
        if (!(unit->TexGenEnabled & (1U << component))) continue;
        if (component == 0) {
            mode = unit->GenModeS;
            plane = mode == GL_EYE_LINEAR ? unit->EyePlaneS : unit->ObjectPlaneS;
        } else if (component == 1) {
            mode = unit->GenModeT;
            plane = mode == GL_EYE_LINEAR ? unit->EyePlaneT : unit->ObjectPlaneT;
        } else if (component == 2) {
            mode = unit->GenModeR;
            plane = mode == GL_EYE_LINEAR ? unit->EyePlaneR : unit->ObjectPlaneR;
        } else {
            mode = unit->GenModeQ;
            plane = mode == GL_EYE_LINEAR ? unit->EyePlaneQ : unit->ObjectPlaneQ;
        }
        if (!linear_generator(mode, plane)) return GL_FALSE;
        generated[component] = 0;
        for (k = 0; k < 4; ++k)
            generated[component] += plane[k] *
                (mode == GL_EYE_LINEAR ? eye[k] : (double)position[k]);
    }
    for (component = 0; component < 4; ++component) {
        transformed[component] = 0;
        for (k = 0; k < 4; ++k)
            transformed[component] += ctx->TextureMatrix[0].m[k*4+component] * generated[k];
        if (!finite_number(transformed[component])) return GL_FALSE;
    }
    stq[0] = (GLfloat)transformed[0];
    stq[1] = (GLfloat)transformed[1];
    stq[2] = (GLfloat)transformed[3];
    return stq[2] != 0.0f;
}
