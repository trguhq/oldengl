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
typedef unsigned int Xgl_trans_member;
typedef int Xgl_pt_type;
typedef int Xgl_facet_type;
typedef struct oldengl_mock_xgl *Xgl_object;
typedef Xgl_object Xgl_sys_state;
typedef Xgl_object Xgl_ctx;
typedef Xgl_object Xgl_ras;
typedef Xgl_object Xgl_win_ras;
typedef Xgl_object Xgl_mem_ras;
typedef Xgl_object Xgl_marker;
typedef Xgl_object Xgl_trans;
typedef Xgl_object Xgl_light;
typedef Xgl_object Xgl_tmap;
typedef Xgl_object Xgl_mipmap_texture;
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
typedef struct { double x, y, z; } Xgl_pt_d3d;
typedef struct { Xgl_pt_d3d pt, normal; } Xgl_plane;
typedef float Xgl_matrix_f3d[4][4];
typedef struct { float x, y, z; Xgl_color color; } Xgl_pt_color_f3d;
typedef struct { float x, y, z; Xgl_color color; Xgl_pt_f3d normal; } Xgl_pt_color_normal_f3d;
typedef struct { float x, y, z; Xgl_color color; float data[1]; } Xgl_pt_color_data_f3d;
typedef struct { float x, y, z; Xgl_color color; Xgl_pt_f3d normal; float data[1]; } Xgl_pt_color_normal_data_f3d;
typedef struct { float x, y, z, w; } Xgl_pt_f3h;
typedef struct { int unused; } Xgl_bbox;
typedef union {
    Xgl_pt_i2d *i2d;
    Xgl_pt_f3d *f3d;
    Xgl_pt_color_f3d *color_f3d;
    Xgl_pt_color_normal_f3d *color_normal_f3d;
    Xgl_pt_f3h *f3h;
    Xgl_pt_color_data_f3d *color_data_f3d;
    Xgl_pt_color_normal_data_f3d *color_normal_data_f3d;
} Xgl_pt_ptr_union;
typedef struct {
    Xgl_pt_type pt_type;
    Xgl_bbox *bbox;
    Xgl_usgn32 num_pts, num_data_values;
    Xgl_pt_ptr_union pts;
} Xgl_pt_list;
/* Subset of the documented texture descriptors used by the native path. */
typedef int Xgl_texture_interp_method;
typedef int Xgl_texture_boundary;
typedef float Xgl_matrix_f2d[3][2];
typedef struct { Xgl_texture_interp_method filter1, filter2; } Xgl_texture_interp_info;
typedef struct {
    int comp, texture_op;
    union {
        struct { struct { Xgl_color_rgb c1, c2; } rgb; } blend;
        struct { struct { Xgl_color_rgb c; } rgb; } decal;
        struct { struct { Xgl_color_rgb c; } rgb; } blend_int;
    } op;
} Xgl_render_component_desc;
typedef struct {
    Xgl_object texture_map;
    float max_u_freq, max_v_freq;
    Xgl_texture_boundary u_boundary, v_boundary;
    Xgl_usgn8 boundary_values[4];
    float depth_interp_factor;
    Xgl_texture_interp_info interp_info;
    Xgl_matrix_f2d orientation_matrix;
} Xgl_texture_general_mipmap_desc;
typedef struct {
    int texture_type;
    union { struct {
        Xgl_usgn32 num_render_comp_desc;
        Xgl_render_component_desc render_component_desc[2];
        Xgl_usgn32 num_channels[2], channel_number[2];
    } color_info; } comp_info;
    union { Xgl_texture_general_mipmap_desc mipmap; } texture_info;
} Xgl_texture_general_desc;
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
    XGL_ROP_COPY, XGL_RENDER_DRAW_BUFFER = 0x10000,
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
    XGL_CTX_NEW_FRAME_CLEAR = 1,
    XGL_TRANS = 1000, XGL_TRANS_DATA_TYPE, XGL_TRANS_DIMENSION,
    XGL_DATA_FLT, XGL_DATA_DBL, XGL_TRANS_3D,
    XGL_CTX_LOCAL_MODEL_TRANS, XGL_CTX_VIEW_TRANS,
    XGL_CTX_VIEW_CLIP_BOUNDS, XGL_CTX_CLIP_PLANES,
    XGL_DEV_MAXIMUM_COORDINATES,
    XGL_WIN_RAS_BUFFERS_REQUESTED, XGL_WIN_RAS_BUFFERS_ALLOCATED,
    XGL_WIN_RAS_BUF_DRAW, XGL_WIN_RAS_BUF_DISPLAY,
    XGL_MEM_RAS_Z_BUFFER_ADDR,
    XGL_BUFFER_SEL_Z, XGL_RENDER_Z_BUFFER = 0x20000,
    XGL_3D_CTX_Z_BUFFER_COMP_METHOD, XGL_3D_CTX_Z_BUFFER_WRITE_MASK,
    XGL_HLHSR_Z_BUFFER,
    XGL_Z_COMP_NEVER, XGL_Z_COMP_LESS_THAN, XGL_Z_COMP_EQUAL,
    XGL_Z_COMP_LESS_THAN_OR_EQUAL, XGL_Z_COMP_GREATER_THAN,
    XGL_Z_COMP_NOT_EQUAL, XGL_Z_COMP_GREATER_THAN_OR_EQUAL, XGL_Z_COMP_ALWAYS,
    XGL_PT_COLOR_NORMAL_F3D, XGL_PT_COLOR_DATA_F3D, XGL_PT_COLOR_NORMAL_DATA_F3D,
    XGL_3D_CTX_LIGHT_NUM = 2000, XGL_3D_CTX_LIGHTS, XGL_3D_CTX_LIGHT_SWITCHES,
    XGL_LIGHT_TYPE, XGL_LIGHT_COLOR, XGL_LIGHT_DIRECTION, XGL_LIGHT_POSITION,
    XGL_LIGHT_ATTENUATION_1, XGL_LIGHT_ATTENUATION_2,
    XGL_LIGHT_SPOT_ANGLE, XGL_LIGHT_SPOT_EXPONENT,
    XGL_LIGHT_AMBIENT, XGL_LIGHT_DIRECTIONAL, XGL_LIGHT_POSITIONAL, XGL_LIGHT_SPOT,
    XGL_SURF_COLOR_CONTEXT, XGL_CTX_SURF_FRONT_COLOR,
    XGL_3D_CTX_SURF_FRONT_AMBIENT, XGL_3D_CTX_SURF_FRONT_DIFFUSE,
    XGL_3D_CTX_SURF_FRONT_SPECULAR, XGL_3D_CTX_SURF_FRONT_LIGHT_COMPONENT,
    XGL_ILLUM_PER_VERTEX, XGL_3D_CTX_SURF_FACE_DISTINGUISH,
    XGL_3D_CTX_SURF_LIGHTING_NORMAL_FLIP, XGL_3D_CTX_SURF_NORMAL_FLIP,
    XGL_3D_CTX_SURF_FRONT_TMAP_NUM = 3000, XGL_3D_CTX_SURF_BACK_TMAP_NUM,
    XGL_3D_CTX_SURF_FRONT_TMAP, XGL_3D_CTX_SURF_FRONT_TMAP_SWITCHES,
    XGL_3D_CTX_SURF_TMAP_PERSP_CORRECTION,
    XGL_MIPMAP_TEXTURE, XGL_MIPMAP_TEXTURE_LEVELS, XGL_MIPMAP_TEXTURE_MEM_RAS_LIST,
    XGL_RENDER_COMP_FINAL_COLOR, XGL_TEXTURE_BOUNDARY_CLAMP_BOUNDARY,
    XGL_TEXTURE_BOUNDARY_WRAP, XGL_TEXTURE_COORD_DATA,
    XGL_TEXTURE_INTERP_BILINEAR, XGL_TEXTURE_INTERP_MIPMAP_BILINEAR,
    XGL_TEXTURE_INTERP_MIPMAP_POINT, XGL_TEXTURE_INTERP_MIPMAP_PT_LINEAR,
    XGL_TEXTURE_INTERP_MIPMAP_TRILINEAR, XGL_TEXTURE_INTERP_POINT,
    XGL_TEXTURE_OP_BLEND_INTRINSIC, XGL_TEXTURE_OP_DECAL_INTRINSIC,
    XGL_TEXTURE_OP_MODULATE, XGL_TEXTURE_OP_NONE, XGL_TEXTURE_OP_REPLACE,
    XGL_TEXTURE_PARAM_EXPLICIT, XGL_TEXTURE_PERSP_PIXEL, XGL_TEXTURE_TYPE_MIPMAP,
    XGL_TMAP_COORD_SOURCE, XGL_TMAP_DESCRIPTOR, XGL_TMAP_PARAM_TYPE,
    XGL_TMAP_T0_INDEX, XGL_TMAP_T1_INDEX,
    XGL_LIGHT_ENABLE_COMP_AMBIENT = 1, XGL_LIGHT_ENABLE_COMP_DIFFUSE = 2,
    XGL_CLIP_XMIN = 0x1, XGL_CLIP_XMAX = 0x2,
    XGL_CLIP_YMIN = 0x4, XGL_CLIP_YMAX = 0x8,
    XGL_CLIP_ZMIN = 0x10, XGL_CLIP_ZMAX = 0x20
};

/* Separate synthetic range keeps new model-clip attributes stable. */
enum {
    XGL_3D_CTX_MODEL_CLIP_PLANE_NUM = 4000,
    XGL_3D_CTX_MODEL_CLIP_PLANES
};

enum {
    XGL_CTX_LINE_COLOR = 4050, XGL_CTX_MARKER_COLOR,
    XGL_LINE_COLOR_CONTEXT, XGL_MARKER_COLOR_CONTEXT
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
void xgl_transform_write_specific(Xgl_trans, void *, Xgl_trans_member);

#endif
