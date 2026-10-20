/* MIT licensed. See LICENSE.
 * XGL's reflection-vector specular equation differs from OpenGL's halfway
 * vector equation. Only compatible ambient/diffuse cases reach this module.
 */
#include "xgl_lighting.h"
#include "xgl_session.h"
#include <float.h>
#include <stdlib.h>
#include <string.h>

struct oldengl_xgl_lighting {
    Xgl_sys_state system;
    Xgl_light lights[MAX_LIGHTS + 1];
    Xgl_boolean enabled[MAX_LIGHTS + 1];
};

static GLboolean unit_rgb(const GLfloat *color)
{
    unsigned int channel;
    for (channel = 0; channel < 3; ++channel)
        if (!(color[channel] >= 0.0f && color[channel] <= 1.0f))
            return GL_FALSE;
    return GL_TRUE;
}

static GLboolean finite_value(GLfloat value)
{
    return value >= -FLT_MAX && value <= FLT_MAX;
}

OldenGLXGLLighting *oldengl_xgl_lighting_create(Xgl_sys_state system)
{
    OldenGLXGLLighting *result;
    if (!system)
        return NULL;
    result = (OldenGLXGLLighting *)calloc(1, sizeof(*result));
    if (result)
        result->system = system;
    return result;
}

void oldengl_xgl_lighting_destroy(OldenGLXGLLighting *lighting)
{
    /* LIGHT_NUM-created lights belong to their native context. */
    free(lighting);
}

GLboolean oldengl_xgl_lighting_eligible(const GLcontext *ctx)
{
    const struct gl_material *material;
    const struct gl_light *light;
    unsigned int index, channel;
    if (!ctx || !ctx->Visual.rgbMode)
        return GL_FALSE;
    if (!ctx->Light.Enabled)
        return GL_TRUE;
    if (ctx->Light.ShadeModel != GL_SMOOTH || ctx->Light.Model.TwoSide ||
        ctx->Light.ColorMaterialEnabled ||
        ctx->Light.Model.ColorControl != GL_SINGLE_COLOR)
        return GL_FALSE;
    material = &ctx->Light.Material[0];
    if (!unit_rgb(material->Ambient) || !unit_rgb(material->Diffuse) ||
        !unit_rgb(material->Specular) || !unit_rgb(material->Emission) ||
        !unit_rgb(ctx->Light.Model.Ambient))
        return GL_FALSE;
    for (index = 0; index < MAX_LIGHTS; ++index) {
        light = &ctx->Light.Light[index];
        if (!light->Enabled)
            continue;
        if (!unit_rgb(light->Ambient) || !unit_rgb(light->Diffuse) ||
            !unit_rgb(light->Specular) || light->SpotCutoff != 180.0f ||
            light->QuadraticAttenuation != 0.0f)
            return GL_FALSE;
        for (channel = 0; channel < 3; ++channel) {
            if (material->Specular[channel] * light->Specular[channel] != 0.0f ||
                !finite_value(light->EyePosition[channel]))
                return GL_FALSE;
        }
        if (!finite_value(light->EyePosition[3]))
            return GL_FALSE;
        if (light->EyePosition[3] == 0.0f) {
            if (light->EyePosition[0] == 0.0f &&
                light->EyePosition[1] == 0.0f && light->EyePosition[2] == 0.0f)
                return GL_FALSE;
        } else {
            if (!(light->ConstantAttenuation > 0.0f) ||
                !finite_value(light->ConstantAttenuation) ||
                !(light->LinearAttenuation >= 0.0f) ||
                !finite_value(light->LinearAttenuation))
                return GL_FALSE;
            /* GL attenuates ambient light; XGL positional lights have no
             * ambient term. A spatially varying ambient term cannot be
             * represented by the separate uniform ambient light below.
             */
            if (light->LinearAttenuation != 0.0f)
                for (channel = 0; channel < 3; ++channel)
                    if (material->Ambient[channel] * light->Ambient[channel] != 0.0f)
                        return GL_FALSE;
        }
    }
    return GL_TRUE;
}

static void rgb(Xgl_color *color, double red, double green, double blue)
{
    memset(color, 0, sizeof(*color));
    color->rgb.r = (float)(red < 1.0 ? red : 1.0);
    color->rgb.g = (float)(green < 1.0 ? green : 1.0);
    color->rgb.b = (float)(blue < 1.0 ? blue : 1.0);
}

GLboolean oldengl_xgl_lighting_configure(OldenGLXGLLighting *lighting,
                                       Xgl_ctx native, const GLcontext *ctx)
{
    const struct gl_material *material;
    const struct gl_light *light;
    unsigned int index, channel, count, slot;
    unsigned long serial;
    double ambient[3];
    Xgl_color color, white;
    Xgl_pt_f3d direction;
    Xgl_pt_d3d position;
    if (!lighting || !native || !oldengl_xgl_lighting_eligible(ctx))
        return GL_FALSE;
    serial = oldengl_xgl_error_serial();
    if (!ctx->Light.Enabled) {
        xgl_object_set(native, XGL_3D_CTX_LIGHT_NUM, (Xgl_usgn32)0,
            XGL_CTX_SURF_FRONT_COLOR_SELECTOR, XGL_SURF_COLOR_VERTEX_ILLUM_DEP,
            XGL_3D_CTX_SURF_FRONT_ILLUMINATION, XGL_ILLUM_NONE_INTERP_COLOR, 0);
        return oldengl_xgl_error_serial() == serial ? GL_TRUE : GL_FALSE;
    }
    material = &ctx->Light.Material[0];
    count = 1; /* Ambient light also contains emission and global ambient. */
    for (channel = 0; channel < 3; ++channel)
        ambient[channel] = material->Emission[channel] +
            material->Ambient[channel] * ctx->Light.Model.Ambient[channel];
    for (index = 0; index < MAX_LIGHTS; ++index) {
        light = &ctx->Light.Light[index];
        if (!light->Enabled)
            continue;
        ++count;
        for (channel = 0; channel < 3; ++channel) {
            double attenuation = 1.0;
            if (light->EyePosition[3] != 0.0f)
                attenuation = 1.0 / light->ConstantAttenuation;
            ambient[channel] += material->Ambient[channel] *
                light->Ambient[channel] * attenuation;
        }
    }
    /* Setting LIGHT_NUM creates context-owned native lights; asking XGL
     * for their handles avoids ambiguous sharing/double-destruction rules.
     */
    memset(lighting->lights, 0, sizeof(lighting->lights));
    xgl_object_set(native, XGL_3D_CTX_LIGHT_NUM, (Xgl_usgn32)count, 0);
    xgl_object_get(native, XGL_3D_CTX_LIGHTS, lighting->lights);
    if (oldengl_xgl_error_serial() != serial)
        return GL_FALSE;
    for (slot = 0; slot < count; ++slot) {
        if (!lighting->lights[slot])
            return GL_FALSE;
        lighting->enabled[slot] = (Xgl_boolean)1;
    }
    rgb(&color, ambient[0], ambient[1], ambient[2]);
    xgl_object_set(lighting->lights[0], XGL_LIGHT_TYPE, XGL_LIGHT_AMBIENT,
                    XGL_LIGHT_COLOR, &color, 0);
    slot = 1;
    for (index = 0; index < MAX_LIGHTS; ++index) {
        light = &ctx->Light.Light[index];
        if (!light->Enabled)
            continue;
        rgb(&color, material->Diffuse[0] * light->Diffuse[0],
                    material->Diffuse[1] * light->Diffuse[1],
                    material->Diffuse[2] * light->Diffuse[2]);
        if (light->EyePosition[3] == 0.0f) {
            direction.x = -light->EyePosition[0];
            direction.y = -light->EyePosition[1];
            direction.z = -light->EyePosition[2];
            xgl_object_set(lighting->lights[slot], XGL_LIGHT_TYPE,
                XGL_LIGHT_DIRECTIONAL, XGL_LIGHT_COLOR, &color,
                XGL_LIGHT_DIRECTION, &direction, 0);
        } else {
            position.x = (double)light->EyePosition[0] / light->EyePosition[3];
            position.y = (double)light->EyePosition[1] / light->EyePosition[3];
            position.z = (double)light->EyePosition[2] / light->EyePosition[3];
            xgl_object_set(lighting->lights[slot], XGL_LIGHT_TYPE,
                XGL_LIGHT_POSITIONAL, XGL_LIGHT_COLOR, &color,
                XGL_LIGHT_POSITION, &position,
                XGL_LIGHT_ATTENUATION_1, (double)light->ConstantAttenuation,
                XGL_LIGHT_ATTENUATION_2, (double)light->LinearAttenuation, 0);
        }
        ++slot;
    }
    rgb(&white, 1.0, 1.0, 1.0);
    xgl_object_set(native, XGL_3D_CTX_LIGHT_SWITCHES, lighting->enabled,
        XGL_CTX_SURF_FRONT_COLOR_SELECTOR, XGL_SURF_COLOR_CONTEXT,
        XGL_CTX_SURF_FRONT_COLOR, &white,
        XGL_3D_CTX_SURF_FRONT_AMBIENT, 1.0,
        XGL_3D_CTX_SURF_FRONT_DIFFUSE, 1.0,
        XGL_3D_CTX_SURF_FRONT_SPECULAR, 0.0,
        XGL_3D_CTX_SURF_FRONT_LIGHT_COMPONENT,
            XGL_LIGHT_ENABLE_COMP_AMBIENT | XGL_LIGHT_ENABLE_COMP_DIFFUSE,
        XGL_3D_CTX_SURF_FRONT_ILLUMINATION, XGL_ILLUM_PER_VERTEX,
        XGL_3D_CTX_SURF_FACE_DISTINGUISH, (Xgl_boolean)0,
        XGL_3D_CTX_SURF_LIGHTING_NORMAL_FLIP, (Xgl_boolean)0,
        XGL_3D_CTX_SURF_NORMAL_FLIP, (Xgl_boolean)0, 0);
    return oldengl_xgl_error_serial() == serial ? GL_TRUE : GL_FALSE;
}
