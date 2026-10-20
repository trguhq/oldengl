/*
 * Test fixture only. The types and function shapes follow the supplied XGL
 * reference manual, sections xgl_struct and each named operator. Attribute
 * numbers here are arbitrary test values, not the native XGL ABI.
 */
#ifndef OLDENGL_TEST_XGL_H
#define OLDENGL_TEST_XGL_H

typedef unsigned int Xgl_usgn32;
typedef int Xgl_sgn32;
typedef unsigned short Xgl_usgn16;
typedef unsigned char Xgl_usgn8;
typedef int Xgl_boolean;
typedef int Xgl_attribute;
typedef int Xgl_obj_type;
typedef int Xgl_color_type;
typedef unsigned int Xgl_flush_action;
typedef unsigned int Xgl_tlist_flags;
typedef int Xgl_pt_type;
typedef int Xgl_facet_type;
typedef struct oldengl_mock_xgl *Xgl_object;
typedef Xgl_object Xgl_sys_state;
typedef Xgl_object Xgl_ctx;
typedef Xgl_object Xgl_ras;
typedef Xgl_object Xgl_win_ras;
typedef Xgl_object Xgl_mem_ras;
typedef Xgl_object Xgl_marker;
extern Xgl_marker xgl_marker_dot;

typedef struct { float r, g, b; } Xgl_color_rgb;
typedef union {
    Xgl_usgn32 index;
    float gray;
    Xgl_color_rgb rgb;
    Xgl_usgn32 z;
} Xgl_color;
typedef struct { Xgl_sgn32 x, y; } Xgl_pt_i2d;
typedef struct { float x, y, z; } Xgl_pt_f3d;
typedef struct { float x, y, z; Xgl_color color; } Xgl_pt_color_f3d;
typedef struct { float x, y, z, w; } Xgl_pt_f3h;
typedef struct { int unused; } Xgl_bbox;
typedef union {
    Xgl_pt_i2d *i2d;
    Xgl_pt_f3d *f3d;
    Xgl_pt_color_f3d *color_f3d;
    Xgl_pt_f3h *f3h;
} Xgl_pt_ptr_union;
typedef struct {
    Xgl_pt_type pt_type;
    Xgl_bbox *bbox;
    Xgl_usgn32 num_pts, num_data_values;
    Xgl_pt_ptr_union pts;
} Xgl_pt_list;
typedef struct { Xgl_color color; } Xgl_color_facet;
typedef struct { Xgl_pt_f3d normal; } Xgl_normal_facet;
typedef struct { Xgl_color color; Xgl_pt_f3d normal; } Xgl_color_normal_facet;
typedef struct {
    Xgl_facet_type facet_type;
    Xgl_usgn32 num_facets;
    union {
        Xgl_color_facet *color_facets;
        Xgl_normal_facet *normal_facets;
        Xgl_color_normal_facet *color_normal_facets;
    } facets;
} Xgl_facet_list;
typedef struct { Xgl_sgn32 xmin, xmax, ymin, ymax; } Xgl_bounds_i2d;
typedef struct { double xmin, xmax, ymin, ymax, zmin, zmax; } Xgl_bounds_d3d;
typedef struct {
    void *X_display;
    int X_screen;
    Xgl_usgn32 X_window;
} Xgl_X_window;
typedef union {
    struct { unsigned int type; void *desc; } win_ras;
    char *sfont_name;
    struct { Xgl_object raster; } accum_buf;
    struct { char *name; void *desc; } stream;
} Xgl_obj_desc;
typedef struct {
    int type, category;
    char *id, *msg, *cur_op, *cur_obj, *operand1, *operand2;
} Xgl_error_info;

enum {
    XGL_WIN_RAS = 1, XGL_MEM_RAS, XGL_2D_CTX,
    XGL_WIN_X, XGL_3D_CTX,
    XGL_DEV_COLOR_TYPE = 20, XGL_DEV_REAL_COLOR_TYPE,
    XGL_RAS_DEPTH, XGL_RAS_WIDTH, XGL_RAS_HEIGHT,
    XGL_MEM_RAS_IMAGE_BUFFER_ADDR,
    XGL_CTX_DEVICE,
    XGL_CTX_DEFERRAL_MODE, XGL_CTX_ROP, XGL_CTX_PLANE_MASK,
    XGL_CTX_RENDER_BUFFER, XGL_CTX_RASTER_FILL_STYLE,
    XGL_CTX_VDC_MAP, XGL_CTX_VDC_ORIENTATION,
    XGL_RAS_SOURCE_BUFFER, XGL_SYS_ST_ERROR_DETECTION,
    XGL_SYS_ST_ERROR_NOTIFICATION_FUNCTION, XGL_SYS_ST_ERROR_INFO,
    XGL_COLOR_RGB = 60, XGL_COLOR_INDEX,
    XGL_DEFER_ASAP, XGL_DEFER_ASTI,
    XGL_ROP_COPY, XGL_RENDER_DRAW_BUFFER,
    XGL_RAS_FILL_COPY, XGL_VDC_MAP_OFF,
    XGL_Y_DOWN_Z_AWAY, XGL_BUFFER_SEL_DRAW,
    XGL_FLUSH_GEOM_DATA = 1, XGL_FLUSH_BUFFERS = 2,
    XGL_FLUSH_SYNCHRONIZE = 4,
    XGL_PT_F3D = 100, XGL_PT_COLOR_F3D, XGL_PT_F3H,
    XGL_FACET_NONE, XGL_FACET_COLOR,
    XGL_TLIST_FLAG_TRI_RESTART, XGL_TLIST_FLAG_TRI_STRIP,
    XGL_TLIST_FLAG_TRI_STAR,
    XGL_CTX_DC_VIEWPORT = 120, XGL_CTX_VDC_WINDOW,
    XGL_3D_CTX_SURF_FACE_CULL, XGL_3D_CTX_HLHSR_MODE,
    XGL_CTX_LINE_COLOR_SELECTOR, XGL_CTX_MARKER_COLOR_SELECTOR,
    XGL_CTX_MARKER, XGL_CTX_SURF_FRONT_FILL_STYLE, XGL_CTX_SURF_EDGE_FLAG,
    XGL_CTX_SURF_FRONT_COLOR_SELECTOR, XGL_3D_CTX_SURF_FRONT_ILLUMINATION,
    XGL_3D_CTX_LINE_COLOR_INTERP, XGL_CTX_BACKGROUND_COLOR,
    XGL_CTX_NEW_FRAME_ACTION, XGL_CTX_LINE_STYLE,
    XGL_VDC_MAP_DEVICE = 160, XGL_CULL_OFF, XGL_HLHSR_NONE,
    XGL_LINE_COLOR_VERTEX, XGL_MARKER_COLOR_POINT,
    XGL_SURF_FILL_SOLID, XGL_SURF_COLOR_VERTEX_ILLUM_DEP,
    XGL_SURF_COLOR_VERTEX_ILLUM_INDEP, XGL_ILLUM_NONE,
    XGL_ILLUM_NONE_INTERP_COLOR, XGL_LINE_SOLID,
    XGL_CTX_NEW_FRAME_CLEAR = 1
};

Xgl_sys_state xgl_open(Xgl_attribute, ...);
void xgl_close(Xgl_sys_state);
Xgl_object xgl_object_create(Xgl_sys_state, Xgl_obj_type, Xgl_obj_desc *, ...);
void xgl_object_destroy(Xgl_object);
void xgl_object_get(Xgl_object, Xgl_attribute, void *);
void xgl_object_set(Xgl_object, ...);
void xgl_context_copy_buffer(Xgl_ctx, Xgl_bounds_i2d *, Xgl_pt_i2d *, Xgl_ras);
void xgl_context_set_pixel(Xgl_ctx, Xgl_pt_i2d *, Xgl_color *);
Xgl_boolean xgl_context_get_pixel(Xgl_ctx, Xgl_pt_i2d *, Xgl_color *);
void xgl_triangle_list(Xgl_ctx, Xgl_facet_list *, Xgl_pt_list *, Xgl_tlist_flags);
void xgl_multipolyline(Xgl_ctx, Xgl_bbox *, Xgl_usgn32, Xgl_pt_list []);
void xgl_multimarker(Xgl_ctx, Xgl_pt_list *);
void xgl_context_flush(Xgl_ctx, Xgl_flush_action);
void xgl_context_new_frame(Xgl_ctx);
void xgl_window_raster_resize(Xgl_win_ras);

#endif
