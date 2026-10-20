/* MIT licensed. Real Xlib GLX integration tests; no native XGL SDK required. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/gl.h>
#include <GL/glx.h>

static unsigned int checks, failures, skips, xerrors;
static int expected_xerror;
static const char *stage = "initialization";

static void check(int condition, const char *name)
{
    ++checks;
    if (!condition) {
        ++failures;
        fprintf(stderr, "FAIL [%s]: %s\n", stage, name);
    }
}

static int xerror(Display *display, XErrorEvent *event)
{
    char message[160];
    XGetErrorText(display, event->error_code, message, sizeof(message));
    fprintf(stderr, "X error [%s]: %s (request %u, resource %lu)\n", stage,
            message, (unsigned int)event->request_code, event->resourceid);
    if (!expected_xerror)
        ++xerrors;
    return 0;
}

static Window window_for(Display *display, XVisualInfo *visual,
                         Colormap *colormap, int position)
{
    XSetWindowAttributes attributes;
    Window window;
    *colormap = XCreateColormap(display, RootWindow(display, visual->screen),
                               visual->visual, AllocNone);
    attributes.colormap = *colormap;
    attributes.border_pixel = 0;
    attributes.event_mask = StructureNotifyMask | ExposureMask;
    window = XCreateWindow(display, RootWindow(display, visual->screen),
        position, 20, 96, 96, 0, visual->depth, InputOutput, visual->visual,
        CWColormap | CWBorderPixel | CWEventMask, &attributes);
    XMapWindow(display, window);
    XSync(display, False);
    return window;
}

static void read_color(int red, int green, int blue, const char *name)
{
    GLubyte pixel[4];
    glReadPixels(17, 23, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    check(abs((int)pixel[0] - red) <= 1 &&
          abs((int)pixel[1] - green) <= 1 &&
          abs((int)pixel[2] - blue) <= 1, name);
}

static void clear_color(float red, float green, float blue)
{
    glClearColor(red, green, blue, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

static int image_component(unsigned long pixel, unsigned long mask)
{
    unsigned int shift = 0;
    unsigned long maximum;
    if (!mask)
        return 0;
    while (!(mask & 1UL)) {
        mask >>= 1;
        ++shift;
    }
    maximum = mask;
    return (int)((((pixel >> shift) & maximum) * 255UL + maximum / 2) / maximum);
}

static void window_color(Display *display, Window window, XVisualInfo *visual,
                          int red, int green, int blue, const char *name)
{
    XImage *image;
    unsigned long pixel;
    XSync(display, False);
    image = XGetImage(display, window, 17, 96 - 24, 1, 1, AllPlanes, ZPixmap);
    check(image != NULL, "XGetImage returns actual window pixels");
    if (!image)
        return;
    pixel = XGetPixel(image, 0, 0);
    check(abs(image_component(pixel, visual->red_mask) - red) <= 1 &&
          abs(image_component(pixel, visual->green_mask) - green) <= 1 &&
          abs(image_component(pixel, visual->blue_mask) - blue) <= 1, name);
    XDestroyImage(image);
}

static void gl_clean(const char *name)
{
    GLenum error = glGetError();
    if (error != GL_NO_ERROR)
        fprintf(stderr, "GL error 0x%x during %s\n", (unsigned int)error, name);
    check(error == GL_NO_ERROR, name);
}

static void test_font(Display *display)
{
    XFontStruct *font;
    char **names;
    int count;
    GLuint base;
    GLfloat before[4], after[4];
    names = XListFonts(display, "fixed", 1, &count);
    if (!names || count < 1) {
        ++skips;
        if (names)
            XFreeFontNames(names);
        fprintf(stderr, "SKIP: X server has no fixed bitmap font\n");
        return;
    }
    font = XLoadQueryFont(display, names[0]);
    XFreeFontNames(names);
    check(font != NULL, "load X font for glXUseXFont");
    if (!font)
        return;
    base = glGenLists(1);
    glXUseXFont(font->fid, 65, 1, (int)base);
    check(glIsList(base), "glXUseXFont creates a bitmap display list");
    glRasterPos2f(0.0f, 0.0f);
    glGetFloatv(GL_CURRENT_RASTER_POSITION, before);
    glCallList(base);
    glGetFloatv(GL_CURRENT_RASTER_POSITION, after);
    check(after[0] > before[0], "X font bitmap advances raster position");
    glDeleteLists(base, 1);
    XFreeFont(display, font);
    gl_clean("X font display list executes without GL errors");
}

int main(void)
{
    Display *display;
    int screen, major, minor, error_base, event_base, value, count, i;
    int legacy_attributes[] = { GLX_RGBA, GLX_DOUBLEBUFFER,
        GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8,
        GLX_DEPTH_SIZE, 16, None };
    int config_attributes[] = { GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT | GLX_PIXMAP_BIT | GLX_PBUFFER_BIT,
        GLX_DOUBLEBUFFER, True, GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8, GLX_DEPTH_SIZE, 16, None };
    int single_attributes[] = { GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_DRAWABLE_TYPE, GLX_PBUFFER_BIT | GLX_PIXMAP_BIT,
        GLX_DOUBLEBUFFER, False, GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8, None };
    int pbuffer_attributes[] = { GLX_PBUFFER_WIDTH, 64,
        GLX_PBUFFER_HEIGHT, 48, GLX_PRESERVED_CONTENTS, True,
        GLX_LARGEST_PBUFFER, False, None };
    GLXFBConfig *all_configs, *configs, *single_configs, config, single_config;
    GLXContext first, second, legacy, pcontext, copied, doomed;
    GLXWindow glx_window;
    GLXPbuffer pbuffer;
    GLXPixmap glx_pixmap, legacy_pixmap;
    Pixmap pixmap;
    Window first_window, second_window;
    Colormap first_map, second_map;
    XVisualInfo *visual, *legacy_visual, *pixmap_visual;
    unsigned int width, height;
    unsigned long selected;
    GLuint texture, list;
    GLfloat color[4];
    void (*proc)(void);
    typedef Bool (*query_version_proc)(Display *, int *, int *);
    query_version_proc query_version;
    const char *extensions;

    display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "FAIL: Cannot open X display (set DISPLAY or run under Xvfb)\n");
        return 1;
    }
    XSetErrorHandler(xerror);
    screen = DefaultScreen(display);
    error_base = event_base = -1;
    check(glXQueryExtension(display, &error_base, &event_base), "GLX binding available");
    check(glXQueryVersion(display, &major, &minor) &&
          (major > 1 || (major == 1 && minor >= 4)), "GLX version is at least 1.4");
    check(glXGetCurrentContext() == NULL && glXGetCurrentDrawable() == None &&
          glXGetCurrentReadDrawable() == None && glXGetCurrentDisplay() == NULL,
          "initial current binding is empty");
    check(glXGetClientString(display, GLX_VENDOR) != NULL, "client vendor string");
    check(glXQueryServerString(display, screen, GLX_VERSION) != NULL,
          "server compatibility version string");
    extensions = glXQueryExtensionsString(display, screen);
    check(extensions != NULL, "extension string query");
    proc = glXGetProcAddress((const GLubyte *)"glXQueryVersion");
    check(proc != NULL, "GLX function proc-address lookup");
    query_version = (query_version_proc)proc;
    check(query_version && query_version(display, &major, &minor),
          "resolved GLX entry point is callable");
    check(glXGetProcAddress((const GLubyte *)"glClearColor") != NULL,
          "GL function proc-address lookup");

    stage = "FBConfig selection";
    all_configs = glXGetFBConfigs(display, screen, &count);
    check(all_configs != NULL && count > 0, "enumerate framebuffer configurations");
    if (all_configs) {
        for (i = 0; i < count && i < 8; ++i) {
            value = 0;
            check(glXGetFBConfigAttrib(display, all_configs[i], GLX_FBCONFIG_ID,
                                       &value) == Success && value != 0,
                  "enumerated configuration has stable nonzero ID");
        }
        XFree(all_configs);
    }
    configs = glXChooseFBConfig(display, screen, config_attributes, &count);
    check(configs != NULL && count > 0, "choose RGB double-buffer window/pixmap/pbuffer config");
    if (!configs || count < 1)
        goto no_config;
    config = configs[0];
    value = 0;
    check(glXGetFBConfigAttrib(display, config, GLX_DOUBLEBUFFER, &value) == Success && value,
          "double-buffer filter honored");
    value = 0;
    check(glXGetFBConfigAttrib(display, config, GLX_DEPTH_SIZE, &value) == Success && value >= 16,
          "minimum depth filter honored");
    value = 0;
    check(glXGetFBConfigAttrib(display, config, GLX_DRAWABLE_TYPE, &value) == Success &&
          (value & (GLX_WINDOW_BIT | GLX_PIXMAP_BIT | GLX_PBUFFER_BIT)) ==
          (GLX_WINDOW_BIT | GLX_PIXMAP_BIT | GLX_PBUFFER_BIT),
          "all requested drawable types supported");
    visual = glXGetVisualFromFBConfig(display, config);
    check(visual != NULL, "derive X visual from framebuffer config");
    XFree(configs);
    if (!visual)
        goto no_config;

    stage = "context and window rendering";
    first_window = window_for(display, visual, &first_map, 20);
    second_window = window_for(display, visual, &second_map, 160);
    glx_window = glXCreateWindow(display, config, first_window, NULL);
    check(glx_window != None, "create GLXWindow wrapper");
    first = glXCreateNewContext(display, config, GLX_RGBA_TYPE, NULL, True);
    check(first != NULL, "create FBConfig-based context");
    if (!first || !glx_window)
        goto close_windows;
    check(glXIsDirect(display, first), "client-side renderer reports direct context");
    value = -1;
    check(glXQueryContext(display, first, GLX_SCREEN, &value) == Success && value == screen,
          "context screen metadata");
    value = 0;
    check(glXQueryContext(display, first, GLX_RENDER_TYPE, &value) == Success &&
          value == GLX_RGBA_TYPE, "context render-type metadata");
    check(glXMakeCurrent(display, glx_window, first), "make GLX window context current");
    check(glXGetCurrentContext() == first && glXGetCurrentDrawable() == glx_window &&
          glXGetCurrentReadDrawable() == glx_window && glXGetCurrentDisplay() == display,
          "current context, draw, read and display queries");
    glViewport(0, 0, 96, 96);
    glDrawBuffer(GL_FRONT);
    glReadBuffer(GL_FRONT);
    clear_color(1.0f, 0.0f, 0.0f);
    glFinish();
    read_color(255, 0, 0, "front clear is readable");
    window_color(display, first_window, visual, 255, 0, 0,
                 "front rendering reaches the actual X window");
    glDrawBuffer(GL_BACK);
    glReadBuffer(GL_BACK);
    clear_color(0.0f, 1.0f, 0.0f);
    read_color(0, 255, 0, "back clear is readable before swap");
    window_color(display, first_window, visual, 255, 0, 0,
                 "back rendering leaves the front window unchanged");
    glXSwapBuffers(display, glx_window);
    window_color(display, first_window, visual, 0, 255, 0,
                 "swap presents the back buffer");
    glXWaitGL();
    glXWaitX();
    gl_clean("double-buffer rendering has no GL errors");

    stage = "sharing and split draw/read";
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    glColor3f(0.25f, 0.5f, 0.75f);
    glEndList();
    second = glXCreateNewContext(display, config, GLX_RGBA_TYPE, first, True);
    check(second != NULL, "create sharing context");
    if (second) {
        check(glXMakeCurrent(display, second_window, second), "bind second raw X window");
        check(glIsTexture(texture) && glIsList(list), "textures and display lists shared");
        glViewport(0, 0, 96, 96);
        glDrawBuffer(GL_FRONT);
        glReadBuffer(GL_FRONT);
        clear_color(0.0f, 0.0f, 1.0f);
        glFinish();
        check(glXMakeContextCurrent(display, glx_window, second_window, first),
              "bind separate draw and read drawables");
        check(glXGetCurrentDrawable() == glx_window &&
              glXGetCurrentReadDrawable() == second_window,
              "split drawable queries preserve supplied handles");
        glReadBuffer(GL_FRONT);
        read_color(0, 0, 255, "glReadPixels uses separate read drawable");
        glDrawBuffer(GL_FRONT);
        clear_color(1.0f, 1.0f, 0.0f);
        glFinish();
        window_color(display, first_window, visual, 255, 255, 0,
                     "split context writes the draw drawable");
        window_color(display, second_window, visual, 0, 0, 255,
                     "split context preserves read drawable");
        check(glXMakeCurrent(display, glx_window, first), "restore combined draw/read");
        glXDestroyContext(display, second);
        check(glIsTexture(texture) && glIsList(list), "shared resources survive peer destruction");
    }
    glDeleteLists(list, 1);
    glDeleteTextures(1, &texture);

    stage = "context copy and X fonts";
    glColor4f(0.125f, 0.375f, 0.625f, 1.0f);
    copied = glXCreateNewContext(display, config, GLX_RGBA_TYPE, NULL, True);
    check(copied != NULL, "create destination for context state copy");
    if (copied) {
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_FOG);
        glEnable(GL_LIGHT0);
        glXCopyContext(display, first, copied, GL_CURRENT_BIT | GL_ENABLE_BIT);
        check(glXMakeCurrent(display, second_window, copied), "bind copied context");
        glGetFloatv(GL_CURRENT_COLOR, color);
        check(fabs(color[0] - 0.125) < 0.01 && fabs(color[1] - 0.375) < 0.01 &&
              fabs(color[2] - 0.625) < 0.01, "selected context state copied");
        check(glIsEnabled(GL_DEPTH_TEST) && glIsEnabled(GL_FOG) && glIsEnabled(GL_LIGHT0),
              "enable group copied independently");
        check(glXMakeCurrent(display, glx_window, first), "restore source context");
        glXDestroyContext(display, copied);
    }
    test_font(display);

    stage = "pbuffer and pixmap rendering";
    single_configs = glXChooseFBConfig(display, screen, single_attributes, &count);
    check(single_configs != NULL && count > 0, "choose single-buffer offscreen config");
    if (single_configs && count > 0) {
        single_config = single_configs[0];
        value = 1;
        check(glXGetFBConfigAttrib(display, single_config, GLX_DOUBLEBUFFER, &value) == Success &&
              !value, "single-buffer filter honored");
        pcontext = glXCreateNewContext(display, single_config, GLX_RGBA_TYPE, NULL, True);
        pbuffer = glXCreatePbuffer(display, single_config, pbuffer_attributes);
        check(pcontext != NULL && pbuffer != None, "create offscreen context and pbuffer");
        if (pcontext && pbuffer) {
            width = height = 0;
            glXQueryDrawable(display, pbuffer, GLX_WIDTH, &width);
            glXQueryDrawable(display, pbuffer, GLX_HEIGHT, &height);
            check(width == 64 && height == 48, "pbuffer dimensions reported");
            glXSelectEvent(display, pbuffer, GLX_PBUFFER_CLOBBER_MASK);
            selected = 0;
            glXGetSelectedEvent(display, pbuffer, &selected);
            check(selected == GLX_PBUFFER_CLOBBER_MASK, "drawable event mask round trip");
            check(glXMakeCurrent(display, pbuffer, pcontext), "make pbuffer current");
            glViewport(0, 0, 64, 48);
            glDrawBuffer(GL_FRONT);
            glReadBuffer(GL_FRONT);
            clear_color(1.0f, 0.0f, 1.0f);
            read_color(255, 0, 255, "pbuffer rendering and readback");
            gl_clean("pbuffer operations have no GL errors");
            check(glXMakeCurrent(display, glx_window, first), "release pbuffer binding");
            glXDestroyPbuffer(display, pbuffer);
        }
        if (pcontext)
            glXDestroyContext(display, pcontext);
        pixmap_visual = glXGetVisualFromFBConfig(display, single_config);
        check(pixmap_visual != NULL, "get pixmap config visual");
        if (pixmap_visual) {
            pixmap = XCreatePixmap(display, RootWindow(display, screen), 64, 48,
                                   (unsigned int)pixmap_visual->depth);
            glx_pixmap = glXCreatePixmap(display, single_config, pixmap, NULL);
            pcontext = glXCreateNewContext(display, single_config, GLX_RGBA_TYPE, NULL, True);
            check(glx_pixmap != None && pcontext != NULL, "create GLX 1.3 pixmap and context");
            if (glx_pixmap && pcontext) {
                check(glXMakeCurrent(display, glx_pixmap, pcontext), "make GLX pixmap current");
                glViewport(0, 0, 64, 48);
                clear_color(0.0f, 1.0f, 1.0f);
                read_color(0, 255, 255, "GLX pixmap rendering and readback");
                check(glXMakeCurrent(display, glx_window, first), "release pixmap binding");
            }
            if (pcontext)
                glXDestroyContext(display, pcontext);
            if (glx_pixmap)
                glXDestroyPixmap(display, glx_pixmap);
            /* Destroying a GLX wrapper must not destroy the caller's pixmap. */
            XFreePixmap(display, pixmap);
            XFree(pixmap_visual);
        }
        XFree(single_configs);
    }

    stage = "legacy visual/context/pixmap API";
    legacy_visual = glXChooseVisual(display, screen, legacy_attributes);
    check(legacy_visual != NULL, "legacy visual selection");
    if (legacy_visual) {
        value = 0;
        check(glXGetConfig(display, legacy_visual, GLX_USE_GL, &value) == Success && value,
              "legacy visual GL capability query");
        value = 0;
        check(glXGetConfig(display, legacy_visual, GLX_DOUBLEBUFFER, &value) == Success && value,
              "legacy doublebuffer selection and query");
        legacy = glXCreateContext(display, legacy_visual, NULL, True);
        check(legacy != NULL, "legacy context creation");
        pixmap = XCreatePixmap(display, RootWindow(display, screen), 64, 48,
                               (unsigned int)legacy_visual->depth);
        legacy_pixmap = glXCreateGLXPixmap(display, legacy_visual, pixmap);
        check(legacy_pixmap != None, "legacy GLX pixmap creation");
        if (legacy && legacy_pixmap) {
            check(glXMakeCurrent(display, legacy_pixmap, legacy), "legacy pixmap binding");
            glViewport(0, 0, 64, 48);
            glDrawBuffer(GL_FRONT);
            glReadBuffer(GL_FRONT);
            clear_color(0.5f, 0.25f, 0.75f);
            read_color(128, 64, 191, "legacy context and pixmap render pixels");
            check(glXMakeCurrent(display, glx_window, first), "release legacy binding");
        }
        if (legacy)
            glXDestroyContext(display, legacy);
        if (legacy_pixmap)
            glXDestroyGLXPixmap(display, legacy_pixmap);
        XFreePixmap(display, pixmap);
        XFree(legacy_visual);
    }

    stage = "lifecycle and invalid binding";
    expected_xerror = 1;
    check(!glXMakeCurrent(display, None, first), "non-null context requires a drawable");
    XSync(display, False);
    expected_xerror = 0;
    check(glXGetCurrentContext() == first, "failed binding preserves previous context");
    doomed = glXCreateNewContext(display, config, GLX_RGBA_TYPE, NULL, True);
    check(doomed != NULL, "create deferred-destruction context");
    if (doomed) {
        check(glXMakeCurrent(display, second_window, doomed), "bind deferred-destruction context");
        glXDestroyContext(display, doomed);
        check(glXGetCurrentContext() == doomed,
              "destroying current context defers deletion until unbind");
        glDrawBuffer(GL_FRONT);
        glReadBuffer(GL_FRONT);
        clear_color(0.0f, 0.0f, 0.0f);
        read_color(0, 0, 0, "destroy-pending current context still renders");
    }
    check(glXMakeCurrent(display, None, NULL), "release all current state");
    check(glXGetCurrentContext() == NULL && glXGetCurrentDrawable() == None &&
          glXGetCurrentReadDrawable() == None && glXGetCurrentDisplay() == NULL,
          "current queries reset after release");
    glXDestroyContext(display, first);
    glXDestroyWindow(display, glx_window);

close_windows:
    XDestroyWindow(display, first_window);
    XDestroyWindow(display, second_window);
    XFreeColormap(display, first_map);
    XFreeColormap(display, second_map);
    XFree(visual);
no_config:
    stage = "final synchronization";
    XSync(display, False);
    check(xerrors == 0, "no unexpected X protocol errors");
    XCloseDisplay(display);
    printf("GLX real-X11 integration: %u checks, %u failures, %u skipped\n",
           checks, failures, skips);
    return failures ? 1 : 0;
}
