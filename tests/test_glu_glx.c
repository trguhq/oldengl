/* MIT licensed. GLU geometry through real GLX windows and synthetic XGL. */
#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glx.h>
#include "mock_native_xgl.h"

extern int putenv(char *);
static int checks, failures;

static void check(int value, const char *message)
{
    ++checks;
    if (!value) { ++failures; fprintf(stderr,"FAIL: %s\n",message); }
}

int main(void)
{
    static char native_setting[] = "OLDENGL_NO_XGL=0";
    static char software_setting[] = "OLDENGL_NO_XGL=1";
    static const GLint sample[3][2] = {{32,32},{48,32},{2,2}};
    int attributes[] = {GLX_RGBA,GLX_RED_SIZE,8,GLX_GREEN_SIZE,8,
        GLX_BLUE_SIZE,8,GLX_DEPTH_SIZE,24,GLX_DOUBLEBUFFER,None};
    Display *display;
    XVisualInfo *visual;
    XSetWindowAttributes window_attributes;
    Colormap colormap;
    Window window;
    GLXContext context;
    GLUquadric *quadric;
    GLubyte pixels[2][3][4];
    struct MockNativeXGLStats before, after;
    int shape, pass, i, channel, equal;
    MockNativeXGLReset();
    display = XOpenDisplay(NULL);
    if (!display) { fprintf(stderr,"test_glu_glx requires an X server\n"); return 1; }
    visual = glXChooseVisual(display,DefaultScreen(display),attributes);
    check(visual != NULL,"choose GLU native GLX window visual");
    if (!visual) { XCloseDisplay(display); return 1; }
    colormap = XCreateColormap(display,RootWindow(display,visual->screen),visual->visual,AllocNone);
    window_attributes.colormap = colormap;
    window_attributes.border_pixel = 0;
    window = XCreateWindow(display,RootWindow(display,visual->screen),10,10,
        64,64,0,visual->depth,InputOutput,visual->visual,
        CWColormap|CWBorderPixel,&window_attributes);
    XMapWindow(display,window); XSync(display,False);
    context = glXCreateContext(display,visual,NULL,True);
    check(context != NULL && glXMakeCurrent(display,window,context),"bind GLU native GLX context");
    if (!context) return 1;
    glViewport(0,0,64,64);
    glDrawBuffer(GL_BACK); glReadBuffer(GL_BACK);
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(-1.2,1.2,-1.2,1.2,-3,3);
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS);
    quadric = gluNewQuadric();
    check(quadric != NULL,"create GLU quadric for native rendering");
    if (!quadric) return 1;
    gluQuadricNormals(quadric,GLU_SMOOTH);
    for (shape = 0; shape < 3; ++shape) {
        for (pass = 0; pass < 2; ++pass) {
            check(putenv(pass ? software_setting : native_setting) == 0,"select independent GLU renderer");
            glLoadIdentity();
            glClearColor(0,0,0,1); glClearDepth(1);
            glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
            glColor3f(0,1,0);
            if (shape == 2) { glTranslatef(0,-0.5f,0); glRotatef(-90,1,0,0); }
            MockNativeXGLGetStats(&before);
            /* Keep complete primitives within Mesa's immediate batch;
             * wrapped strips deliberately retain software vertex setup. */
            if (shape == 0) gluSphere(quadric,0.8,8,6);
            else if (shape == 1) gluDisk(quadric,0.2,0.8,16,2);
            else gluCylinder(quadric,0.6,0.4,1,16,4);
            glFinish();
            MockNativeXGLGetStats(&after);
            check(pass ? after.triangles == before.triangles : after.triangles > before.triangles,
                  "GLU geometry follows native and forced-software routing");
            if (!pass) {
                check(after.transforms > before.transforms && after.depth_tests > before.depth_tests,
                      "GLU primitives retain native XGL transforms and depth testing");
                if (after.transforms == before.transforms || after.depth_tests == before.depth_tests)
                    fprintf(stderr,"  shape %d: transforms +%lu, depth tests +%lu, triangles +%lu\n",
                        shape,after.transforms-before.transforms,after.depth_tests-before.depth_tests,
                        after.triangles-before.triangles);
            }
            for (i = 0; i < 3; ++i)
                glReadPixels(sample[i][0],sample[i][1],1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixels[pass][i]);
            check(glGetError() == GL_NO_ERROR,"GLU native drawing has no GL errors");
        }
        equal = 1;
        for (i = 0; i < 3; ++i) for (channel = 0; channel < 4; ++channel)
            if (abs((int)pixels[0][i][channel]-pixels[1][i][channel]) > 2) equal = 0;
        check(equal,"GLU native pixels agree with independent software rendering");
        check(pixels[0][shape == 1 ? 1 : 0][1] == 255 && pixels[0][2][1] == 0,
              "GLU geometry fills its expected region and preserves the background");
    }
    gluDeleteQuadric(quadric);
    glXMakeCurrent(display,None,NULL);
    glXDestroyContext(display,context);
    XDestroyWindow(display,window); XFreeColormap(display,colormap); XFree(visual);
    XSync(display,False);
    MockNativeXGLGetStats(&after);
    check(after.live_objects == 0 && after.opens == after.closes,"GLU integration releases native resources");
    XCloseDisplay(display);
    printf("GLU/GLX native integration: %d checks, %d failures (synthetic XGL SDK)\n",checks,failures);
    return failures ? 1 : 0;
}
