/* MIT licensed. Object-space batches before Mesa's transform/light stages. */
#ifndef OLDENGL_XGL_TNL_H
#define OLDENGL_XGL_TNL_H

#include "mtypes.h"

struct vertex_buffer;

struct oldengl_xgl_vertex {
    GLfloat position[4];
    GLfloat color[4];
    GLfloat normal[3];
    GLfloat texcoord[4];          /* raw texture unit 0: s, t, r, q */
};

struct oldengl_xgl_command {
    GLenum mode;                 /* GL_POINTS, GL_LINES, or GL_TRIANGLES */
    GLuint first, count;
};

struct oldengl_xgl_batch {
    struct oldengl_xgl_vertex *vertices;
    struct oldengl_xgl_command *commands;
    GLuint vertex_count, command_count;
};

/* Bounded temporary storage; larger/unsupported batches use Mesa normally. */
#define OLDENGL_XGL_MAX_BATCH_VERTICES 65536U

GLboolean oldengl_xgl_tnl_build_batch(const GLcontext *,
                                    const struct vertex_buffer *,
                                    struct oldengl_xgl_batch *);
void oldengl_xgl_tnl_release_batch(struct oldengl_xgl_batch *);
void oldengl_xgl_tnl_install(GLcontext *);

/* The XMesa transport preflights all native state before issuing any draw.
 * True means the entire batch was rendered; unsupported state returns false
 * before drawing. A native device failure also returns false for software
 * replay; authoritative software buffers are committed only on success.
 * The batch's storage belongs to the caller and expires after this call. */
GLboolean oldengl_xgl_xmesa_draw_object_batch(GLcontext *,
                                            const struct oldengl_xgl_batch *);

#endif
