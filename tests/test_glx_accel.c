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
    check(after.triangles == before.triangles,
          "a partial scissor still requires software fallback");
    read_color(32,28,0,255,255,"software scissor preserves pixels outside its rectangle");
    read_color(8,8,255,0,0,"software scissor writes pixels inside its rectangle");

    glDisable(GL_SCISSOR_TEST); glScissor(0,0,64,64);
    glDisable(GL_ALPHA_TEST); glAlphaFunc(GL_ALWAYS,0);
    glDisable(GL_COLOR_LOGIC_OP);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    before = after;
    glColor4f(0,0,1,0.5f); triangle(); glFinish();
    MockNativeXGLGetStats(&after);
    check(after.triangles == before.triangles,
          "blending that changes destination color still uses software");
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
    glColor3f(1,0,0); triangle_z(0.0f); glFinish();
    glDisable(GL_BLEND); glDepthFunc(GL_EQUAL);
    glColor3f(0,1,0); triangle_z(0.0f); glFinish();
    read_color(32,28,0,255,0,"native GL_EQUAL consumes software depth without quantization changes");
    glDepthFunc(GL_LESS); glColor3f(0,0,1); triangle_z(0.0f); glFinish();
    read_color(32,28,0,255,0,"native GL_LESS rejects geometry at matching software depth");
    glDepthFunc(GL_EQUAL); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glColor3f(1,0,1); triangle_z(0.0f); glFinish();
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
    /* Force a semantically identical Mesa reference through an unsupported
     * blend state. This compares native lighting against the actual CPU
     * renderer without depending on our fixture's equation for expectations. */
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClear(GL_COLOR_BUFFER_BIT); triangle(); glFinish();
    glReadPixels(32,28,1,1,GL_RGBA,GL_UNSIGNED_BYTE,software);
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
    textured_perspective_triangle();
    glReadPixels(32,35,1,1,GL_RGBA,GL_UNSIGNED_BYTE,software);
    equal = 1;
    for (i = 0; i < 3; ++i) if (abs((int)native[i]-software[i]) > 2) equal = 0;
    check(equal,"native perspective texture interpolation agrees with software reference");
    glDisable(GL_BLEND); glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP);
    MockNativeXGLGetStats(&before); textured_quad(0.25f,0.25f,0.25f,0.25f);
    MockNativeXGLGetStats(&after);
    check(after.texture_samples == before.texture_samples,"unsupported GL_CLAMP semantics retain software texture fallback");
    read_color(32,28,255,0,0,"software texture fallback preserves expected texel color");
    glDisable(GL_TEXTURE_2D); glDeleteTextures(1,&texture);
}

int main(void)
{
    int attributes[] = {GLX_RGBA, GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8,
                        GLX_BLUE_SIZE, 8, GLX_DEPTH_SIZE, 24, None};
    int back_attributes[] = {GLX_RGBA, GLX_DOUBLEBUFFER, GLX_RED_SIZE, 8,
                             GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8,
                             GLX_DEPTH_SIZE, 24, None};
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
    check(after.triangles == before.triangles, "blended triangle uses software fallback");
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
        }
        XFree(back_visual);
    }
    check(glGetError() == GL_NO_ERROR, "no unexpected GL errors");
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
