/* MIT licensed. Minimal Xlib + XGLMesa application for Solaris 2.6. */
#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/xglmesa.h>

static void draw(void)
{
    glClearColor(0.05f, 0.05f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glBegin(GL_TRIANGLES);
    glColor3f(1.0f, 0.0f, 0.0f); glVertex2f(-0.8f, -0.8f);
    glColor3f(0.0f, 1.0f, 0.0f); glVertex2f( 0.8f, -0.8f);
    glColor3f(0.0f, 0.0f, 1.0f); glVertex2f( 0.0f,  0.8f);
    glEnd();
    glFlush();
}

int main(void)
{
    Display *display;
    int screen, done;
    XVisualInfo visual;
    XSetWindowAttributes attributes;
    Colormap colormap;
    Window window;
    Atom close_window;
    XEvent event;
    XGLMesaContext context;

    display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Cannot open X display\n");
        return 1;
    }
    screen = DefaultScreen(display);
    if (!XMatchVisualInfo(display, screen, 24, TrueColor, &visual)) {
        fprintf(stderr, "This example requires a 24-bit TrueColor visual\n");
        XCloseDisplay(display);
        return 1;
    }
    colormap = XCreateColormap(display, RootWindow(display, screen),
                              visual.visual, AllocNone);
    attributes.colormap = colormap;
    attributes.border_pixel = 0;
    attributes.event_mask = ExposureMask | StructureNotifyMask | KeyPressMask;
    window = XCreateWindow(display, RootWindow(display, screen), 0, 0, 512, 512,
        0, visual.depth, InputOutput, visual.visual,
        CWColormap | CWBorderPixel | CWEventMask, &attributes);
    XStoreName(display, window, "OldenGL: OpenGL software rendering through XGL");
    close_window = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display, window, &close_window, 1);
    XMapWindow(display, window);
    XSync(display, False);
    context = XGLMesaCreateContext(display, window, NULL);
    if (!context || !XGLMesaMakeCurrent(context)) {
        fprintf(stderr, "%s\n", XGLMesaGetLastError());
        if (context)
            XGLMesaDestroyContext(context);
        XDestroyWindow(display, window);
        XFreeColormap(display, colormap);
        XCloseDisplay(display);
        return 1;
    }
    printf("Renderer: %s\n", (const char *)glGetString(GL_RENDERER));
    done = 0;
    while (!done) {
        XNextEvent(display, &event);
        if (event.type == Expose && event.xexpose.count == 0)
            draw();
        else if (event.type == ConfigureNotify) {
            if (!XGLMesaResizeBuffers(context, event.xconfigure.width,
                                     event.xconfigure.height)) {
                fprintf(stderr, "%s\n", XGLMesaGetLastError());
                done = 1;
            } else {
                glViewport(0, 0, event.xconfigure.width, event.xconfigure.height);
                draw();
            }
        } else if (event.type == KeyPress ||
            (event.type == ClientMessage &&
             (Atom)event.xclient.data.l[0] == close_window)) {
            done = 1;
        }
    }
    XGLMesaDestroyContext(context);
    XDestroyWindow(display, window);
    XFreeColormap(display, colormap);
    XCloseDisplay(display);
    return 0;
}
