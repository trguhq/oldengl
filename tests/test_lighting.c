/* MIT licensed. Recorded native light state and GL/XGL compatibility checks.
 * Synthetic SDK values verify adapter behavior, not Sun hardware execution.
 */
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xgl_lighting.h"

struct oldengl_mock_xgl {
    Xgl_usgn32 count;
    int type, illumination;
    Xgl_color color;
    Xgl_pt_f3d direction;
    Xgl_pt_d3d position;
    double attenuation[2], angle, exponent;
};

static struct oldengl_mock_xgl lights[MAX_LIGHTS+1];
static int checks, failures, calls, fail_attribute;
static unsigned long error_serial;

static void check(int condition, const char *name)
{
    ++checks;
    if (!condition) { ++failures; fprintf(stderr, "FAIL: %s\n", name); }
}

static int near(double a, double b) { return fabs(a-b) < 0.000001; }
unsigned long oldengl_xgl_error_serial(void) { return error_serial; }

void xgl_object_set(Xgl_object object, ...)
{
    va_list args;
    int attribute;
    va_start(args, object);
    while ((attribute = va_arg(args, int)) != 0) {
        ++calls;
        if (attribute == fail_attribute) { ++error_serial; fail_attribute = 0; }
        switch (attribute) {
        case XGL_3D_CTX_LIGHT_NUM: object->count = va_arg(args, Xgl_usgn32); break;
        case XGL_3D_CTX_LIGHT_SWITCHES: (void)va_arg(args, Xgl_boolean *); break;
        case XGL_LIGHT_TYPE: object->type = va_arg(args, int); break;
        case XGL_LIGHT_COLOR: object->color = *va_arg(args, Xgl_color *); break;
        case XGL_LIGHT_DIRECTION: object->direction = *va_arg(args, Xgl_pt_f3d *); break;
        case XGL_LIGHT_POSITION: object->position = *va_arg(args, Xgl_pt_d3d *); break;
        case XGL_LIGHT_ATTENUATION_1: object->attenuation[0] = va_arg(args, double); break;
        case XGL_LIGHT_ATTENUATION_2: object->attenuation[1] = va_arg(args, double); break;
        case XGL_LIGHT_SPOT_ANGLE: object->angle = va_arg(args, double); break;
        case XGL_LIGHT_SPOT_EXPONENT: object->exponent = va_arg(args, double); break;
        case XGL_3D_CTX_SURF_FRONT_ILLUMINATION:
            object->illumination = va_arg(args, int); break;
        case XGL_3D_CTX_SURF_FRONT_AMBIENT:
        case XGL_3D_CTX_SURF_FRONT_DIFFUSE:
        case XGL_3D_CTX_SURF_FRONT_SPECULAR:
            (void)va_arg(args, double); break;
        case XGL_CTX_SURF_FRONT_COLOR: (void)va_arg(args, Xgl_color *); break;
        case XGL_CTX_SURF_FRONT_COLOR_SELECTOR:
        case XGL_3D_CTX_SURF_FRONT_LIGHT_COMPONENT:
        case XGL_3D_CTX_SURF_FACE_DISTINGUISH:
        case XGL_3D_CTX_SURF_LIGHTING_NORMAL_FLIP:
        case XGL_3D_CTX_SURF_NORMAL_FLIP:
            (void)va_arg(args, int); break;
        default: fprintf(stderr, "Unknown lighting attribute %d\n", attribute); abort();
        }
    }
    va_end(args);
}

void xgl_object_get(Xgl_object object, Xgl_attribute attribute, void *result)
{
    Xgl_light *output = (Xgl_light *)result;
    Xgl_usgn32 i;
    if (attribute != XGL_3D_CTX_LIGHTS || object->count > MAX_LIGHTS+1) abort();
    for (i = 0; i < object->count; ++i) output[i] = &lights[i];
}

int main(void)
{
    GLcontext *ctx = (GLcontext *)calloc(1, sizeof(*ctx));
    struct gl_light *light;
    struct gl_material *material;
    struct oldengl_mock_xgl native;
    OldenGLXGLLighting *state;
    int before;
    memset(&native, 0, sizeof(native));
    check(ctx != NULL, "allocate GL state");
    if (!ctx) return 1;
    check(!oldengl_xgl_lighting_create(NULL), "missing native system is rejected");
    check(!oldengl_xgl_lighting_eligible(NULL), "missing GL state is rejected");
    state = oldengl_xgl_lighting_create(&native);
    check(state != NULL, "create native light adapter");
    if (!state) { free(ctx); return 1; }
    ctx->Visual.rgbMode = GL_TRUE;
    ctx->Light.Enabled = GL_TRUE;
    ctx->Light.ShadeModel = GL_SMOOTH;
    ctx->Light.Model.ColorControl = GL_SINGLE_COLOR;
    material = &ctx->Light.Material[0];
    material->Ambient[0] = 0.2f; material->Ambient[1] = 0.3f; material->Ambient[2] = 0.4f;
    material->Diffuse[0] = 0.25f; material->Diffuse[1] = 0.5f; material->Diffuse[2] = 0.75f;
    material->Emission[0] = 0.1f; material->Emission[1] = 0.2f; material->Emission[2] = 0.3f;
    ctx->Light.Model.Ambient[0] = 0.4f;
    ctx->Light.Model.Ambient[1] = 0.5f;
    ctx->Light.Model.Ambient[2] = 0.6f;
    light = &ctx->Light.Light[0];
    light->Enabled = GL_TRUE;
    light->EyePosition[0] = 2; light->EyePosition[1] = 4;
    light->EyePosition[2] = 6; light->EyePosition[3] = 2;
    light->EyeDirection[2] = -2;
    light->Diffuse[0] = 0.8f; light->Diffuse[1] = 0.6f; light->Diffuse[2] = 0.4f;
    light->ConstantAttenuation = 2;
    light->LinearAttenuation = 0.25f;
    light->SpotCutoff = 45; light->SpotExponent = 8;
    check(oldengl_xgl_lighting_eligible(ctx), "compatible positional spotlight is native eligible");
    check(oldengl_xgl_lighting_configure(state, &native, ctx), "configure native spotlight");
    check(native.count == 2, "spotlight keeps separate global/emission ambient light");
    check(lights[1].type == XGL_LIGHT_SPOT, "native light is a spot source");
    check(near(lights[1].position.x,1) && near(lights[1].position.y,2) &&
          near(lights[1].position.z,3), "homogeneous light position maps to eye-space position");
    check(lights[1].direction.x == 0 && lights[1].direction.y == 0 &&
          lights[1].direction.z == -2, "spot propagation uses transformed eye-space direction");
    check(near(lights[1].angle,1.5707963267948966), "45 degree GL cutoff gives 90 degree XGL spread in radians");
    check(near(lights[1].exponent,8), "spot concentration exponent is retained");
    check(near(lights[1].attenuation[0],2) && near(lights[1].attenuation[1],0.25),
          "spotlight retains constant and linear distance attenuation");
    check(near(lights[1].color.rgb.r,0.2) && near(lights[1].color.rgb.g,0.3) &&
          near(lights[1].color.rgb.b,0.3), "native light includes diffuse material products");
    check(near(lights[0].color.rgb.r,0.18) && near(lights[0].color.rgb.g,0.35) &&
          near(lights[0].color.rgb.b,0.54), "global ambient and emission remain outside the spot cone");
    check(native.illumination == XGL_ILLUM_PER_VERTEX, "spotlight enables native vertex illumination");
    light->Ambient[0] = 0.1f;
    check(!oldengl_xgl_lighting_eligible(ctx), "spot-attenuated ambient retains software lighting");
    material->Ambient[0] = 0;
    check(oldengl_xgl_lighting_eligible(ctx), "zero effective material ambient permits a spot light ambient value");
    material->Ambient[0] = 0.2f; light->Ambient[0] = 0;
    material->Specular[0] = 1; light->Specular[0] = 1;
    check(!oldengl_xgl_lighting_eligible(ctx), "incompatible GL specular equation retains software lighting");
    material->Specular[0] = 0; light->Specular[0] = 0;
    light->EyePosition[3] = 0;
    check(!oldengl_xgl_lighting_eligible(ctx), "directional GL spotlight is not mapped to positional XGL spot");
    light->EyePosition[3] = 2;
    light->SpotExponent = 0;
    check(!oldengl_xgl_lighting_eligible(ctx), "GL zero concentration stays outside documented XGL exponent range");
    light->SpotExponent = 0.5f;
    check(!oldengl_xgl_lighting_eligible(ctx), "fractional exponent below one retains software lighting");
    light->SpotExponent = 1;
    check(oldengl_xgl_lighting_eligible(ctx), "minimum XGL spot exponent is accepted");
    light->SpotExponent = 128;
    check(oldengl_xgl_lighting_eligible(ctx), "maximum GL spot exponent is accepted");
    light->SpotExponent = 129;
    check(!oldengl_xgl_lighting_eligible(ctx), "invalid spot exponent is rejected before native mutation");
    light->SpotExponent = 0; light->SpotCutoff = 180;
    check(oldengl_xgl_lighting_eligible(ctx), "disabled spot cone ignores its unused exponent");
    check(oldengl_xgl_lighting_configure(state, &native, ctx) &&
          lights[1].type == XGL_LIGHT_POSITIONAL, "disabling spot cone restores a positional light");
    light->SpotCutoff = 90; light->SpotExponent = 1;
    check(oldengl_xgl_lighting_configure(state, &native, ctx) &&
          near(lights[1].angle,3.141592653589793), "90 degree GL cutoff maps to a native hemisphere");
    light->SpotCutoff = 91;
    check(!oldengl_xgl_lighting_eligible(ctx), "invalid cone cutoff is rejected");
    light->SpotCutoff = -1;
    check(!oldengl_xgl_lighting_eligible(ctx), "negative cone cutoff is rejected");
    light->SpotCutoff = 45; light->EyeDirection[2] = 0;
    check(!oldengl_xgl_lighting_eligible(ctx), "zero spotlight direction is rejected");
    light->EyeDirection[2] = -2; light->QuadraticAttenuation = 0.5f;
    check(!oldengl_xgl_lighting_eligible(ctx), "unsupported quadratic attenuation retains software lighting");
    before = calls;
    check(!oldengl_xgl_lighting_configure(state, &native, ctx) && calls == before,
          "unsupported light state makes no native state changes");
    light->QuadraticAttenuation = 0; light->ConstantAttenuation = 0;
    check(!oldengl_xgl_lighting_eligible(ctx), "potential zero attenuation denominator remains a fallback");
    light->ConstantAttenuation = 2; light->LinearAttenuation = -1;
    check(!oldengl_xgl_lighting_eligible(ctx), "negative linear attenuation is rejected");
    light->LinearAttenuation = 0.25f; fail_attribute = XGL_LIGHT_SPOT_ANGLE;
    check(!oldengl_xgl_lighting_configure(state, &native, ctx), "native spotlight configuration errors are reported");
    check(oldengl_xgl_lighting_configure(state, &native, ctx), "spotlight setup can retry after an isolated error");
    ctx->Light.Enabled = GL_FALSE;
    check(oldengl_xgl_lighting_configure(state, &native, ctx) && native.count == 0 &&
          native.illumination == XGL_ILLUM_NONE_INTERP_COLOR,
          "disabling GL lighting clears native spot lights and vertex illumination");
    oldengl_xgl_lighting_destroy(state);
    free(ctx);
    printf("Native lighting: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
