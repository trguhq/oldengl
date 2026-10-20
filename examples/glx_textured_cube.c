/* MIT licensed. OpenGL 1.1 / GLX textured and lit rotating cube.
 * Run with --frames N for a bounded smoke test, or press any key to exit.
 * No GLU, GLUT, or platform-specific OpenGL extensions are required.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <poll.h>
#include <sys/time.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/gl.h>
#include <GL/glx.h>

#define TEXTURE_SIZE 64

static const GLfloat corners[6][4][3] = {
    { {-1,-1, 1}, { 1,-1, 1}, { 1, 1, 1}, {-1, 1, 1} },
    { { 1,-1,-1}, {-1,-1,-1}, {-1, 1,-1}, { 1, 1,-1} },
    { { 1,-1, 1}, { 1,-1,-1}, { 1, 1,-1}, { 1, 1, 1} },
    { {-1,-1,-1}, {-1,-1, 1}, {-1, 1, 1}, {-1, 1,-1} },
    { {-1, 1, 1}, { 1, 1, 1}, { 1, 1,-1}, {-1, 1,-1} },
    { {-1,-1,-1}, { 1,-1,-1}, { 1,-1, 1}, {-1,-1, 1} }
};
static const GLfloat normals[6][3] = {
    {0,0,1}, {0,0,-1}, {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}
};
static const GLfloat texcoords[4][2] = {
    {0,0}, {2,0}, {2,2}, {0,2}
};
static const unsigned int indices[6] = { 0, 1, 2, 0, 2, 3 };

static double seconds_now(void)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    return (double)now.tv_sec + (double)now.tv_usec / 1000000.0;
}

static void resize_projection(int width, int height)
{
    GLdouble aspect;
    if (width < 1) width = 1;
    if (height < 1) height = 1;
    aspect = (GLdouble)width / (GLdouble)height;
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-0.6 * aspect, 0.6 * aspect, -0.6, 0.6, 1.0, 20.0);
    glMatrixMode(GL_MODELVIEW);
}

static GLuint initialize_scene(void)
{
    GLubyte checker[TEXTURE_SIZE][TEXTURE_SIZE][3];
    const GLfloat ambient_light[4] = {0.2f, 0.2f, 0.2f, 1.0f};
    const GLfloat no_light[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    const GLfloat diffuse_light[4] = {0.9f, 0.9f, 0.9f, 1.0f};
    const GLfloat direction[4] = {2.0f, 3.0f, 4.0f, 0.0f};
    const GLfloat material_ambient[4] = {0.3f, 0.3f, 0.3f, 1.0f};
    const GLfloat material_diffuse[4] = {0.9f, 0.9f, 0.9f, 1.0f};
    GLuint texture;
    int x, y, bright;

    for (y = 0; y < TEXTURE_SIZE; ++y)
        for (x = 0; x < TEXTURE_SIZE; ++x) {
            bright = ((x / 8) ^ (y / 8)) & 1;
            checker[y][x][0] = (GLubyte)(bright ? 240 : 35);
            checker[y][x][1] = (GLubyte)(bright ? 200 : 55);
            checker[y][x][2] = (GLubyte)(bright ? 80 : 145);
        }
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, TEXTURE_SIZE, TEXTURE_SIZE, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, checker);
    glEnable(GL_TEXTURE_2D);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient_light);
    glLightfv(GL_LIGHT0, GL_AMBIENT, no_light);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse_light);
    glLightfv(GL_LIGHT0, GL_SPECULAR, no_light);
    glLightfv(GL_LIGHT0, GL_POSITION, direction);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, material_ambient);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, material_diffuse);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, no_light);
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, no_light);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHTING);
    glEnable(GL_NORMALIZE);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glClearColor(0.035f, 0.045f, 0.075f, 1.0f);
    return texture;
}

static void render_cube(double elapsed)
{
    unsigned int face, vertex, corner;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, -4.8f);
    glRotatef((GLfloat)fmod(25.0 + elapsed * 23.0, 360.0), 1.0f, 0.0f, 0.0f);
    glRotatef((GLfloat)fmod(35.0 + elapsed * 37.0, 360.0), 0.0f, 1.0f, 0.0f);
    glBegin(GL_TRIANGLES);
    for (face = 0; face < 6; ++face) {
        glNormal3fv(normals[face]);
        for (vertex = 0; vertex < 6; ++vertex) {
            corner = indices[vertex];
            glTexCoord2fv(texcoords[corner]);
            glVertex3fv(corners[face][corner]);
        }
    }
    glEnd();
}

int main(int argc, char **argv)
{
    int attributes[] = {
        GLX_RGBA, GLX_DOUBLEBUFFER,
        GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8,
        GLX_ALPHA_SIZE, 0, GLX_DEPTH_SIZE, 24, None
    };
    Display *display = NULL;
    XVisualInfo *visual = NULL;
    XSetWindowAttributes settings;
    Window window = None;
    Colormap colormap = None;
    GLXContext context = NULL;
    GLuint texture = 0;
    Atom close_window;
    XEvent event;
    struct pollfd connection;
    double start_time;
    long frame_limit = 0, frames = 0;
    char *end;
    GLint depth_bits, alpha_bits;
    int current = 0, done = 0, mapped = 0, status = 1;

    if (argc == 3 && strcmp(argv[1], "--frames") == 0) {
        frame_limit = strtol(argv[2], &end, 10);
        if (!*argv[2] || *end || frame_limit < 1) {
            fprintf(stderr, "--frames requires a positive integer\n");
            return 1;
        }
    } else if (argc != 1) {
        fprintf(stderr, "Usage: %s [--frames N]\n", argv[0]);
        return 1;
    }
    display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Cannot open X display\n");
        goto cleanup;
    }
    visual = glXChooseVisual(display, DefaultScreen(display), attributes);
    if (!visual) {
        fprintf(stderr, "No double-buffered RGB visual with 24-bit depth\n");
        goto cleanup;
    }
    colormap = XCreateColormap(display, RootWindow(display, visual->screen),
                              visual->visual, AllocNone);
    memset(&settings, 0, sizeof(settings));
    settings.colormap = colormap;
    settings.event_mask = ExposureMask | StructureNotifyMask | KeyPressMask;
    window = XCreateWindow(display, RootWindow(display, visual->screen),
        0, 0, 640, 480, 0, visual->depth, InputOutput, visual->visual,
        CWColormap | CWBorderPixel | CWEventMask, &settings);
    XStoreName(display, window, "OldenGL GLX: textured cube; press any key to exit");
    close_window = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display, window, &close_window, 1);
    context = glXCreateContext(display, visual, NULL, True);
    if (!context || !glXMakeCurrent(display, window, context)) {
        fprintf(stderr, "Cannot create or bind GLX context\n");
        goto cleanup;
    }
    current = 1;
    XMapWindow(display, window);
    XFlush(display);
    glDrawBuffer(GL_BACK);
    texture = initialize_scene();
    resize_projection(640, 480);
    if (!texture || glGetError() != GL_NO_ERROR) {
        fprintf(stderr, "OpenGL texture or scene initialization failed\n");
        goto cleanup;
    }
    glGetIntegerv(GL_DEPTH_BITS, &depth_bits);
    glGetIntegerv(GL_ALPHA_BITS, &alpha_bits);
    printf("Renderer: %s; depth=%d, alpha=%d\n",
           (const char *)glGetString(GL_RENDERER), depth_bits, alpha_bits);
    connection.fd = ConnectionNumber(display);
    connection.events = POLLIN;
    start_time = seconds_now();
    while (!done) {
        while (XPending(display)) {
            XNextEvent(display, &event);
            if (event.type == ConfigureNotify)
                resize_projection(event.xconfigure.width, event.xconfigure.height);
            else if (event.type == MapNotify) mapped = 1;
            else if (event.type == UnmapNotify) mapped = 0;
            else if (event.type == KeyPress ||
                (event.type == ClientMessage &&
                 (Atom)event.xclient.data.l[0] == close_window))
                done = 1;
            else if (event.type == DestroyNotify) { window = None; done = 1; }
        }
        if (!done && mapped) {
            render_cube(seconds_now() - start_time);
            glXSwapBuffers(display, window);
            ++frames;
            if (frame_limit && frames >= frame_limit) done = 1;
        }
        if (!done) (void)poll(&connection, 1, mapped ? 16 : 100);
    }
    status = 0;

cleanup:
    if (current) {
        if (texture) glDeleteTextures(1, &texture);
        glXMakeCurrent(display, None, NULL);
    }
    if (context) glXDestroyContext(display, context);
    if (window) XDestroyWindow(display, window);
    if (colormap) XFreeColormap(display, colormap);
    if (visual) XFree(visual);
    if (display) XCloseDisplay(display);
    return status;
}
