/* Native presentation test double. No X server or XGL runtime is exercised. */
#include "mock_xgl.h"
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MOCK_WINDOWS 16
#define MOCK_SYS_STATE 100

struct oldengl_mock_display { int valid; };
struct oldengl_mock_screen { int number; };
struct mock_window {
    int valid, width, height, rgb;
    unsigned int *presented;
};
struct oldengl_mock_xgl {
    int type, width, height, depth, color_type;
    Window window;
    Xgl_object device;
    unsigned int *pixels;
    void (*notify)(Xgl_sys_state);
    int illumination, line_interp;
    Xgl_color background;
    Xgl_marker marker;
    Xgl_bounds_d3d viewport, vdc_window;
    int viewport_set;
};

static struct oldengl_mock_xgl dot_marker;
Xgl_marker xgl_marker_dot = &dot_marker;

static struct oldengl_mock_display display = { 1 };
static struct oldengl_mock_screen screen = { 0 };
static struct mock_window windows[MOCK_WINDOWS];
static Visual rgb_visual = { 1, TrueColor, 0xff0000, 0x00ff00, 0x0000ff, 8, 256 };
static Visual index_visual = { 2, 3, 0, 0, 0, 8, 256 };
static unsigned int red_shift, green_shift = 8, blue_shift = 16;
static int fail_open, fail_type, fail_after;
static int copies, flushes, synchronizes, live_objects, opens, closes, resizes;
static Xgl_sys_state active_system;
static int error_next_copy;
static int triangles, lines, markers, uploads, downloads, clears;
static Xgl_pt_color_f3d last_vertices[3];

Display *MockXGLDisplay(void) { return &display; }
void MockXGLReset(void)
{
    int i;
    assert(live_objects == 0);
    for (i = 0; i < MOCK_WINDOWS; ++i) {
        free(windows[i].presented);
        memset(&windows[i], 0, sizeof(windows[i]));
    }
    red_shift = 0; green_shift = 8; blue_shift = 16;
    fail_open = fail_type = fail_after = 0;
    copies = flushes = synchronizes = opens = closes = resizes = 0;
    triangles = lines = markers = uploads = downloads = clears = 0;
    memset(last_vertices, 0, sizeof(last_vertices));
    active_system = NULL; error_next_copy = 0;
    MockXGLWindow(1, 8, 8, 1);
    MockXGLWindow(2, 4, 3, 1);
    MockXGLWindow(3, 8, 8, 0);
}
void MockXGLWindow(Window win, int width, int height, int rgb)
{
    struct mock_window *w;
    assert(win > 0 && win < MOCK_WINDOWS);
    w = &windows[win];
    free(w->presented);
    w->width = width; w->height = height; w->rgb = rgb; w->valid = 1;
    w->presented = (unsigned int *)calloc((size_t)width * height, sizeof(unsigned int));
    assert(w->presented != NULL);
}
void MockXGLPacking(unsigned int r, unsigned int g, unsigned int b)
{
    red_shift = r; green_shift = g; blue_shift = b;
}
void MockXGLFailOpen(int fail) { fail_open = fail; }
void MockXGLFailCreate(int type, int after) { fail_type = type; fail_after = after; }
void MockXGLErrorNextCopy(void) { error_next_copy = 1; }
void *MockXGLFramebuffer(Window win)
{
    assert(win > 0 && win < MOCK_WINDOWS && windows[win].valid);
    return windows[win].presented;
}
int MockXGLTriangles(void) { return triangles; }
int MockXGLLines(void) { return lines; }
int MockXGLMarkers(void) { return markers; }
int MockXGLUploads(void) { return uploads; }
int MockXGLDownloads(void) { return downloads; }
int MockXGLClears(void) { return clears; }
Xgl_pt_color_f3d MockXGLLastVertex(unsigned int index)
{
    assert(index < 3);
    return last_vertices[index];
}
unsigned int MockXGLPixel(Window win, int x, int y)
{
    struct mock_window *w = &windows[win];
    assert(w->valid && x >= 0 && y >= 0 && x < w->width && y < w->height);
    return w->presented[y * w->width + x];
}
int MockXGLCopies(void) { return copies; }
int MockXGLFlushes(void) { return flushes; }
int MockXGLSynchronizes(void) { return synchronizes; }
int MockXGLLiveObjects(void) { return live_objects; }
int MockXGLOpens(void) { return opens; }
int MockXGLCloses(void) { return closes; }
int MockXGLResizes(void) { return resizes; }

Status XGetWindowAttributes(Display *dpy, Window win, XWindowAttributes *a)
{
    struct mock_window *w;
    if (dpy != &display || win == 0 || win >= MOCK_WINDOWS || !windows[win].valid)
        return 0;
    w = &windows[win];
    memset(a, 0, sizeof(*a));
    a->width = w->width; a->height = w->height;
    a->depth = w->rgb ? 24 : 8;
    a->visual = w->rgb ? &rgb_visual : &index_visual;
    a->class = InputOutput; a->screen = &screen;
    return 1;
}
int XSync(Display *dpy, Bool discard) { assert(dpy == &display); (void)discard; return 0; }
int XFlush(Display *dpy) { assert(dpy == &display); return 0; }
int XDefaultScreen(Display *dpy) { assert(dpy == &display); return 0; }
int XScreenNumberOfScreen(Screen *s) { assert(s == &screen); return 0; }

static void set_attributes(Xgl_object obj, va_list ap)
{
    int attr;
    while ((attr = va_arg(ap, int)) != 0) {
        switch (attr) {
        case XGL_CTX_DEVICE:
            obj->device = va_arg(ap, Xgl_object);
            break;
        case XGL_SYS_ST_ERROR_NOTIFICATION_FUNCTION:
            obj->notify = va_arg(ap, void (*)(Xgl_sys_state));
            break;
        case XGL_MEM_RAS_IMAGE_BUFFER_ADDR:
            assert(!"backend must use memory owned by XGL");
            (void)va_arg(ap, void *);
            break;
        case XGL_RAS_WIDTH: obj->width = va_arg(ap, int); break;
        case XGL_RAS_HEIGHT: obj->height = va_arg(ap, int); break;
        case XGL_RAS_DEPTH: obj->depth = va_arg(ap, int); break;
        case XGL_DEV_COLOR_TYPE:
            obj->color_type = va_arg(ap, int);
            break;
        case XGL_3D_CTX_SURF_FRONT_ILLUMINATION:
            obj->illumination = va_arg(ap, int); break;
        case XGL_3D_CTX_LINE_COLOR_INTERP:
            obj->line_interp = va_arg(ap, int); break;
        case XGL_CTX_MARKER:
            obj->marker = va_arg(ap, Xgl_marker); break;
        case XGL_CTX_BACKGROUND_COLOR:
            obj->background = *va_arg(ap, Xgl_color *); break;
        case XGL_CTX_DC_VIEWPORT:
            obj->viewport = *va_arg(ap, Xgl_bounds_d3d *);
            obj->viewport_set = 1; break;
        case XGL_CTX_VDC_WINDOW:
            obj->vdc_window = *va_arg(ap, Xgl_bounds_d3d *); break;
        default:
            (void)va_arg(ap, unsigned int);
            break;
        }
    }
}

Xgl_sys_state xgl_open(Xgl_attribute attr, ...)
{
    Xgl_object result;
    va_list ap;
    if (fail_open) return NULL;
    result = (Xgl_object)calloc(1, sizeof(*result));
    assert(result != NULL);
    result->type = MOCK_SYS_STATE;
    va_start(ap, attr);
    assert(attr == XGL_SYS_ST_ERROR_DETECTION);
    (void)va_arg(ap, int);
    set_attributes(result, ap);
    va_end(ap);
    ++live_objects; ++opens;
    assert(active_system == NULL);
    active_system = result;
    return result;
}
void xgl_close(Xgl_sys_state state)
{
    assert(state && state->type == MOCK_SYS_STATE);
    --live_objects; ++closes;
    assert(state == active_system);
    active_system = NULL;
    free(state);
}
Xgl_object xgl_object_create(Xgl_sys_state state, Xgl_obj_type type,
                             Xgl_obj_desc *desc, ...)
{
    Xgl_object obj;
    va_list ap;
    assert(state && state->type == MOCK_SYS_STATE);
    if (type == fail_type && fail_after-- == 0) return NULL;
    obj = (Xgl_object)calloc(1, sizeof(*obj));
    assert(obj != NULL);
    obj->type = type; obj->width = obj->height = 1;
    obj->depth = 32; obj->color_type = XGL_COLOR_RGB;
    if (type == XGL_WIN_RAS) {
        Xgl_X_window *xw;
        struct mock_window *w;
        assert(desc && desc->win_ras.type == XGL_WIN_X);
        xw = (Xgl_X_window *)desc->win_ras.desc;
        assert(xw && xw->X_display == &display && xw->X_screen == 0);
        obj->window = (Window)xw->X_window;
        assert(obj->window > 0 && obj->window < MOCK_WINDOWS);
        w = &windows[obj->window];
        assert(w->valid);
        obj->width = w->width; obj->height = w->height;
        obj->depth = w->rgb ? 24 : 8;
        obj->color_type = w->rgb ? XGL_COLOR_RGB : XGL_COLOR_INDEX;
    }
    va_start(ap, desc);
    set_attributes(obj, ap);
    va_end(ap);
    if (type == XGL_MEM_RAS) {
        assert(obj->width > 0 && obj->height > 0 && obj->depth == 32);
        obj->pixels = (unsigned int *)calloc((size_t)obj->width * obj->height,
                                             sizeof(unsigned int));
        assert(obj->pixels != NULL);
    }
    ++live_objects;
    return obj;
}
void xgl_object_destroy(Xgl_object obj)
{
    assert(obj != NULL && obj->type != MOCK_SYS_STATE);
    --live_objects;
    free(obj->pixels);
    free(obj);
}
void xgl_object_get(Xgl_object obj, Xgl_attribute attr, void *value)
{
    assert(obj != NULL && value != NULL);
    switch (attr) {
    case XGL_SYS_ST_ERROR_INFO:
        memset(value, 0, sizeof(Xgl_error_info));
        ((Xgl_error_info *)value)->msg = "injected XGL copy failure";
        break;
    case XGL_MEM_RAS_IMAGE_BUFFER_ADDR: *(Xgl_usgn32 **)value = obj->pixels; break;
    case XGL_RAS_WIDTH: *(Xgl_sgn32 *)value = obj->width; break;
    case XGL_RAS_HEIGHT: *(Xgl_sgn32 *)value = obj->height; break;
    case XGL_RAS_DEPTH: *(Xgl_sgn32 *)value = obj->depth; break;
    case XGL_DEV_COLOR_TYPE:
    case XGL_DEV_REAL_COLOR_TYPE: *(Xgl_color_type *)value = obj->color_type; break;
    default: assert(!"unsupported mock object_get attribute"); break;
    }
}
void xgl_object_set(Xgl_object obj, ...)
{
    va_list ap;
    va_start(ap, obj);
    set_attributes(obj, ap);
    va_end(ap);
}
void xgl_context_set_pixel(Xgl_ctx ctx, Xgl_pt_i2d *point, Xgl_color *value)
{
    unsigned int r, g, b;
    Xgl_object raster;
    assert(ctx && ctx->type == XGL_2D_CTX && point && value);
    raster = ctx->device;
    assert(raster && raster->type == XGL_MEM_RAS);
    assert(point->x >= 0 && point->y >= 0 && point->x < raster->width && point->y < raster->height);
    r = (unsigned int)(value->rgb.r * 255.0f + 0.5f);
    g = (unsigned int)(value->rgb.g * 255.0f + 0.5f);
    b = (unsigned int)(value->rgb.b * 255.0f + 0.5f);
    raster->pixels[point->y * raster->width + point->x] =
        (r << red_shift) | (g << green_shift) | (b << blue_shift);
}
void xgl_context_copy_buffer(Xgl_ctx dest, Xgl_bounds_i2d *rectangle,
                              Xgl_pt_i2d *position, Xgl_ras src)
{
    int x, y, width, height, dx, dy, sx, sy, sw, sh, dw, dh;
    unsigned int *source_pixels, *target_pixels;
    Xgl_object target;
    assert(dest && src);
    if (error_next_copy) {
        error_next_copy = 0;
        assert(active_system && active_system->notify);
        active_system->notify(active_system);
        return;
    }
    target = dest->device;
    assert(target && (target->type == XGL_WIN_RAS || target->type == XGL_MEM_RAS));
    assert(src->type == XGL_WIN_RAS || src->type == XGL_MEM_RAS);
    if (src->type == XGL_WIN_RAS) {
        source_pixels = windows[src->window].presented;
        sw = windows[src->window].width; sh = windows[src->window].height;
    } else { source_pixels = src->pixels; sw = src->width; sh = src->height; }
    if (target->type == XGL_WIN_RAS) {
        target_pixels = windows[target->window].presented;
        dw = windows[target->window].width; dh = windows[target->window].height;
    } else { target_pixels = target->pixels; dw = target->width; dh = target->height; }
    sx = rectangle ? rectangle->xmin : 0;
    sy = rectangle ? rectangle->ymin : 0;
    width = rectangle ? rectangle->xmax - rectangle->xmin + 1 : sw;
    height = rectangle ? rectangle->ymax - rectangle->ymin + 1 : sh;
    dx = position ? position->x : 0;
    dy = position ? position->y : 0;
    for (y = 0; y < height; ++y)
        for (x = 0; x < width; ++x)
            if (sx+x >= 0 && sy+y >= 0 && sx+x < sw && sy+y < sh &&
                dx+x >= 0 && dy+y >= 0 && dx+x < dw && dy+y < dh)
                target_pixels[(dy+y)*dw+dx+x] = source_pixels[(sy+y)*sw+sx+x];
    if (src->type == XGL_MEM_RAS && target->type == XGL_WIN_RAS) ++uploads;
    if (src->type == XGL_WIN_RAS && target->type == XGL_MEM_RAS) ++downloads;
    ++copies;
}
void xgl_context_flush(Xgl_ctx ctx, Xgl_flush_action action)
{
    assert(ctx && (ctx->type == XGL_2D_CTX || ctx->type == XGL_3D_CTX));
    ++flushes;
    if (action & XGL_FLUSH_SYNCHRONIZE) ++synchronizes;
}
void xgl_window_raster_resize(Xgl_win_ras raster)
{
    struct mock_window *w;
    assert(raster && raster->type == XGL_WIN_RAS);
    w = &windows[raster->window];
    raster->width = w->width; raster->height = w->height;
    ++resizes;
}

static unsigned int *device_pixels(Xgl_object raster, int *width, int *height)
{
    assert(raster != NULL);
    if (raster->type == XGL_WIN_RAS) {
        struct mock_window *w = &windows[raster->window];
        *width = w->width; *height = w->height;
        return w->presented;
    }
    assert(raster->type == XGL_MEM_RAS);
    *width = raster->width; *height = raster->height;
    return raster->pixels;
}

static unsigned int rgb_word(const Xgl_color *value)
{
    unsigned int r, g, b;
    r = (unsigned int)(value->rgb.r * 255.0f + 0.5f);
    g = (unsigned int)(value->rgb.g * 255.0f + 0.5f);
    b = (unsigned int)(value->rgb.b * 255.0f + 0.5f);
    return (r << red_shift) | (g << green_shift) | (b << blue_shift);
}

static void write_pixel(Xgl_ctx ctx, int x, int y, const Xgl_color *value)
{
    int width, height;
    unsigned int *pixels, mask;
    pixels = device_pixels(ctx->device, &width, &height);
    if (x < 0 || y < 0 || x >= width || y >= height) return;
    if (ctx->viewport_set && (x < ctx->viewport.xmin || x > ctx->viewport.xmax ||
                              y < ctx->viewport.ymin || y > ctx->viewport.ymax))
        return;
    mask = (255U << red_shift) | (255U << green_shift) | (255U << blue_shift);
    pixels[y*width+x] = (pixels[y*width+x] & ~mask) | rgb_word(value);
}

Xgl_boolean xgl_context_get_pixel(Xgl_ctx ctx, Xgl_pt_i2d *position,
                                  Xgl_color *value)
{
    int width, height;
    unsigned int word;
    unsigned int *pixels = device_pixels(ctx->device, &width, &height);
    assert(position && value && position->x >= 0 && position->y >= 0 &&
           position->x < width && position->y < height);
    word = pixels[position->y*width+position->x];
    value->rgb.r = ((word >> red_shift) & 255U) / 255.0f;
    value->rgb.g = ((word >> green_shift) & 255U) / 255.0f;
    value->rgb.b = ((word >> blue_shift) & 255U) / 255.0f;
    return 0; /* False means unobscured, rather than unsuccessful. */
}

static Xgl_pt_color_f3d vertex_at(Xgl_pt_list *list, unsigned int index)
{
    Xgl_pt_color_f3d result;
    assert(index < list->num_pts);
    assert(list->pt_type == XGL_PT_COLOR_F3D);
    result = list->pts.color_f3d[index];
    return result;
}

void xgl_triangle_list(Xgl_ctx ctx, Xgl_facet_list *facets,
                       Xgl_pt_list *points, Xgl_tlist_flags flags)
{
    unsigned int i;
    int x, y, width, height;
    float denominator, a, b, c;
    Xgl_pt_color_f3d v0, v1, v2;
    Xgl_color color;
    assert(ctx && ctx->type == XGL_3D_CTX && points);
    assert(facets == NULL || facets->facet_type == XGL_FACET_NONE);
    assert(flags == 0 || flags == XGL_TLIST_FLAG_TRI_RESTART);
    assert(points->num_pts % 3 == 0);
    (void)device_pixels(ctx->device, &width, &height);
    for (i = 0; i < points->num_pts; i += 3) {
        v0 = vertex_at(points, i); v1 = vertex_at(points, i+1);
        v2 = vertex_at(points, i+2);
        last_vertices[0] = v0; last_vertices[1] = v1; last_vertices[2] = v2;
        ++triangles;
        denominator = (v1.y-v2.y)*(v0.x-v2.x)+(v2.x-v1.x)*(v0.y-v2.y);
        if (denominator == 0.0f) continue;
        for (y = 0; y < height; ++y)
            for (x = 0; x < width; ++x) {
                a = ((v1.y-v2.y)*(x-v2.x)+(v2.x-v1.x)*(y-v2.y))/denominator;
                b = ((v2.y-v0.y)*(x-v2.x)+(v0.x-v2.x)*(y-v2.y))/denominator;
                c = 1.0f-a-b;
                if (a < 0.0f || b < 0.0f || c < 0.0f) continue;
                if (ctx->illumination == XGL_ILLUM_NONE_INTERP_COLOR) {
                    color.rgb.r = a*v0.color.rgb.r+b*v1.color.rgb.r+c*v2.color.rgb.r;
                    color.rgb.g = a*v0.color.rgb.g+b*v1.color.rgb.g+c*v2.color.rgb.g;
                    color.rgb.b = a*v0.color.rgb.b+b*v1.color.rgb.b+c*v2.color.rgb.b;
                } else color = v0.color;
                write_pixel(ctx, x, y, &color);
            }
    }
}

void xgl_multipolyline(Xgl_ctx ctx, Xgl_bbox *bbox, Xgl_usgn32 count,
                       Xgl_pt_list points[])
{
    unsigned int i, j;
    int step, steps, x, y;
    float dx, dy, t;
    Xgl_pt_color_f3d v0, v1;
    Xgl_color color;
    assert(ctx && ctx->type == XGL_3D_CTX && bbox == NULL);
    for (i = 0; i < count; ++i)
        for (j = 0; j+1 < points[i].num_pts; ++j) {
            v0 = vertex_at(&points[i], j); v1 = vertex_at(&points[i], j+1);
            last_vertices[0] = v0; last_vertices[1] = v1;
            ++lines;
            dx = v1.x-v0.x; dy = v1.y-v0.y;
            steps = (int)ceil(fabs(dx) > fabs(dy) ? fabs(dx) : fabs(dy));
            if (steps < 1) steps = 1;
            for (step = 0; step < steps; ++step) {
                t = (float)step/(float)steps;
                x = (int)floor(v0.x+dx*t+0.5f);
                y = (int)floor(v0.y+dy*t+0.5f);
                if (ctx->line_interp) {
                    color.rgb.r = v0.color.rgb.r*(1.0f-t)+v1.color.rgb.r*t;
                    color.rgb.g = v0.color.rgb.g*(1.0f-t)+v1.color.rgb.g*t;
                    color.rgb.b = v0.color.rgb.b*(1.0f-t)+v1.color.rgb.b*t;
                } else color = v1.color;
                write_pixel(ctx, x, y, &color);
            }
        }
}

void xgl_multimarker(Xgl_ctx ctx, Xgl_pt_list *points)
{
    unsigned int i;
    Xgl_pt_color_f3d v;
    assert(ctx && ctx->type == XGL_3D_CTX && ctx->marker == xgl_marker_dot);
    for (i = 0; i < points->num_pts; ++i) {
        v = vertex_at(points, i); last_vertices[0] = v;
        write_pixel(ctx, (int)floor(v.x+0.5f), (int)floor(v.y+0.5f), &v.color);
        ++markers;
    }
}

void xgl_context_new_frame(Xgl_ctx ctx)
{
    int x, y, width, height;
    (void)device_pixels(ctx->device, &width, &height);
    for (y = 0; y < height; ++y)
        for (x = 0; x < width; ++x)
            write_pixel(ctx, x, y, &ctx->background);
    ++clears;
}
