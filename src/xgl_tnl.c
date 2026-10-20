/* MIT licensed. Normalize and dispatch complete object-space vertex batches. */
#include "xgl_tnl.h"
#include "tnl/t_context.h"
#include "tnl/t_pipeline.h"
#include "mmath.h"
#include <float.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static GLboolean finite_value(GLfloat value)
{
    return value >= -FLT_MAX && value <= FLT_MAX;
}

static const GLubyte *element_address(const void *base, GLuint index,
                                      size_t stride)
{
    if (!base || (stride && (size_t)index > ((size_t)-1) / stride))
        return NULL;
    return (const GLubyte *)base + (size_t)index * stride;
}

static GLboolean color_component(const GLubyte *source, GLenum type,
                                  GLuint component, GLfloat *result)
{
    GLbyte b;
    GLubyte ub;
    GLshort s;
    GLushort us;
    GLint i;
    GLuint ui;
    GLfloat f;
    GLdouble d;
    switch (type) {
    case GL_BYTE:
        memcpy(&b, source + component * sizeof(b), sizeof(b));
        *result = BYTE_TO_FLOAT(b); break;
    case GL_UNSIGNED_BYTE:
        memcpy(&ub, source + component * sizeof(ub), sizeof(ub));
        *result = (GLfloat)ub / 255.0f; break;
    case GL_SHORT:
        memcpy(&s, source + component * sizeof(s), sizeof(s));
        *result = SHORT_TO_FLOAT(s); break;
    case GL_UNSIGNED_SHORT:
        memcpy(&us, source + component * sizeof(us), sizeof(us));
        *result = USHORT_TO_FLOAT(us); break;
    case GL_INT:
        memcpy(&i, source + component * sizeof(i), sizeof(i));
        *result = INT_TO_FLOAT(i); break;
    case GL_UNSIGNED_INT:
        memcpy(&ui, source + component * sizeof(ui), sizeof(ui));
        *result = UINT_TO_FLOAT(ui); break;
    case GL_FLOAT:
        memcpy(&f, source + component * sizeof(f), sizeof(f));
        *result = f; break;
    case GL_DOUBLE:
        memcpy(&d, source + component * sizeof(d), sizeof(d));
        if (!(d >= -FLT_MAX && d <= FLT_MAX)) return GL_FALSE;
        *result = (GLfloat)d; break;
    default:
        return GL_FALSE;
    }
    return finite_value(*result);
}

static GLboolean read_vertex(const GLcontext *ctx,
                              const struct vertex_buffer *vb, GLuint position,
                              struct oldengl_xgl_vertex *result)
{
    GLuint index = vb->Elts ? vb->Elts[position] : position;
    const struct gl_client_array *color = vb->ColorPtr[0];
    const GLvector4f *texture = vb->TexCoordPtr[0];
    const GLubyte *address;
    GLuint component;
    if (index >= vb->Count || index >= vb->ObjPtr->count)
        return GL_FALSE;
    if (vb->Flag && (vb->Flag[index] & (VERT_MATERIAL | VERT_EVAL_ANY)))
        return GL_FALSE;
    address = element_address(vb->ObjPtr->data, index, vb->ObjPtr->stride);
    if (!address) return GL_FALSE;
    result->position[0] = result->position[1] = result->position[2] = 0.0f;
    result->position[3] = 1.0f;
    memcpy(result->position, address, vb->ObjPtr->size * sizeof(GLfloat));
    for (component = 0; component < 4; ++component)
        if (!finite_value(result->position[component])) return GL_FALSE;
    /* XGL's colored/normal 3D point records have no homogeneous w member.
     * Keep arbitrary homogeneous source vertices on the full Mesa path. */
    if (result->position[3] != 1.0f) return GL_FALSE;
    if (color) {
        if (!color->Ptr || color->Size < 3 || color->Size > 4 || color->StrideB < 0)
            return GL_FALSE;
        address = element_address(color->Ptr, index, (size_t)color->StrideB);
        if (!address) return GL_FALSE;
        result->color[3] = 1.0f;
        for (component = 0; component < (GLuint)color->Size; ++component)
            if (!color_component(address, color->Type, component,
                                  &result->color[component])) return GL_FALSE;
    } else {
        memcpy(result->color, ctx->Current.Color, sizeof(result->color));
        for (component = 0; component < 4; ++component)
            if (!finite_value(result->color[component])) return GL_FALSE;
    }
    if (vb->NormalPtr && vb->NormalPtr->data) {
        if (vb->NormalPtr->count && index >= vb->NormalPtr->count)
            return GL_FALSE;
        address = element_address(vb->NormalPtr->data, index, vb->NormalPtr->stride);
        if (!address) return GL_FALSE;
        memcpy(result->normal, address, sizeof(result->normal));
    } else {
        memcpy(result->normal, ctx->Current.Normal, sizeof(result->normal));
    }
    for (component = 0; component < 3; ++component)
        if (!finite_value(result->normal[component])) return GL_FALSE;
    if (texture) {
        if (!texture->data || texture->size < 1 || texture->size > 4 ||
            (texture->stride && texture->count && index >= texture->count))
            return GL_FALSE;
        address = element_address(texture->data, index, texture->stride);
        if (!address) return GL_FALSE;
        result->texcoord[0] = result->texcoord[1] = result->texcoord[2] = 0.0f;
        result->texcoord[3] = 1.0f;
        memcpy(result->texcoord, address, texture->size * sizeof(GLfloat));
    } else {
        memcpy(result->texcoord, ctx->Current.Texcoord[0], sizeof(result->texcoord));
    }
    for (component = 0; component < 4; ++component)
        if (!finite_value(result->texcoord[component])) return GL_FALSE;
    return GL_TRUE;
}

static GLuint expanded_count(GLenum mode, GLuint count)
{
    switch (mode) {
    case GL_POINTS: return count;
    case GL_LINES: return (count / 2) * 2;
    case GL_LINE_STRIP: return count > 1 ? (count - 1) * 2 : 0;
    case GL_LINE_LOOP: return count > 1 ? count * 2 : 0;
    case GL_TRIANGLES: return (count / 3) * 3;
    case GL_TRIANGLE_STRIP: case GL_TRIANGLE_FAN: case GL_POLYGON:
        return count > 2 ? (count - 2) * 3 : 0;
    case GL_QUADS: return (count / 4) * 6;
    case GL_QUAD_STRIP: return count > 3 ? ((count - 2) / 2) * 6 : 0;
    default: return 0;
    }
}

static GLboolean measure(const struct vertex_buffer *vb,
                          GLuint *vertex_count, GLuint *command_count)
{
    GLuint first = vb->FirstPrimitive, groups = 0;
    *vertex_count = *command_count = 0;
    for (;;) {
        GLuint flags, length, expanded;
        GLenum mode;
        if (first >= vb->Size || first > OLDENGL_XGL_MAX_BATCH_VERTICES ||
            ++groups > OLDENGL_XGL_MAX_BATCH_VERTICES)
            return GL_FALSE;
        flags = vb->Primitive[first];
        length = vb->PrimitiveLength[first];
        mode = flags & PRIM_MODE_MASK;
        if (length > OLDENGL_XGL_MAX_BATCH_VERTICES - first ||
            (!vb->Elts && (first > vb->Count || length > vb->Count - first)))
            return GL_FALSE;
        if (length) {
            if (mode > GL_POLYGON ||
                (flags & (PRIM_BEGIN | PRIM_END)) != (PRIM_BEGIN | PRIM_END))
                return GL_FALSE;
            expanded = expanded_count(mode, length);
            if (expanded > OLDENGL_XGL_MAX_BATCH_VERTICES - *vertex_count)
                return GL_FALSE;
            *vertex_count += expanded;
            if (expanded) ++*command_count;
        } else if (!(flags & PRIM_LAST)) {
            return GL_FALSE;
        }
        if (flags & PRIM_LAST) return GL_TRUE;
        first += length;
    }
}

static GLboolean emit_vertex(const GLcontext *ctx,
                              const struct vertex_buffer *vb, GLuint position,
                              struct oldengl_xgl_batch *batch, GLuint *next)
{
    if (!read_vertex(ctx, vb, position, &batch->vertices[*next])) return GL_FALSE;
    ++*next;
    return GL_TRUE;
}

GLboolean oldengl_xgl_tnl_build_batch(const GLcontext *ctx,
                                      const struct vertex_buffer *vb,
                                      struct oldengl_xgl_batch *batch)
{
    GLuint vertices, commands, first, next = 0, command = 0;
    if (!batch) return GL_FALSE;
    memset(batch, 0, sizeof(*batch));
    if (!ctx || !vb || !vb->ObjPtr || !vb->ObjPtr->data ||
        vb->ObjPtr->size < 2 || vb->ObjPtr->size > 4 ||
        !vb->Primitive || !vb->PrimitiveLength || !vb->Count ||
        vb->Material || vb->MaterialMask ||
        !measure(vb, &vertices, &commands) || !vertices || !commands)
        return GL_FALSE;
    batch->vertices = (struct oldengl_xgl_vertex *)
        malloc((size_t)vertices * sizeof(*batch->vertices));
    batch->commands = (struct oldengl_xgl_command *)
        malloc((size_t)commands * sizeof(*batch->commands));
    if (!batch->vertices || !batch->commands) goto failed;
    first = vb->FirstPrimitive;
    for (;;) {
        GLuint flags = vb->Primitive[first];
        GLuint length = vb->PrimitiveLength[first];
        GLenum mode = flags & PRIM_MODE_MASK;
        GLuint count = expanded_count(mode, length);
        GLuint j;
        if (count) {
            struct oldengl_xgl_command *draw = &batch->commands[command++];
            draw->mode = mode == GL_POINTS ? GL_POINTS :
                mode <= GL_LINE_STRIP ? GL_LINES : GL_TRIANGLES;
            draw->first = next;
            draw->count = count;
#define EMIT(N) do { if (!emit_vertex(ctx, vb, first + (N), batch, &next)) goto failed; } while (0)
            switch (mode) {
            case GL_POINTS:
                for (j = 0; j < length; ++j) EMIT(j);
                break;
            case GL_LINES:
                for (j = 0; j + 1 < length; j += 2) { EMIT(j); EMIT(j + 1); }
                break;
            case GL_LINE_STRIP: case GL_LINE_LOOP:
                for (j = 1; j < length; ++j) { EMIT(j - 1); EMIT(j); }
                if (mode == GL_LINE_LOOP) { EMIT(length - 1); EMIT(0); }
                break;
            case GL_TRIANGLES:
                for (j = 0; j + 2 < length; j += 3) {
                    EMIT(j); EMIT(j + 1); EMIT(j + 2);
                }
                break;
            case GL_TRIANGLE_STRIP:
                for (j = 2; j < length; ++j) {
                    if ((j + ((flags & PRIM_PARITY) ? 1U : 0U)) & 1U) {
                        EMIT(j - 1); EMIT(j - 2);
                    } else { EMIT(j - 2); EMIT(j - 1); }
                    EMIT(j);
                }
                break;
            case GL_TRIANGLE_FAN:
                for (j = 2; j < length; ++j) { EMIT(0); EMIT(j - 1); EMIT(j); }
                break;
            case GL_POLYGON:
                /* GL_POLYGON uses its first vertex as flat provoking vertex. */
                for (j = 2; j < length; ++j) { EMIT(j - 1); EMIT(j); EMIT(0); }
                break;
            case GL_QUADS:
                for (j = 0; j + 3 < length; j += 4) {
                    EMIT(j); EMIT(j + 1); EMIT(j + 3);
                    EMIT(j + 1); EMIT(j + 2); EMIT(j + 3);
                }
                break;
            case GL_QUAD_STRIP:
                for (j = 0; j + 3 < length; j += 2) {
                    EMIT(j + 2); EMIT(j); EMIT(j + 3);
                    EMIT(j); EMIT(j + 1); EMIT(j + 3);
                }
                break;
            default:
                goto failed;
            }
#undef EMIT
        }
        if (flags & PRIM_LAST) break;
        first += length;
    }
    batch->vertex_count = next;
    batch->command_count = command;
    return GL_TRUE;
failed:
    oldengl_xgl_tnl_release_batch(batch);
    return GL_FALSE;
}

void oldengl_xgl_tnl_release_batch(struct oldengl_xgl_batch *batch)
{
    if (!batch) return;
    free(batch->vertices);
    free(batch->commands);
    memset(batch, 0, sizeof(*batch));
}

static void destroy_stage(struct gl_pipeline_stage *stage)
{
    (void)stage;
}

static void check_stage(GLcontext *ctx, struct gl_pipeline_stage *stage)
{
    stage->active = ctx->RenderMode == GL_RENDER;
}

static GLboolean run_stage(GLcontext *ctx, struct gl_pipeline_stage *stage)
{
    struct oldengl_xgl_batch batch;
    GLboolean handled;
    (void)stage;
    if (!oldengl_xgl_tnl_build_batch(ctx, &TNL_CONTEXT(ctx)->vb, &batch))
        return GL_TRUE;
    handled = oldengl_xgl_xmesa_draw_object_batch(ctx, &batch);
    oldengl_xgl_tnl_release_batch(&batch);
    return handled ? GL_FALSE : GL_TRUE;
}

static const struct gl_pipeline_stage object_stage = {
    "XGL object-space transform/light/clip dispatch",
    _NEW_RENDERMODE,
    _NEW_ALL,
    GL_TRUE,
    VERT_OBJ | VERT_RGBA | VERT_NORM | VERT_MATERIAL | VERT_TEX0,
    0,
    0, NULL,
    destroy_stage,
    check_stage,
    run_stage
};

void oldengl_xgl_tnl_install(GLcontext *ctx)
{
    const struct gl_pipeline_stage *stages[MAX_PIPELINE_STAGES + 1];
    GLuint i;
    if (!ctx || TNL_CONTEXT(ctx)->pipeline.stages[0].run == run_stage)
        return;
    stages[0] = &object_stage;
    for (i = 0; i + 1 < MAX_PIPELINE_STAGES && _tnl_default_pipeline[i]; ++i)
        stages[i + 1] = _tnl_default_pipeline[i];
    stages[i + 1] = NULL;
    _tnl_destroy_pipeline(ctx);
    _tnl_install_pipeline(ctx, stages);
}
