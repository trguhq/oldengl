/* MIT licensed. Actual XMesa/GLX hooks, real Xlib, synthetic XGL runtime. */
#include "mock_native_xgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/gl.h>
#include <GL/glx.h>

/* putenv is available on Solaris 2.6; unsetenv is not required there. */
extern int putenv(char *);
extern char **environ;

static int checks, failures, protocol_errors;

static void check(int condition, const char *name)
{
    ++checks;
    if (!condition) { ++failures; fprintf(stderr, "FAIL: %s\n", name); }
}

static int xerror(Display *display, XErrorEvent *event)
{
    char text[128];
    XGetErrorText(display, event->error_code, text, sizeof(text));
    fprintf(stderr, "Unexpected X error: %s\n", text);
    ++protocol_errors;
    return 0;
}

static Window create_window(Display *display, XVisualInfo *visual,
                             Colormap *colormap, int x)
{
    XSetWindowAttributes attributes;
    Window result;
    *colormap = XCreateColormap(display, RootWindow(display, visual->screen),
                               visual->visual, AllocNone);
    attributes.colormap = *colormap;
    attributes.border_pixel = 0;
    result = XCreateWindow(display, RootWindow(display, visual->screen),
        x, 10, 64, 64, 0, visual->depth, InputOutput, visual->visual,
        CWColormap | CWBorderPixel, &attributes);
    XMapWindow(display, result);
    XSync(display, False);
    return result;
}

static void read_color(int x, int y, int r, int g, int b, const char *message)
{
    GLubyte pixel[4];
    glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    check(abs((int)pixel[0]-r) <= 2 && abs((int)pixel[1]-g) <= 2 &&
          abs((int)pixel[2]-b) <= 2, message);
    if (abs((int)pixel[0]-r) > 2 || abs((int)pixel[1]-g) > 2 ||
        abs((int)pixel[2]-b) > 2)
        fprintf(stderr, "  pixel (%d,%d)=(%u,%u,%u), expected (%d,%d,%d)\n",
            x, y, (unsigned int)pixel[0], (unsigned int)pixel[1],
            (unsigned int)pixel[2], r, g, b);
}

static void triangle(void)
{
    glBegin(GL_TRIANGLES);
    glVertex3f(-0.8f, -0.8f, 0.0f);
    glVertex3f(0.8f, -0.8f, 0.0f);
    glVertex3f(0.0f, 0.8f, 0.0f);
    glEnd();
}

static void triangle_z(GLfloat z)
{
    glBegin(GL_TRIANGLES);
    glVertex3f(-0.8f, -0.8f, z);
    glVertex3f(0.8f, -0.8f, z);
    glVertex3f(0.0f, 0.8f, z);
    glEnd();
}

static void remove_native_override(void)
{
    char **entry, **following;
    for (entry = environ; *entry; ) {
        if (strncmp(*entry, "OLDENGL_NO_XGL=", 15) == 0) {
            for (following = entry; *following; ++following)
                following[0] = following[1];
        } else ++entry;
    }
}

static void force_software(int enabled)
{
    static char disabled[] = "OLDENGL_NO_XGL=1";
    glFinish();
    if (enabled) {
        if (putenv(disabled) != 0) abort();
    } else remove_native_override();
}

static void test_default_routing(void)
{
    static char enabled[] = "OLDENGL_NO_XGL=0";
    static char disabled[] = "OLDENGL_NO_XGL=1";
    struct MockNativeXGLStats before, after;
    remove_native_override();
    check(getenv("OLDENGL_NO_XGL") == NULL, "native default requires no environment opt-in");
    MockNativeXGLGetStats(&before);
    glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
    glColor3f(1,0,0); triangle(); glFinish();
    MockNativeXGLGetStats(&after);
    check(after.triangles > before.triangles && after.clears > before.clears,
          "unset override dispatches eligible geometry and clear to XGL");
    read_color(32,28,255,0,0,"native default preserves ordinary GL readback");

    check(putenv(enabled) == 0, "set explicit native-enabled override");
    before = after;
    glColor3f(0,1,0); triangle(); glFinish();
    MockNativeXGLGetStats(&after);
    check(after.triangles > before.triangles, "zero override keeps native dispatch enabled");
    read_color(32,28,0,255,0,"explicit native-enabled draw reaches the window");

    check(putenv(disabled) == 0, "set explicit software override");
    before = after;
    glClearColor(0.25f,0.5f,0.75f,1); glClear(GL_COLOR_BUFFER_BIT);
    glColor3f(0,0,1); triangle(); glFinish();
    MockNativeXGLGetStats(&after);
    check(after.triangles == before.triangles && after.clears == before.clears,
          "software override bypasses eligible native geometry and clear");
    read_color(32,28,0,0,255,"software override preserves ordinary GL rendering");
    read_color(2,2,64,128,191,"software override also preserves color clear");

    remove_native_override();
    check(getenv("OLDENGL_NO_XGL") == NULL, "remove the software override");
    before = after;
    glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
    glColor3f(1,1,0); triangle(); glFinish();
    MockNativeXGLGetStats(&after);
    check(after.triangles > before.triangles && after.clears > before.clears,
          "native dispatch resumes immediately when the override is removed");
    read_color(32,28,255,255,0,"reenabled native draw replaces software output");
}

static void test_semantic_noops(void)
{
    struct MockNativeXGLStats before, after;
    glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
    glAlphaFunc(GL_ALWAYS,0.5f); glEnable(GL_ALPHA_TEST);
    glLogicOp(GL_COPY); glEnable(GL_COLOR_LOGIC_OP);
    glBlendFunc(GL_ONE,GL_ZERO); glEnable(GL_BLEND);
    glScissor(-8,-8,80,80); glEnable(GL_SCISSOR_TEST);
    MockNativeXGLGetStats(&before);
    glClearColor(0.125f,0.25f,0.5f,1); glClear(GL_COLOR_BUFFER_BIT); glFinish();
    MockNativeXGLGetStats(&after);
    check(after.clears > before.clears,
          "a full-window scissor keeps whole color clears on XGL");
    read_color(2,2,32,64,128,"native clear preserves color with a full-window scissor");
    before = after;
    glColor3f(0,1,1); triangle(); glFinish();
    MockNativeXGLGetStats(&after);
    check(after.triangles > before.triangles,
          "compatible no-op fragment states keep eligible geometry on XGL");
    read_color(32,28,0,255,255,"combined no-op states preserve native fragment color");

    before = after;
    glScissor(0,0,16,64);
    glColor3f(1,0,0); triangle(); glFinish();
    MockNativeXGLGetStats(&after);
    check(after.triangles > before.triangles,
          "XGL rasterization feeds software partial-scissor processing");
    read_color(32,28,0,255,255,"software scissor preserves pixels outside its rectangle");
    read_color(8,8,255,0,0,"software scissor writes pixels inside its rectangle");

    glDisable(GL_SCISSOR_TEST); glScissor(0,0,64,64);
    glDisable(GL_ALPHA_TEST); glAlphaFunc(GL_ALWAYS,0);
    glDisable(GL_COLOR_LOGIC_OP);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    before = after;
    glColor4f(0,0,1,0.5f); triangle(); glFinish();
    MockNativeXGLGetStats(&after);
    check(after.triangles > before.triangles,
          "XGL rasterization feeds software framebuffer blending");
    read_color(32,28,0,127,255,"software blending consumes the preceding native result");
    glDisable(GL_BLEND); glBlendFunc(GL_ONE,GL_ZERO);
}

static void test_depth(void)
{
    static const GLenum functions[8] = {GL_NEVER, GL_LESS, GL_EQUAL, GL_LEQUAL,
        GL_GREATER, GL_NOTEQUAL, GL_GEQUAL, GL_ALWAYS};
    static const GLfloat candidate[8] = {-0.5f,-0.5f,0,0,0.5f,0.5f,0,0.5f};
    static const int passes[8] = {0,1,1,1,1,1,1,1};
    struct MockNativeXGLStats before, after;
    GLfloat depth;
    int i;
    MockNativeXGLGetStats(&before);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    for (i = 0; i < 8; ++i) {
        glDepthFunc(GL_LESS); glClearDepth(1);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glColor3f(1, 0, 0); triangle_z(0); glFinish();
        glDepthFunc(functions[i]); glColor3f(0, 1, 0);
        triangle_z(candidate[i]); glFinish();
        read_color(32, 28, passes[i] ? 0 : 255, passes[i] ? 255 : 0, 0,
                   "native depth comparison preserves GL fragment result");
    }
    glDepthFunc(GL_LESS); glClearDepth(1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glColor3f(1, 0, 0); triangle_z(-0.4f); glFinish();
    glReadPixels(32, 28, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
    check(fabs(depth - 0.3) < 0.00001, "native depth results return to Mesa depth readback");
    glDepthMask(GL_FALSE); glDepthFunc(GL_ALWAYS);
    glColor3f(0, 0, 1); triangle_z(0.8f); glFinish();
    glDepthMask(GL_TRUE); glDepthFunc(GL_LESS);
    glColor3f(0, 1, 0); triangle_z(0.0f); glFinish();
    read_color(32, 28, 0, 0, 255, "native disabled depth writes preserve earlier Z values");
    glDepthMask(GL_TRUE); glDepthFunc(GL_LESS); glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    force_software(1);
    glColor3f(1,0,0); triangle_z(0.0f); glFinish();
    force_software(0);
    glDisable(GL_BLEND); glDepthFunc(GL_EQUAL);
    glColor3f(0,1,0); triangle_z(0.0f); glFinish();
    read_color(32,28,0,255,0,"native GL_EQUAL consumes software depth without quantization changes");
    glDepthFunc(GL_LESS); glColor3f(0,0,1); triangle_z(0.0f); glFinish();
    read_color(32,28,0,255,0,"native GL_LESS rejects geometry at matching software depth");
    glDepthFunc(GL_EQUAL); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    force_software(1);
    glColor3f(1,0,1); triangle_z(0.0f); glFinish();
    force_software(0);
    read_color(32,28,255,0,255,"software GL_EQUAL consumes native depth without quantization changes");
    glDisable(GL_BLEND);
    MockNativeXGLGetStats(&after);
    check(after.depth_tests > before.depth_tests,
          "depth-tested GL batches use native XGL depth comparison");
    check(after.uploads > before.uploads && after.downloads > before.downloads,
          "native depth batches exchange authoritative Mesa Z values");
    glDisable(GL_DEPTH_TEST);
}

static void test_lighting(void)
{
    GLfloat zero[] = {0,0,0,1}, white[] = {1,1,1,1};
    GLfloat red[] = {1,0,0,1}, green[] = {0,1,0,1};
    GLfloat quarter_red[] = {0.25f,0,0,1};
    GLfloat ambient_material[] = {0.5f,0.25f,0.1f,1};
    GLfloat emission[] = {0.1f,0.2f,0,1};
    GLfloat global_ambient[] = {0.2f,0.2f,0.2f,1};
    GLfloat light_ambient[] = {0.4f,0.4f,0.4f,1};
    GLfloat direction[] = {0,0,1,0}, position[] = {0,0,2,1};
    GLubyte native[4], software[4];
    struct MockNativeXGLStats before, after;
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, zero);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, zero);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, red);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, zero);
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, zero);
    glLightfv(GL_LIGHT0, GL_POSITION, direction);
    glLightfv(GL_LIGHT0, GL_AMBIENT, zero);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, white);
    glLightfv(GL_LIGHT0, GL_SPECULAR, zero);
    glEnable(GL_LIGHTING); glEnable(GL_LIGHT0); glEnable(GL_NORMALIZE);
    glNormal3f(0,0,1);
    glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
    MockNativeXGLGetStats(&before);
    triangle(); glFinish();
    read_color(32, 28, 255, 0, 0, "native directional diffuse lighting computes material color");
    /* Mesa may retain a pending per-vertex material flag on the first
     * immediate batch after glMaterial. Consume it before verifying the
     * fixed-material native route. */
    triangle(); glFinish();
    MockNativeXGLGetStats(&after);
    check(after.lights > before.lights && after.triangles > before.triangles,
          "lit GL triangles configure and consume native XGL light objects");
    /* Depth excludes the legacy raster-only callback, so this verifies
     * that a local viewer does not reject the native lighting stage when
     * no specular term can depend on the viewer position. */
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_ALWAYS);
    glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER,GL_TRUE);
    MockNativeXGLGetStats(&before);
    triangle(); glFinish();
    MockNativeXGLGetStats(&after);
    check(after.triangles > before.triangles && after.depth_tests > before.depth_tests,
          "local viewer keeps compatible ambient/diffuse lighting on XGL");
    read_color(32,28,255,0,0,"local viewer does not change native zero-specular lighting");
    glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER,GL_FALSE);
    glDisable(GL_DEPTH_TEST); glDepthFunc(GL_LESS);
    glNormal3f(0,0,-1); triangle(); glFinish();
    read_color(32, 28, 0, 0, 0, "native diffuse lighting respects transformed vertex normals");
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, quarter_red);
    glNormal3f(0,0,2); glDisable(GL_NORMALIZE);
    triangle(); glFinish();
    read_color(32, 28, 128, 0, 0,
               "unnormalized nonunit normals retain GL lighting through software fallback");
    glEnable(GL_NORMALIZE); triangle(); glFinish();
    read_color(32, 28, 64, 0, 0, "native GL_NORMALIZE lighting uses unit-length normals");
    glNormal3f(0,0,1);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, zero);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambient_material);
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, emission);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, global_ambient);
    glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
    triangle(); glFinish();
    read_color(32, 28, 102, 89, 15, "native ambient lighting includes global, light and material emission");
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, zero);
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, zero);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, zero);
    glLightfv(GL_LIGHT0, GL_AMBIENT, zero);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, green);
    glLightfv(GL_LIGHT0, GL_POSITION, position);
    glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, 2.0f);
    glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT); triangle(); glFinish();
    glReadPixels(32,28,1,1,GL_RGBA,GL_UNSIGNED_BYTE,native);
    check(native[1] > 100 && native[1] < 130 && native[0] == 0 && native[2] == 0,
          "native positional lighting applies constant attenuation");
    /* Explicitly bypass XGL for an independent Mesa reference. Blend state
     * now uses native coverage plus software fragment processing. */
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    force_software(1);
    glClear(GL_COLOR_BUFFER_BIT); triangle(); glFinish();
    glReadPixels(32,28,1,1,GL_RGBA,GL_UNSIGNED_BYTE,software);
    force_software(0);
    check(abs((int)native[0]-software[0]) <= 2 &&
          abs((int)native[1]-software[1]) <= 2 &&
          abs((int)native[2]-software[2]) <= 2,
          "native ambient/diffuse result agrees with software reference");
    glDisable(GL_BLEND);
    glDisable(GL_LIGHTING); glDisable(GL_LIGHT0); glDisable(GL_NORMALIZE);
}

static void textured_quad(GLfloat s0, GLfloat t0, GLfloat s1, GLfloat t1)
{
    glBegin(GL_QUADS);
    glTexCoord2f(s0,t0); glVertex3f(-0.8f,-0.8f,0);
    glTexCoord2f(s1,t0); glVertex3f(0.8f,-0.8f,0);
    glTexCoord2f(s1,t1); glVertex3f(0.8f,0.8f,0);
    glTexCoord2f(s0,t1); glVertex3f(-0.8f,0.8f,0);
    glEnd(); glFinish();
}

static void textured_perspective_triangle(void)
{
    glBegin(GL_TRIANGLES);
    glTexCoord2f(0,0); glVertex3f(-1.5f,-1.0f,-2);
    glTexCoord2f(1,0); glVertex3f(1.5f,-1.0f,-2);
    glTexCoord2f(0.5f,1); glVertex3f(0,2.0f,-5);
    glEnd(); glFinish();
}

static void test_texture(void)
{
    GLubyte image[] = {255,0,0,255, 0,255,0,255,
                       0,0,255,255, 255,255,255,255};
    GLubyte yellow[] = {255,255,0,255}, translucent_blue[] = {0,0,255,128};
    GLubyte green[] = {0,255,0,255};
    GLubyte native[4], software[4];
    GLfloat s_plane[] = {0,0,0,0.75f}, t_plane[] = {0,0,0,0.25f};
    GLuint texture;
    struct MockNativeXGLStats before, after;
    int i, equal;
    glGenTextures(1,&texture); glBindTexture(GL_TEXTURE_2D,texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,image);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
    glEnable(GL_TEXTURE_2D); glColor4f(1,1,1,1);
    MockNativeXGLGetStats(&before);
    textured_quad(0,0,1,1);
    read_color(16,16,255,0,0,"native texture keeps the GL bottom image row");
    read_color(48,16,0,255,0,"native texture reads adjacent texels");
    read_color(16,48,0,0,255,"native texture reverses top-down XGL raster storage");
    MockNativeXGLGetStats(&after);
    check(after.texture_maps > before.texture_maps && after.texture_samples > before.texture_samples &&
          after.triangles > before.triangles,"ordinary textured GL geometry configures and samples native XGL texture maps");
    textured_quad(1.25f,0.25f,1.25f,0.25f);
    read_color(32,28,255,0,0,"native texture repeat wraps normalized coordinates");
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    textured_quad(1.25f,0.25f,1.25f,0.25f);
    read_color(32,28,0,255,0,"native texture clamp-to-edge selects boundary texels");
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
    glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE); glColor3f(0.5f,1,1);
    textured_quad(0.25f,0.25f,0.25f,0.25f);
    read_color(32,28,128,0,0,"native texture modulation uses primary vertex color");
    glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
    MockNativeXGLGetStats(&before);
    glTexSubImage2D(GL_TEXTURE_2D,0,0,0,1,1,GL_RGBA,GL_UNSIGNED_BYTE,yellow);
    textured_quad(0.25f,0.25f,0.25f,0.25f);
    read_color(32,28,255,255,0,"native texture cache refreshes glTexSubImage2D changes");
    MockNativeXGLGetStats(&after);
    check(after.mipmaps > before.mipmaps,"changed GL texels upload a fresh native mipmap object");
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,translucent_blue);
    glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_DECAL); glColor3f(1,0,0);
    textured_quad(0,0,1,1);
    read_color(32,28,127,0,128,"native RGBA decal combines intrinsic texture alpha");
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,image);
    glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    textured_quad(0.5f,0.5f,0.5f,0.5f);
    read_color(32,28,128,128,128,"native bilinear texture filtering blends four texels");
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D,1,GL_RGBA,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,green);
    textured_quad(0,0,128,128);
    read_color(32,28,0,255,0,"native mipmap filtering samples the supplied smaller GL image");
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glMatrixMode(GL_TEXTURE); glLoadIdentity(); glTranslatef(0.5f,0,0);
    glMatrixMode(GL_MODELVIEW);
    textured_quad(0.25f,0.25f,0.25f,0.25f);
    read_color(32,28,0,255,0,"native texture coordinates include the GL texture matrix");
    glMatrixMode(GL_TEXTURE); glLoadIdentity(); glMatrixMode(GL_MODELVIEW);
    glTexGeni(GL_S,GL_TEXTURE_GEN_MODE,GL_OBJECT_LINEAR);
    glTexGeni(GL_T,GL_TEXTURE_GEN_MODE,GL_OBJECT_LINEAR);
    glTexGenfv(GL_S,GL_OBJECT_PLANE,s_plane); glTexGenfv(GL_T,GL_OBJECT_PLANE,t_plane);
    glEnable(GL_TEXTURE_GEN_S); glEnable(GL_TEXTURE_GEN_T);
    textured_quad(0,0,0,0);
    read_color(32,28,0,255,0,"native texture coordinates include object-linear texgen");
    glDisable(GL_TEXTURE_GEN_S); glDisable(GL_TEXTURE_GEN_T);
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glFrustum(-1,1,-1,1,1,10);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    MockNativeXGLGetStats(&before);
    textured_perspective_triangle();
    glReadPixels(32,35,1,1,GL_RGBA,GL_UNSIGNED_BYTE,native);
    MockNativeXGLGetStats(&after);
    check(after.texture_samples > before.texture_samples && native[0] == 0 &&
          native[1] == 255 && native[2] == 0,
          "native perspective texture interpolation selects a texel distinct from affine UVs");
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    force_software(1);
    textured_perspective_triangle();
    glReadPixels(32,35,1,1,GL_RGBA,GL_UNSIGNED_BYTE,software);
    force_software(0);
    equal = 1;
    for (i = 0; i < 3; ++i) if (abs((int)native[i]-software[i]) > 2) equal = 0;
    check(equal,"native perspective texture interpolation agrees with software reference");
    glDisable(GL_BLEND); glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP);
    MockNativeXGLGetStats(&before); textured_quad(0.25f,0.25f,0.25f,0.25f);
    MockNativeXGLGetStats(&after);
    check(after.texture_samples == before.texture_samples && after.triangles > before.triangles,
          "unsupported GL_CLAMP sampling keeps native rasterization and software texturing");
    read_color(32,28,255,0,0,"software texture fallback preserves expected texel color");
    glDisable(GL_TEXTURE_2D); glDeleteTextures(1,&texture);
}

/* Every scene has an independent Mesa reference. Samples are well inside
 * primitives so this suite checks fragment semantics, not XGL edge rules. */
struct hybrid_snapshot {
    GLubyte color[5][4], stencil;
    GLfloat depth;
};

static void hybrid_snapshot(struct hybrid_snapshot *snapshot)
{
    static const GLint positions[5][2] = {{32,28},{31,28},{8,8},{48,12},{32,35}};
    int i;
    for (i = 0; i < 5; ++i)
        glReadPixels(positions[i][0],positions[i][1],1,1,GL_RGBA,
                     GL_UNSIGNED_BYTE,snapshot->color[i]);
    glReadPixels(32,28,1,1,GL_DEPTH_COMPONENT,GL_FLOAT,&snapshot->depth);
    glReadPixels(32,28,1,1,GL_STENCIL_INDEX,GL_UNSIGNED_BYTE,&snapshot->stencil);
}

static void varying_depth_triangle(void)
{
    glBegin(GL_TRIANGLES);
    glVertex3f(-0.8f,-0.8f,-0.5f);
    glVertex3f(0.8f,-0.8f,0.5f);
    glVertex3f(0.0f,0.8f,0.0f);
    glEnd();
}

static void hybrid_scene(int scene)
{
    GLubyte stipple[128];
    GLubyte texels[16] = {255,0,0,255, 0,255,0,0,
                         0,0,255,255, 255,255,255,255};
    GLfloat fog_color[4] = {0,0,1,1};
    GLuint texture = 0;
    int i, was_forced;
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
    glMatrixMode(GL_TEXTURE); glPushMatrix(); glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glDisable(GL_LIGHTING); glDisable(GL_FOG); glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND); glDisable(GL_ALPHA_TEST); glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST); glDisable(GL_COLOR_LOGIC_OP);
    glDisable(GL_TEXTURE_1D); glDisable(GL_TEXTURE_2D);
    glDisable(GL_POLYGON_STIPPLE); glDisable(GL_CULL_FACE);
    glDisable(GL_POLYGON_SMOOTH); glDisable(GL_POINT_SMOOTH);
    glDisable(GL_LINE_SMOOTH); glDisable(GL_LINE_STIPPLE);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glShadeModel(GL_SMOOTH); glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    glDepthMask(GL_TRUE); glStencilMask(255);
    glClearColor(0,0,0,1); glClearDepth(1); glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
    switch (scene) {
    case 0:
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        /* Both primitives share one cassette. Compositing only a final
         * scratch image would lose the first triangle's contribution. */
        glBegin(GL_TRIANGLES);
        glColor4f(1,0,0,0.5f);
        glVertex3f(-0.8f,-0.8f,0); glVertex3f(0.8f,-0.8f,0); glVertex3f(0,0.8f,0);
        glColor4f(0,0,1,0.5f);
        glVertex3f(-0.8f,-0.8f,0); glVertex3f(0.8f,-0.8f,0); glVertex3f(0,0.8f,0);
        glEnd();
        break;
    case 1:
        glClearColor(1,1,1,1); glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(0,0,0,1); triangle();
        break;
    case 2:
        glEnable(GL_ALPHA_TEST); glAlphaFunc(GL_GREATER,0.5f);
        glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS);
        glEnable(GL_STENCIL_TEST); glStencilFunc(GL_ALWAYS,0,255);
        glStencilOp(GL_KEEP,GL_KEEP,GL_INCR);
        glColor4f(1,0,0,0.25f); triangle_z(-0.5f);
        glColor4f(0,1,0,1); triangle_z(0.0f);
        break;
    case 3:
        glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS);
        glEnable(GL_STENCIL_TEST); glStencilFunc(GL_NEVER,7,255);
        glStencilOp(GL_REPLACE,GL_KEEP,GL_KEEP);
        glColor3f(1,0,0); triangle_z(-0.5f);
        glStencilFunc(GL_EQUAL,7,255); glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP);
        glColor3f(0,1,0); triangle_z(0.0f);
        break;
    case 4:
        glEnable(GL_FOG); glFogi(GL_FOG_MODE,GL_LINEAR);
        glFogf(GL_FOG_START,0); glFogf(GL_FOG_END,1);
        glFogfv(GL_FOG_COLOR,fog_color);
        glColor3f(1,0,0); triangle_z(-0.5f);
        break;
    case 5:
        glClearColor(0.25f,0.5f,0.75f,1); glClear(GL_COLOR_BUFFER_BIT);
        glColorMask(GL_FALSE,GL_TRUE,GL_FALSE,GL_FALSE);
        glColor3f(1,1,1); triangle();
        break;
    case 6:
        glClearColor(0.25f,0.5f,0.75f,1); glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_COLOR_LOGIC_OP); glLogicOp(GL_XOR);
        glColor3f(1,1,1); triangle();
        break;
    case 7:
        for (i = 0; i < 128; ++i) stipple[i] = 0xaa;
        glPolygonStipple(stipple); glEnable(GL_POLYGON_STIPPLE);
        glColor3f(1,1,1); triangle();
        break;
    case 8:
        glGenTextures(1,&texture); glBindTexture(GL_TEXTURE_2D,texture);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,texels);
        glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
        glEnable(GL_TEXTURE_2D); glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_GREATER,0.5f); glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glTexCoord2f(0.75f,0.25f); glColor3f(1,1,1); triangle_z(-0.5f);
        glTexCoord2f(0.25f,0.25f); triangle_z(0.0f);
        break;
    case 9:
        glClearColor(0,0,0,0.25f); glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_BLEND); glBlendFunc(GL_DST_ALPHA,GL_ONE_MINUS_DST_ALPHA);
        glColor4f(1,0,0,1); triangle();
        break;
    case 10:
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_CULL_FACE); glCullFace(GL_BACK); glFrontFace(GL_CCW);
        glColor4f(0,1,0,1); triangle();
        glCullFace(GL_FRONT); glColor4f(1,0,0,1); triangle_z(-0.5f);
        break;
    case 11:
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS);
        glColor3f(1,0,0); triangle_z(0.0f);
        glPolygonOffset(0,-100); glEnable(GL_POLYGON_OFFSET_FILL);
        glColor3f(0,1,0); triangle_z(0.0f);
        break;
    case 12: case 13: case 14:
        if (scene == 12) texels[3] = 128;
        else if (scene == 13) { texels[7] = 128; texels[11] = 0; }
        else for (i = 3; i < 16; i += 4) texels[i] = 255;
        glGenTextures(1,&texture); glBindTexture(GL_TEXTURE_2D,texture);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,texels);
        glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,
                    scene == 12 ? GL_MODULATE : GL_REPLACE);
        glEnable(GL_TEXTURE_2D);
        if (scene == 12) {
            glClearColor(0,0,1,1); glClear(GL_COLOR_BUFFER_BIT);
            glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
            glColor4f(0.5f,1,1,0.5f); glTexCoord2f(0.25f,0.25f); triangle();
        } else if (scene == 13) {
            glMatrixMode(GL_PROJECTION); glLoadIdentity(); glFrustum(-1,1,-1,1,1,10);
            glMatrixMode(GL_MODELVIEW); glLoadIdentity();
            glEnable(GL_ALPHA_TEST); glAlphaFunc(GL_GREATER,0.25f);
            glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS); glColor4f(1,1,1,1);
            textured_perspective_triangle();
        } else {
            glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
            glColor4f(1,1,1,0.5f);
            glBegin(GL_TRIANGLES);
            glTexCoord4f(0,0,0,1); glVertex3f(-0.8f,-0.8f,0);
            glTexCoord4f(1,0,0,2); glVertex3f(0.8f,-0.8f,0);
            glTexCoord4f(0.5f,1,0,1.5f); glVertex3f(0,0.8f,0);
            glEnd();
        }
        break;
    case 15:
        glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS);
        was_forced = getenv("OLDENGL_NO_XGL") != NULL;
        force_software(1); glColor3f(1,0,0); varying_depth_triangle(); glFinish();
        force_software(was_forced);
        glDepthFunc(GL_EQUAL); glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        glColor3f(0,1,0); varying_depth_triangle();
        break;
    default: abort();
    }
    glFinish();
    if (texture) glDeleteTextures(1,&texture);
    glMatrixMode(GL_TEXTURE); glPopMatrix();
    glMatrixMode(GL_MODELVIEW); glPopMatrix();
    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glPopAttrib();
}

static void test_hybrid_effects(void)
{
    struct hybrid_snapshot native, software;
    struct MockNativeXGLStats before, after;
    static const char *names[16] = {
        "ordered transparent primitives", "black fragment coverage",
        "alpha before stencil/depth", "stencil before depth",
        "fragment fog", "channel masks", "color logic",
        "polygon stipple", "software texture alpha before depth",
        "destination alpha blending", "hybrid face culling", "hybrid polygon offset",
        "native RGB texture with software blending", "native perspective RGB with software alpha",
        "varying projective q with software texturing",
        "varying software depth followed by hybrid GL_EQUAL"
    };
    int scene, i, component, equal;
    for (scene = 0; scene < 16; ++scene) {
        force_software(0); MockNativeXGLGetStats(&before);
        hybrid_scene(scene); hybrid_snapshot(&native);
        MockNativeXGLGetStats(&after);
        check(after.triangles > before.triangles,names[scene]);
        if (scene == 12 || scene == 13)
            check(after.texture_samples > before.texture_samples,
                  "compatible RGB texture sampling remains native during software fragment operations");
        if (scene == 14)
            check(after.texture_samples == before.texture_samples,
                  "varying projective texture q uses software sampling with native coverage");
        force_software(1); hybrid_scene(scene); hybrid_snapshot(&software);
        force_software(0);
        equal = 1;
        for (i = 0; i < 5; ++i) for (component = 0; component < 4; ++component)
            if (abs((int)native.color[i][component]-software.color[i][component]) > 2)
                equal = 0;
        check(equal,"hybrid fragment colors agree with independent Mesa rendering");
        if (!equal) fprintf(stderr,"  hybrid scene: %s; center native=(%u,%u,%u,%u), Mesa=(%u,%u,%u,%u)\n",
            names[scene],native.color[0][0],native.color[0][1],native.color[0][2],native.color[0][3],
            software.color[0][0],software.color[0][1],software.color[0][2],software.color[0][3]);
        check(fabs(native.depth-software.depth) < 0.00001,
              "hybrid depth agrees with independent Mesa rendering");
        if (fabs(native.depth-software.depth) >= 0.00001)
            fprintf(stderr,"  hybrid scene: %s; depth native=%.9g Mesa=%.9g\n",
                    names[scene],native.depth,software.depth);
        check(native.stencil == software.stencil,
              "hybrid stencil agrees with independent Mesa rendering");
        if (scene == 0)
            check(abs((int)native.color[0][0]-64) <= 2 &&
                  abs((int)native.color[0][2]-128) <= 2,
                  "overlapping translucent primitives compose in submission order");
        if (scene == 1)
            check(native.color[0][0] == 0 && native.color[0][1] == 0 && native.color[0][2] == 0,
                  "black covered fragments remain distinguishable from uncovered scratch pixels");
        if (scene == 2)
            check(native.color[0][1] == 255 && native.stencil == 1 && fabs(native.depth-0.5f) < 0.00001,
                  "rejected alpha fragments do not update stencil or depth");
        if (scene == 3)
            check(native.color[0][1] == 255 && native.stencil == 7 && fabs(native.depth-0.5f) < 0.00001,
                  "stencil rejection leaves depth available to later geometry");
        if (scene == 15)
            check(native.color[0][0] == 0 && native.color[0][1] == 255,
                  "hybrid GL_EQUAL matches exact Mesa varying-depth interpolation");
        if (scene == 12)
            check(abs((int)native.color[0][0]-32) <= 2 &&
                  abs((int)native.color[0][2]-191) <= 2,
                  "software texture alpha blends native RGB without applying RGB modulation twice");
        if (scene == 13)
            check(native.color[4][0] == 0 && native.color[4][1] == 255 && native.color[4][2] == 0,
                  "hybrid texture sampling preserves perspective-correct native RGB");
        if (scene == 10 || scene == 11)
            check(native.color[0][1] == 255 && native.color[0][0] == 0,
                  "hybrid raster dispatch preserves culling and polygon-offset setup");
        if (scene == 8)
            check(native.color[0][0] == 255 && fabs(native.depth-0.5f) < 0.00001,
                  "software texture alpha is tested before committing depth");
    }
}

static void test_hybrid_points_lines(void)
{
    struct MockNativeXGLStats before, after;
    GLubyte native[4], software[4];
    int mode, i, equal, pass;
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glDisable(GL_LIGHTING); glDisable(GL_TEXTURE_2D); glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST); glDisable(GL_ALPHA_TEST); glDisable(GL_FOG);
    glDisable(GL_COLOR_LOGIC_OP); glDisable(GL_SCISSOR_TEST);
    glDisable(GL_POINT_SMOOTH); glDisable(GL_LINE_SMOOTH); glDisable(GL_LINE_STIPPLE);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    glPointSize(1); glLineWidth(1);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    for (mode = 0; mode < 2; ++mode) {
        for (pass = 0; pass < 2; ++pass) {
            force_software(pass);
            glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
            MockNativeXGLGetStats(&before);
            glColor4f(1,0,0,0.5f);
            glBegin(mode == 0 ? GL_POINTS : GL_LINES);
            if (mode == 0) glVertex2f(0.0078125f,0.0078125f);
            else {
                glVertex2f(-0.75f,0.0078125f);
                glVertex2f(0.75f,0.0078125f);
            }
            glEnd(); glFinish();
            if (!pass) {
                glReadPixels(32,32,1,1,GL_RGBA,GL_UNSIGNED_BYTE,native);
                MockNativeXGLGetStats(&after);
                check(mode == 0 ? after.points > before.points : after.lines > before.lines,
                      "blended unit points and lines retain XGL rasterization");
            } else glReadPixels(32,32,1,1,GL_RGBA,GL_UNSIGNED_BYTE,software);
        }
        equal = 1;
        for (i = 0; i < 4; ++i)
            if (abs((int)native[i]-software[i]) > 2) equal = 0;
        check(equal && native[0] > 100 && native[1] == 0 && native[2] == 0,
              "hybrid point/line alpha blending agrees with Mesa");
    }
    force_software(0); glPopAttrib();
}

static void test_hybrid_alpha_visual(Display *display)
{
    int attributes[] = {GLX_RGBA,GLX_RED_SIZE,8,GLX_GREEN_SIZE,8,GLX_BLUE_SIZE,8,
                        GLX_ALPHA_SIZE,8,GLX_DEPTH_SIZE,24,GLX_STENCIL_SIZE,8,None};
    XVisualInfo *visual;
    Window window;
    Colormap colormap;
    GLXContext context;
    GLint alpha_bits;
    struct MockNativeXGLStats before, after;
    struct hybrid_snapshot native, software;
    int i, equal, scene;
    visual = glXChooseVisual(display,DefaultScreen(display),attributes);
    check(visual != NULL,"choose RGB visual with retained software alpha");
    if (!visual) return;
    window = create_window(display,visual,&colormap,250);
    context = glXCreateContext(display,visual,NULL,True);
    check(context != NULL && glXMakeCurrent(display,window,context),
          "bind alpha-buffer GLX context for hybrid rendering");
    if (context) {
        glViewport(0,0,64,64); glDrawBuffer(GL_FRONT); glReadBuffer(GL_FRONT);
        glGetIntegerv(GL_ALPHA_BITS,&alpha_bits);
        check(alpha_bits >= 8,"hybrid test retains an actual alpha buffer");
        force_software(0); MockNativeXGLGetStats(&before);
        hybrid_scene(9); hybrid_snapshot(&native);
        MockNativeXGLGetStats(&after);
        check(after.triangles > before.triangles,
              "alpha-buffer visuals use XGL coverage with software alpha storage");
        force_software(1); hybrid_scene(9); hybrid_snapshot(&software); force_software(0);
        equal = 1;
        for (i = 0; i < 4; ++i)
            if (abs((int)native.color[0][i]-software.color[0][i]) > 2) equal = 0;
        check(equal && abs((int)native.color[0][0]-64) <= 2 &&
              abs((int)native.color[0][3]-112) <= 2,
              "destination alpha affects hybrid color and alpha exactly as Mesa");
        for (scene = 12; scene <= 13; ++scene) {
            MockNativeXGLGetStats(&before); hybrid_scene(scene); hybrid_snapshot(&native);
            MockNativeXGLGetStats(&after);
            check(after.texture_samples > before.texture_samples,
                  "alpha-buffer hybrid rendering retains native RGB texture sampling");
            force_software(1); hybrid_scene(scene); hybrid_snapshot(&software); force_software(0);
            equal = 1;
            for (i = 0; i < 4; ++i)
                if (abs((int)native.color[0][i]-software.color[0][i]) > 2) equal = 0;
            check(equal,"native RGB and software texture alpha agree with Mesa in alpha visuals");
        }
        check(glGetError() == GL_NO_ERROR,"hybrid alpha visual has no GL errors");
        glXMakeCurrent(display,None,NULL); glXDestroyContext(display,context);
    }
    XDestroyWindow(display,window); XFreeColormap(display,colormap); XFree(visual);
}

static void test_hybrid_destinations(void)
{
    GLubyte color[2][2][4], after_none[2][4], stencil[2], zfail_stencil[2];
    GLfloat depth[2];
    GLboolean visible[2][2];
    struct MockNativeXGLStats before, after;
    int pass, buffer, component, equal, occlusion;
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glDisable(GL_LIGHTING); glDisable(GL_FOG); glDisable(GL_TEXTURE_2D);
    glDisable(GL_ALPHA_TEST); glDisable(GL_COLOR_LOGIC_OP);
    glDisable(GL_SCISSOR_TEST); glDisable(GL_CULL_FACE);
    glDisable(GL_STENCIL_TEST); glDisable(GL_DEPTH_TEST);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE); glDepthMask(GL_TRUE); glStencilMask(255);
    occlusion = strstr((const char *)glGetString(GL_EXTENSIONS),"GL_HP_occlusion_test") != NULL;
    check(occlusion,"occlusion extension is available for hybrid query checks");
    for (pass = 0; pass < 2; ++pass) {
        force_software(pass); glDrawBuffer(GL_FRONT_AND_BACK); glReadBuffer(GL_FRONT);
        glDisable(GL_DEPTH_TEST); glDisable(GL_STENCIL_TEST); glDisable(GL_BLEND);
        glClearColor(0,0,1,1); glClearDepth(1); glClearStencil(0);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        MockNativeXGLGetStats(&before);
        glColor4f(1,0,0,0.5f); triangle(); glFinish();
        MockNativeXGLGetStats(&after);
        if (!pass) check(after.triangles > before.triangles,
                         "front-and-back blending keeps native rasterization");
        for (buffer = 0; buffer < 2; ++buffer) {
            glReadBuffer(buffer == 0 ? GL_FRONT : GL_BACK);
            glReadPixels(32,28,1,1,GL_RGBA,GL_UNSIGNED_BYTE,color[pass][buffer]);
        }
        glDrawBuffer(GL_NONE); glReadBuffer(GL_FRONT);
        glDisable(GL_BLEND); glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS);
        glEnable(GL_STENCIL_TEST); glStencilFunc(GL_ALWAYS,5,255);
        glStencilOp(GL_KEEP,GL_KEEP,GL_REPLACE);
        MockNativeXGLGetStats(&before);
        glColor3f(0,1,0); triangle_z(-0.5f); glFinish();
        MockNativeXGLGetStats(&after);
        if (!pass) check(after.triangles > before.triangles,
                         "GL_NONE depth/stencil work keeps native rasterization");
        glReadPixels(32,28,1,1,GL_RGBA,GL_UNSIGNED_BYTE,after_none[pass]);
        glReadPixels(32,28,1,1,GL_DEPTH_COMPONENT,GL_FLOAT,&depth[pass]);
        glReadPixels(32,28,1,1,GL_STENCIL_INDEX,GL_UNSIGNED_BYTE,&stencil[pass]);
        visible[pass][0] = visible[pass][1] = GL_FALSE;
        zfail_stencil[pass] = 0;
        if (occlusion) {
            glStencilOp(GL_KEEP,GL_INCR,GL_KEEP);
            glEnable(GL_OCCLUSION_TEST_HP);
            glGetBooleanv(GL_OCCLUSION_TEST_RESULT_HP,&visible[pass][0]);
            glDepthFunc(GL_GREATER); triangle_z(-0.75f); glFinish();
            glGetBooleanv(GL_OCCLUSION_TEST_RESULT_HP,&visible[pass][0]);
            glReadPixels(32,28,1,1,GL_STENCIL_INDEX,GL_UNSIGNED_BYTE,&zfail_stencil[pass]);
            glDepthFunc(GL_LESS); triangle_z(-0.75f); glFinish();
            glGetBooleanv(GL_OCCLUSION_TEST_RESULT_HP,&visible[pass][1]);
            glDisable(GL_OCCLUSION_TEST_HP);
        }
    }
    force_software(0);
    equal = 1;
    for (buffer = 0; buffer < 2; ++buffer) for (component = 0; component < 4; ++component)
        if (abs((int)color[0][buffer][component]-color[1][buffer][component]) > 2) equal = 0;
    check(equal && abs((int)color[0][0][0]-128) <= 2 &&
          abs((int)color[0][1][2]-127) <= 2,
          "hybrid blending updates both GL color buffers like Mesa");
    equal = 1;
    for (component = 0; component < 4; ++component)
        if (after_none[0][component] != color[0][0][component] ||
            after_none[1][component] != color[1][0][component]) equal = 0;
    check(equal,"GL_NONE leaves color untouched while processing fragments");
    check(fabs(depth[0]-depth[1]) < 0.00001 && fabs(depth[0]-0.25f) < 0.00001 &&
          stencil[0] == 5 && stencil[1] == 5,
          "GL_NONE hybrid rendering writes authoritative depth and stencil");
    if (occlusion) {
        check(zfail_stencil[0] == 6 && zfail_stencil[1] == 6,
              "depth-rejected hybrid spans preserve stencil Z-fail updates without reporting occlusion");
        check(!visible[0][0] && visible[0][1] &&
              visible[0][0] == visible[1][0] && visible[0][1] == visible[1][1],
              "hybrid occlusion queries distinguish depth-rejected and passing fragments");
        if (visible[0][0] || !visible[0][1] ||
            visible[0][0] != visible[1][0] || visible[0][1] != visible[1][1])
            fprintf(stderr,"  occlusion native=(%d,%d), Mesa=(%d,%d)\n",
                    visible[0][0],visible[0][1],visible[1][0],visible[1][1]);
    }
    glPopAttrib();
}

static void test_hybrid_capture_failure(Display *display)
{
    int attributes[] = {GLX_RGBA,GLX_RED_SIZE,8,GLX_GREEN_SIZE,8,GLX_BLUE_SIZE,8,
                        GLX_ALPHA_SIZE,8,GLX_DEPTH_SIZE,24,GLX_STENCIL_SIZE,8,None};
    XVisualInfo *visual;
    Window window;
    Colormap colormap;
    GLXContext context;
    struct hybrid_snapshot replay, software;
    struct MockNativeXGLStats before, after;
    int i, equal;
    visual = glXChooseVisual(display,DefaultScreen(display),attributes);
    check(visual != NULL,"choose temporary visual for capture failure replay");
    if (!visual) return;
    window = create_window(display,visual,&colormap,330);
    context = glXCreateContext(display,visual,NULL,True);
    check(context != NULL && glXMakeCurrent(display,window,context),
          "bind capture-failure context");
    if (context) {
        glViewport(0,0,64,64); glDrawBuffer(GL_FRONT); glReadBuffer(GL_FRONT);
        force_software(0); MockNativeXGLGetStats(&before);
        MockNativeXGLFailAfterColorReadback();
        hybrid_scene(2); hybrid_snapshot(&replay);
        MockNativeXGLGetStats(&after);
        check(after.injected_errors == before.injected_errors+1 && after.triangles > before.triangles,
              "fixture interrupts capture after native RGB but before coverage completion");
        force_software(1); hybrid_scene(2); hybrid_snapshot(&software); force_software(0);
        equal = 1;
        for (i = 0; i < 4; ++i)
            if (abs((int)replay.color[0][i]-software.color[0][i]) > 2) equal = 0;
        check(equal && fabs(replay.depth-software.depth) < 0.00001 &&
              replay.stencil == software.stencil,
              "failed capture replays against unmodified software color/depth/stencil");
        check(replay.color[0][1] == 255 && replay.stencil == 1 &&
              fabs(replay.depth-0.5f) < 0.00001,
              "failure replay retains alpha-before-stencil/depth ordering");
        check(glGetError() == GL_NO_ERROR,"native capture failure does not invent a GL error");
        glXMakeCurrent(display,None,NULL); glXDestroyContext(display,context);
    }
    XDestroyWindow(display,window); XFreeColormap(display,colormap); XFree(visual);
}

static void test_hybrid_split_buffers(Display *display, XVisualInfo *visual,
                                      Window draw, GLXContext context)
{
    Window read;
    Colormap colormap;
    struct MockNativeXGLStats before, after;
    read = create_window(display,visual,&colormap,410);
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glDisable(GL_LIGHTING); glDisable(GL_FOG); glDisable(GL_TEXTURE_2D);
    glDisable(GL_ALPHA_TEST); glDisable(GL_DEPTH_TEST); glDisable(GL_STENCIL_TEST);
    glDisable(GL_COLOR_LOGIC_OP); glDisable(GL_SCISSOR_TEST); glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND); glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    glDrawBuffer(GL_BACK); glClearColor(0,0,1,1); glClear(GL_COLOR_BUFFER_BIT);
    check(glXMakeCurrent(display,read,context),"initialize separate double-buffer read drawable");
    glDrawBuffer(GL_FRONT); glClearColor(0,1,1,1); glClear(GL_COLOR_BUFFER_BIT);
    glDrawBuffer(GL_BACK); glClearColor(1,1,0,1); glClear(GL_COLOR_BUFFER_BIT);
    check(glXMakeContextCurrent(display,draw,read,context),
          "bind separate draw and read buffers for hybrid blending");
    glDrawBuffer(GL_BACK); glReadBuffer(GL_BACK);
    read_color(32,28,255,255,0,"split read uses the read drawable's own back buffer");
    glReadBuffer(GL_FRONT);
    read_color(32,28,0,255,255,"split read uses the read drawable's front buffer");
    MockNativeXGLGetStats(&before);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1,0,0,0.5f); triangle(); glFinish();
    MockNativeXGLGetStats(&after);
    check(after.triangles > before.triangles,"split draw/read blending keeps native rasterization");
    read_color(32,28,0,255,255,"hybrid blend preserves the separate read drawable");
    check(glXMakeCurrent(display,draw,context),"restore combined draw/read after hybrid blend");
    glReadBuffer(GL_BACK);
    read_color(32,28,128,0,127,"split hybrid blend reads its draw back buffer as destination");
    glPopAttrib();
    XDestroyWindow(display,read); XFreeColormap(display,colormap);
}

int main(void)
{
    int attributes[] = {GLX_RGBA, GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8,
                        GLX_BLUE_SIZE, 8, GLX_DEPTH_SIZE, 24, GLX_STENCIL_SIZE, 8, None};
    int back_attributes[] = {GLX_RGBA, GLX_DOUBLEBUFFER, GLX_RED_SIZE, 8,
                             GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8,
                             GLX_DEPTH_SIZE, 24, GLX_STENCIL_SIZE, 8, None};
    Display *display;
    XVisualInfo *visual, *back_visual;
    Colormap maps[3];
    Window windows[3];
    GLXContext contexts[3];
    struct MockNativeXGLStats before, after;
    GLfloat vertices[] = {-0.8f,-0.8f,0, 0.8f,-0.8f,0, 0,0.8f,0};
    int i;
    MockNativeXGLReset();
    display = XOpenDisplay(NULL);
    if (!display) { fprintf(stderr, "Set DISPLAY to a real X server or Xvfb\n"); return 1; }
    XSetErrorHandler(xerror);
    visual = glXChooseVisual(display, DefaultScreen(display), attributes);
    check(visual != NULL, "choose a no-alpha RGB front visual");
    if (!visual) return 1;
    windows[0] = create_window(display, visual, maps, 10);
    windows[1] = create_window(display, visual, maps+1, 90);
    contexts[0] = glXCreateContext(display, visual, NULL, True);
    contexts[1] = glXCreateContext(display, visual, contexts[0], True);
    check(contexts[0] && contexts[1], "create native-eligible sharing contexts");
    if (!contexts[0] || !contexts[1]) return 1;
    check(glXMakeCurrent(display, windows[0], contexts[0]), "bind native context");
    glViewport(0, 0, 64, 64);
    glDrawBuffer(GL_FRONT); glReadBuffer(GL_FRONT);
    test_default_routing();
    test_semantic_noops();
    glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT); glFinish();
    MockNativeXGLGetStats(&before);
    check(before.clears > 0, "GL clear reached XGL through the XMesa driver hook");
    glColor3f(1, 0, 0); triangle(); glFinish();
    MockNativeXGLGetStats(&after);
    check(after.triangles > before.triangles, "ordinary GL triangle reached XGL");
    read_color(32, 28, 255, 0, 0, "GL readback sees native triangle in Xlib surface");

    before = after;
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0, 0, 1, 0.5f); triangle(); glFinish();
    MockNativeXGLGetStats(&after);
    check(after.triangles > before.triangles, "blended triangle retains native XGL rasterization");
    read_color(32, 28, 127, 0, 128, "software blend consumes the native window pixels");
    glDisable(GL_BLEND); glColor3f(0, 1, 0); triangle(); glFinish();
    MockNativeXGLGetStats(&after);
    check(after.triangles > before.triangles, "native rendering resumes after software blend");
    read_color(32, 28, 0, 255, 0, "native triangle replaces software result");

    glColor3f(1, 1, 0); glBegin(GL_POINTS); glVertex2f(-0.75f, -0.75f); glEnd();
    glFinish(); MockNativeXGLGetStats(&after);
    check(after.points > 0, "GL point reaches native multimarker");
    glBegin(GL_LINES); glVertex2f(-0.6f, 0.7f); glVertex2f(0.6f, 0.7f); glEnd();
    glFinish(); MockNativeXGLGetStats(&after);
    check(after.lines > 0, "GL line reaches native polyline");

    before = after;
    glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity(); glTranslatef(0.5f, 0, 0);
    glColor3f(0, 0, 1); triangle(); glFinish();
    read_color(48, 28, 0, 0, 255, "transformed native geometry reaches expected screen location");
    MockNativeXGLGetStats(&after);
    check(after.transforms > before.transforms,
          "GL matrix state is delegated to native XGL transform objects");
    glLoadIdentity();
    glVertexPointer(3, GL_FLOAT, 0, vertices); glEnableClientState(GL_VERTEX_ARRAY);
    glColor3f(1, 0, 1); glDrawArrays(GL_TRIANGLES, 0, 3); glFinish();
    read_color(32, 28, 255, 0, 255, "vertex array geometry renders through native binding");
    glDisableClientState(GL_VERTEX_ARRAY);

    /* The near plane cuts one raw object-space vertex. Native XGL, rather
     * than Mesa's CPU clipping stage, must clip the submitted triangle. */
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glFrustum(-1, 1, -1, 1, 1, 10);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glClear(GL_COLOR_BUFFER_BIT);
    glColor3f(1, 1, 0);
    glBegin(GL_TRIANGLES);
    glVertex3f(-1.5f, -1.0f, -2.0f);
    glVertex3f(1.5f, -1.0f, -2.0f);
    glVertex3f(0.0f, 1.0f, -0.5f);
    glEnd(); glFinish();
    read_color(32, 24, 255, 255, 0, "native homogeneous clipping retains visible portion");
    read_color(32, 58, 0, 0, 0, "native near-plane clipping removes invisible portion");
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    test_depth();
    test_lighting();
    test_texture();
    test_hybrid_effects();
    test_hybrid_points_lines();

    glColor3f(0, 1, 1); triangle();
    before = after;
    check(glXMakeCurrent(display, windows[1], contexts[1]), "context switch flushes old native batch");
    glViewport(0, 0, 64, 64);
    glClearColor(0.25f, 0.5f, 0.75f, 1); glClear(GL_COLOR_BUFFER_BIT); glFinish();
    read_color(32, 28, 64, 128, 191, "second native context uses its own surface");
    check(glXMakeCurrent(display, windows[0], contexts[0]), "restore first native context");
    read_color(32, 28, 0, 255, 255, "switch preserves first context native result");

    back_visual = glXChooseVisual(display, DefaultScreen(display), back_attributes);
    check(back_visual != NULL, "choose double-buffer visual");
    contexts[2] = NULL; windows[2] = 0; maps[2] = 0;
    if (back_visual) {
        windows[2] = create_window(display, back_visual, maps+2, 170);
        contexts[2] = glXCreateContext(display, back_visual, NULL, True);
        check(contexts[2] != NULL && glXMakeCurrent(display, windows[2], contexts[2]),
              "bind double-buffer context");
        if (contexts[2]) {
            glViewport(0, 0, 64, 64);
            glDrawBuffer(GL_BACK); glReadBuffer(GL_BACK);
            MockNativeXGLGetStats(&before);
            glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
            glColor3f(1, 0, 0); triangle(); glFinish();
            MockNativeXGLGetStats(&after);
            check(after.triangles > before.triangles,
                  "back-buffer geometry reaches native XGL rasterization");
            check(after.uploads > before.uploads && after.downloads > before.downloads,
                  "back-buffer color state transfers between native and software storage");
            read_color(32, 28, 255, 0, 0, "back-buffer path preserves GL readback");
            glXSwapBuffers(display, windows[2]);
            glReadBuffer(GL_FRONT);
            read_color(32, 28, 255, 0, 0, "swap exposes back-buffer result");
            glReadBuffer(GL_BACK);
            glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glColor4f(0, 0, 1, 0.5f); triangle(); glFinish();
            read_color(32, 28, 127, 0, 128, "software back-buffer blend consumes native result");
            glDisable(GL_BLEND); glColor3f(0, 1, 0); triangle(); glFinish();
            read_color(32, 28, 0, 255, 0, "native back-buffer rendering resumes after software fallback");
            test_depth();
            test_hybrid_effects();
            glReadBuffer(GL_FRONT);
            read_color(32,28,255,0,0,"hybrid back rendering never displays scratch capture pixels");
            glReadBuffer(GL_BACK);
            glXSwapBuffers(display,windows[2]); glReadBuffer(GL_FRONT);
            read_color(32,28,0,255,0,"swap presents the completed hybrid back image");
            glReadBuffer(GL_BACK);
            test_hybrid_points_lines();
            test_hybrid_destinations();
            test_hybrid_split_buffers(display,back_visual,windows[2],contexts[2]);
        }
        XFree(back_visual);
    }
    check(glGetError() == GL_NO_ERROR, "no unexpected GL errors");
    test_hybrid_alpha_visual(display);
    test_hybrid_capture_failure(display);
    glXMakeCurrent(display, None, NULL);
    for (i = 0; i < 3; ++i) {
        if (contexts[i]) glXDestroyContext(display, contexts[i]);
        if (windows[i]) XDestroyWindow(display, windows[i]);
        if (maps[i]) XFreeColormap(display, maps[i]);
    }
    XFree(visual);
    XSync(display, False);
    MockNativeXGLGetStats(&after);
    check(after.live_objects == 0 && after.opens == after.closes,
          "native contexts release every XGL object and System State");
    check(protocol_errors == 0, "native/software rendering produced no X protocol errors");
    XCloseDisplay(display);
    printf("GLX/XGL real-Xlib integration: %d checks, %d failures (synthetic XGL SDK)\n",
           checks, failures);
    return failures ? 1 : 0;
}
