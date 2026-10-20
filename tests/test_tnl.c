/* MIT licensed. Object-space batching and pre-transform pipeline regressions. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <GL/osmesa.h>
#include "context.h"
#include "tnl/t_context.h"
#include "xgl_tnl.h"

static int checks, failures, callbacks;
static GLboolean consume_batch;
static struct oldengl_xgl_vertex captured;
static GLuint captured_count;

static void check(int condition, const char *name)
{
    ++checks;
    if (!condition) { ++failures; fprintf(stderr, "FAIL: %s\n", name); }
}

GLboolean oldengl_xgl_xmesa_draw_object_batch(GLcontext *ctx,
                                             const struct oldengl_xgl_batch *batch)
{
    (void)ctx;
    ++callbacks;
    captured = batch->vertices[0];
    captured_count = batch->vertex_count;
    return consume_batch;
}

struct fixture {
    struct vertex_buffer vb;
    GLvector4f object;
    GLvector3f normal;
    GLvector4f texture;
    struct gl_client_array color;
    GLfloat positions[8][4], normals[8][3], texcoords[8][4];
    GLubyte colors[8][4];
    GLuint primitive[16], length[16], flags[8], indices[8];
};

static void setup(struct fixture *f, GLenum mode)
{
    unsigned int i;
    memset(f, 0, sizeof(*f));
    for (i = 0; i < 8; ++i) {
        f->positions[i][0] = (GLfloat)i;
        f->positions[i][3] = 1.0f;
        f->normals[i][0] = (GLfloat)i;
        f->normals[i][2] = 1.0f;
        f->colors[i][0] = (GLubyte)(i * 20);
        f->colors[i][1] = (GLubyte)(255 - i);
        f->colors[i][3] = 255;
        f->texcoords[i][0] = (GLfloat)i * 0.125f;
        f->texcoords[i][1] = (GLfloat)i * 0.25f;
        f->texcoords[i][2] = 0.5f;
        f->texcoords[i][3] = 1.0f;
        f->indices[i] = i;
    }
    f->object.data = f->positions;
    f->object.count = 8;
    f->object.size = 4;
    f->object.stride = sizeof(f->positions[0]);
    f->normal.data = f->normals;
    f->normal.count = 8;
    f->normal.stride = sizeof(f->normals[0]);
    f->color.Ptr = f->colors;
    f->color.Size = 4;
    f->color.Type = GL_UNSIGNED_BYTE;
    f->color.StrideB = sizeof(f->colors[0]);
    f->texture.data = f->texcoords;
    f->texture.count = 8;
    f->texture.size = 4;
    f->texture.stride = sizeof(f->texcoords[0]);
    f->primitive[0] = mode | PRIM_BEGIN | PRIM_END | PRIM_LAST;
    f->length[0] = 8;
    f->vb.Size = 16;
    f->vb.Count = 8;
    f->vb.ObjPtr = &f->object;
    f->vb.NormalPtr = &f->normal;
    f->vb.ColorPtr[0] = &f->color;
    f->vb.Primitive = f->primitive;
    f->vb.PrimitiveLength = f->length;
    f->vb.Flag = f->flags;
}

static void assembly_tests(GLcontext *ctx)
{
    struct fixture f;
    struct oldengl_xgl_batch batch;
    unsigned int i;
    GLenum modes[] = { GL_POINTS, GL_LINES, GL_LINE_LOOP, GL_LINE_STRIP,
        GL_TRIANGLES, GL_TRIANGLE_STRIP, GL_TRIANGLE_FAN, GL_QUADS,
        GL_QUAD_STRIP, GL_POLYGON };
    GLuint counts[] = { 8, 8, 16, 14, 6, 18, 18, 12, 18, 18 };
    GLenum output[] = { GL_POINTS, GL_LINES, GL_LINES, GL_LINES,
        GL_TRIANGLES, GL_TRIANGLES, GL_TRIANGLES, GL_TRIANGLES,
        GL_TRIANGLES, GL_TRIANGLES };
    GLfloat original[8][4];
    for (i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i) {
        setup(&f, modes[i]);
        memcpy(original, f.positions, sizeof(original));
        check(oldengl_xgl_tnl_build_batch(ctx, &f.vb, &batch), "core primitive mode normalizes");
        if (batch.vertices) {
            check(batch.vertex_count == counts[i] && batch.command_count == 1,
                  "primitive expansion count is correct");
            check(batch.commands[0].mode == output[i] &&
                  batch.commands[0].count == counts[i], "native command has independent primitive mode");
            check(memcmp(original, f.positions, sizeof(original)) == 0,
                  "normalization never modifies source vertices");
            if (modes[i] == GL_TRIANGLE_STRIP)
                check(batch.vertices[3].position[0] == 2 &&
                      batch.vertices[4].position[0] == 1 &&
                      batch.vertices[5].position[0] == 3,
                      "triangle strip alternates winding and preserves provoking vertex");
            if (modes[i] == GL_QUADS)
                check(batch.vertices[2].position[0] == 3 &&
                      batch.vertices[5].position[0] == 3,
                      "both quad triangles use the fourth provoking vertex");
            if (modes[i] == GL_QUAD_STRIP)
                check(batch.vertices[2].position[0] == 3 &&
                      batch.vertices[5].position[0] == 3 &&
                      batch.vertices[8].position[0] == 5,
                      "quad strip flat provoking order is preserved");
            if (modes[i] == GL_POLYGON)
                check(batch.vertices[2].position[0] == 0 &&
                      batch.vertices[5].position[0] == 0,
                      "polygon uses its first flat provoking vertex");
            if (modes[i] == GL_LINE_LOOP)
                check(batch.vertices[14].position[0] == 7 &&
                      batch.vertices[15].position[0] == 0,
                      "line loop closes to its original first vertex");
        }
        oldengl_xgl_tnl_release_batch(&batch);
    }
    setup(&f, GL_TRIANGLES);
    f.vb.Elts = f.indices;
    f.indices[0] = 7; f.indices[1] = 0; f.indices[2] = 3;
    f.length[0] = 3;
    check(oldengl_xgl_tnl_build_batch(ctx, &f.vb, &batch), "indexed batch normalizes");
    if (batch.vertices) {
        check(batch.vertices[0].position[0] == 7 && batch.vertices[1].position[0] == 0 &&
              batch.vertices[2].position[0] == 3, "element index order is preserved");
        check(fabs(batch.vertices[0].color[0] - 140.0f / 255.0f) < 0.00001 &&
              batch.vertices[0].normal[0] == 7, "indexed colors and normals match indexed positions");
    }
    oldengl_xgl_tnl_release_batch(&batch);
    setup(&f, GL_POINTS);
    f.vb.FirstPrimitive = 2;
    f.primitive[2] = GL_POINTS | PRIM_BEGIN | PRIM_END;
    f.length[2] = 2;
    f.primitive[4] = GL_TRIANGLES | PRIM_BEGIN | PRIM_END | PRIM_LAST;
    f.length[4] = 3;
    check(oldengl_xgl_tnl_build_batch(ctx, &f.vb, &batch), "multiple primitive commands normalize atomically");
    if (batch.vertices)
        check(batch.command_count == 2 && batch.vertex_count == 5 &&
              batch.commands[1].first == 2 && batch.vertices[2].position[0] == 4,
              "nonzero first primitive and command offsets are retained");
    oldengl_xgl_tnl_release_batch(&batch);
}

static void attribute_and_guard_tests(GLcontext *ctx)
{
    struct fixture f;
    struct oldengl_xgl_batch batch;
    GLbyte signed_color[4] = { -128, 0, 127, 127 };
    GLfloat constant_normal[3] = { 0, 1, 0 };
    GLuint material_mask = 0;
    setup(&f, GL_POINTS);
    f.color.Ptr = signed_color;
    f.color.Type = GL_BYTE;
    f.color.StrideB = 0;
    f.normal.data = (GLfloat (*)[3])constant_normal;
    f.normal.stride = 0;
    f.object.size = 2;
    check(oldengl_xgl_tnl_build_batch(ctx, &f.vb, &batch), "constant signed color and normal normalize");
    if (batch.vertices) {
        check(batch.vertices[7].color[0] == -1 && batch.vertices[7].color[2] == 1 &&
              fabs(batch.vertices[7].color[1] - 1.0f / 255.0f) < 0.00001,
              "signed integer colors use OpenGL normalization");
        check(batch.vertices[7].normal[1] == 1 && batch.vertices[7].position[2] == 0 &&
              batch.vertices[7].position[3] == 1, "constant normal and implicit vertex components are expanded");
    }
    oldengl_xgl_tnl_release_batch(&batch);
#define REJECT(NAME) do { check(!oldengl_xgl_tnl_build_batch(ctx, &f.vb, &batch), NAME); \
    check(!batch.vertices && !batch.commands, "rejected batch retains no temporary allocation"); } while (0)
    setup(&f, GL_TRIANGLES); f.positions[0][3] = 2; REJECT("homogeneous source w falls back");
    setup(&f, GL_TRIANGLES); f.positions[0][0] = (GLfloat)HUGE_VAL; REJECT("nonfinite source position falls back");
    setup(&f, GL_TRIANGLES); f.flags[0] = VERT_MATERIAL; REJECT("pervertex material flag falls back");
    setup(&f, GL_TRIANGLES); f.flags[0] = VERT_EVAL_C1; REJECT("unevaluated source flag falls back");
    setup(&f, GL_TRIANGLES); f.vb.MaterialMask = &material_mask; REJECT("material change storage falls back");
    setup(&f, GL_TRIANGLE_STRIP); f.primitive[0] &= ~PRIM_END; REJECT("wrapped incomplete strip falls back");
    setup(&f, GL_POINTS); f.object.size = 1; REJECT("invalid position size falls back");
    setup(&f, GL_POINTS); f.length[0] = OLDENGL_XGL_MAX_BATCH_VERTICES + 1; REJECT("oversized batch falls back before accessing input");
    setup(&f, GL_POINTS); f.vb.Elts = f.indices; f.indices[0] = 8; REJECT("out of range index falls back");
    setup(&f, GL_POINTS); f.color.Type = GL_2_BYTES; REJECT("unknown color representation falls back");
    setup(&f, GL_POINTS); f.primitive[0] &= ~PRIM_LAST; f.length[0] = 0; REJECT("unterminated empty command falls back");
#undef REJECT
}

static void draw_triangle(void)
{
    glColor4f(0.25f, 0.5f, 0.75f, 1.0f);
    glNormal3f(0, 0, 1);
    glBegin(GL_TRIANGLES);
    glVertex3f(-0.75f, -0.75f, 0);
    glVertex3f(0.75f, -0.75f, 0);
    glVertex3f(0, 0.75f, 0);
    glEnd();
    glFinish();
}

static void texture_coordinate_tests(GLcontext *ctx)
{
    struct fixture f;
    struct oldengl_xgl_batch batch;
    GLfloat original[4], coordinates[8][4];
    memcpy(original, ctx->Current.Texcoord[0], sizeof(original));
    ctx->Current.Texcoord[0][0] = 0.125f;
    ctx->Current.Texcoord[0][1] = 0.25f;
    ctx->Current.Texcoord[0][2] = 0.5f;
    ctx->Current.Texcoord[0][3] = 2.0f;
    setup(&f, GL_POINTS);
    check(oldengl_xgl_tnl_build_batch(ctx, &f.vb, &batch), "missing coordinate array uses current unit-zero coordinates");
    if (batch.vertices)
        check(batch.vertices[7].texcoord[0] == 0.125f &&
              batch.vertices[7].texcoord[1] == 0.25f &&
              batch.vertices[7].texcoord[2] == 0.5f &&
              batch.vertices[7].texcoord[3] == 2.0f,
              "current s, t, r and projective q are retained");
    oldengl_xgl_tnl_release_batch(&batch);
    memcpy(ctx->Current.Texcoord[0], original, sizeof(original));
    setup(&f, GL_POINTS);
    memcpy(coordinates, f.texcoords, sizeof(coordinates));
    f.vb.TexCoordPtr[0] = &f.texture;
    f.texture.size = 2;
    f.vb.Elts = f.indices;
    f.indices[0] = 7;
    f.length[0] = 1;
    check(oldengl_xgl_tnl_build_batch(ctx, &f.vb, &batch), "indexed padded texture coordinate vector normalizes");
    if (batch.vertices) {
        check(batch.vertices[0].texcoord[0] == 0.875f &&
              batch.vertices[0].texcoord[1] == 1.75f &&
              batch.vertices[0].texcoord[2] == 0.0f &&
              batch.vertices[0].texcoord[3] == 1.0f,
              "texture coordinates use element indices, byte strides and implicit r/q");
        check(memcmp(coordinates, f.texcoords, sizeof(coordinates)) == 0,
              "normalization leaves raw coordinate source untouched");
    }
    oldengl_xgl_tnl_release_batch(&batch);
    setup(&f, GL_POINTS);
    f.vb.TexCoordPtr[0] = &f.texture;
    f.texture.size = 1;
    f.texcoords[0][0] = 0.375f;
    check(oldengl_xgl_tnl_build_batch(ctx, &f.vb, &batch), "one-component texture coordinates normalize");
    if (batch.vertices)
        check(batch.vertices[0].texcoord[0] == 0.375f &&
              batch.vertices[0].texcoord[1] == 0 &&
              batch.vertices[0].texcoord[2] == 0 && batch.vertices[0].texcoord[3] == 1,
              "one-component texture coordinates receive GL implicit values");
    oldengl_xgl_tnl_release_batch(&batch);
    setup(&f, GL_POINTS);
    f.vb.TexCoordPtr[0] = &f.texture;
    f.texture.stride = 0;
    f.texture.count = 1;
    f.texcoords[0][3] = 0.5f;
    check(oldengl_xgl_tnl_build_batch(ctx, &f.vb, &batch), "constant projective coordinate vector normalizes");
    if (batch.vertices)
        check(batch.vertices[7].texcoord[2] == 0.5f &&
              batch.vertices[7].texcoord[3] == 0.5f,
              "zero-stride coordinates remain constant through the batch");
    oldengl_xgl_tnl_release_batch(&batch);
    setup(&f, GL_POINTS);
    f.vb.TexCoordPtr[0] = &f.texture;
    f.texcoords[0][1] = (GLfloat)HUGE_VAL;
    check(!oldengl_xgl_tnl_build_batch(ctx, &f.vb, &batch) &&
          !batch.vertices && !batch.commands, "nonfinite texture coordinates fall back atomically");
    setup(&f, GL_POINTS);
    f.vb.TexCoordPtr[0] = &f.texture;
    f.texture.size = 5;
    check(!oldengl_xgl_tnl_build_batch(ctx, &f.vb, &batch) &&
          !batch.vertices && !batch.commands, "invalid coordinate size falls back atomically");
}

static void stage_tests(GLcontext *ctx)
{
    GLubyte pixel[4];
    GLfloat current[4];
    GLfloat array_vertices[3][3] = { {-0.75f, -0.75f, 0}, {0.75f, -0.75f, 0}, {0, 0.75f, 0} };
    GLshort array_texture[3][2] = { {2, 4}, {3, 5}, {6, 7} };
    GLuint stages;
    oldengl_xgl_tnl_install(ctx);
    stages = TNL_CONTEXT(ctx)->pipeline.nr_stages;
    oldengl_xgl_tnl_install(ctx);
    check(TNL_CONTEXT(ctx)->pipeline.nr_stages == stages && stages > 8,
          "pre-transform installation is idempotent and preserves default stages");
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.125f, 0, 0);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    consume_batch = GL_TRUE;
    callbacks = 0;
    glTexCoord4f(0.125f, 0.25f, 0.5f, 2.0f);
    draw_triangle();
    check(callbacks > 0 && captured_count == 3, "ordinary GL immediate draw reaches pre-transform transport");
    check(captured.position[0] == -0.75f && captured.position[1] == -0.75f &&
          captured.position[3] == 1, "transport sees original object coordinates before modelview");
    check(captured.color[0] == 0.25f && captured.normal[2] == 1,
          "transport sees pre-lighting attributes");
    check(captured.texcoord[0] == 0.125f && captured.texcoord[2] == 0.5f &&
          captured.texcoord[3] == 2.0f,
          "pre-transform stage imports raw unit-zero texture coordinates");
    glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    check(pixel[0] == 0 && pixel[1] == 0 && pixel[2] == 0,
          "accepted native batch skips CPU transformation and rasterization");
    glGetFloatv(GL_CURRENT_COLOR, current);
    check(current[0] == 0.25f && current[2] == 0.75f,
          "accepted batch preserves GL current attribute side effects");
    consume_batch = GL_FALSE;
    callbacks = 0;
    draw_triangle();
    glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    check(callbacks > 0 && abs((int)pixel[0] - 64) <= 1 &&
          abs((int)pixel[1] - 128) <= 1 && abs((int)pixel[2] - 191) <= 1,
          "declined native batch resumes complete Mesa pipeline with fresh inputs");
    check(glGetError() == GL_NO_ERROR, "stage integration preserves GL error state");
    consume_batch = GL_TRUE;
    callbacks = 0;
    glVertexPointer(3, GL_FLOAT, 0, array_vertices);
    glTexCoordPointer(2, GL_SHORT, 0, array_texture);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glFinish();
    check(callbacks > 0 && captured.texcoord[0] == 2 && captured.texcoord[1] == 4 &&
          captured.texcoord[2] == 0 && captured.texcoord[3] == 1,
          "typed GL texture-coordinate arrays reach native transport without color normalization");
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    check(glGetError() == GL_NO_ERROR, "typed coordinate import preserves GL error state");
}

int main(void)
{
    GLubyte framebuffer[64 * 64 * 4];
    OSMesaContext context = OSMesaCreateContext(OSMESA_RGBA, NULL);
    GLcontext *core;
    if (!context || !OSMesaMakeCurrent(context, framebuffer, GL_UNSIGNED_BYTE, 64, 64))
        return 1;
    core = _mesa_get_current_context();
    assembly_tests(core);
    attribute_and_guard_tests(core);
    texture_coordinate_tests(core);
    stage_tests(core);
    _mesa_make_current(NULL, NULL);
    OSMesaDestroyContext(context);
    printf("Object-space TNL: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
