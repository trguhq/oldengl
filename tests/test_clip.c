/* MIT licensed. Native user-plane mapping and preflight regressions. */
#include "xgl_clip.h"
#include <float.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

struct oldengl_mock_xgl {
    Xgl_usgn32 count;
    Xgl_plane planes[MAX_CLIP_PLANES];
};

static int checks, failures, calls, list_calls, fail_attribute;
static unsigned long serial;

static void check(int condition, const char *name)
{
    ++checks;
    if (!condition) { ++failures; fprintf(stderr, "FAIL: %s\n", name); }
}

unsigned long oldengl_xgl_error_serial(void) { return serial; }

void xgl_object_set(Xgl_object object, ...)
{
    va_list args;
    int attribute;
    va_start(args, object);
    while ((attribute = va_arg(args, int)) != 0) {
        ++calls;
        if (attribute == XGL_3D_CTX_MODEL_CLIP_PLANE_NUM)
            object->count = va_arg(args, Xgl_usgn32);
        else if (attribute == XGL_3D_CTX_MODEL_CLIP_PLANES) {
            const Xgl_plane *planes = va_arg(args, Xgl_plane *);
            ++list_calls;
            if (!object->count || object->count > MAX_CLIP_PLANES) ++failures;
            else memcpy(object->planes, planes, object->count*sizeof(*planes));
        } else ++failures;
        if (attribute == fail_attribute) { fail_attribute = 0; ++serial; }
    }
    va_end(args);
}

static int near(double a, double b) { return fabs(a-b) < 1.0e-10; }

static double side(const Xgl_plane *plane, double x, double y, double z)
{
    return plane->normal.x*(x-plane->pt.x) +
        plane->normal.y*(y-plane->pt.y) + plane->normal.z*(z-plane->pt.z);
}

int main(void)
{
    GLcontext ctx;
    GLfloat matrix[16];
    struct oldengl_mock_xgl native;
    int before, i;
    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(matrix, 0, sizeof(matrix));
    matrix[0] = matrix[5] = matrix[10] = matrix[15] = 1.0f;
    ctx.ModelView.m = matrix;
    check(!oldengl_xgl_clip_eligible(NULL), "null context rejected");
    check(!oldengl_xgl_clip_configure(NULL, &ctx), "null native context rejected");
    check(oldengl_xgl_clip_eligible(&ctx), "disabled planes eligible");
    check(oldengl_xgl_clip_configure(&native, &ctx) && native.count == 0 &&
        list_calls == 0, "disabled planes only set zero count");

    ctx.Transform.ClipEnabled[0] = GL_TRUE;
    ctx.Transform.EyeUserPlane[0][0] = 2;
    ctx.Transform.EyeUserPlane[0][3] = -4;
    check(oldengl_xgl_clip_eligible(&ctx), "finite plane eligible");
    check(oldengl_xgl_clip_configure(&native, &ctx) && native.count == 1,
        "one enabled plane configured");
    check(near(native.planes[0].normal.x, 1) && near(native.planes[0].pt.x, 2),
        "nonunit equation maps unit normal and point");
    check(side(native.planes, 3, 0, 0) > 0 && side(native.planes, 1, 0, 0) < 0,
        "positive GL half-space retained");
    check(near(side(native.planes, 2, 10, -1), 0), "boundary lies on plane");

    ctx.Transform.ClipEnabled[3] = GL_TRUE;
    ctx.Transform.EyeUserPlane[3][1] = -3;
    ctx.Transform.EyeUserPlane[3][3] = 6;
    check(oldengl_xgl_clip_configure(&native, &ctx) && native.count == 2,
        "sparse enabled planes compacted in GL order");
    check(near(native.planes[1].normal.y, -1) && near(native.planes[1].pt.y, 2),
        "negative normal orientation preserved");
    check(side(native.planes+1, 0, 1, 0) > 0 && side(native.planes+1, 0, 3, 0) < 0,
        "negative-normal kept half-space preserved");

    ctx.Transform.EyeUserPlane[0][0] = 3;
    ctx.Transform.EyeUserPlane[0][1] = 4;
    ctx.Transform.EyeUserPlane[0][3] = -10;
    check(oldengl_xgl_clip_configure(&native, &ctx), "oblique plane configured");
    check(near(native.planes[0].normal.x, 0.6) && near(native.planes[0].normal.y, 0.8),
        "oblique normal normalized");
    check(near(side(native.planes, 2, 1, 0), 0), "oblique equation offset preserved");

    before = calls;
    memset(ctx.Transform.EyeUserPlane[0], 0, sizeof(ctx.Transform.EyeUserPlane[0]));
    check(!oldengl_xgl_clip_eligible(&ctx), "zero normal rejected");
    check(!oldengl_xgl_clip_configure(&native, &ctx) && calls == before,
        "degenerate plane rejected before native state changes");
    ctx.Transform.EyeUserPlane[0][3] = 1;
    check(!oldengl_xgl_clip_eligible(&ctx), "constant positive equation safely falls back");
    ctx.Transform.EyeUserPlane[0][0] = (GLfloat)HUGE_VAL;
    check(!oldengl_xgl_clip_eligible(&ctx), "nonfinite equation rejected");
    ctx.Transform.ClipEnabled[0] = GL_FALSE;
    check(oldengl_xgl_clip_eligible(&ctx), "disabled invalid equation ignored");
    matrix[3] = 0.5f;
    check(!oldengl_xgl_clip_eligible(&ctx), "projective model/view safely falls back");
    ctx.Transform.ClipEnabled[3] = GL_FALSE;
    check(oldengl_xgl_clip_eligible(&ctx), "projective matrix allowed without user clips");
    check(oldengl_xgl_clip_configure(&native, &ctx) && native.count == 0,
        "disabling all planes removes previous native planes");

    matrix[3] = 0;
    for (i = 0; i < MAX_CLIP_PLANES; ++i) {
        ctx.Transform.ClipEnabled[i] = GL_TRUE;
        memset(ctx.Transform.EyeUserPlane[i], 0, sizeof(ctx.Transform.EyeUserPlane[i]));
        ctx.Transform.EyeUserPlane[i][i%3] = 1;
        ctx.Transform.EyeUserPlane[i][3] = (GLfloat)i;
    }
    check(oldengl_xgl_clip_configure(&native, &ctx) && native.count == MAX_CLIP_PLANES,
        "all six GL user planes configured");
    check(near(native.planes[MAX_CLIP_PLANES-1].pt.z, -(MAX_CLIP_PLANES-1)),
        "last plane has correct offset");
    before = list_calls;
    fail_attribute = XGL_3D_CTX_MODEL_CLIP_PLANE_NUM;
    check(!oldengl_xgl_clip_configure(&native, &ctx) && list_calls == before,
        "count error stops before list upload");
    fail_attribute = XGL_3D_CTX_MODEL_CLIP_PLANES;
    check(!oldengl_xgl_clip_configure(&native, &ctx), "plane upload error propagated");

    memset(ctx.Transform.ClipEnabled, 0, sizeof(ctx.Transform.ClipEnabled));
    ctx.Transform.ClipEnabled[0] = GL_TRUE;
    ctx.Transform.EyeUserPlane[0][0] = FLT_MIN;
    ctx.Transform.EyeUserPlane[0][1] = ctx.Transform.EyeUserPlane[0][2] = 0;
    ctx.Transform.EyeUserPlane[0][3] = -FLT_MIN;
    check(oldengl_xgl_clip_configure(&native, &ctx) &&
        near(native.planes[0].normal.x, 1) && near(native.planes[0].pt.x, 1),
        "tiny finite coefficients normalize without underflow");
    ctx.Transform.EyeUserPlane[0][0] = FLT_MAX;
    ctx.Transform.EyeUserPlane[0][3] = -FLT_MAX;
    check(oldengl_xgl_clip_configure(&native, &ctx) &&
        near(native.planes[0].normal.x, 1) && near(native.planes[0].pt.x, 1),
        "large finite coefficients normalize without overflow");

    printf("Native user clip mapping: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
