/* Portable behavioral regression checks, not an OpenGL conformance suite.
 * Build with the library under test and -lm. No display server is required.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <GL/gl.h>
#include <GL/osmesa.h>

#define W 64
#define H 64
static GLuint framebuffer[W * H];
static const char *group;
static int failures;
static int checks;
static int groups;

static void check(int condition, const char *what)
{
    ++checks;
    if (!condition) {
        ++failures;
        fprintf(stderr, "FAIL [%s]: %s\n", group, what);
    }
}

static void no_error(void)
{
    GLenum error;
    error = glGetError();
    if (error != GL_NO_ERROR)
        fprintf(stderr, "GL error [%s]: 0x%x\n", group, (unsigned) error);
    check(error == GL_NO_ERROR, "no unexpected OpenGL error");
}

static void pixel(int x, int y, int r, int g, int b, int tolerance,
                  const char *what)
{
    GLubyte p[4];
    glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, p);
    if (abs((int)p[0] - r) > tolerance ||
        abs((int)p[1] - g) > tolerance ||
        abs((int)p[2] - b) > tolerance) {
        fprintf(stderr, "pixel [%s] (%d,%d): got %u,%u,%u expected %d,%d,%d +/- %d\n",
                group, x, y, (unsigned)p[0], (unsigned)p[1], (unsigned)p[2],
                r, g, b, tolerance);
        check(0, what);
    } else {
        check(1, what);
    }
}

static OSMesaContext start(const char *name)
{
    OSMesaContext c;
    group = name;
    ++groups;
    memset(framebuffer, 0, sizeof(framebuffer));
    c = OSMesaCreateContext(OSMESA_RGBA, NULL);
    if (!c || !OSMesaMakeCurrent(c, framebuffer, GL_UNSIGNED_BYTE, W, H)) {
        fprintf(stderr, "Cannot create offscreen context for %s\n", group);
        exit(2);
    }
    glViewport(0, 0, W, H);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
            GL_STENCIL_BUFFER_BIT | GL_ACCUM_BUFFER_BIT);
    return c;
}

static void stop(OSMesaContext c)
{
    no_error();
    OSMesaDestroyContext(c);
}

static void rectangle(GLfloat z)
{
    glBegin(GL_QUADS);
    glVertex3f(-0.75f, -0.75f, z);
    glVertex3f( 0.75f, -0.75f, z);
    glVertex3f( 0.75f,  0.75f, z);
    glVertex3f(-0.75f,  0.75f, z);
    glEnd();
}

static void state_and_errors(void)
{
    OSMesaContext c;
    GLfloat m[16];
    GLfloat color[4];
    GLint i;
    c = start("state, errors and stacks");
    check(glGetString(GL_VERSION) != NULL, "version string exists");
    glGetIntegerv(GL_DEPTH_BITS, &i);
    check(i >= 16, "software depth buffer exists");
    glGetIntegerv(GL_STENCIL_BITS, &i);
    check(i >= 8, "software stencil buffer exists");
    glGetIntegerv(GL_ACCUM_RED_BITS, &i);
    check(i >= 16, "software accumulation buffer exists");
    glPushMatrix();
    glTranslatef(0.25f, 0.5f, -0.75f);
    glGetFloatv(GL_MODELVIEW_MATRIX, m);
    check(fabs(m[12] - 0.25) < 1e-6 && fabs(m[13] - 0.5) < 1e-6,
          "column-major matrix translation");
    glPopMatrix();
    glGetFloatv(GL_MODELVIEW_MATRIX, m);
    check(m[12] == 0 && m[0] == 1, "matrix stack restores previous state");
    glColor3f(1, 0, 0);
    glPushAttrib(GL_CURRENT_BIT | GL_ENABLE_BIT);
    glColor3f(0, 1, 0);
    glEnable(GL_BLEND);
    glPopAttrib();
    glGetFloatv(GL_CURRENT_COLOR, color);
    check(color[0] == 1 && color[1] == 0 && !glIsEnabled(GL_BLEND),
          "server attribute stack restores current color and enables");
    glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPopClientAttrib();
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &i);
    check(i == 4, "client attribute stack restores pixel storage");
    glEnable(0xdead);
    check(glGetError() == GL_INVALID_ENUM, "invalid capability reports invalid enum");
    glBegin(GL_POINTS);
    glMatrixMode(GL_PROJECTION);
    glEnd();
    check(glGetError() == GL_INVALID_OPERATION, "illegal begin/end command reports error");
    glPopMatrix();
    check(glGetError() == GL_STACK_UNDERFLOW, "matrix stack underflow reports error");
    stop(c);
}

static void geometry(void)
{
    OSMesaContext c;
    GLdouble plane[4] = {1, 0, 0, 0};
    c = start("geometry, transforms, clipping and culling");
    glColor3f(1, 0, 0);
    glBegin(GL_TRIANGLES);
    glVertex2f(-0.9f, -0.9f);
    glVertex2f( 0.9f, -0.9f);
    glVertex2f(0, 0.9f);
    glEnd();
    pixel(32, 24, 255, 0, 0, 1, "triangle rasterizes interior");
    pixel(3, 60, 0, 0, 0, 0, "triangle leaves exterior untouched");
    glClear(GL_COLOR_BUFFER_BIT);
    glClipPlane(GL_CLIP_PLANE0, plane);
    glEnable(GL_CLIP_PLANE0);
    glColor3f(0, 1, 0);
    rectangle(0);
    pixel(20, 32, 0, 0, 0, 0, "user clipping removes negative half-space");
    pixel(44, 32, 0, 255, 0, 1, "user clipping preserves positive half-space");
    glDisable(GL_CLIP_PLANE0);
    glClear(GL_COLOR_BUFFER_BIT);
    glPushMatrix();
    glTranslatef(0.5f, 0, 0);
    glScalef(0.25f, 0.25f, 1);
    glColor3f(0, 0, 1);
    rectangle(0);
    glPopMatrix();
    pixel(48, 32, 0, 0, 255, 1, "transformed primitive appears at translated position");
    pixel(32, 32, 0, 0, 0, 0, "transformed primitive leaves original center clear");
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    rectangle(0);
    pixel(32, 32, 0, 0, 0, 0, "front face culling suppresses counterclockwise quad");
    stop(c);
}

static void fragment_tests(void)
{
    OSMesaContext c;
    GLubyte stencil;
    c = start("depth, stencil, alpha, blending, scissor");
    glEnable(GL_DEPTH_TEST);
    glColor3f(1, 0, 0);
    rectangle(-0.5f);
    glColor3f(0, 1, 0);
    rectangle(0.5f);
    pixel(32, 32, 255, 0, 0, 1, "depth test rejects farther quad");
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, 3, 255);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    rectangle(0);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glReadPixels(32, 32, 1, 1, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, &stencil);
    check(stencil == 3, "stencil replacement writes reference");
    glStencilFunc(GL_EQUAL, 2, 255);
    glColor3f(0, 1, 0);
    rectangle(0);
    pixel(32, 32, 255, 0, 0, 1, "stencil comparison rejects fragment");
    glStencilFunc(GL_EQUAL, 3, 255);
    glColor3f(0, 0, 1);
    rectangle(0);
    pixel(32, 32, 0, 0, 255, 1, "stencil comparison accepts fragment");
    glDisable(GL_STENCIL_TEST);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.5f);
    glColor4f(0, 1, 0, 0.25f);
    rectangle(0);
    pixel(32, 32, 0, 0, 255, 1, "alpha test rejects low alpha");
    glDisable(GL_ALPHA_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1, 0, 0, 0.5f);
    rectangle(0);
    pixel(32, 32, 128, 0, 127, 2, "source alpha blend combines colors");
    glDisable(GL_BLEND);
    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, 16, 16);
    glClearColor(0, 1, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    pixel(8, 8, 0, 255, 0, 1, "scissored clear writes inside rectangle");
    pixel(32, 32, 128, 0, 127, 2, "scissored clear preserves outside rectangle");
    stop(c);
}

static void accumulation(void)
{
    OSMesaContext c;
    c = start("accumulation");
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glAccum(GL_LOAD, 0.25f);
    glClearColor(0, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glAccum(GL_ACCUM, 0.75f);
    glAccum(GL_RETURN, 1);
    pixel(32, 32, 64, 0, 191, 2, "accumulation combines weighted frames");
    stop(c);
}

static void textures(void)
{
    OSMesaContext c;
    GLuint texture;
    GLubyte tex[16] = {0,255,0,255, 0,255,0,255,
                      0,255,0,255, 0,255,0,255};
    GLubyte result[16];
    GLubyte red[4] = {255,0,0,255};
    GLint width;
    c = start("texture objects, 1D/2D sampling, update and copy");
    glGenTextures(1, &texture);
    check(texture != 0 && !glIsTexture(texture), "generated texture is reserved, not yet an object");
    glBindTexture(GL_TEXTURE_2D, texture);
    check(glIsTexture(texture), "binding creates texture object");
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, tex);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
    check(width == 2, "texture dimensions query");
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, result);
    check(memcmp(tex, result, sizeof(tex)) == 0, "texture image round trip");
    glEnable(GL_TEXTURE_2D);
    glTexCoord2f(0.75f, 0.75f);
    rectangle(0);
    pixel(32, 32, 0, 255, 0, 1, "2D nearest texture sampling");
    glTexSubImage2D(GL_TEXTURE_2D, 0, 1, 1, 1, 1, GL_RGBA,
                   GL_UNSIGNED_BYTE, red);
    rectangle(0);
    pixel(32, 32, 255, 0, 0, 1, "2D texture subimage update affects sampling");
    glDisable(GL_TEXTURE_2D);
    glClearColor(0, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, 2, 2, 0);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, result);
    check(result[0] == 0 && result[1] == 0 && result[2] == 255,
          "framebuffer-to-texture copy");
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, 1, 1, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, red);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glEnable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0);     glVertex2f(-0.75f, -0.75f);
    glTexCoord2f(128, 0);   glVertex2f( 0.75f, -0.75f);
    glTexCoord2f(128, 128); glVertex2f( 0.75f,  0.75f);
    glTexCoord2f(0, 128);   glVertex2f(-0.75f,  0.75f);
    glEnd();
    pixel(32, 32, 255, 0, 0, 1, "texture minification selects supplied mipmap level");
    glDisable(GL_TEXTURE_2D);
    glDeleteTextures(1, &texture);
    check(!glIsTexture(texture), "deleting texture removes object");
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA, 1, 0, GL_RGBA,
                GL_UNSIGNED_BYTE, red);
    glEnable(GL_TEXTURE_1D);
    glTexCoord1f(0.5f);
    rectangle(0);
    pixel(32, 32, 255, 0, 0, 1, "1D texture sampling");
    stop(c);
}

static void lists_and_arrays(void)
{
    OSMesaContext c;
    GLuint list;
    GLfloat vertices[6] = {-0.8f,-0.8f, 0.8f,-0.8f, 0,0.8f};
    GLubyte indices[3] = {0,1,2};
    GLvoid *pointer;
    c = start("display lists and vertex arrays");
    list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    glColor3f(1, 0, 0);
    rectangle(0);
    glEndList();
    check(glIsList(list), "compiled display list exists");
    pixel(32, 32, 0, 0, 0, 0, "GL_COMPILE does not execute geometry");
    glCallList(list);
    pixel(32, 32, 255, 0, 0, 1, "display list replays geometry and state");
    glDeleteLists(list, 1);
    check(!glIsList(list), "display list deletion");
    glClear(GL_COLOR_BUFFER_BIT);
    glColor3f(0, 1, 0);
    glVertexPointer(2, GL_FLOAT, 0, vertices);
    glEnableClientState(GL_VERTEX_ARRAY);
    glGetPointerv(GL_VERTEX_ARRAY_POINTER, &pointer);
    check(pointer == vertices, "vertex pointer query");
    glDrawArrays(GL_TRIANGLES, 0, 3);
    pixel(32, 24, 0, 255, 0, 1, "glDrawArrays renders array geometry");
    glColor3f(0, 0, 1);
    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_BYTE, indices);
    pixel(32, 24, 0, 0, 255, 1, "glDrawElements renders indexed geometry");
    glDisableClientState(GL_VERTEX_ARRAY);
    stop(c);
}

static void evaluators(void)
{
    OSMesaContext c;
    GLfloat control[2][2][3] = {
        {{-0.75f,-0.75f,0}, {-0.75f,0.75f,0}},
        {{ 0.75f,-0.75f,0}, { 0.75f,0.75f,0}}
    };
    GLint order[2];
    c = start("evaluators");
    glMap2f(GL_MAP2_VERTEX_3, 0, 1, 6, 2, 0, 1, 3, 2, &control[0][0][0]);
    glGetMapiv(GL_MAP2_VERTEX_3, GL_ORDER, order);
    check(order[0] == 2 && order[1] == 2, "evaluator control map order");
    glEnable(GL_MAP2_VERTEX_3);
    glMapGrid2f(4, 0, 1, 4, 0, 1);
    glColor3f(1, 1, 0);
    glEvalMesh2(GL_FILL, 0, 4, 0, 4);
    pixel(25, 30, 255, 255, 0, 1, "evaluator mesh rasterizes surface interior");
    pixel(2, 2, 0, 0, 0, 0, "evaluator mesh respects control surface bounds");
    stop(c);
}

static void selection_and_feedback(void)
{
    OSMesaContext c;
    GLuint selection[16];
    GLfloat feedback[16];
    GLint count;
    c = start("selection and feedback");
    memset(selection, 0, sizeof(selection));
    glSelectBuffer(16, selection);
    glRenderMode(GL_SELECT);
    glInitNames();
    glPushName(42);
    rectangle(0);
    count = glRenderMode(GL_RENDER);
    check(count == 1, "selection reports one hit");
    check(selection[0] == 1 && selection[3] == 42,
          "selection records name stack");
    check(selection[1] <= selection[2] && selection[1] > 0,
          "selection records ordered depth interval");
    memset(feedback, 0, sizeof(feedback));
    glFeedbackBuffer(16, GL_2D, feedback);
    glRenderMode(GL_FEEDBACK);
    glPassThrough(7);
    glBegin(GL_POINTS);
    glVertex2f(0, 0);
    glEnd();
    count = glRenderMode(GL_RENDER);
    check(count == 5, "feedback contains passthrough token and point");
    check(feedback[0] == GL_PASS_THROUGH_TOKEN && feedback[1] == 7 &&
          feedback[2] == GL_POINT_TOKEN,
          "feedback tokens retain order");
    check(fabs(feedback[3] - 32) < 0.01 && fabs(feedback[4] - 32) < 0.01,
          "feedback uses transformed viewport coordinates");
    stop(c);
}

static void pixel_operations(void)
{
    OSMesaContext c;
    GLubyte data[16] = {255,0,0,255, 0,255,0,255,
                       0,0,255,255, 255,255,255,255};
    GLubyte bitmap[2] = {0xc0, 0xc0};
    c = start("pixel drawing, copying, bitmap and transfer");
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glRasterPos2f(-0.5f, -0.5f);
    glDrawPixels(2, 2, GL_RGBA, GL_UNSIGNED_BYTE, data);
    pixel(16, 16, 255, 0, 0, 1, "draw pixels lower-left orientation");
    pixel(16, 17, 0, 0, 255, 1, "draw pixels second row");
    glRasterPos2f(0, 0);
    glCopyPixels(16, 16, 2, 2, GL_COLOR);
    pixel(32, 32, 255, 0, 0, 1, "copy pixels reads source framebuffer");
    glColor3f(1, 1, 0);
    glRasterPos2f(0.5f, 0.5f);
    glBitmap(2, 2, 0, 0, 0, 0, bitmap);
    pixel(48, 48, 255, 255, 0, 1, "bitmap bits draw raster color");
    glPixelTransferf(GL_RED_SCALE, 0);
    glRasterPos2f(-0.75f, 0.5f);
    glDrawPixels(2, 2, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glPixelTransferf(GL_RED_SCALE, 1);
    pixel(8, 48, 0, 0, 0, 1, "pixel transfer scale affects drawing");
    stop(c);
}

static void lighting_and_fog(void)
{
    OSMesaContext c;
    GLfloat ambient[4] = {0,0,0,1};
    GLfloat diffuse[4] = {1,1,1,1};
    GLfloat position[4] = {0,0,1,0};
    GLfloat red[4] = {1,0,0,1};
    GLfloat blue[4] = {0,0,1,1};
    c = start("lighting and fog");
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_POSITION, position);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, red);
    glNormal3f(0, 0, 1);
    rectangle(0);
    pixel(32, 32, 255, 0, 0, 2, "diffuse lighting applies material and normal");
    glDisable(GL_LIGHTING);
    glEnable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogf(GL_FOG_START, 0);
    glFogf(GL_FOG_END, 1);
    glFogfv(GL_FOG_COLOR, blue);
    glColor3f(1, 0, 0);
    rectangle(-0.5f);
    pixel(32, 32, 128, 0, 128, 3, "linear fog blends using eye-space depth");
    stop(c);
}

static void sharing(void)
{
    OSMesaContext c;
    OSMesaContext shared;
    GLuint list;
    GLuint texture;
    GLuint otherbuffer[W * H];
    c = start("shared display lists and textures");
    list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    glColor3f(0, 1, 1);
    rectangle(0);
    glEndList();
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    shared = OSMesaCreateContext(OSMESA_RGBA, c);
    check(shared != NULL, "shared context creation");
    if (shared) {
        check(OSMesaMakeCurrent(shared, otherbuffer, GL_UNSIGNED_BYTE, W, H),
              "shared context binding");
        check(glIsList(list), "display list visible in sharing context");
        check(glIsTexture(texture), "texture visible in sharing context");
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        glCallList(list);
        pixel(32, 32, 0, 255, 255, 1, "shared list executes in second context");
        no_error();
        OSMesaDestroyContext(shared);
        check(OSMesaMakeCurrent(c, framebuffer, GL_UNSIGNED_BYTE, W, H),
              "original context can be rebound after shared context destruction");
        check(glIsList(list), "shared objects survive non-owner context destruction");
    }
    glDeleteLists(list, 1);
    glDeleteTextures(1, &texture);
    stop(c);
}

static void logic_and_stipple(void)
{
    OSMesaContext c;
    GLubyte stipple[128];
    GLubyte returned[128];
    c = start("logic operations, polygon stipple and write masks");
    glColor3ub(85, 170, 15);
    rectangle(0);
    glEnable(GL_COLOR_LOGIC_OP);
    glLogicOp(GL_XOR);
    glColor3ub(255, 15, 240);
    rectangle(0);
    pixel(32, 32, 170, 165, 255, 0, "color logic operation XORs source and destination");
    glDisable(GL_COLOR_LOGIC_OP);
    glClear(GL_COLOR_BUFFER_BIT);
    memset(stipple, 0, sizeof(stipple));
    glPolygonStipple(stipple);
    glGetPolygonStipple(returned);
    check(memcmp(stipple, returned, sizeof(stipple)) == 0,
          "polygon stipple pattern round trip");
    glEnable(GL_POLYGON_STIPPLE);
    glColor3f(1, 1, 1);
    rectangle(0);
    pixel(32, 32, 0, 0, 0, 0, "zero polygon stipple rejects all samples");
    memset(stipple, 255, sizeof(stipple));
    glPolygonStipple(stipple);
    rectangle(0);
    pixel(32, 32, 255, 255, 255, 1, "one polygon stipple accepts all samples");
    glDisable(GL_POLYGON_STIPPLE);
    glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE);
    glColor3f(0, 0, 0);
    rectangle(0);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    pixel(32, 32, 0, 255, 255, 1, "color write mask preserves masked channels");
    stop(c);
}

static void color_index(void)
{
    OSMesaContext c;
    GLubyte indices[W * H];
    GLubyte value;
    group = "color-index software context";
    ++groups;
    c = OSMesaCreateContext(OSMESA_COLOR_INDEX, NULL);
    check(c != NULL, "color-index context creation");
    if (!c)
        return;
    if (!OSMesaMakeCurrent(c, indices, GL_UNSIGNED_BYTE, W, H)) {
        check(0, "color-index context binding");
        OSMesaDestroyContext(c);
        return;
    }
    glClearIndex(5);
    glClear(GL_COLOR_BUFFER_BIT);
    glReadPixels(2, 2, 1, 1, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, &value);
    check(value == 5, "color-index clear and readback");
    glIndexi(17);
    rectangle(0);
    glReadPixels(32, 32, 1, 1, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, &value);
    check(value == 17, "color-index primitive rasterization");
    stop(c);
    c = OSMesaCreateContextExt(OSMESA_COLOR_INDEX, 16, 8, 16, NULL);
    check(c != NULL, "color-index extended context ignores inapplicable accumulation request");
    if (c) {
        check(OSMesaMakeCurrent(c, indices, GL_UNSIGNED_BYTE, W, H),
              "color-index extended context can be bound");
        glClearIndex(9);
        glClear(GL_COLOR_BUFFER_BIT);
        glReadPixels(2, 2, 1, 1, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, &value);
        check(value == 9, "color-index extended context renders after ancillary buffer selection");
        stop(c);
    }
}

int main(void)
{
    state_and_errors();
    geometry();
    fragment_tests();
    accumulation();
    textures();
    lists_and_arrays();
    evaluators();
    selection_and_feedback();
    pixel_operations();
    lighting_and_fog();
    sharing();
    logic_and_stipple();
    color_index();
    printf("%s: %d checks in %d groups, %d failures\n",
           failures ? "FAIL" : "PASS", checks, groups, failures);
    return failures ? 1 : 0;
}
