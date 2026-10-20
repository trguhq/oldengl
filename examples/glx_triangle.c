/* MIT licensed. Conventional GLX window; native-eligible GL_FRONT drawing. */
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/gl.h>
#include <GL/glx.h>

int main(void)
{
    int attributes[] = { GLX_RGBA, GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8,
                         GLX_BLUE_SIZE, 8, None };
    Display *display = XOpenDisplay(NULL);
    XVisualInfo *visual;
    XSetWindowAttributes settings;
    Colormap colormap;
    Window window;
    GLXContext context;
    XEvent event;
    int done = 0;
    if (!display) return 1;
    visual = glXChooseVisual(display, DefaultScreen(display), attributes);
    if (!visual) { XCloseDisplay(display); return 1; }
    colormap = XCreateColormap(display, RootWindow(display, visual->screen),
                              visual->visual, AllocNone);
    settings.colormap = colormap;
    settings.border_pixel = 0;
    settings.event_mask = ExposureMask | StructureNotifyMask | KeyPressMask;
    window = XCreateWindow(display, RootWindow(display, visual->screen),
        0, 0, 480, 480, 0, visual->depth, InputOutput, visual->visual,
        CWColormap | CWBorderPixel | CWEventMask, &settings);
    XStoreName(display, window, "OldenGL GLX: press a key to exit");
    context = glXCreateContext(display, visual, NULL, True);
    XMapWindow(display, window);
    XSync(display, False);
    if (!context || !glXMakeCurrent(display, window, context)) {
        if (context) glXDestroyContext(display, context);
        XDestroyWindow(display, window); XFreeColormap(display, colormap);
        XFree(visual); XCloseDisplay(display); return 1;
    }
    printf("GL renderer: %s\n", (const char *)glGetString(GL_RENDERER));
    while (!done) {
        XNextEvent(display, &event);
        if (event.type == ConfigureNotify)
            glViewport(0, 0, event.xconfigure.width, event.xconfigure.height);
        if (event.type == Expose || event.type == ConfigureNotify) {
            glClearColor(0.05f, 0.05f, 0.12f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            glBegin(GL_TRIANGLES);
            glColor3f(1, 0, 0); glVertex2f(-0.8f, -0.8f);
            glColor3f(0, 1, 0); glVertex2f(0.8f, -0.8f);
            glColor3f(0, 0, 1); glVertex2f(0, 0.8f);
            glEnd();
            glFlush();
        }
        if (event.type == KeyPress) done = 1;
    }
    glXMakeCurrent(display, None, NULL);
    glXDestroyContext(display, context);
    XDestroyWindow(display, window);
    XFreeColormap(display, colormap);
    XFree(visual);
    XCloseDisplay(display);
    return 0;
}
