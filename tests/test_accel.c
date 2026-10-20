/* MIT licensed. Native dispatch and mixed-buffer tests with a test XGL SDK. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <GL/osmesa.h>
#include "context.h"
#include "xgl_accel.h"
#include "mock_xgl.h"

static int checks, failures;
static void check(int value, const char *name)
{
    ++checks;
    if (!value) { ++failures; fprintf(stderr, "FAIL: %s\n", name); }
}

static void make_vertex(SWvertex *v, float x, float y, int r, int g, int b)
{
    memset(v, 0, sizeof(*v));
    v->win[0] = x; v->win[1] = y; v->win[3] = 1.0f;
    v->color[0] = (GLchan)r; v->color[1] = (GLchan)g;
    v->color[2] = (GLchan)b; v->color[3] = 255;
}

int main(void)
{
    OSMesaContext context;
    GLcontext *core;
    OldenGLXGLAccel *native;
    SWvertex a, b, c;
    GLubyte pixel[4];
    int count;
    struct oldengl_xgl_accel_stats stats;
    MockXGLReset();
    context = OSMesaCreateContextExt(OSMESA_RGBA, 24, 8, 16, NULL);
    check(context != NULL, "create software context");
    if (!context) return 1;
    check(OSMesaMakeCurrent(context, MockXGLFramebuffer(1), GL_UNSIGNED_BYTE, 8, 8),
          "bind software to the shared test surface");
    OSMesaPixelStore(OSMESA_Y_UP, 0);
    core = _mesa_get_current_context();
    /* A no-alpha front visual is what the real XMesa adapter accepts. */
    core->Visual.alphaBits = 0;
    native = oldengl_xgl_accel_create(MockXGLDisplay(), 1, 0, 8, 8);
    check(native != NULL, "create native XGL primitive context");
    if (!native) return 1;
    make_vertex(&a, 1.5f, 1.5f, 255, 0, 0);
    make_vertex(&b, 6.5f, 1.5f, 255, 0, 0);
    make_vertex(&c, 1.5f, 6.5f, 255, 0, 0);
    check(oldengl_xgl_accel_triangle(native, core, &a, &b, &c), "native triangle accepted");
    check(MockXGLTriangles() == 1, "triangle dispatched through XGL");
    glReadPixels(2, 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    check(pixel[0] == 255 && pixel[1] == 0 && pixel[2] == 0,
          "software readback sees native triangle");
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    count = MockXGLTriangles();
    check(!oldengl_xgl_accel_triangle(native, core, &a, &b, &c),
          "blended triangles fall back");
    check(MockXGLTriangles() == count, "fallback does not submit a native triangle");
    glColor4f(0.0f, 0.0f, 1.0f, 0.5f);
    glRectf(-1.0f, -1.0f, 1.0f, 1.0f);
    glFinish();
    glReadPixels(2, 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    check(abs((int)pixel[0]-127) <= 1 && abs((int)pixel[2]-128) <= 1,
          "software blending consumes native pixels in the shared surface");
    glDisable(GL_BLEND);
    check(oldengl_xgl_accel_triangle(native, core, &a, &b, &c),
          "native rendering resumes after software fallback");
    glReadPixels(2, 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    check(pixel[0] == 255 && pixel[2] == 0, "native result replaces software result");
    glEnable(GL_DEPTH_TEST);
    check(!oldengl_xgl_accel_triangle(native, core, &a, &b, &c), "depth testing falls back");
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_STENCIL_TEST);
    check(!oldengl_xgl_accel_point(native, core, &a), "stencil testing falls back");
    glDisable(GL_STENCIL_TEST);
    glEnable(GL_SCISSOR_TEST);
    check(!oldengl_xgl_accel_clear(native, core), "scissored clear falls back");
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_ALPHA_TEST);
    check(!oldengl_xgl_accel_triangle(native, core, &a, &b, &c), "alpha testing falls back");
    glDisable(GL_ALPHA_TEST);
    glColorMask(GL_FALSE, GL_TRUE, GL_TRUE, GL_TRUE);
    check(!oldengl_xgl_accel_triangle(native, core, &a, &b, &c), "masked color falls back");
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    check(oldengl_xgl_accel_point(native, core, &a), "native one-pixel point accepted");
    check(MockXGLMarkers() == 1, "point dispatched through XGL multimarker");
    check(oldengl_xgl_accel_line(native, core, &a, &b), "native unit-width line accepted");
    check(MockXGLLines() == 1, "line dispatched through XGL polyline");
    glLineWidth(2.0f);
    check(!oldengl_xgl_accel_line(native, core, &a, &b), "wide line falls back");
    glLineWidth(1.0f);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT_AND_BACK);
    count = MockXGLTriangles();
    check(oldengl_xgl_accel_triangle(native, core, &a, &b, &c) &&
          MockXGLTriangles() == count, "culled triangles are discarded before XGL submission");
    glDisable(GL_CULL_FACE);
    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    check(oldengl_xgl_accel_clear(native, core), "full native color clear accepted");
    check(MockXGLClears() == 1, "clear dispatched through XGL new frame");
    glReadPixels(2, 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    check(pixel[0] == 0 && pixel[1] == 255, "native clear visible to software readback");
    check(oldengl_xgl_accel_copy_pixels(native, core, 0, 0, 2, 2, 4, 4),
          "identity native raster copy accepted");
    glPixelZoom(2.0f, 1.0f);
    check(!oldengl_xgl_accel_copy_pixels(native, core, 0, 0, 2, 2, 4, 4),
          "scaled raster copy falls back");
    oldengl_xgl_accel_get_stats(native, &stats);
    check(stats.triangles == 2 && stats.points == 1 && stats.lines == 1 &&
          stats.clears == 1 && stats.copies == 1 && stats.fallbacks >= 4,
          "native operation counters describe actual submitted work");
    oldengl_xgl_accel_destroy(native);
    check(MockXGLLiveObjects() == 0 && MockXGLCloses() == 1,
          "native objects and shared session released");
    _mesa_make_current(NULL, NULL);
    OSMesaDestroyContext(context);
    printf("XGL native dispatch mock: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
