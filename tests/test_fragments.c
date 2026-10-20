/* MIT licensed. Real Mesa fragment-stage tests for XGL-captured spans. */
#include <GL/osmesa.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "context.h"
#include "state.h"
#include "swrast/s_context.h"
#include "xgl_fragments.h"

static int checks, failures;
static void check(int condition, const char *message)
{
    ++checks;
    if (!condition) { ++failures; fprintf(stderr, "FAIL: %s\n", message); }
}

static void vertices(GLcontext *ctx, SWvertex v[3], GLubyte alpha)
{
    unsigned int i;
    memset(v, 0, 3*sizeof(*v));
    v[1].win[0] = 16; v[2].win[1] = 16;
    for (i = 0; i < 3; ++i) {
        v[i].win[2] = ctx->DepthMaxF*0.25f;
        v[i].win[3] = 1;
        v[i].color[0] = 255; v[i].color[3] = alpha;
        v[i].fog = 1;
        v[i].texcoord[0][3] = 1;
    }
}

static void emit(GLcontext *ctx, SWvertex v[3], const GLubyte rgb[1][3],
                   GLboolean textured)
{
    struct oldengl_xgl_fragment_setup setup;
    _mesa_update_state(ctx);
    _swrast_validate_derived(ctx);
    check(oldengl_xgl_fragments_prepare(ctx, GL_TRIANGLES,
        v, v+1, v+2, &setup), "prepare captured fragment attributes");
    if (!setup.workspace) return;
    setup.texture_rgb_complete = textured;
    oldengl_xgl_fragments_emit_span(&setup, 4, 4, 1, rgb);
    oldengl_xgl_fragments_release(&setup);
}

static void pixel(int r, int g, int b, const char *message)
{
    GLubyte rgba[4];
    glReadPixels(4,4,1,1,GL_RGBA,GL_UNSIGNED_BYTE,rgba);
    check(abs((int)rgba[0]-r)<=1 && abs((int)rgba[1]-g)<=1 &&
        abs((int)rgba[2]-b)<=1, message);
}

int main(void)
{
    OSMesaContext os;
    GLcontext *ctx;
    GLubyte buffer[16*16*4], stencil, texture[4] = {0,255,0,64};
    GLubyte stipple[128];
    const GLubyte red[1][3] = {{255,0,0}}, green[1][3] = {{0,255,0}};
    SWvertex v[3], point;
    GLfloat depth, expected_depth;
    GLuint name, expected_z;
    struct oldengl_xgl_fragment_setup setup;
    GLubyte expected[4], actual[4];
    const GLubyte image2[16] = {255,0,0,255, 0,255,0,255,
        0,0,255,255, 255,255,255,255};
    os = OSMesaCreateContextExt(OSMESA_RGBA,24,8,0,NULL);
    check(os != NULL, "create real Mesa fragment-test context");
    if (!os) return 1;
    check(OSMesaMakeCurrent(os,buffer,GL_UNSIGNED_BYTE,16,16),"bind fragment-test framebuffer");
    ctx = _mesa_get_current_context();
    vertices(ctx,v,128);
    glClearColor(0,0,1,1); glClearDepth(1); glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    emit(ctx,v,red,GL_FALSE);
    pixel(128,0,127,"software blending consumes native RGB and reconstructed alpha");
    glDisable(GL_BLEND); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST); glEnable(GL_ALPHA_TEST); glAlphaFunc(GL_GREATER,0.75f);
    emit(ctx,v,red,GL_FALSE);
    pixel(0,0,255,"alpha rejection preserves destination color");
    glReadPixels(4,4,1,1,GL_DEPTH_COMPONENT,GL_FLOAT,&depth);
    check(depth==1.0f,"alpha rejection precedes depth write");
    glDisable(GL_ALPHA_TEST);
    glEnable(GL_STENCIL_TEST); glStencilFunc(GL_ALWAYS,3,255);
    glStencilOp(GL_KEEP,GL_KEEP,GL_REPLACE);
    emit(ctx,v,red,GL_FALSE);
    glReadPixels(4,4,1,1,GL_STENCIL_INDEX,GL_UNSIGNED_BYTE,&stencil);
    check(stencil==3,"captured fragments apply stencil depth-pass operation");
    glStencilFunc(GL_EQUAL,4,255); emit(ctx,v,green,GL_FALSE);
    pixel(255,0,0,"stencil rejection preserves preceding native fragment result");
    glDisable(GL_STENCIL_TEST); glDisable(GL_DEPTH_TEST);
    glClearColor(1,0,1,1); glClear(GL_COLOR_BUFFER_BIT);
    glColorMask(GL_FALSE,GL_TRUE,GL_FALSE,GL_FALSE); emit(ctx,v,green,GL_FALSE);
    pixel(255,255,255,"fragment channel mask preserves unselected channels");
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    memset(stipple,0,sizeof(stipple)); glPolygonStipple(stipple);
    glEnable(GL_POLYGON_STIPPLE); emit(ctx,v,red,GL_FALSE);
    pixel(255,255,255,"polygon stipple applies to captured polygon spans");
    glDisable(GL_POLYGON_STIPPLE);
    glClearColor(0,0,1,1); glClear(GL_COLOR_BUFFER_BIT);
    glGenTextures(1,&name); glBindTexture(GL_TEXTURE_2D,name);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,texture);
    glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
    glEnable(GL_TEXTURE_2D); glEnable(GL_ALPHA_TEST); glAlphaFunc(GL_GREATER,0.5f);
    emit(ctx,v,red,GL_FALSE);
    pixel(0,0,255,"software texture alpha is tested before color/depth writes");
    glAlphaFunc(GL_GREATER,0.1f); emit(ctx,v,red,GL_FALSE);
    pixel(0,255,0,"unsupported texture stage completes captured native primary RGB");
    glDisable(GL_ALPHA_TEST); glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glClear(GL_COLOR_BUFFER_BIT); emit(ctx,v,green,GL_TRUE);
    pixel(0,64,191,"native textured RGB is not textured twice and software preserves texture alpha");
    glDisable(GL_BLEND);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,image2);
    vertices(ctx,v,255);
    v[0].win[3] = 1; v[1].win[3] = 0.5f; v[2].win[3] = 0.25f;
    v[1].texcoord[0][0] = 3; v[1].texcoord[0][3] = 2;
    v[2].texcoord[0][1] = 3;
    glClear(GL_COLOR_BUFFER_BIT);
    glBegin(GL_TRIANGLES);
    glTexCoord4f(0,0,0,1); glVertex4f(-1,-1,0,1);
    glTexCoord4f(3,0,0,2); glVertex4f(2,-2,0,2);
    glTexCoord4f(0,3,0,1); glVertex4f(-4,4,0,4);
    glEnd(); glFinish();
    glReadPixels(4,4,1,1,GL_RGBA,GL_UNSIGNED_BYTE,expected);
    glClear(GL_COLOR_BUFFER_BIT); emit(ctx,v,red,GL_FALSE);
    glReadPixels(4,4,1,1,GL_RGBA,GL_UNSIGNED_BYTE,actual);
    check(!memcmp(actual,expected,4),"perspective texture attributes and varying q agree with real Mesa triangle");
    pixel(0,255,0,"perspective texture result differs from affine coordinate interpolation");
    point = v[0]; point.win[0] = point.win[1] = 4.5f;
    point.win[3] = 0.25f;
    point.texcoord[0][0] = 0.75f; point.texcoord[0][1] = 0.25f;
    point.texcoord[0][3] = 0;
    glClear(GL_COLOR_BUFFER_BIT);
    _mesa_update_state(ctx); _swrast_validate_derived(ctx);
    check(oldengl_xgl_fragments_prepare(ctx,GL_POINTS,&point,NULL,NULL,&setup),
          "prepare point with zero texture q and nonunit reciprocal clip w");
    if (setup.workspace) {
        oldengl_xgl_fragments_emit_span(&setup,4,4,1,red);
        oldengl_xgl_fragments_release(&setup);
    }
    pixel(0,255,0,"point zero-q convention uses raw coordinates without reciprocal clip w");
    v[2].texcoord[0][3] = -1;
    _mesa_update_state(ctx); _swrast_validate_derived(ctx);
    check(!oldengl_xgl_fragments_prepare(ctx,GL_TRIANGLES,v,v+1,v+2,&setup) &&
        !setup.workspace,"projective texture pole falls back before allocating or committing");
    v[2].texcoord[0][3] = 1;
    v[0].win[0] = (GLfloat)HUGE_VAL;
    check(!oldengl_xgl_fragments_prepare(ctx,GL_TRIANGLES,v,v+1,v+2,&setup),
          "nonfinite source attributes fall back before target writes");
    glDeleteTextures(1,&name);
    glDisable(GL_TEXTURE_2D); glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_ALWAYS); glDepthMask(GL_TRUE);
    glClear(GL_DEPTH_BUFFER_BIT);
    glBegin(GL_TRIANGLES);
    glVertex3f(-1,-1,-0.5f); glVertex3f(1,-1,0); glVertex3f(-1,0.4f,0.5f);
    glEnd(); glFinish();
    glReadPixels(4,4,1,1,GL_DEPTH_COMPONENT,GL_FLOAT,&expected_depth);
    expected_z = ((GLuint *)ctx->DrawBuffer->DepthBuffer)[4*16+4];
    vertices(ctx,v,255);
    v[2].win[1] = 11.2f;
    v[1].win[2] = ctx->DepthMaxF*0.5f;
    v[2].win[2] = ctx->DepthMaxF*0.75f;
    glClear(GL_DEPTH_BUFFER_BIT); emit(ctx,v,red,GL_FALSE);
    glReadPixels(4,4,1,1,GL_DEPTH_COMPONENT,GL_FLOAT,&depth);
    check(fabs(depth-expected_depth)<0.000001,
          "varying triangle depth matches Mesa's snapped XY gradient");
    check(((GLuint *)ctx->DrawBuffer->DepthBuffer)[4*16+4] == expected_z,
          "captured varying depth preserves exact Mesa integer Z stepping");
    glDepthFunc(GL_EQUAL); emit(ctx,v,green,GL_FALSE);
    pixel(0,255,0,"hybrid GL_EQUAL accepts matching varying software depth");
    glDepthFunc(GL_LESS); emit(ctx,v,red,GL_FALSE);
    pixel(0,255,0,"hybrid GL_LESS rejects matching varying depth");
    ctx->OcclusionResult = GL_FALSE;
    check(oldengl_xgl_fragments_prepare(ctx,GL_TRIANGLES,v,v+1,v+2,&setup),
          "prepare exact triangle depth anchors without rendering");
    check(ctx->OcclusionResult == GL_FALSE,
          "depth attribute preparation does not report premature occlusion");
    oldengl_xgl_fragments_release(&setup);
    _mesa_make_current(NULL,NULL);
    OSMesaDestroyContext(os);
    printf("Captured fragment pipeline: %d checks, %d failures\n",checks,failures);
    return failures ? 1 : 0;
}
