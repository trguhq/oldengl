/*
 * MIT licensed. Test fixture only: synthetic XGL SDK, real Xlib transport.
 * Pixel generation here tests command routing and buffer ownership; it is
 * not evidence of the coverage rules or hardware execution of Sun XGL.
 */
#include "mock_native_xgl.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <xgl/xgl.h>
#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MOCK_SYSTEM 10001
#define MOCK_LIGHT 10002
#define MOCK_TMAP 10003
#define MOCK_MAX_LIGHTS 16
#define MOCK_MAX_LEVELS 16

struct oldengl_mock_xgl {
    int type, width, height, depth, color_type;
    Display *display;
    Window window;
    Visual *visual;
    Xgl_object device;
    Xgl_usgn32 *pixels;
    Xgl_usgn32 *z_pixels;
    Xgl_usgn32 *back_pixels;
    int buffers, draw_buffer, display_buffer, source_buffer;
    int render_buffer, hlhsr, z_compare;
    Xgl_usgn32 z_write, plane_mask;
    Xgl_object local_model, view_transform;
    double matrix[4][4];
    int clip_flags;
    Xgl_bounds_d3d clip_bounds;
    Xgl_object lights[MOCK_MAX_LIGHTS];
    Xgl_boolean light_switches[MOCK_MAX_LIGHTS];
    int light_count, light_type;
    Xgl_color light_color;
    double light_direction[3], light_position[3];
    double attenuation_constant, attenuation_linear;
    double ambient_coefficient, diffuse_coefficient;
    Xgl_object tmap;
    int tmap_enabled, texture_levels;
    Xgl_texture_general_desc texture_descriptor;
    Xgl_usgn32 *texture_pixels[MOCK_MAX_LEVELS];
    int texture_width[MOCK_MAX_LEVELS], texture_height[MOCK_MAX_LEVELS];
    void (*notify)(Xgl_sys_state);
    int illumination, line_interp;
    Xgl_color background;
    Xgl_marker marker;
    Xgl_bounds_d3d viewport, vdc_window;
    int viewport_set;
};

struct surface {
    Xgl_object raster;
    XImage *image;
    Xgl_usgn32 *pixels;
    int width, height, allocated;
    int buffer;
};

static struct oldengl_mock_xgl dot;
Xgl_marker xgl_marker_dot = &dot;
static Xgl_sys_state active_system;
static struct MockNativeXGLStats stats;
static int fail_creation, fail_after_color_readback, color_readback_done;

void MockNativeXGLReset(void)
{
    assert(stats.live_objects == 0);
    memset(&stats, 0, sizeof(stats));
    fail_creation = fail_after_color_readback = color_readback_done = 0;
    active_system = NULL;
}

void MockNativeXGLGetStats(struct MockNativeXGLStats *value) { *value = stats; }
void MockNativeXGLFailNextCreation(int type) { fail_creation = type; }
void MockNativeXGLFailAfterColorReadback(void)
{
    fail_after_color_readback = 1; color_readback_done = 0;
}

static unsigned int component(unsigned long pixel, unsigned long mask)
{
    unsigned int shift = 0;
    if (!mask) return 0;
    while (!(mask & 1UL)) { mask >>= 1; ++shift; }
    return (unsigned int)((((pixel >> shift) & mask) * 255UL + mask / 2) / mask);
}

static unsigned long channel(unsigned int value, unsigned long mask)
{
    unsigned int shift = 0;
    if (!mask) return 0;
    while (!(mask & 1UL)) { mask >>= 1; ++shift; }
    return (((unsigned long)value * mask + 127) / 255) << shift;
}

static unsigned int to_byte(float value)
{
    if (value <= 0.0f) return 0;
    if (value >= 1.0f) return 255;
    return (unsigned int)(value * 255.0f + 0.5f);
}

static Xgl_usgn32 color_word(const Xgl_color *value)
{
    return to_byte(value->rgb.r) | (to_byte(value->rgb.g) << 8) |
           (to_byte(value->rgb.b) << 16);
}

static void dimensions(Xgl_object raster)
{
    XWindowAttributes attributes;
    assert(raster && raster->type == XGL_WIN_RAS);
    assert(XGetWindowAttributes(raster->display, raster->window, &attributes));
    if (raster->width != attributes.width || raster->height != attributes.height) {
        free(raster->z_pixels); raster->z_pixels = NULL;
        free(raster->back_pixels); raster->back_pixels = NULL;
    }
    raster->width = attributes.width;
    raster->height = attributes.height;
    raster->visual = attributes.visual;
    raster->depth = attributes.depth;
}

static void allocate_depth(Xgl_object raster)
{
    size_t count;
    if (raster->z_pixels) return;
    count = (size_t)raster->width * raster->height;
    raster->z_pixels = (Xgl_usgn32 *)calloc(count, sizeof(Xgl_usgn32));
    assert(raster->z_pixels);
}

static void allocate_back(Xgl_object raster)
{
    if (!raster->back_pixels) {
        raster->back_pixels = (Xgl_usgn32 *)calloc((size_t)raster->width*raster->height,
                                                  sizeof(Xgl_usgn32));
        assert(raster->back_pixels);
    }
}

static void surface_begin(struct surface *s, Xgl_object raster)
{
    int x, y;
    unsigned long pixel;
    memset(s, 0, sizeof(*s));
    s->raster = raster;
    s->buffer = raster->draw_buffer;
    if (raster->type == XGL_WIN_RAS) {
        dimensions(raster);
        s->width = raster->width; s->height = raster->height;
        if (raster->draw_buffer != raster->display_buffer) {
            allocate_back(raster);
            s->pixels = raster->back_pixels;
            return;
        }
        s->image = XGetImage(raster->display, raster->window, 0, 0,
            (unsigned int)s->width, (unsigned int)s->height, AllPlanes, ZPixmap);
        assert(s->image);
        s->pixels = (Xgl_usgn32 *)calloc((size_t)s->width * s->height,
                                        sizeof(Xgl_usgn32));
        assert(s->pixels);
        s->allocated = 1;
        for (y = 0; y < s->height; ++y)
            for (x = 0; x < s->width; ++x) {
                pixel = XGetPixel(s->image, x, y);
                s->pixels[y*s->width+x] =
                    component(pixel, raster->visual->red_mask) |
                    (component(pixel, raster->visual->green_mask) << 8) |
                    (component(pixel, raster->visual->blue_mask) << 16);
            }
    } else {
        assert(raster->type == XGL_MEM_RAS);
        s->width = raster->width; s->height = raster->height;
        s->pixels = raster->pixels;
    }
}

static void surface_end(struct surface *s, int write)
{
    int x, y;
    Xgl_usgn32 pixel;
    unsigned long native;
    GC gc;
    Xgl_object r = s->raster;
    if (s->image) {
        if (write) {
            for (y = 0; y < s->height; ++y)
                for (x = 0; x < s->width; ++x) {
                    pixel = s->pixels[y*s->width+x];
                    native = channel(pixel & 255U, r->visual->red_mask) |
                        channel((pixel >> 8) & 255U, r->visual->green_mask) |
                        channel((pixel >> 16) & 255U, r->visual->blue_mask);
                    XPutPixel(s->image, x, y, native);
                }
            gc = XCreateGC(r->display, r->window, 0, NULL);
            XPutImage(r->display, r->window, gc, s->image, 0, 0, 0, 0,
                      (unsigned int)s->width, (unsigned int)s->height);
            XFreeGC(r->display, gc);
        }
        XDestroyImage(s->image);
    }
    if (s->allocated) free(s->pixels);
}

static void write_pixel(struct surface *s, Xgl_ctx ctx,
                         int x, int y, const Xgl_color *value, double z)
{
    Xgl_usgn32 incoming, current;
    int passes = 1;
    if (x < 0 || y < 0 || x >= s->width || y >= s->height) return;
    if (ctx->viewport_set &&
        (x < ctx->viewport.xmin || x > ctx->viewport.xmax ||
         y < ctx->viewport.ymin || y > ctx->viewport.ymax)) return;
    if (ctx->hlhsr == XGL_HLHSR_Z_BUFFER) {
        allocate_depth(s->raster);
        incoming = (Xgl_usgn32)(z <= 0 ? 0 : (z >= 16777215.0 ? 16777215U : floor(z + 0.5)));
        current = s->raster->z_pixels[y*s->width+x] & 0xffffffU;
        switch (ctx->z_compare) {
        case XGL_Z_COMP_NEVER: passes = 0; break;
        case XGL_Z_COMP_LESS_THAN: passes = incoming < current; break;
        case XGL_Z_COMP_EQUAL: passes = incoming == current; break;
        case XGL_Z_COMP_LESS_THAN_OR_EQUAL: passes = incoming <= current; break;
        case XGL_Z_COMP_GREATER_THAN: passes = incoming > current; break;
        case XGL_Z_COMP_NOT_EQUAL: passes = incoming != current; break;
        case XGL_Z_COMP_GREATER_THAN_OR_EQUAL: passes = incoming >= current; break;
        case XGL_Z_COMP_ALWAYS: passes = 1; break;
        default: assert(!"invalid native depth comparison");
        }
        ++stats.depth_tests;
        if (!passes) return;
        if (ctx->z_write) s->raster->z_pixels[y*s->width+x] = incoming;
    }
    if (ctx->render_buffer & XGL_RENDER_DRAW_BUFFER)
        s->pixels[y*s->width+x] = (s->pixels[y*s->width+x] & ~ctx->plane_mask) |
            (color_word(value) & ctx->plane_mask);
}

static void attributes(Xgl_object object, va_list args)
{
    int attribute;
    while ((attribute = va_arg(args, int)) != 0) {
        switch (attribute) {
        case XGL_CTX_DEVICE: object->device = va_arg(args, Xgl_object); break;
        case XGL_SYS_ST_ERROR_NOTIFICATION_FUNCTION:
            object->notify = va_arg(args, void (*)(Xgl_sys_state)); break;
        case XGL_RAS_WIDTH: object->width = va_arg(args, int); break;
        case XGL_RAS_HEIGHT: object->height = va_arg(args, int); break;
        case XGL_RAS_DEPTH: object->depth = va_arg(args, int); break;
        case XGL_DEV_COLOR_TYPE: object->color_type = va_arg(args, int); break;
        case XGL_3D_CTX_SURF_FRONT_ILLUMINATION:
            object->illumination = va_arg(args, int); break;
        case XGL_3D_CTX_LINE_COLOR_INTERP:
            object->line_interp = va_arg(args, int); break;
        case XGL_CTX_MARKER: object->marker = va_arg(args, Xgl_marker); break;
        case XGL_CTX_BACKGROUND_COLOR:
            object->background = *va_arg(args, Xgl_color *); break;
        case XGL_CTX_DC_VIEWPORT:
            object->viewport = *va_arg(args, Xgl_bounds_d3d *);
            object->viewport_set = 1; break;
        case XGL_CTX_VDC_WINDOW:
            object->vdc_window = *va_arg(args, Xgl_bounds_d3d *); break;
        case XGL_CTX_LOCAL_MODEL_TRANS:
            object->local_model = va_arg(args, Xgl_object); break;
        case XGL_CTX_VIEW_TRANS:
            object->view_transform = va_arg(args, Xgl_object); break;
        case XGL_CTX_VIEW_CLIP_BOUNDS:
            object->clip_bounds = *va_arg(args, Xgl_bounds_d3d *); break;
        case XGL_CTX_CLIP_PLANES:
            object->clip_flags = va_arg(args, int); break;
        case XGL_WIN_RAS_BUFFERS_REQUESTED:
            object->buffers = va_arg(args, int);
            if (object->buffers > 2) object->buffers = 2;
            if (object->buffers > 1) allocate_back(object);
            break;
        case XGL_WIN_RAS_BUF_DRAW: object->draw_buffer = va_arg(args, int); break;
        case XGL_WIN_RAS_BUF_DISPLAY: object->display_buffer = va_arg(args, int); break;
        case XGL_CTX_RENDER_BUFFER: object->render_buffer = va_arg(args, int); break;
        case XGL_CTX_PLANE_MASK: object->plane_mask = va_arg(args, Xgl_usgn32); break;
        case XGL_RAS_SOURCE_BUFFER: object->source_buffer = va_arg(args, int); break;
        case XGL_3D_CTX_HLHSR_MODE:
            object->hlhsr = va_arg(args, int);
            if (object->hlhsr == XGL_HLHSR_Z_BUFFER && object->device)
                allocate_depth(object->device);
            break;
        case XGL_3D_CTX_Z_BUFFER_COMP_METHOD:
            object->z_compare = va_arg(args, int); break;
        case XGL_3D_CTX_Z_BUFFER_WRITE_MASK:
            object->z_write = va_arg(args, Xgl_usgn32); break;
        case XGL_TRANS_DATA_TYPE:
        case XGL_TRANS_DIMENSION:
        case XGL_3D_CTX_SURF_BACK_TMAP_NUM:
            (void)va_arg(args, int); break;
        case XGL_3D_CTX_SURF_FRONT_TMAP_NUM: {
            int count = (int)va_arg(args, Xgl_usgn32);
            assert(count == 0 || count == 1);
            if (!count && object->tmap) {
                xgl_object_destroy(object->tmap); object->tmap = NULL;
            } else if (count && !object->tmap) {
                object->tmap = (Xgl_object)calloc(1, sizeof(*object));
                assert(object->tmap); object->tmap->type = MOCK_TMAP;
                ++stats.live_objects; ++stats.texture_maps;
            }
            object->tmap_enabled = count;
            break;
        }
        case XGL_3D_CTX_SURF_FRONT_TMAP_SWITCHES:
            object->tmap_enabled = *va_arg(args, Xgl_boolean *); break;
        case XGL_3D_CTX_SURF_TMAP_PERSP_CORRECTION:
            assert(va_arg(args, int) == XGL_TEXTURE_PERSP_PIXEL); break;
        case XGL_TMAP_COORD_SOURCE:
        case XGL_TMAP_PARAM_TYPE:
        case XGL_TMAP_T0_INDEX:
        case XGL_TMAP_T1_INDEX:
            (void)va_arg(args, int); break;
        case XGL_TMAP_DESCRIPTOR:
            object->texture_descriptor = *va_arg(args, Xgl_texture_general_desc *); break;
        case XGL_MIPMAP_TEXTURE_LEVELS:
            object->texture_levels = (int)va_arg(args, Xgl_usgn32);
            assert(object->texture_levels > 0 && object->texture_levels <= MOCK_MAX_LEVELS);
            break;
        case XGL_MIPMAP_TEXTURE_MEM_RAS_LIST: {
            Xgl_mem_ras *rasters = va_arg(args, Xgl_mem_ras *);
            int i;
            size_t size;
            for (i = 0; i < object->texture_levels; ++i) {
                object->texture_width[i] = rasters[i]->width;
                object->texture_height[i] = rasters[i]->height;
                size = (size_t)rasters[i]->width*rasters[i]->height*sizeof(Xgl_usgn32);
                free(object->texture_pixels[i]);
                object->texture_pixels[i] = (Xgl_usgn32 *)malloc(size);
                assert(object->texture_pixels[i]);
                memcpy(object->texture_pixels[i], rasters[i]->pixels, size);
            }
            break;
        }
        case XGL_3D_CTX_LIGHT_NUM: {
            int count = (int)va_arg(args, Xgl_usgn32), i;
            assert(count >= 0 && count <= MOCK_MAX_LIGHTS);
            if (count != object->light_count) {
                for (i = 0; i < object->light_count; ++i) xgl_object_destroy(object->lights[i]);
                memset(object->lights, 0, sizeof(object->lights));
                object->light_count = count;
                for (i = 0; i < count; ++i) {
                    object->lights[i] = (Xgl_object)calloc(1, sizeof(*object));
                    assert(object->lights[i]); object->lights[i]->type = MOCK_LIGHT;
                    object->lights[i]->attenuation_constant = 1;
                    ++stats.live_objects; ++stats.lights;
                }
            }
            break;
        }
        case XGL_3D_CTX_LIGHT_SWITCHES:
            memcpy(object->light_switches, va_arg(args, Xgl_boolean *),
                   (size_t)object->light_count*sizeof(Xgl_boolean)); break;
        case XGL_LIGHT_TYPE: object->light_type = va_arg(args, int); break;
        case XGL_LIGHT_COLOR: object->light_color = *va_arg(args, Xgl_color *); break;
        case XGL_LIGHT_DIRECTION: {
            Xgl_pt_f3d *direction = va_arg(args, Xgl_pt_f3d *);
            object->light_direction[0] = direction->x;
            object->light_direction[1] = direction->y;
            object->light_direction[2] = direction->z;
            break;
        }
        case XGL_LIGHT_POSITION: {
            Xgl_pt_d3d *position = va_arg(args, Xgl_pt_d3d *);
            object->light_position[0] = position->x;
            object->light_position[1] = position->y;
            object->light_position[2] = position->z;
            break;
        }
        case XGL_LIGHT_ATTENUATION_1: object->attenuation_constant = va_arg(args, double); break;
        case XGL_LIGHT_ATTENUATION_2: object->attenuation_linear = va_arg(args, double); break;
        case XGL_3D_CTX_SURF_FRONT_AMBIENT: object->ambient_coefficient = va_arg(args, double); break;
        case XGL_3D_CTX_SURF_FRONT_DIFFUSE: object->diffuse_coefficient = va_arg(args, double); break;
        case XGL_3D_CTX_SURF_FRONT_SPECULAR: (void)va_arg(args, double); break;
        case XGL_CTX_SURF_FRONT_COLOR: (void)va_arg(args, Xgl_color *); break;
        case XGL_3D_CTX_SURF_FRONT_LIGHT_COMPONENT:
        case XGL_3D_CTX_SURF_FACE_DISTINGUISH:
        case XGL_3D_CTX_SURF_LIGHTING_NORMAL_FLIP:
        case XGL_3D_CTX_SURF_NORMAL_FLIP:
            (void)va_arg(args, int); break;
        case XGL_SYS_ST_ERROR_DETECTION:
        case XGL_CTX_DEFERRAL_MODE:
        case XGL_CTX_ROP:
        case XGL_CTX_RASTER_FILL_STYLE:
        case XGL_CTX_VDC_MAP:
        case XGL_CTX_VDC_ORIENTATION:
        case XGL_3D_CTX_SURF_FACE_CULL:
        case XGL_CTX_LINE_COLOR_SELECTOR:
        case XGL_CTX_MARKER_COLOR_SELECTOR:
        case XGL_CTX_SURF_FRONT_FILL_STYLE:
        case XGL_CTX_SURF_EDGE_FLAG:
        case XGL_CTX_SURF_FRONT_COLOR_SELECTOR:
        case XGL_CTX_NEW_FRAME_ACTION:
        case XGL_CTX_LINE_STYLE:
            (void)va_arg(args, int); break;
        default:
            fprintf(stderr, "Unsupported native fixture attribute %d\n", attribute);
            abort();
        }
    }
}

Xgl_sys_state xgl_open(Xgl_attribute attribute, ...)
{
    Xgl_object result;
    va_list args;
    assert(!active_system);
    result = (Xgl_object)calloc(1, sizeof(*result)); assert(result);
    result->type = MOCK_SYSTEM;
    va_start(args, attribute);
    assert(attribute == XGL_SYS_ST_ERROR_DETECTION);
    (void)va_arg(args, int);
    attributes(result, args);
    va_end(args);
    active_system = result;
    ++stats.opens; ++stats.live_objects;
    return result;
}

void xgl_close(Xgl_sys_state state)
{
    assert(state == active_system);
    active_system = NULL;
    --stats.live_objects; ++stats.closes;
    free(state);
}

Xgl_object xgl_object_create(Xgl_sys_state state, Xgl_obj_type type,
                            Xgl_obj_desc *descriptor, ...)
{
    Xgl_object object;
    va_list args;
    assert(state == active_system);
    if (type == fail_creation) { fail_creation = 0; return NULL; }
    object = (Xgl_object)calloc(1, sizeof(*object)); assert(object);
    object->type = type; object->width = object->height = 1;
    object->depth = 32; object->color_type = XGL_COLOR_RGB;
    object->buffers = 1; object->source_buffer = XGL_BUFFER_SEL_DRAW;
    object->render_buffer = XGL_RENDER_DRAW_BUFFER;
    object->z_compare = XGL_Z_COMP_LESS_THAN_OR_EQUAL;
    object->z_write = 0xffffffffU;
    object->plane_mask = 0xffffffffU;
    object->matrix[0][0] = object->matrix[1][1] = 1;
    object->matrix[2][2] = object->matrix[3][3] = 1;
    object->clip_bounds.xmin = object->clip_bounds.ymin = -1;
    object->clip_bounds.xmax = object->clip_bounds.ymax = object->clip_bounds.zmax = 1;
    if (type == XGL_WIN_RAS) {
        Xgl_X_window *window;
        assert(descriptor && descriptor->win_ras.type == XGL_WIN_X);
        window = (Xgl_X_window *)descriptor->win_ras.desc;
        object->display = (Display *)window->X_display;
        object->window = (Window)window->X_window;
        dimensions(object);
    }
    va_start(args, descriptor); attributes(object, args); va_end(args);
    if (type == XGL_MEM_RAS) {
        object->pixels = (Xgl_usgn32 *)calloc((size_t)object->width * object->height,
                                             sizeof(Xgl_usgn32));
        assert(object->pixels);
    }
    ++stats.live_objects;
    if (type == XGL_MIPMAP_TEXTURE) ++stats.mipmaps;
    return object;
}

void xgl_object_destroy(Xgl_object object)
{
    int i;
    assert(object && object->type != MOCK_SYSTEM);
    for (i = 0; i < object->light_count; ++i) xgl_object_destroy(object->lights[i]);
    if (object->tmap) xgl_object_destroy(object->tmap);
    for (i = 0; i < MOCK_MAX_LEVELS; ++i) free(object->texture_pixels[i]);
    free(object->pixels); free(object->z_pixels); free(object->back_pixels);
    free(object); --stats.live_objects;
}

void xgl_object_set(Xgl_object object, ...)
{
    va_list args;
    va_start(args, object); attributes(object, args); va_end(args);
}

void xgl_object_get(Xgl_object object, Xgl_attribute attribute, void *result)
{
    assert(object && result);
    switch (attribute) {
    case XGL_SYS_ST_ERROR_INFO:
        memset(result, 0, sizeof(Xgl_error_info));
        ((Xgl_error_info *)result)->msg = "synthetic XGL failure"; break;
    case XGL_MEM_RAS_IMAGE_BUFFER_ADDR:
        *(Xgl_usgn32 **)result = object->pixels; break;
    case XGL_MEM_RAS_Z_BUFFER_ADDR:
        allocate_depth(object); *(Xgl_usgn32 **)result = object->z_pixels; break;
    case XGL_WIN_RAS_BUFFERS_ALLOCATED: *(Xgl_usgn32 *)result = (Xgl_usgn32)object->buffers; break;
    case XGL_WIN_RAS_BUF_DRAW: *(Xgl_usgn32 *)result = (Xgl_usgn32)object->draw_buffer; break;
    case XGL_WIN_RAS_BUF_DISPLAY: *(Xgl_usgn32 *)result = (Xgl_usgn32)object->display_buffer; break;
    case XGL_DEV_MAXIMUM_COORDINATES: {
        Xgl_pt_d3d *maximum = (Xgl_pt_d3d *)result;
        maximum->x = object->width - 1;
        maximum->y = object->height - 1;
        maximum->z = 16777215.0;
        break;
    }
    case XGL_3D_CTX_LIGHTS:
        memcpy(result, object->lights, (size_t)object->light_count*sizeof(Xgl_object)); break;
    case XGL_3D_CTX_SURF_FRONT_TMAP:
        *(Xgl_tmap *)result = object->tmap; break;
    case XGL_RAS_WIDTH: *(Xgl_sgn32 *)result = object->width; break;
    case XGL_RAS_HEIGHT: *(Xgl_sgn32 *)result = object->height; break;
    case XGL_RAS_DEPTH: *(Xgl_sgn32 *)result = object->depth; break;
    case XGL_DEV_COLOR_TYPE: case XGL_DEV_REAL_COLOR_TYPE:
        *(Xgl_color_type *)result = object->color_type; break;
    default:
        fprintf(stderr, "Unsupported native fixture query %d\n", attribute); abort();
    }
}

void xgl_window_raster_resize(Xgl_win_ras raster) { dimensions(raster); }

void xgl_context_flush(Xgl_ctx context, Xgl_flush_action action)
{
    if (context->device && context->device->display)
        XSync(context->device->display, False);
    if (action & XGL_FLUSH_SYNCHRONIZE) ++stats.synchronizes;
}

void xgl_context_set_pixel(Xgl_ctx context, Xgl_pt_i2d *point, Xgl_color *color)
{
    struct surface s;
    surface_begin(&s, context->device);
    write_pixel(&s, context, point->x, point->y, color, 0);
    surface_end(&s, 1);
}

Xgl_boolean xgl_context_get_pixel(Xgl_ctx context, Xgl_pt_i2d *point,
                                 Xgl_color *color)
{
    Xgl_usgn32 word;
    struct surface s;
    surface_begin(&s, context->device);
    assert(point->x >= 0 && point->y >= 0 && point->x < s.width && point->y < s.height);
    word = s.pixels[point->y*s.width+point->x];
    color->rgb.r = (word & 255U) / 255.0f;
    color->rgb.g = ((word >> 8) & 255U) / 255.0f;
    color->rgb.b = ((word >> 16) & 255U) / 255.0f;
    surface_end(&s, 0);
    return 0;
}

void xgl_context_copy_buffer(Xgl_ctx context, Xgl_bounds_i2d *rectangle,
                             Xgl_pt_i2d *position, Xgl_ras source)
{
    struct surface src, dst;
    int x, y, sx, sy, dx, dy, width, height, saved_source_buffer;
    if (fail_after_color_readback && color_readback_done &&
        source->type == XGL_WIN_RAS && context->device->type == XGL_MEM_RAS &&
        source->source_buffer == XGL_BUFFER_SEL_Z) {
        fail_after_color_readback = color_readback_done = 0;
        ++stats.injected_errors;
        assert(active_system && active_system->notify);
        active_system->notify(active_system);
        return;
    }
    if (source->source_buffer == XGL_BUFFER_SEL_Z) {
        Xgl_object destination = context->device;
        allocate_depth(source); allocate_depth(destination);
        sx = rectangle ? rectangle->xmin : 0; sy = rectangle ? rectangle->ymin : 0;
        dx = position ? position->x : 0; dy = position ? position->y : 0;
        width = rectangle ? rectangle->xmax-rectangle->xmin+1 : source->width;
        height = rectangle ? rectangle->ymax-rectangle->ymin+1 : source->height;
        assert(context->z_compare == XGL_Z_COMP_ALWAYS);
        if (context->z_write)
            for (y = 0; y < height; ++y)
                for (x = 0; x < width; ++x)
                    if (sx+x >= 0 && sy+y >= 0 && sx+x < source->width && sy+y < source->height &&
                        dx+x >= 0 && dy+y >= 0 && dx+x < destination->width && dy+y < destination->height)
                        destination->z_pixels[(dy+y)*destination->width+dx+x] =
                            source->z_pixels[(sy+y)*source->width+sx+x] & 0xffffffU;
        ++stats.copies;
        if (source->type == XGL_MEM_RAS && destination->type == XGL_WIN_RAS) ++stats.uploads;
        if (source->type == XGL_WIN_RAS && destination->type == XGL_MEM_RAS) ++stats.downloads;
        return;
    }
    saved_source_buffer = source->draw_buffer;
    if (source->source_buffer != XGL_BUFFER_SEL_DRAW)
        source->draw_buffer = source->source_buffer;
    surface_begin(&src, source); surface_begin(&dst, context->device);
    source->draw_buffer = saved_source_buffer;
    sx = rectangle ? rectangle->xmin : 0; sy = rectangle ? rectangle->ymin : 0;
    dx = position ? position->x : 0; dy = position ? position->y : 0;
    width = rectangle ? rectangle->xmax - rectangle->xmin + 1 : src.width;
    height = rectangle ? rectangle->ymax - rectangle->ymin + 1 : src.height;
    for (y = 0; y < height; ++y)
        for (x = 0; x < width; ++x)
            if (sx+x >= 0 && sy+y >= 0 && sx+x < src.width && sy+y < src.height &&
                dx+x >= 0 && dy+y >= 0 && dx+x < dst.width && dy+y < dst.height)
                dst.pixels[(dy+y)*dst.width+dx+x] = src.pixels[(sy+y)*src.width+sx+x];
    surface_end(&src, 0); surface_end(&dst, 1);
    ++stats.copies;
    if (source->type == XGL_MEM_RAS && context->device->type == XGL_WIN_RAS) ++stats.uploads;
    if (source->type == XGL_WIN_RAS && context->device->type == XGL_MEM_RAS) {
        ++stats.downloads;
        if (fail_after_color_readback) color_readback_done = 1;
    }
}

void xgl_transform_write_specific(Xgl_trans transform, void *matrix,
                                  Xgl_trans_member membership)
{
    Xgl_matrix_f3d *source = (Xgl_matrix_f3d *)matrix;
    int row, column;
    (void)membership;
    assert(transform && transform->type == XGL_TRANS);
    for (row = 0; row < 4; ++row)
        for (column = 0; column < 4; ++column)
            transform->matrix[row][column] = (*source)[row][column];
    ++stats.transforms;
}

void xgl_context_new_frame(Xgl_ctx context)
{
    struct surface s;
    int x, y;
    surface_begin(&s, context->device);
    for (y = 0; y < s.height; ++y)
        for (x = 0; x < s.width; ++x)
            s.pixels[y*s.width+x] = color_word(&context->background);
    surface_end(&s, 1); ++stats.clears;
}

struct transformed_vertex {
    double position[4];
    double texture[2], inverse_w;
    Xgl_color color;
};

static void multiply(double destination[4], const double source[4], Xgl_object transform)
{
    int row, column;
    if (!transform) { memcpy(destination, source, sizeof(double)*4); return; }
    for (column = 0; column < 4; ++column) {
        destination[column] = 0;
        for (row = 0; row < 4; ++row)
            destination[column] += source[row] * transform->matrix[row][column];
    }
}

static void eye_normal(double output[3], const double input[3], Xgl_object model)
{
    double a[3][3], inverse[3][3], determinant, length;
    int i, j;
    for (i = 0; i < 3; ++i)
        for (j = 0; j < 3; ++j) a[i][j] = model ? model->matrix[i][j] : (i == j);
    determinant = a[0][0]*(a[1][1]*a[2][2]-a[1][2]*a[2][1])-
        a[0][1]*(a[1][0]*a[2][2]-a[1][2]*a[2][0])+
        a[0][2]*(a[1][0]*a[2][1]-a[1][1]*a[2][0]);
    assert(determinant != 0);
    inverse[0][0] = (a[1][1]*a[2][2]-a[1][2]*a[2][1])/determinant;
    inverse[0][1] = (a[0][2]*a[2][1]-a[0][1]*a[2][2])/determinant;
    inverse[0][2] = (a[0][1]*a[1][2]-a[0][2]*a[1][1])/determinant;
    inverse[1][0] = (a[1][2]*a[2][0]-a[1][0]*a[2][2])/determinant;
    inverse[1][1] = (a[0][0]*a[2][2]-a[0][2]*a[2][0])/determinant;
    inverse[1][2] = (a[0][2]*a[1][0]-a[0][0]*a[1][2])/determinant;
    inverse[2][0] = (a[1][0]*a[2][1]-a[1][1]*a[2][0])/determinant;
    inverse[2][1] = (a[0][1]*a[2][0]-a[0][0]*a[2][1])/determinant;
    inverse[2][2] = (a[0][0]*a[1][1]-a[0][1]*a[1][0])/determinant;
    for (i = 0; i < 3; ++i) {
        output[i] = 0;
        for (j = 0; j < 3; ++j) output[i] += inverse[i][j]*input[j];
    }
    length = sqrt(output[0]*output[0]+output[1]*output[1]+output[2]*output[2]);
    if (length > 0) for (i = 0; i < 3; ++i) output[i] /= length;
}

static Xgl_color illuminate(Xgl_ctx context, const double eye[4], const double normal[3])
{
    Xgl_color result;
    double sum[3] = {0, 0, 0}, n[3], direction[3], distance, amount;
    int i, j;
    eye_normal(n, normal, context->local_model);
    for (i = 0; i < context->light_count; ++i) {
        Xgl_object light = context->lights[i];
        if (!context->light_switches[i]) continue;
        if (light->light_type == XGL_LIGHT_AMBIENT) {
            amount = context->ambient_coefficient;
        } else {
            if (light->light_type == XGL_LIGHT_DIRECTIONAL) {
                for (j = 0; j < 3; ++j) direction[j] = -light->light_direction[j];
            } else {
                assert(light->light_type == XGL_LIGHT_POSITIONAL);
                for (j = 0; j < 3; ++j) direction[j] = light->light_position[j]-eye[j]/eye[3];
            }
            distance = sqrt(direction[0]*direction[0]+direction[1]*direction[1]+direction[2]*direction[2]);
            amount = 0;
            if (distance > 0)
                for (j = 0; j < 3; ++j) amount += n[j]*direction[j]/distance;
            if (amount < 0) amount = 0;
            amount *= context->diffuse_coefficient;
            if (light->light_type == XGL_LIGHT_POSITIONAL)
                amount /= light->attenuation_constant+light->attenuation_linear*distance;
        }
        sum[0] += amount*light->light_color.rgb.r;
        sum[1] += amount*light->light_color.rgb.g;
        sum[2] += amount*light->light_color.rgb.b;
    }
    result.rgb.r = (float)(sum[0] > 1 ? 1 : sum[0]);
    result.rgb.g = (float)(sum[1] > 1 ? 1 : sum[1]);
    result.rgb.b = (float)(sum[2] > 1 ? 1 : sum[2]);
    return result;
}

static struct transformed_vertex vertex_at(Xgl_ctx context,
                                            const Xgl_pt_list *list, unsigned int index)
{
    struct transformed_vertex result;
    double input[4], eye[4], normal[3] = {0, 0, 1};
    const unsigned char *record;
    const float *data;
    size_t offset;
    assert(index < list->num_pts);
    result.texture[0] = result.texture[1] = 0;
    result.inverse_w = 1;
    if (list->pt_type == XGL_PT_COLOR_NORMAL_F3D ||
        list->pt_type == XGL_PT_COLOR_NORMAL_DATA_F3D) {
        Xgl_pt_color_normal_f3d *p;
        if (list->pt_type == XGL_PT_COLOR_NORMAL_DATA_F3D) {
            offset = offsetof(Xgl_pt_color_normal_data_f3d, data);
            record = (const unsigned char *)list->pts.color_normal_data_f3d +
                index*(offset+list->num_data_values*sizeof(float));
            data = (const float *)(record+offset);
            assert(list->num_data_values == 2);
            result.texture[0] = data[0]; result.texture[1] = data[1];
            p = (Xgl_pt_color_normal_f3d *)record;
        } else p = list->pts.color_normal_f3d + index;
        input[0] = p->x; input[1] = p->y; input[2] = p->z;
        result.color = p->color;
        normal[0] = p->normal.x; normal[1] = p->normal.y; normal[2] = p->normal.z;
    } else {
        Xgl_pt_color_f3d *p;
        if (list->pt_type == XGL_PT_COLOR_DATA_F3D) {
            offset = offsetof(Xgl_pt_color_data_f3d, data);
            record = (const unsigned char *)list->pts.color_data_f3d +
                index*(offset+list->num_data_values*sizeof(float));
            data = (const float *)(record+offset);
            assert(list->num_data_values == 2);
            result.texture[0] = data[0]; result.texture[1] = data[1];
            p = (Xgl_pt_color_f3d *)record;
        } else {
            assert(list->pt_type == XGL_PT_COLOR_F3D);
            p = list->pts.color_f3d + index;
        }
        input[0] = p->x; input[1] = p->y; input[2] = p->z;
        result.color = p->color;
    }
    input[3] = 1;
    multiply(eye, input, context->local_model);
    if (context->illumination == XGL_ILLUM_PER_VERTEX && context->light_count) {
        assert(list->pt_type == XGL_PT_COLOR_NORMAL_F3D ||
               list->pt_type == XGL_PT_COLOR_NORMAL_DATA_F3D);
        result.color = illuminate(context, eye, normal);
    }
    multiply(result.position, eye, context->view_transform);
    return result;
}

static double clip_distance(const struct transformed_vertex *v, Xgl_ctx context, int plane)
{
    switch (plane) {
    case 0: return v->position[0] - context->clip_bounds.xmin*v->position[3];
    case 1: return context->clip_bounds.xmax*v->position[3] - v->position[0];
    case 2: return v->position[1] - context->clip_bounds.ymin*v->position[3];
    case 3: return context->clip_bounds.ymax*v->position[3] - v->position[1];
    case 4: return v->position[2] - context->clip_bounds.zmin*v->position[3];
    default:return context->clip_bounds.zmax*v->position[3] - v->position[2];
    }
}

static struct transformed_vertex interpolate(struct transformed_vertex a,
                                             struct transformed_vertex b, double amount)
{
    struct transformed_vertex result;
    int i;
    for (i = 0; i < 4; ++i) result.position[i] = a.position[i] + amount*(b.position[i]-a.position[i]);
    for (i = 0; i < 2; ++i) result.texture[i] = a.texture[i]+amount*(b.texture[i]-a.texture[i]);
    result.inverse_w = 1;
    result.color.rgb.r = (float)(a.color.rgb.r + amount*(b.color.rgb.r-a.color.rgb.r));
    result.color.rgb.g = (float)(a.color.rgb.g + amount*(b.color.rgb.g-a.color.rgb.g));
    result.color.rgb.b = (float)(a.color.rgb.b + amount*(b.color.rgb.b-a.color.rgb.b));
    return result;
}

static int project(struct transformed_vertex *v)
{
    int i;
    double w = v->position[3];
    if (w <= 0) return 0;
    v->inverse_w = 1.0/w;
    for (i = 0; i < 3; ++i) v->position[i] /= w;
    v->position[3] = 1;
    return 1;
}

void xgl_multimarker(Xgl_ctx context, Xgl_pt_list *points)
{
    struct surface s;
    struct transformed_vertex vertex;
    unsigned int i;
    int plane, outside;
    /* The real XGL API permits Z-only selection for raster operators, but
     * primitive operators are no-ops without the draw buffer selected. */
    if (!(context->render_buffer & XGL_RENDER_DRAW_BUFFER)) return;
    surface_begin(&s, context->device);
    for (i = 0; i < points->num_pts; ++i) {
        vertex = vertex_at(context, points, i);
        outside = 0;
        for (plane = 0; plane < 6; ++plane)
            if ((context->clip_flags & (1 << plane)) && clip_distance(&vertex, context, plane) < 0)
                outside = 1;
        if (outside || !project(&vertex)) continue;
        write_pixel(&s, context, (int)floor(vertex.position[0]+0.5),
                    (int)floor(vertex.position[1]+0.5), &vertex.color, vertex.position[2]);
        ++stats.points;
    }
    surface_end(&s, 1);
}

void xgl_multipolyline(Xgl_ctx context, Xgl_bbox *bbox, Xgl_usgn32 count,
                       Xgl_pt_list points[])
{
    struct surface s;
    struct transformed_vertex a, b;
    Xgl_color color;
    unsigned int i, j;
    int step, steps, plane, outside;
    double dx, dy, t, da, db;
    (void)bbox;
    if (!(context->render_buffer & XGL_RENDER_DRAW_BUFFER)) return;
    surface_begin(&s, context->device);
    for (i = 0; i < count; ++i)
        for (j = 0; j+1 < points[i].num_pts; ++j) {
            a = vertex_at(context, points+i, j); b = vertex_at(context, points+i, j+1);
            outside = 0;
            for (plane = 0; plane < 6; ++plane) {
                if (!(context->clip_flags & (1 << plane))) continue;
                da = clip_distance(&a, context, plane); db = clip_distance(&b, context, plane);
                if (da < 0 && db < 0) { outside = 1; break; }
                if (da < 0) a = interpolate(a, b, da/(da-db));
                else if (db < 0) b = interpolate(a, b, da/(da-db));
            }
            if (outside || !project(&a) || !project(&b)) continue;
            dx = b.position[0]-a.position[0]; dy = b.position[1]-a.position[1];
            steps = (int)ceil(fabs(dx) > fabs(dy) ? fabs(dx) : fabs(dy));
            if (steps < 1) steps = 1;
            for (step = 0; step < steps; ++step) {
                t = (double)step/steps;
                if (context->line_interp) {
                    color.rgb.r = a.color.rgb.r*(1-t)+b.color.rgb.r*t;
                    color.rgb.g = a.color.rgb.g*(1-t)+b.color.rgb.g*t;
                    color.rgb.b = a.color.rgb.b*(1-t)+b.color.rgb.b*t;
                } else color = b.color;
                write_pixel(&s, context, (int)floor(a.position[0]+dx*t+0.5),
                            (int)floor(a.position[1]+dy*t+0.5), &color,
                            a.position[2] + t*(b.position[2]-a.position[2]));
            }
            ++stats.lines;
        }
    surface_end(&s, 1);
}

static int texture_index(int index, int size, int boundary)
{
    if (boundary == XGL_TEXTURE_BOUNDARY_WRAP) {
        index %= size;
        if (index < 0) index += size;
        return index;
    }
    assert(boundary == XGL_TEXTURE_BOUNDARY_CLAMP_BOUNDARY);
    return index < 0 ? 0 : (index >= size ? size-1 : index);
}

static void texture_texel(Xgl_object image, int level, int x, int y,
                          const Xgl_texture_general_mipmap_desc *description,
                          double result[4])
{
    Xgl_usgn32 word;
    int channel_index;
    x = texture_index(x, image->texture_width[level], description->u_boundary);
    y = texture_index(y, image->texture_height[level], description->v_boundary);
    /* GL's t=0 is the bottom of the image; native raster rows are top first. */
    y = image->texture_height[level]-1-y;
    word = image->texture_pixels[level][y*image->texture_width[level]+x];
    for (channel_index = 0; channel_index < 4; ++channel_index)
        result[channel_index] = ((word >> (channel_index*8)) & 255U)/255.0;
}

static void texture_level(Xgl_object image, int level, double s, double t,
                          const Xgl_texture_general_mipmap_desc *description,
                          int linear, double result[4])
{
    double x = s*image->texture_width[level], y = t*image->texture_height[level];
    double a[4], b[4], c[4], d[4], fx, fy;
    int ix, iy, i;
    if (!linear) {
        texture_texel(image, level, (int)floor(x), (int)floor(y), description, result);
        return;
    }
    x -= 0.5; y -= 0.5;
    ix = (int)floor(x); iy = (int)floor(y); fx = x-ix; fy = y-iy;
    texture_texel(image, level, ix, iy, description, a);
    texture_texel(image, level, ix+1, iy, description, b);
    texture_texel(image, level, ix, iy+1, description, c);
    texture_texel(image, level, ix+1, iy+1, description, d);
    for (i = 0; i < 4; ++i)
        result[i] = (1-fy)*((1-fx)*a[i]+fx*b[i])+fy*((1-fx)*c[i]+fx*d[i]);
}

static void texture_coordinates(const struct transformed_vertex *a,
                                const struct transformed_vertex *b,
                                const struct transformed_vertex *c,
                                double u, double v, double w, double st[2])
{
    double reciprocal = u*a->inverse_w+v*b->inverse_w+w*c->inverse_w;
    int i;
    assert(reciprocal != 0);
    for (i = 0; i < 2; ++i)
        st[i] = (u*a->texture[i]*a->inverse_w+v*b->texture[i]*b->inverse_w+
                 w*c->texture[i]*c->inverse_w)/reciprocal;
}

static void texture_color(Xgl_ctx context, const struct transformed_vertex *a,
                          const struct transformed_vertex *b,
                          const struct transformed_vertex *c,
                          double u, double v, double w,
                          double ux, double vx, double uy, double vy,
                          Xgl_color *color)
{
    Xgl_texture_general_desc *descriptor;
    Xgl_texture_general_mipmap_desc *description;
    Xgl_render_component_desc *component_description;
    Xgl_object image;
    double st[2], stx[2], sty[2], texel[4], next[4], input[3], output[3];
    double dxs, dxt, dys, dyt, footprint, lambda, fraction;
    int method, level = 0, linear = 0, blend_levels = 0, i;
    descriptor = &context->tmap->texture_descriptor;
    description = &descriptor->texture_info.mipmap;
    component_description = descriptor->comp_info.color_info.render_component_desc;
    image = description->texture_map;
    assert(image && image->texture_levels);
    texture_coordinates(a,b,c,u,v,w,st);
    texture_coordinates(a,b,c,u+ux,v+vx,w-ux-vx,stx);
    texture_coordinates(a,b,c,u+uy,v+vy,w-uy-vy,sty);
    dxs = (stx[0]-st[0])*image->texture_width[0];
    dxt = (stx[1]-st[1])*image->texture_height[0];
    dys = (sty[0]-st[0])*image->texture_width[0];
    dyt = (sty[1]-st[1])*image->texture_height[0];
    footprint = sqrt(dxs*dxs+dxt*dxt);
    if (dys*dys+dyt*dyt > footprint*footprint) footprint = sqrt(dys*dys+dyt*dyt);
    lambda = footprint > 0 ? log(footprint)/log(2.0) : -100;
    method = lambda <= 0 ? description->interp_info.filter2 : description->interp_info.filter1;
    if (method == XGL_TEXTURE_INTERP_BILINEAR) linear = 1;
    else if (method == XGL_TEXTURE_INTERP_MIPMAP_POINT ||
             method == XGL_TEXTURE_INTERP_MIPMAP_BILINEAR) {
        level = (int)floor(lambda+0.5);
        linear = method == XGL_TEXTURE_INTERP_MIPMAP_BILINEAR;
    } else if (method == XGL_TEXTURE_INTERP_MIPMAP_PT_LINEAR ||
               method == XGL_TEXTURE_INTERP_MIPMAP_TRILINEAR) {
        level = (int)floor(lambda); blend_levels = 1;
        linear = method == XGL_TEXTURE_INTERP_MIPMAP_TRILINEAR;
    } else assert(method == XGL_TEXTURE_INTERP_POINT);
    if (level < 0) level = 0;
    if (level >= image->texture_levels) level = image->texture_levels-1;
    texture_level(image,level,st[0],st[1],description,linear,texel);
    if (blend_levels && level+1 < image->texture_levels) {
        fraction = lambda-floor(lambda);
        texture_level(image,level+1,st[0],st[1],description,linear,next);
        for (i = 0; i < 4; ++i) texel[i] += fraction*(next[i]-texel[i]);
    }
    input[0] = color->rgb.r; input[1] = color->rgb.g; input[2] = color->rgb.b;
    for (i = 0; i < 3; ++i) {
        switch (component_description->texture_op) {
        case XGL_TEXTURE_OP_NONE: output[i] = input[i]; break;
        case XGL_TEXTURE_OP_MODULATE: output[i] = input[i]*texel[i]; break;
        case XGL_TEXTURE_OP_REPLACE: output[i] = texel[i]; break;
        case XGL_TEXTURE_OP_DECAL_INTRINSIC:
            output[i] = input[i]*(1-texel[3])+texel[i]*texel[3]; break;
        case XGL_TEXTURE_OP_BLEND_INTRINSIC:
            output[i] = input[i]*(1-texel[i])+texel[i]*
                ((const float *)&component_description->op.blend_int.rgb.c)[i]; break;
        default: assert(!"unknown texture operation"); output[i] = 0;
        }
    }
    color->rgb.r = (float)output[0]; color->rgb.g = (float)output[1]; color->rgb.b = (float)output[2];
    ++stats.texture_samples;
}

static void raster_triangle(struct surface *s, Xgl_ctx context,
                            struct transformed_vertex a, struct transformed_vertex b,
                            struct transformed_vertex c)
{
    Xgl_color color;
    int x, y;
    double denominator, u, v, w, ux, vx, uy, vy;
    if (!project(&a) || !project(&b) || !project(&c)) return;
    denominator = (b.position[1]-c.position[1])*(a.position[0]-c.position[0])+
                  (c.position[0]-b.position[0])*(a.position[1]-c.position[1]);
    if (denominator == 0.0) return;
    ux = (b.position[1]-c.position[1])/denominator;
    vx = (c.position[1]-a.position[1])/denominator;
    uy = (c.position[0]-b.position[0])/denominator;
    vy = (a.position[0]-c.position[0])/denominator;
    for (y = 0; y < s->height; ++y)
        for (x = 0; x < s->width; ++x) {
                u = ((b.position[1]-c.position[1])*(x-c.position[0])+
                     (c.position[0]-b.position[0])*(y-c.position[1]))/denominator;
                v = ((c.position[1]-a.position[1])*(x-c.position[0])+
                     (a.position[0]-c.position[0])*(y-c.position[1]))/denominator;
                w = 1-u-v;
                if (u < 0 || v < 0 || w < 0) continue;
                if (context->illumination == XGL_ILLUM_NONE_INTERP_COLOR ||
                    context->illumination == XGL_ILLUM_PER_VERTEX) {
                    color.rgb.r = u*a.color.rgb.r+v*b.color.rgb.r+w*c.color.rgb.r;
                    color.rgb.g = u*a.color.rgb.g+v*b.color.rgb.g+w*c.color.rgb.g;
                    color.rgb.b = u*a.color.rgb.b+v*b.color.rgb.b+w*c.color.rgb.b;
                } else color = a.color;
                if (context->tmap_enabled)
                    texture_color(context,&a,&b,&c,u,v,w,ux,vx,uy,vy,&color);
                write_pixel(s, context, x, y, &color,
                    u*a.position[2]+v*b.position[2]+w*c.position[2]);
        }
}

void xgl_triangle_list(Xgl_ctx context, Xgl_facet_list *facets,
                       Xgl_pt_list *points, Xgl_tlist_flags flags)
{
    struct surface s;
    struct transformed_vertex polygon[16], clipped[16], a, b;
    unsigned int i;
    int count, next_count, plane, j;
    double da, db;
    assert(!facets || facets->facet_type == XGL_FACET_NONE);
    assert(flags == 0 || flags == XGL_TLIST_FLAG_TRI_RESTART);
    assert(points->num_pts % 3 == 0);
    if (!(context->render_buffer & XGL_RENDER_DRAW_BUFFER)) return;
    surface_begin(&s, context->device);
    for (i = 0; i < points->num_pts; i += 3) {
        for (j = 0; j < 3; ++j) polygon[j] = vertex_at(context, points, i+j);
        ++stats.triangles;
        count = 3;
        for (plane = 0; plane < 6 && count; ++plane) {
            if (!(context->clip_flags & (1 << plane))) continue;
            next_count = 0;
            a = polygon[count-1]; da = clip_distance(&a, context, plane);
            for (j = 0; j < count; ++j) {
                b = polygon[j]; db = clip_distance(&b, context, plane);
                if ((da < 0) != (db < 0)) clipped[next_count++] = interpolate(a, b, da/(da-db));
                if (db >= 0) clipped[next_count++] = b;
                assert(next_count <= 16);
                a = b; da = db;
            }
            memcpy(polygon, clipped, (size_t)next_count*sizeof(polygon[0]));
            count = next_count;
        }
        for (j = 1; j+1 < count; ++j)
            raster_triangle(&s, context, polygon[0], polygon[j], polygon[j+1]);
    }
    surface_end(&s, 1);
}
