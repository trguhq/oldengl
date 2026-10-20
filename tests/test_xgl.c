/*
 * Portable integration tests: real Mesa OpenGL software rendering, mocked
 * Xlib/XGL presentation. These do not validate the native Solaris SDK or DGA.
 */
#include <GL/xglmesa.h>
#include "mock/mock_xgl.h"
#include "../src/xgl_pixels.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures, assertions;

#define CHECK(expr) do { \
    ++assertions; \
    if (!(expr)) { \
        fprintf(stderr, "%s:%d: %s failed (%s)\n", __FILE__, __LINE__, \
                #expr, XGLMesaGetLastError()); \
        ++failures; \
    } \
} while (0)

static void paint_rows(void)
{
    glDisable(GL_SCISSOR_TEST);
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 4, 8, 4);
    glClearColor(0, 1, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
}

static void queued_triangle(GLfloat red, GLfloat green, GLfloat blue)
{
    glColor3f(red, green, blue);
    glBegin(GL_TRIANGLES);
    glVertex2f(-1, -1);
    glVertex2f(1, -1);
    glVertex2f(0, 1);
    glEnd();
}

static void test_presentation(unsigned int red, unsigned int green,
                              unsigned int blue)
{
    XGLMesaContext context;
    GLsizei width, height;
    const GLubyte *rgba;
    int before;
    MockXGLReset();
    MockXGLPacking(red, green, blue);
    context = XGLMesaCreateContext(MockXGLDisplay(), 1, NULL);
    CHECK(context != NULL);
    if (!context) return;
    CHECK(XGLMesaGetCurrentContext() == NULL);
    CHECK(XGLMesaMakeCurrent(context));
    CHECK(XGLMesaGetCurrentContext() == context);
    CHECK(XGLMesaGetColorBuffer(context, &width, &height, &rgba));
    CHECK(width == 8 && height == 8);
    paint_rows();
    before = MockXGLCopies();
    glFlush();
    CHECK(MockXGLCopies() > before);
    CHECK(MockXGLPixel(1, 1, 1) == (255U << green));
    CHECK(MockXGLPixel(1, 1, 6) == (255U << red));
    CHECK(rgba[0] == 255 && rgba[1] == 0 && rgba[2] == 0 && rgba[3] == 255);
    CHECK(rgba[4 * 8 * 7] == 0 && rgba[4 * 8 * 7 + 1] == 255);
    before = MockXGLCopies();
    glFinish();
    CHECK(MockXGLCopies() > before);
    before = MockXGLSynchronizes();
    CHECK(XGLMesaSwapBuffers(context));
    CHECK(MockXGLSynchronizes() > before);
    glClearColor(0, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    CHECK(XGLMesaSwapBuffers(context));
    CHECK(MockXGLPixel(1, 3, 3) == (255U << blue));
    MockXGLErrorNextCopy();
    CHECK(!XGLMesaSwapBuffers(context));
    CHECK(strstr(XGLMesaGetLastError(), "injected XGL copy failure") != NULL);
    CHECK(XGLMesaSwapBuffers(context));
    CHECK(glGetError() == GL_NO_ERROR);
    CHECK(XGLMesaMakeCurrent(NULL));
    CHECK(XGLMesaGetCurrentContext() == NULL);
    CHECK(!XGLMesaSwapBuffers(context));
    XGLMesaDestroyContext(context);
    CHECK(MockXGLLiveObjects() == 0);
    CHECK(MockXGLOpens() == 1 && MockXGLCloses() == 1);
}

static void test_contexts_and_resize(void)
{
    XGLMesaContext a, b;
    GLuint texture, list;
    GLint viewport[4], scissor[4];
    GLsizei width, height;
    const GLubyte *rgba, *original;
    int live;
    MockXGLReset();
    a = XGLMesaCreateContext(MockXGLDisplay(), 1, NULL);
    CHECK(a != NULL);
    if (!a) return;
    CHECK(XGLMesaMakeCurrent(a));
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    glColor3f(1, 0, 0);
    glEndList();
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    queued_triangle(1, 0, 0);
    b = XGLMesaCreateContext(MockXGLDisplay(), 2, a);
    CHECK(b != NULL);
    if (!b) { XGLMesaDestroyContext(a); return; }
    CHECK(XGLMesaGetCurrentContext() == a);
    CHECK(MockXGLOpens() == 1);
    CHECK(XGLMesaMakeCurrent(b));
    CHECK(MockXGLPixel(1, 4, 4) == 255U);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    queued_triangle(0, 1, 0);
    CHECK(glIsTexture(texture));
    CHECK(glIsList(list));
    CHECK(!XGLMesaSwapBuffers(a));
    CHECK(XGLMesaGetCurrentContext() == b);
    CHECK(XGLMesaGetColorBuffer(a, &width, &height, &original));
    CHECK(!XGLMesaResizeBuffers(a, 0, 10));
    CHECK(!XGLMesaResizeBuffers(a, 1000000, 1000000));
    CHECK(XGLMesaGetColorBuffer(a, &width, &height, &rgba));
    CHECK(width == 8 && height == 8 && rgba == original);
    MockXGLFailCreate(XGL_MEM_RAS, 0);
    CHECK(!XGLMesaResizeBuffers(a, 6, 5));
    CHECK(XGLMesaGetColorBuffer(a, &width, &height, &rgba));
    CHECK(width == 8 && height == 8 && rgba == original);
    CHECK(XGLMesaGetCurrentContext() == b);
    MockXGLFailCreate(0, 0);
    MockXGLWindow(1, 6, 5, 1);
    CHECK(XGLMesaResizeBuffers(a, 6, 5));
    CHECK(XGLMesaGetCurrentContext() == b);
    CHECK(XGLMesaGetColorBuffer(a, &width, &height, &rgba));
    CHECK(width == 6 && height == 5);
    CHECK(XGLMesaMakeCurrent(a));
    CHECK(MockXGLPixel(2, 2, 1) == 0xff00U);
    glViewport(1, 2, 3, 4);
    glScissor(2, 1, 3, 2);
    MockXGLWindow(1, 9, 7, 1);
    CHECK(XGLMesaResizeBuffers(a, 9, 7));
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetIntegerv(GL_SCISSOR_BOX, scissor);
    CHECK(viewport[0] == 1 && viewport[1] == 2 && viewport[2] == 3 && viewport[3] == 4);
    CHECK(scissor[0] == 2 && scissor[1] == 1 && scissor[2] == 3 && scissor[3] == 2);
    CHECK(XGLMesaGetColorBuffer(a, &width, &height, &rgba));
    CHECK(width == 9 && height == 7);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    CHECK(XGLMesaSwapBuffers(a));
    CHECK(MockXGLPixel(1, 8, 6) == 255U);
    CHECK(MockXGLResizes() == 2);
    CHECK(XGLMesaGetColorBuffer(a, &width, &height, &original));
    MockXGLFailCreate(XGL_2D_CTX, 0);
    CHECK(!XGLMesaResizeBuffers(a, 5, 3));
    CHECK(XGLMesaGetCurrentContext() == a);
    CHECK(XGLMesaGetColorBuffer(a, &width, &height, &rgba));
    CHECK(width == 9 && height == 7 && rgba == original);
    MockXGLFailCreate(0, 0);
    CHECK(XGLMesaSwapBuffers(a));
    CHECK(MockXGLPixel(1, 8, 6) == 255U);
    live = MockXGLLiveObjects();
    glBegin(GL_POINTS);
    CHECK(!XGLMesaMakeCurrent(b));
    CHECK(!XGLMesaMakeCurrent(NULL));
    CHECK(!XGLMesaResizeBuffers(a, 4, 4));
    CHECK(!XGLMesaSwapBuffers(a));
    XGLMesaDestroyContext(a);
    CHECK(MockXGLLiveObjects() == live);
    glEnd();
    CHECK(XGLMesaGetCurrentContext() == a);
    glViewport(0, 0, 9, 7);
    queued_triangle(0, 0, 1);
    XGLMesaDestroyContext(a);
    CHECK(MockXGLPixel(1, 4, 3) == 0xff0000U);
    CHECK(XGLMesaGetCurrentContext() == NULL);
    CHECK(MockXGLCloses() == 0);
    CHECK(XGLMesaMakeCurrent(b));
    CHECK(glIsTexture(texture));
    CHECK(glIsList(list));
    glDeleteTextures(1, &texture);
    glDeleteLists(list, 1);
    CHECK(glGetError() == GL_NO_ERROR);
    XGLMesaDestroyContext(b);
    CHECK(XGLMesaGetCurrentContext() == NULL);
    CHECK(MockXGLLiveObjects() == 0);
    CHECK(MockXGLCloses() == 1);
}

static void test_failures(void)
{
    static const int types[] = { XGL_WIN_RAS, XGL_2D_CTX, XGL_MEM_RAS, XGL_2D_CTX };
    static const int after[] = { 0, 0, 0, 1 };
    unsigned int i;
    XGLMesaContext context;
    MockXGLReset();
    CHECK(XGLMesaCreateContext(NULL, 1, NULL) == NULL);
    CHECK(XGLMesaCreateContext(MockXGLDisplay(), 0, NULL) == NULL);
    CHECK(XGLMesaCreateContext(MockXGLDisplay(), 3, NULL) == NULL);
    CHECK(MockXGLLiveObjects() == 0);
    MockXGLFailOpen(1);
    CHECK(XGLMesaCreateContext(MockXGLDisplay(), 1, NULL) == NULL);
    CHECK(MockXGLLiveObjects() == 0);
    MockXGLFailOpen(0);
    for (i = 0; i < sizeof(types) / sizeof(types[0]); ++i) {
        MockXGLFailCreate(types[i], after[i]);
        context = XGLMesaCreateContext(MockXGLDisplay(), 1, NULL);
        CHECK(context == NULL);
        if (context) XGLMesaDestroyContext(context);
        CHECK(MockXGLLiveObjects() == 0);
        CHECK(MockXGLCloses() == MockXGLOpens());
    }
    MockXGLFailCreate(0, 0);
    context = XGLMesaCreateContext(MockXGLDisplay(), 1, NULL);
    CHECK(context != NULL);
    if (context) {
        CHECK(XGLMesaMakeCurrent(context));
        XGLMesaDestroyContext(context);
    }
    CHECK(MockXGLLiveObjects() == 0);
    MockXGLReset();
}

static void test_pixel_conversion(void)
{
    static const unsigned char image[] = {
        1, 2, 3, 255,  4, 5, 6, 0,
        7, 8, 9, 128, 10, 11, 12, 64
    };
    unsigned int words[4];
    CHECK(xglmesa_pack_rgba(words, image, 2, 2, 16, 8, 0));
    CHECK(words[0] == 0x070809U && words[1] == 0x0a0b0cU);
    CHECK(words[2] == 0x010203U && words[3] == 0x040506U);
    CHECK(xglmesa_pack_rgba(words, image, 2, 2, 0, 8, 24));
    CHECK(words[0] == 0x09000807U && words[3] == 0x06000504U);
    CHECK(!xglmesa_pack_rgba(NULL, image, 2, 2, 0, 8, 16));
    CHECK(!xglmesa_pack_rgba(words, NULL, 2, 2, 0, 8, 16));
    CHECK(!xglmesa_pack_rgba(words, image, 0, 2, 0, 8, 16));
    CHECK(!xglmesa_pack_rgba(words, image, 2, 0, 0, 8, 16));
    CHECK(!xglmesa_pack_rgba(words, image, (size_t)-1, 2, 0, 8, 16));
    CHECK(!xglmesa_pack_rgba(words, image, 2, 2, 32, 8, 16));
    CHECK(!xglmesa_pack_rgba(words, image, 2, 2, 0, 7, 16));
    CHECK(!xglmesa_pack_rgba(words, image, 2, 2, 0, 8, 8));
}

int main(void)
{
    test_pixel_conversion();
    test_presentation(0, 8, 16);
    test_presentation(16, 8, 0);
    test_contexts_and_resize();
    test_failures();
    if (failures) {
        fprintf(stderr, "XGL mock integration: %d/%d assertions failed\n", failures, assertions);
        return EXIT_FAILURE;
    }
    printf("XGL mock integration: %d assertions passed (not native SDK validation)\n", assertions);
    return EXIT_SUCCESS;
}
