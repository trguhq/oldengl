/*
 * MIT licensed. See LICENSE.
 * XGL binding: Mesa performs GL operations; XGL presents RGB memory rasters.
 * The internal Mesa driver interface is pinned to the bundled Mesa version.
 */
#include <GL/xglmesa.h>
#include <GL/osmesa.h>
#include <X11/Xutil.h>
#include <xgl/xgl.h>
#include "context.h"
#include "mtypes.h"
#include "config.h"
#include "xgl_pixels.h"
#include "xgl_session.h"
#include <stdlib.h>
#include <string.h>

struct xglmesa_context {
    Display *display;
    Window window;
    OSMesaContext mesa;
    Xgl_object window_raster;
    Xgl_object copy_context;
    Xgl_object memory_raster;
    Xgl_usgn32 *memory;
    GLubyte *rgba;
    GLsizei width, height;
    unsigned int red_shift, green_shift, blue_shift;
    GLboolean presented;
    struct xglmesa_context *next;
};

static XGLMesaContext contexts;

static void set_error(const char *message)
{
    oldengl_xgl_set_error(message);
}

static int acquire_system(void)
{
    return oldengl_xgl_acquire() != 0;
}

static void release_system(void)
{
    oldengl_xgl_release();
}

static XGLMesaContext
find_context(OSMesaContext mesa)
{
    XGLMesaContext context;
    for (context = contexts; context; context = context->next)
        if (context->mesa == mesa)
            return context;
    return 0;
}

static int
valid_context(XGLMesaContext wanted)
{
    XGLMesaContext context;
    for (context = contexts; context; context = context->next)
        if (context == wanted)
            return 1;
    set_error("Invalid XGLMesa context");
    return 0;
}

static int
outside_begin_end(void)
{
    GLcontext *core = _mesa_get_current_context();
    if (core && core->Driver.CurrentExecPrimitive != PRIM_OUTSIDE_BEGIN_END) {
        set_error("Window-system operation inside glBegin/glEnd");
        return 0;
    }
    return 1;
}

/* Resolve the manuals' inconsistent prose about RGB word layout using XGL. */
static int
calibrate(Xgl_object raster, Xgl_usgn32 *words,
          unsigned int *red, unsigned int *green, unsigned int *blue)
{
    Xgl_object context;
    Xgl_pt_i2d position;
    Xgl_color color;
    unsigned int channel, shift;
    unsigned int shifts[3];
    unsigned long serial = oldengl_xgl_error_serial();

    context = xgl_object_create(oldengl_xgl_system(), XGL_2D_CTX, 0,
        XGL_CTX_DEVICE, raster,
        XGL_CTX_DEFERRAL_MODE, XGL_DEFER_ASAP, 0);
    if (!context || oldengl_xgl_error_serial() != serial) {
        if (context)
            xgl_object_destroy(context);
        return 0;
    }
    position.x = position.y = 0;
    for (channel = 0; channel < 3; ++channel) {
        words[0] = 0;
        color.rgb.r = channel == 0 ? 1.0f : 0.0f;
        color.rgb.g = channel == 1 ? 1.0f : 0.0f;
        color.rgb.b = channel == 2 ? 1.0f : 0.0f;
        xgl_context_set_pixel(context, &position, &color);
        xgl_context_flush(context, XGL_FLUSH_SYNCHRONIZE);
        for (shift = 0; shift <= 24; shift += 8)
            if (words[0] == ((Xgl_usgn32)0xffUL << shift))
                break;
        if (shift > 24 || oldengl_xgl_error_serial() != serial) {
            xgl_object_destroy(context);
            if (oldengl_xgl_error_serial() == serial)
                set_error("Unrecognized XGL 32-bit RGB memory raster format");
            return 0;
        }
        shifts[channel] = shift;
    }
    xgl_object_destroy(context);
    if (shifts[0] == shifts[1] || shifts[0] == shifts[2] ||
        shifts[1] == shifts[2]) {
        set_error("XGL RGB memory raster channels overlap");
        return 0;
    }
    *red = shifts[0];
    *green = shifts[1];
    *blue = shifts[2];
    return 1;
}

static int
allocate_buffers(GLsizei width, GLsizei height, GLubyte **rgba,
                 Xgl_object *raster, Xgl_usgn32 **memory,
                 unsigned int *red, unsigned int *green, unsigned int *blue)
{
    size_t bytes;
    unsigned long serial = oldengl_xgl_error_serial();

    *rgba = 0;
    *raster = 0;
    *memory = 0;
    if (width < 1 || height < 1 || width > MAX_WIDTH || height > MAX_HEIGHT ||
        (size_t)width > ((size_t)-1) / (size_t)height / 4) {
        set_error("Buffer dimensions exceed the renderer limits");
        return 0;
    }
    bytes = (size_t)width * (size_t)height * 4;
    *rgba = (GLubyte *)calloc(1, bytes);
    if (!*rgba) {
        set_error("Cannot allocate the software RGBA framebuffer");
        return 0;
    }
    *raster = xgl_object_create(oldengl_xgl_system(), XGL_MEM_RAS, 0,
        XGL_DEV_COLOR_TYPE, XGL_COLOR_RGB,
        XGL_RAS_DEPTH, 32,
        XGL_RAS_WIDTH, width,
        XGL_RAS_HEIGHT, height, 0);
    if (!*raster || oldengl_xgl_error_serial() != serial)
        goto failed;
    xgl_object_get(*raster, XGL_MEM_RAS_IMAGE_BUFFER_ADDR, memory);
    if (!*memory || oldengl_xgl_error_serial() != serial)
        goto failed;
    if (!calibrate(*raster, *memory, red, green, blue))
        goto failed;
    memset(*memory, 0, bytes);
    return 1;

failed:
    if (!oldengl_xgl_last_error()[0])
        set_error("Cannot create an XGL RGB memory raster");
    if (*raster)
        xgl_object_destroy(*raster);
    free(*rgba);
    *raster = 0;
    *rgba = 0;
    *memory = 0;
    return 0;
}

static void
present(GLcontext *core)
{
    XGLMesaContext context = find_context((OSMesaContext)core->DriverCtx);
    unsigned long serial = oldengl_xgl_error_serial();

    if (!context)
        return;
    context->presented = GL_FALSE;
    if (!xglmesa_pack_rgba(context->memory, context->rgba,
            (size_t)context->width, (size_t)context->height,
            context->red_shift, context->green_shift, context->blue_shift)) {
        set_error("Cannot convert RGBA framebuffer to XGL pixels");
        return;
    }
    /* Flush Xlib before DGA access; synchronize XGL before reusing source data. */
    XSync(context->display, False);
    xgl_context_copy_buffer(context->copy_context, 0, 0, context->memory_raster);
    xgl_context_flush(context->copy_context, XGL_FLUSH_SYNCHRONIZE);
    context->presented = oldengl_xgl_error_serial() == serial ? GL_TRUE : GL_FALSE;
}

XGLMesaContext
XGLMesaCreateContext(Display *display, Window window, XGLMesaContext share)
{
    XGLMesaContext context;
    XWindowAttributes attributes;
    Xgl_X_window xwindow;
    Xgl_obj_desc description;
    unsigned long serial;

    oldengl_xgl_clear_error();
    if (!outside_begin_end())
        return 0;
    if (!display || !window || (share && !valid_context(share))) {
        set_error("Invalid display, window, or sharing context");
        return 0;
    }
    if (sizeof(Xgl_usgn32) != 4 || sizeof(unsigned int) != 4) {
        set_error("This XGL backend requires 32-bit XGL pixel words");
        return 0;
    }
    if (!XGetWindowAttributes(display, window, &attributes)) {
        set_error("XGetWindowAttributes failed");
        return 0;
    }
    if (attributes.class != InputOutput ||
        (attributes.visual->class != TrueColor &&
         attributes.visual->class != DirectColor)) {
        set_error("An InputOutput TrueColor or DirectColor window is required");
        return 0;
    }
    context = (XGLMesaContext)calloc(1, sizeof(*context));
    if (!context) {
        set_error("Cannot allocate an XGLMesa context");
        return 0;
    }
    if (!acquire_system()) {
        free(context);
        return 0;
    }
    serial = oldengl_xgl_error_serial();
    context->display = display;
    context->window = window;
    context->width = attributes.width;
    context->height = attributes.height;
    context->mesa = OSMesaCreateContextExt(OSMESA_RGBA, 24, 8, 16,
                                         share ? share->mesa : 0);
    if (!context->mesa) {
        set_error("Cannot create the Mesa software rendering context");
        goto failed;
    }
    memset(&description, 0, sizeof(description));
    memset(&xwindow, 0, sizeof(xwindow));
    xwindow.X_display = display;
    xwindow.X_window = (Xgl_usgn32)window;
    xwindow.X_screen = XScreenNumberOfScreen(attributes.screen);
    description.win_ras.type = XGL_WIN_X;
    description.win_ras.desc = &xwindow;
    XSync(display, False);
    context->window_raster = xgl_object_create(oldengl_xgl_system(), XGL_WIN_RAS,
        &description, XGL_DEV_COLOR_TYPE, XGL_COLOR_RGB, 0);
    if (!context->window_raster || oldengl_xgl_error_serial() != serial)
        goto failed;
    context->copy_context = xgl_object_create(oldengl_xgl_system(), XGL_2D_CTX, 0,
        XGL_CTX_DEVICE, context->window_raster,
        XGL_CTX_DEFERRAL_MODE, XGL_DEFER_ASAP,
        XGL_CTX_ROP, XGL_ROP_COPY,
        XGL_CTX_RASTER_FILL_STYLE, XGL_RAS_FILL_COPY, 0);
    if (!context->copy_context || oldengl_xgl_error_serial() != serial)
        goto failed;
    if (!allocate_buffers(context->width, context->height, &context->rgba,
            &context->memory_raster, &context->memory,
            &context->red_shift, &context->green_shift, &context->blue_shift))
        goto failed;
    context->next = contexts;
    contexts = context;
    return context;

failed:
    if (!oldengl_xgl_last_error()[0])
        set_error("Cannot create the XGL window raster or copy context");
    if (context->copy_context)
        xgl_object_destroy(context->copy_context);
    if (context->window_raster)
        xgl_object_destroy(context->window_raster);
    if (context->mesa)
        OSMesaDestroyContext(context->mesa);
    release_system();
    free(context);
    return 0;
}

GLboolean
XGLMesaMakeCurrent(XGLMesaContext context)
{
    GLcontext *core;
    oldengl_xgl_clear_error();
    if (!outside_begin_end())
        return GL_FALSE;
    if (context && !valid_context(context))
        return GL_FALSE;
    /*
     * Mesa's low-level context switch does not flush the old TNL vertex
     * queue. Complete it while its original framebuffer is still attached;
     * this also makes later noncurrent resize/destruction safe.
     */
    if (_mesa_get_current_context())
        glFlush();
    if (!context) {
        _mesa_make_current(0, 0);
        return GL_TRUE;
    }
    if (!OSMesaMakeCurrent(context->mesa, context->rgba, GL_UNSIGNED_BYTE,
                           context->width, context->height)) {
        set_error("OSMesaMakeCurrent failed");
        return GL_FALSE;
    }
    /* A positive row length also avoids a legacy OSMesa zero-stride bug. */
    OSMesaPixelStore(OSMESA_ROW_LENGTH, context->width);
    OSMesaPixelStore(OSMESA_Y_UP, 1);
    core = _mesa_get_current_context();
    core->Driver.Flush = present;
    core->Driver.Finish = present;
    return GL_TRUE;
}

XGLMesaContext
XGLMesaGetCurrentContext(void)
{
    return find_context(OSMesaGetCurrentContext());
}

GLboolean
XGLMesaResizeBuffers(XGLMesaContext context, GLsizei width, GLsizei height)
{
    GLubyte *rgba;
    Xgl_object raster;
    Xgl_usgn32 *memory;
    unsigned int red, green, blue;
    unsigned long serial;

    oldengl_xgl_clear_error();
    if (!valid_context(context) || !outside_begin_end())
        return GL_FALSE;
    if (!allocate_buffers(width, height, &rgba, &raster, &memory,
                           &red, &green, &blue))
        return GL_FALSE;
    if (OSMesaGetCurrentContext() == context->mesa) {
        /* Complete buffered primitives against their original framebuffer. */
        glFinish();
        if (!OSMesaMakeCurrent(context->mesa, rgba, GL_UNSIGNED_BYTE,
                               width, height)) {
            xgl_object_destroy(raster);
            free(rgba);
            set_error("OSMesaMakeCurrent failed while resizing");
            return GL_FALSE;
        }
        OSMesaPixelStore(OSMESA_ROW_LENGTH, width);
        OSMesaPixelStore(OSMESA_Y_UP, 1);
    }
    serial = oldengl_xgl_error_serial();
    XSync(context->display, False);
    xgl_window_raster_resize(context->window_raster);
    xgl_object_destroy(context->memory_raster);
    free(context->rgba);
    context->memory_raster = raster;
    context->memory = memory;
    context->rgba = rgba;
    context->width = width;
    context->height = height;
    context->red_shift = red;
    context->green_shift = green;
    context->blue_shift = blue;
    context->presented = GL_FALSE;
    return oldengl_xgl_error_serial() == serial ? GL_TRUE : GL_FALSE;
}

GLboolean
XGLMesaSwapBuffers(XGLMesaContext context)
{
    oldengl_xgl_clear_error();
    if (!valid_context(context) || !outside_begin_end())
        return GL_FALSE;
    if (OSMesaGetCurrentContext() != context->mesa) {
        set_error("The context to present must be current");
        return GL_FALSE;
    }
    context->presented = GL_FALSE;
    glFinish();
    return context->presented;
}

void
XGLMesaDestroyContext(XGLMesaContext context)
{
    XGLMesaContext *link;
    if (!context)
        return;
    oldengl_xgl_clear_error();
    if (!valid_context(context) || !outside_begin_end())
        return;
    if (OSMesaGetCurrentContext() == context->mesa) {
        glFinish();
        _mesa_make_current(0, 0);
    }
    for (link = &contexts; *link != context; link = &(*link)->next)
        ;
    *link = context->next;
    OSMesaDestroyContext(context->mesa);
    xgl_object_destroy(context->copy_context);
    xgl_object_destroy(context->window_raster);
    xgl_object_destroy(context->memory_raster);
    free(context->rgba);
    free(context);
    release_system();
}

GLboolean
XGLMesaGetColorBuffer(XGLMesaContext context, GLsizei *width,
                     GLsizei *height, const GLubyte **pixels)
{
    oldengl_xgl_clear_error();
    if (!valid_context(context) || !width || !height || !pixels) {
        set_error("Invalid context or color-buffer output pointer");
        return GL_FALSE;
    }
    *width = context->width;
    *height = context->height;
    *pixels = context->rgba;
    return GL_TRUE;
}

const char *
XGLMesaGetLastError(void)
{
    return oldengl_xgl_last_error();
}
