/* MIT licensed. Texture state, upload, invalidation, and coordinate tests.
 * This recording XGL fixture checks the API contract; it is not hardware.
 */
#include <stdarg.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xgl_texture.h"

struct oldengl_mock_xgl {
    int type;
    GLuint width, height, levels;
    Xgl_usgn32 *pixels;
    size_t count;
    Xgl_object texture;
    Xgl_texture_general_desc descriptor;
};

static int checks, failures, live, uploads;
static unsigned long error_serial;
static int fail_descriptor;

static void check(int condition, const char *name)
{
    ++checks;
    if (!condition) { ++failures; fprintf(stderr, "FAIL: %s\n", name); }
}

unsigned long oldengl_xgl_error_serial(void) { return error_serial; }

static Xgl_object object_new(int type)
{
    Xgl_object object = (Xgl_object)calloc(1, sizeof(*object));
    if (object) { object->type = type; ++live; }
    return object;
}

static void attributes(Xgl_object object, va_list args)
{
    int attribute;
    while ((attribute = va_arg(args, int)) != 0) {
        switch (attribute) {
        case XGL_DEV_COLOR_TYPE: case XGL_RAS_DEPTH:
        case XGL_TMAP_COORD_SOURCE: case XGL_TMAP_PARAM_TYPE:
        case XGL_3D_CTX_SURF_TMAP_PERSP_CORRECTION:
            (void)va_arg(args, int); break;
        case XGL_RAS_WIDTH: object->width = va_arg(args, GLuint); break;
        case XGL_RAS_HEIGHT: object->height = va_arg(args, GLuint); break;
        case XGL_TMAP_T0_INDEX: case XGL_TMAP_T1_INDEX:
        case XGL_3D_CTX_SURF_BACK_TMAP_NUM:
            (void)va_arg(args, Xgl_usgn32); break;
        case XGL_3D_CTX_SURF_FRONT_TMAP_NUM: {
            Xgl_usgn32 count = va_arg(args, Xgl_usgn32);
            if (!count && object->texture) {
                xgl_object_destroy(object->texture);
                object->texture = NULL;
            } else if (count && !object->texture) object->texture = object_new(-1);
            break;
        }
        case XGL_3D_CTX_SURF_FRONT_TMAP_SWITCHES:
            (void)va_arg(args, Xgl_boolean *); break;
        case XGL_TMAP_DESCRIPTOR: {
            Xgl_texture_general_desc *descriptor = va_arg(args, Xgl_texture_general_desc *);
            if (fail_descriptor) { fail_descriptor = 0; ++error_serial; }
            else object->descriptor = *descriptor;
            break;
        }
        case XGL_MIPMAP_TEXTURE_LEVELS:
            object->levels = va_arg(args, Xgl_usgn32); break;
        case XGL_MIPMAP_TEXTURE_MEM_RAS_LIST: {
            Xgl_mem_ras *rasters = va_arg(args, Xgl_mem_ras *);
            GLuint i;
            size_t offset = 0;
            object->width = rasters[0]->width;
            object->height = rasters[0]->height;
            free(object->pixels);
            object->count = 0;
            for (i = 0; i < object->levels; ++i)
                object->count += (size_t)rasters[i]->width * rasters[i]->height;
            object->pixels = (Xgl_usgn32 *)malloc(object->count * sizeof(Xgl_usgn32));
            for (i = 0; i < object->levels; ++i) {
                size_t count = (size_t)rasters[i]->width * rasters[i]->height;
                memcpy(object->pixels+offset, rasters[i]->pixels, count*sizeof(Xgl_usgn32));
                offset += count;
            }
            ++uploads;
            break;
        }
        default: fprintf(stderr, "unknown XGL attribute %d\n", attribute); abort();
        }
    }
}

Xgl_object xgl_object_create(Xgl_sys_state system, Xgl_obj_type type,
                             Xgl_obj_desc *description, ...)
{
    Xgl_object object = object_new(type);
    va_list args;
    (void)system; (void)description;
    va_start(args, description);
    attributes(object, args);
    va_end(args);
    if (type == XGL_MEM_RAS)
        object->pixels = (Xgl_usgn32 *)calloc((size_t)object->width*object->height,
                                             sizeof(Xgl_usgn32));
    return object;
}

void xgl_object_set(Xgl_object object, ...)
{
    va_list args;
    va_start(args, object);
    attributes(object, args);
    va_end(args);
}

void xgl_object_get(Xgl_object object, Xgl_attribute attribute, void *result)
{
    if (attribute == XGL_MEM_RAS_IMAGE_BUFFER_ADDR)
        *(Xgl_usgn32 **)result = object->pixels;
    else if (attribute == XGL_3D_CTX_SURF_FRONT_TMAP)
        *(Xgl_tmap *)result = object->texture;
    else abort();
}

void xgl_object_destroy(Xgl_object object)
{
    if (!object) return;
    if (object->texture) xgl_object_destroy(object->texture);
    free(object->pixels);
    free(object);
    --live;
}

static void fetch(const struct gl_texture_image *image, GLint x, GLint y,
                  GLint z, GLvoid *rgba)
{
    (void)z;
    memcpy(rgba, (const GLubyte *)image->Data + ((size_t)y*image->Width+x)*4, 4);
}

int main(void)
{
    GLcontext *ctx = (GLcontext *)calloc(1, sizeof(*ctx));
    struct gl_texture_object object;
    struct gl_texture_image image, last;
    Xgl_object native = object_new(XGL_3D_CTX), mipmap;
    OldenGLXGLTexture *state;
    GLfloat matrix[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    GLfloat model[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    GLfloat input[4] = {0.25f,0.75f,0,1}, position[4] = {0.5f,0.25f,0,1}, stq[3];
    GLubyte pixels[16] = {255,0,0,255, 0,255,0,128, 0,0,255,64, 255,255,255,0};
    GLubyte bottom[4] = {17,34,51,68};
    GLenum formats[6] = {GL_RGB,GL_RGBA,GL_LUMINANCE,GL_LUMINANCE_ALPHA,GL_INTENSITY,GL_ALPHA};
    GLenum filters[6] = {GL_NEAREST,GL_LINEAR,GL_NEAREST_MIPMAP_NEAREST,
        GL_LINEAR_MIPMAP_NEAREST,GL_NEAREST_MIPMAP_LINEAR,GL_LINEAR_MIPMAP_LINEAR};
    int i, before;
    memset(&object, 0, sizeof(object)); memset(&image, 0, sizeof(image));
    memset(&last, 0, sizeof(last));
    ctx->Visual.rgbMode = GL_TRUE;
    ctx->Texture._ReallyEnabled = TEXTURE0_2D;
    ctx->Texture.Unit[0]._Current = &object;
    ctx->Texture.Unit[0].EnvMode = GL_MODULATE;
    ctx->TextureMatrix[0].m = matrix; ctx->ModelView.m = model;
    object.Complete = GL_TRUE; object.Target = GL_TEXTURE_2D;
    object.MinFilter = object.MagFilter = GL_NEAREST;
    object.WrapS = object.WrapT = GL_REPEAT; object.MaxAnisotropy = 1;
    object.MinLod = -1000; object.MaxLod = 1000;
    object.Image[0] = &image; object.Image[1] = &last;
    image.Format = last.Format = GL_RGBA;
    image.Width = image.Height = 2; image.Depth = 1;
    image.Data = pixels; image.FetchTexel = fetch;
    last.Width = last.Height = last.Depth = 1;
    last.Data = bottom; last.FetchTexel = fetch;
    state = oldengl_xgl_texture_create(native);
    check(state != NULL, "create native texture helper");
    check(oldengl_xgl_texture_eligible(ctx), "RGBA nearest repeat accepted");
    check(oldengl_xgl_texture_coordinates(ctx,input,position,stq) &&
        stq[0]==0.25f && stq[1]==0.75f && stq[2]==1, "identity texture coordinates");
    matrix[0]=2; matrix[5]=3; matrix[12]=0.5f; matrix[13]=-0.25f;
    check(oldengl_xgl_texture_coordinates(ctx,input,position,stq) &&
        stq[0]==1 && stq[1]==2, "texture matrix scales and translates UV");
    matrix[0]=matrix[5]=1; matrix[12]=matrix[13]=0;
    ctx->Texture.Unit[0].TexGenEnabled = S_BIT;
    ctx->Texture.Unit[0].GenModeS = GL_OBJECT_LINEAR;
    ctx->Texture.Unit[0].ObjectPlaneS[0] = 2;
    check(oldengl_xgl_texture_eligible(ctx) &&
        oldengl_xgl_texture_coordinates(ctx,input,position,stq) && stq[0]==1,
        "object linear generation precedes texture transform");
    ctx->Texture.Unit[0].GenModeS = GL_EYE_LINEAR;
    ctx->Texture.Unit[0].EyePlaneS[0] = 1; model[12]=0.5f;
    check(oldengl_xgl_texture_coordinates(ctx,input,position,stq) && stq[0]==1,
        "eye linear generation uses modelview position");
    model[12]=0; ctx->Texture.Unit[0].GenModeS=GL_SPHERE_MAP;
    check(!oldengl_xgl_texture_eligible(ctx), "sphere generation falls back");
    check(oldengl_xgl_texture_rgb_eligible(ctx),
        "post-TNL sphere coordinates retain native RGB sampling");
    ctx->Texture.Unit[0].TexGenEnabled=0;
    matrix[15]=2;
    check(oldengl_xgl_texture_coordinates(ctx,input,position,stq) && stq[2]==2,
        "constant projective q preserved for batch check");
    matrix[15]=0;
    check(!oldengl_xgl_texture_coordinates(ctx,input,position,stq), "zero q falls back");
    matrix[15]=1;
    check(oldengl_xgl_texture_configure(state,native,ctx), "upload RGBA texture");
    mipmap=native->texture->descriptor.texture_info.mipmap.texture_map;
    check(mipmap->pixels[0]==0x40ff0000U && mipmap->pixels[2]==0xff0000ffU,
        "XGL texels are reversed rows of AABBGGRR words");
    before=uploads;
    check(oldengl_xgl_texture_configure(state,native,ctx) && uploads==before,
        "unchanged image reuses native mipmap");
    pixels[0]=127;
    check(oldengl_xgl_texture_configure(state,native,ctx) && uploads==before+1,
        "TexSubImage content changes invalidate native cache");
    ctx->Texture.Unit[0].EnvMode=GL_REPLACE;
    check(oldengl_xgl_texture_configure(state,native,ctx) &&
        native->texture->descriptor.comp_info.color_info.render_component_desc[0].texture_op==XGL_TEXTURE_OP_REPLACE,
        "replace environment uses native replacement");
    ctx->Texture.Unit[0].EnvMode=GL_DECAL;
    check(oldengl_xgl_texture_configure(state,native,ctx) &&
        native->texture->descriptor.comp_info.color_info.render_component_desc[0].texture_op==XGL_TEXTURE_OP_DECAL_INTRINSIC,
        "RGBA decal blends with incoming color");
    ctx->Texture.Unit[0].EnvMode=GL_BLEND; ctx->Texture.Unit[0].EnvColor[0]=0.75f;
    check(oldengl_xgl_texture_configure(state,native,ctx) &&
        native->texture->descriptor.comp_info.color_info.render_component_desc[0].texture_op==XGL_TEXTURE_OP_BLEND_INTRINSIC &&
        native->texture->descriptor.comp_info.color_info.render_component_desc[0].op.blend_int.rgb.c.r==0.75f,
        "blend environment retains incoming color and constant");
    ctx->Texture.Unit[0].EnvMode=GL_MODULATE;
    for (i=0;i<6;++i) {
        image.Format=formats[i];
        check(oldengl_xgl_texture_eligible(ctx), "GL base color format accepted");
    }
    image.Format=GL_RGBA;
    object._MaxLevel=1;
    for (i=0;i<6;++i) {
        object.MinFilter=filters[i];
        check(oldengl_xgl_texture_configure(state,native,ctx), "GL minification filter accepted");
    }
    mipmap=native->texture->descriptor.texture_info.mipmap.texture_map;
    check(mipmap->levels==2 && mipmap->count==5 && mipmap->pixels[4]==0x44332211U,
        "exact application mip levels uploaded including final1x1");
    object.MagFilter=GL_LINEAR; object.MinFilter=GL_NEAREST_MIPMAP_NEAREST;
    check(!oldengl_xgl_texture_eligible(ctx), "different GL minmag transition falls back");
    object.MinFilter=GL_LINEAR_MIPMAP_LINEAR;
    check(oldengl_xgl_texture_eligible(ctx), "trilinear minification and linear magnification accepted");
    object.MagFilter=GL_NEAREST;
    image.Height=1;
    check(!oldengl_xgl_texture_eligible(ctx), "rectangular mip tail falls back");
    object.MinFilter=GL_NEAREST;
    check(oldengl_xgl_texture_eligible(ctx), "rectangular nonmip image accepted");
    image.Height=2; object.WrapS=GL_CLAMP;
    check(!oldengl_xgl_texture_eligible(ctx), "GL clamp border semantics fall back");
    object.WrapS=GL_REPEAT; image.Border=1;
    check(!oldengl_xgl_texture_eligible(ctx), "explicit texture border falls back");
    image.Border=0; ctx->Visual.alphaBits=8;
    check(!oldengl_xgl_texture_eligible(ctx), "alpha framebuffer falls back");
    check(oldengl_xgl_texture_rgb_eligible(ctx),
        "captured RGB retains native sampling with software framebuffer alpha");
    ctx->Visual.alphaBits=0; ctx->Texture._ReallyEnabled|=TEXTURE1_2D;
    check(!oldengl_xgl_texture_eligible(ctx), "multiple texture units fall back");
    ctx->Texture._ReallyEnabled=TEXTURE0_2D;
    ctx->Texture._ReallyEnabled=TEXTURE0_1D;
    object.Target=GL_TEXTURE_1D; image.Height=1;
    object.WrapT=GL_CLAMP;
    check(oldengl_xgl_texture_eligible(ctx),
        "nonmipmapped 1D texture ignores unused T wrap state");
    check(oldengl_xgl_texture_coordinates(ctx,input,position,stq) &&
        stq[0]==0.25f && stq[1]==0.5f && stq[2]==1,
        "native 1D coordinates use the center of a one-row image");
    matrix[4]=2; matrix[7]=4;
    check(oldengl_xgl_texture_coordinates(ctx,input,position,stq) &&
        stq[0]==1.75f && stq[1]==2 && stq[2]==4,
        "1D retains T contributions to transformed S and projective Q");
    matrix[4]=matrix[7]=0;
    matrix[5]=FLT_MAX; input[1]=2;
    check(oldengl_xgl_texture_coordinates(ctx,input,position,stq) &&
        stq[0]==0.25f && stq[1]==0.5f && stq[2]==1,
        "unused transformed T range does not disable native 1D sampling");
    matrix[5]=1; input[1]=0.75f;
    check(oldengl_xgl_texture_configure(state,native,ctx), "upload 1D image to native texture");
    mipmap=native->texture->descriptor.texture_info.mipmap.texture_map;
    check(mipmap->width==2 && mipmap->height==1 && mipmap->levels==1 && mipmap->count==2,
        "native 1D image has one row and exactly the application texels");
    check(mipmap->pixels[0]==0xff00007fU && mipmap->pixels[1]==0x8000ff00U,
        "native 1D upload preserves left-to-right RGBA texels");
    check(native->texture->descriptor.texture_info.mipmap.v_boundary==
        XGL_TEXTURE_BOUNDARY_CLAMP_BOUNDARY,
        "unused native 1D T coordinate clamps to its only image row");
    object.MinFilter=object.MagFilter=GL_LINEAR;
    check(oldengl_xgl_texture_configure(state,native,ctx) &&
        native->texture->descriptor.texture_info.mipmap.interp_info.filter1==XGL_TEXTURE_INTERP_BILINEAR,
        "linear 1D sampling uses native filtering on the single row");
    for (i=2;i<6;++i) {
        object.MinFilter=filters[i];
        check(!oldengl_xgl_texture_eligible(ctx), "1D mipmapped filters retain software sampling");
    }
    object.MinFilter=object.MagFilter=GL_NEAREST;
    object.WrapS=GL_CLAMP;
    check(!oldengl_xgl_texture_eligible(ctx), "1D GL clamp border semantics retain software sampling");
    object.WrapS=GL_REPEAT; image.Height=2;
    check(!oldengl_xgl_texture_eligible(ctx), "native 1D upload requires exactly one image row");
    image.Height=1; ctx->Visual.alphaBits=8;
    check(!oldengl_xgl_texture_eligible(ctx) && oldengl_xgl_texture_rgb_eligible(ctx),
        "1D native RGB capture can retain software framebuffer alpha");
    ctx->Visual.alphaBits=0; ctx->Texture._ReallyEnabled=TEXTURE0_2D;
    check(!oldengl_xgl_texture_eligible(ctx), "texture enable and object dimensions must match");
    object.Target=GL_TEXTURE_2D; image.Height=2; object.WrapT=GL_REPEAT;
    pixels[0]=63; fail_descriptor=1;
    check(!oldengl_xgl_texture_configure(state,native,ctx), "descriptor failure reported before drawing");
    ctx->Texture._ReallyEnabled=0;
    check(oldengl_xgl_texture_configure(state,native,ctx) && !native->texture,
        "untextured batch clears native texture state");
    xgl_object_destroy(native);
    oldengl_xgl_texture_destroy(state);
    check(live==0, "all owned texture objects released after failure and reset");
    free(ctx);
    printf("%d texture checks, %d failures\n",checks,failures);
    return failures ? 1 : 0;
}
