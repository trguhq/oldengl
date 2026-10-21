/* MIT licensed. Mesa GLU behavior through real GL and GLU implementations.
 * This is a regression suite, not GLU conformance certification.
 */
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/osmesa.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 64
#define HEIGHT 64
static GLubyte framebuffer[WIDTH*HEIGHT*4];
static int checks, failures;
static const char *group;

static void check(int condition, const char *message)
{
    ++checks;
    if (!condition) {
        ++failures;
        fprintf(stderr,"FAIL [%s]: %s\n",group,message);
    }
}

static int near(double a, double b) { return fabs(a-b) < 0.00001; }

static void pixel(int x, int y, int red, int green, int blue, const char *message)
{
    GLubyte color[4];
    int equal;
    glReadPixels(x,y,1,1,GL_RGBA,GL_UNSIGNED_BYTE,color);
    equal = abs((int)color[0]-red) <= 2 && abs((int)color[1]-green) <= 2 &&
        abs((int)color[2]-blue) <= 2;
    check(equal,message);
    if (!equal) fprintf(stderr,"  pixel (%d,%d) = %u,%u,%u, expected %d,%d,%d\n",
        x,y,color[0],color[1],color[2],red,green,blue);
}

static void reset(const char *name)
{
    group = name;
    glDisable(GL_LIGHTING); glDisable(GL_TEXTURE_1D); glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND); glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE);
    glDisable(GL_STENCIL_TEST); glDisable(GL_ALPHA_TEST); glDisable(GL_FOG);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(-1,1,-1,1,-2,2);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glMatrixMode(GL_TEXTURE); glLoadIdentity(); glMatrixMode(GL_MODELVIEW);
    glPixelStorei(GL_UNPACK_ALIGNMENT,1); glPixelStorei(GL_PACK_ALIGNMENT,1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH,0); glPixelStorei(GL_PACK_ROW_LENGTH,0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS,0); glPixelStorei(GL_PACK_SKIP_ROWS,0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS,0); glPixelStorei(GL_PACK_SKIP_PIXELS,0);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
}

static void no_gl_error(void)
{
    GLenum error = glGetError();
    check(error == GL_NO_ERROR,"no unexpected GL error");
    if (error) fprintf(stderr,"  GL error = 0x%x\n",(unsigned int)error);
}

static void matrix_tests(void)
{
    GLdouble model[16], projection[16], matrix[16], singular[16];
    GLdouble x, y, z, ox, oy, oz, ow;
    GLint viewport[4] = {0,0,WIDTH,HEIGHT};
    reset("matrices, project and unproject");
    check(gluGetString(GLU_VERSION) != NULL &&
          strncmp((const char *)gluGetString(GLU_VERSION),"1.3",3) == 0,
          "GLU reports the implemented 1.3 interface");
    check(gluCheckExtension((const GLubyte *)"GLU_EXT_nurbs_tessellator",
          gluGetString(GLU_EXTENSIONS)),"NURBS tessellator extension is advertised");
    check(!gluCheckExtension((const GLubyte *)"GLU_EXT_nurbs",
          gluGetString(GLU_EXTENSIONS)),"extension lookup matches whole tokens");
    check(gluErrorString(GLU_INVALID_ENUM) != NULL &&
          gluErrorString(GL_INVALID_ENUM) != NULL,
          "GLU describes both GL and utility errors");
    check(gluErrorString(0xdead) == NULL,"unknown error codes have no description");
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    gluPerspective(60,1.5,1,100); glGetDoublev(GL_PROJECTION_MATRIX,projection);
    check(fabs(projection[0]-1.1547005383792515) < 0.00001 &&
          fabs(projection[5]-1.7320508075688772) < 0.00001,
          "perspective field of view and aspect produce the expected scale");
    check(near(projection[10],-101.0/99.0) && near(projection[11],-1) &&
          near(projection[14],-200.0/99.0) && near(projection[15],0),
          "perspective near/far values produce the expected projection");
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    gluLookAt(0,0,5,0,0,0,0,1,0); glGetDoublev(GL_MODELVIEW_MATRIX,model);
    check(near(model[0],1) && near(model[5],1) && near(model[10],1) &&
          near(model[14],-5),"look-at translates the eye into the origin");
    check(gluProject(0.2,-0.3,0.4,model,projection,viewport,&x,&y,&z),
          "project accepts a visible object point");
    check(x > 0 && x < WIDTH && y > 0 && y < HEIGHT && z > 0 && z < 1,
          "project produces window coordinates and normalized depth");
    check(gluUnProject(x,y,z,model,projection,viewport,&ox,&oy,&oz),
          "unproject accepts the projected point");
    check(near(ox,0.2) && near(oy,-0.3) && near(oz,0.4),
          "project/unproject round trip recovers the object point");
    memset(singular,0,sizeof(singular));
    check(!gluUnProject(x,y,z,model,singular,viewport,&ox,&oy,&oz),
          "unproject rejects a singular projection");
    glLoadIdentity(); glGetDoublev(GL_MODELVIEW_MATRIX,model);
    check(gluUnProject4(48,16,0.75,1,model,model,viewport,0,1,&ox,&oy,&oz,&ow),
          "four-coordinate unproject accepts identity matrices");
    check(near(ox,0.5) && near(oy,-0.5) && near(oz,0.5) && near(ow,1),
          "four-coordinate unproject preserves homogeneous output");
    gluPickMatrix(32,32,16,8,viewport); glGetDoublev(GL_MODELVIEW_MATRIX,matrix);
    check(near(matrix[0],4) && near(matrix[5],8) &&
          near(matrix[12],0) && near(matrix[13],0),"pick matrix scales the selected window region");
    glLoadIdentity(); gluPickMatrix(32,32,0,8,viewport);
    glGetDoublev(GL_MODELVIEW_MATRIX,matrix);
    check(near(matrix[0],1) && near(matrix[5],1),"zero-size pick region leaves the matrix unchanged");
    glLoadIdentity(); gluOrtho2D(-2,2,-1,1); glGetDoublev(GL_MODELVIEW_MATRIX,matrix);
    check(near(matrix[0],0.5) && near(matrix[5],1) && near(matrix[10],-1),
          "2D orthographic utility delegates the expected GL transform");
    no_gl_error();
}

static void image_tests(void)
{
    static const GLubyte input[12] = {255,0,0,0,255,0,0,0,255,255,255,255};
    static const GLubyte line[12] = {255,0,0,0,255,0,255,0,0,0,255,0};
    GLubyte output[4] = {0,0,0,0}, padded[100], level[48];
    GLfloat floating[3];
    GLuint texture;
    GLint value;
    int x, y, offset, equal;
    reset("image scaling and mipmaps");
    check(gluScaleImage(GL_RGB,2,2,GL_UNSIGNED_BYTE,input,1,1,GL_UNSIGNED_BYTE,output) == 0,
          "image scaler accepts byte RGB images");
    check(abs((int)output[0]-128) <= 1 && abs((int)output[1]-128) <= 1 &&
          abs((int)output[2]-128) <= 1,"image downsampling averages all input pixels");
    check(gluScaleImage(GL_RGB,2,2,GL_UNSIGNED_BYTE,input,1,1,GL_FLOAT,floating) == 0 &&
          fabs(floating[0]-0.5) < 0.01 && fabs(floating[1]-0.5) < 0.01 &&
          fabs(floating[2]-0.5) < 0.01,"image scaling converts normalized component types");
    check(gluScaleImage(0xdead,2,2,GL_UNSIGNED_BYTE,input,1,1,GL_UNSIGNED_BYTE,output) == GLU_INVALID_ENUM,
          "unsupported image format reports invalid enum");
    check(gluScaleImage(GL_RGB,-1,2,GL_UNSIGNED_BYTE,input,1,1,GL_UNSIGNED_BYTE,output) == GLU_INVALID_VALUE,
          "negative image dimensions report invalid value");
    memset(padded,0,sizeof(padded));
    for (y = 0; y < 4; ++y) for (x = 0; x < 4; ++x) {
        offset = (y+1)*20+(x+1)*3;
        padded[offset] = x%2 ? 0 : 255;
        padded[offset+1] = x%2 ? 255 : 0;
    }
    glGenTextures(1,&texture); glBindTexture(GL_TEXTURE_2D,texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT,4);
    glPixelStorei(GL_UNPACK_ROW_LENGTH,6); glPixelStorei(GL_UNPACK_SKIP_ROWS,1);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS,1);
    check(gluBuild2DMipmaps(GL_TEXTURE_2D,GL_RGB,4,4,GL_RGB,GL_UNSIGNED_BYTE,padded) == 0,
          "2D mipmaps honor padded and skipped unpack rows");
    glGetIntegerv(GL_UNPACK_ROW_LENGTH,&value);
    check(value == 6,"mipmap construction restores unpack row length");
    glGetIntegerv(GL_UNPACK_SKIP_ROWS,&value);
    check(value == 1,"mipmap construction restores unpack skipped rows");
    glGetIntegerv(GL_UNPACK_SKIP_PIXELS,&value);
    check(value == 1,"mipmap construction restores unpack skipped pixels");
    glGetIntegerv(GL_UNPACK_ALIGNMENT,&value);
    check(value == 4,"mipmap construction restores unpack row alignment");
    glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&value);
    check(value == 4,"base mip level retains image width");
    glGetTexLevelParameteriv(GL_TEXTURE_2D,1,GL_TEXTURE_WIDTH,&value);
    check(value == 2,"first reduced mip level has half the width");
    glGetTexLevelParameteriv(GL_TEXTURE_2D,2,GL_TEXTURE_HEIGHT,&value);
    check(value == 1,"mipmap chain reaches a one-pixel image");
    glGetTexImage(GL_TEXTURE_2D,0,GL_RGB,GL_UNSIGNED_BYTE,level);
    equal = 1;
    for (y = 0; y < 4; ++y) for (x = 0; x < 4; ++x) {
        offset = (y*4+x)*3;
        if (level[offset] != (x%2 ? 0 : 255) ||
            level[offset+1] != (x%2 ? 255 : 0) || level[offset+2] != 0) equal = 0;
    }
    check(equal,
          "base texture upload reads the selected source rectangle");
    glGetTexImage(GL_TEXTURE_2D,2,GL_RGB,GL_UNSIGNED_BYTE,output);
    if (abs((int)output[0]-128) > 1 || abs((int)output[1]-128) > 1 || output[2] != 0)
        fprintf(stderr,"  final mip texel = %u,%u,%u\n",output[0],output[1],output[2]);
    check(abs((int)output[0]-128) <= 1 && abs((int)output[1]-128) <= 1 && output[2] == 0,
          "smallest mip level contains the filtered source average");
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE); glEnable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
    glTexCoord2f(0,0); glVertex2f(-0.8f,-0.8f);
    glTexCoord2f(32,0); glVertex2f(0.8f,-0.8f);
    glTexCoord2f(32,32); glVertex2f(0.8f,0.8f);
    glTexCoord2f(0,32); glVertex2f(-0.8f,0.8f);
    glEnd(); glFinish();
    pixel(32,32,128,128,0,"GL renders the utility-generated mipmap chain");
    glDisable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_1D,texture+1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH,0); glPixelStorei(GL_UNPACK_SKIP_ROWS,0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    check(gluBuild1DMipmaps(GL_TEXTURE_1D,GL_RGB,4,GL_RGB,GL_UNSIGNED_BYTE,line) == 0,
          "1D mipmap construction succeeds");
    glGetTexImage(GL_TEXTURE_1D,2,GL_RGB,GL_UNSIGNED_BYTE,output);
    check(abs((int)output[0]-128) <= 1 && abs((int)output[1]-128) <= 1 && output[2] == 0,
          "1D mipmap chain filters to the correct final texel");
    glDeleteTextures(1,&texture); ++texture; glDeleteTextures(1,&texture);
    no_gl_error();
}

static int checker_image(const GLubyte *image, int width, int height, int depth)
{
    int x, y, z, offset;
    for (z = 0; z < depth; ++z) for (y = 0; y < height; ++y)
        for (x = 0; x < width; ++x) {
            offset = ((z*height+y)*width+x)*3;
            if (image[offset] != (x%2 ? 0 : 255) ||
                image[offset+1] != (x%2 ? 255 : 0) || image[offset+2] != 0)
                return 0;
        }
    return 1;
}

static void mipmap_storage_tests(void)
{
    GLushort scalar[80], packed[40];
    GLubyte padded[600], image[192], texel[4];
    GLuint texture[3];
    GLint value;
    int x, y, z, offset;
    reset("mipmap component types and volume storage");
    glGenTextures(3,texture); memset(scalar,0,sizeof(scalar));
    for (y = 0; y < 4; ++y) for (x = 0; x < 4; ++x) {
        offset = (y+1)*16+(x+1)*3;
        scalar[offset] = x%2 ? 0 : 65535;
        scalar[offset+1] = x%2 ? 65535 : 0;
    }
    glBindTexture(GL_TEXTURE_2D,texture[0]);
    glPixelStorei(GL_UNPACK_ALIGNMENT,8); glPixelStorei(GL_UNPACK_ROW_LENGTH,5);
    glPixelStorei(GL_UNPACK_SKIP_ROWS,1); glPixelStorei(GL_UNPACK_SKIP_PIXELS,1);
    glPixelStorei(GL_UNPACK_SWAP_BYTES,GL_TRUE);
    check(gluBuild2DMipmaps(GL_TEXTURE_2D,GL_RGB,4,4,GL_RGB,GL_UNSIGNED_SHORT,scalar) == 0,
          "unsigned-short mipmaps accept padded rows and skipped source pixels");
    glGetTexImage(GL_TEXTURE_2D,0,GL_RGB,GL_UNSIGNED_BYTE,image);
    check(checker_image(image,4,4,1),"unsigned-short base image retains every selected source row");
    glGetTexImage(GL_TEXTURE_2D,2,GL_RGB,GL_UNSIGNED_BYTE,texel);
    check(abs((int)texel[0]-128) <= 1 && abs((int)texel[1]-128) <= 1 && texel[2] == 0,
          "unsigned-short mipmap filtering uses the actual source row stride");
    glGetIntegerv(GL_UNPACK_ALIGNMENT,&value);
    check(value == 8,"unsigned-short mipmaps restore source row alignment");
    glGetIntegerv(GL_UNPACK_SWAP_BYTES,&value);
    check(value == GL_TRUE,"unsigned-short mipmaps restore source byte swapping");
    glGetIntegerv(GL_UNPACK_ROW_LENGTH,&value);
    check(value == 5,"unsigned-short mipmaps restore source row length");
    glGetIntegerv(GL_UNPACK_SKIP_ROWS,&value);
    check(value == 1,"unsigned-short mipmaps restore skipped source rows");
    glGetIntegerv(GL_UNPACK_SKIP_PIXELS,&value);
    check(value == 1,"unsigned-short mipmaps restore skipped source pixels");

    memset(packed,0,sizeof(packed));
    for (y = 0; y < 4; ++y) for (x = 0; x < 4; ++x)
        packed[(y+1)*8+x+1] = (GLushort)(x%2 ? 0x07e0 : 0xf800);
    glBindTexture(GL_TEXTURE_2D,texture[1]);
    glPixelStorei(GL_UNPACK_ROW_LENGTH,6); glPixelStorei(GL_UNPACK_SWAP_BYTES,GL_FALSE);
    check(gluBuild2DMipmaps(GL_TEXTURE_2D,GL_RGB,4,4,GL_RGB,GL_UNSIGNED_SHORT_5_6_5,packed) == 0,
          "packed 565 mipmaps accept padded source rows");
    glGetTexImage(GL_TEXTURE_2D,0,GL_RGB,GL_UNSIGNED_BYTE,image);
    check(checker_image(image,4,4,1),"packed 565 base image preserves every selected texel");
    glGetTexImage(GL_TEXTURE_2D,2,GL_RGB,GL_UNSIGNED_BYTE,texel);
    check(abs((int)texel[0]-128) <= 8 && abs((int)texel[1]-128) <= 8 && texel[2] == 0,
          "packed 565 mipmaps filter to the quantized source average");
    glGetIntegerv(GL_UNPACK_ALIGNMENT,&value);
    check(value == 8,"packed mipmaps restore unpack alignment");
    glGetIntegerv(GL_UNPACK_ROW_LENGTH,&value);
    check(value == 6,"packed mipmaps restore unpack row length");

    memset(padded,0,sizeof(padded));
    for (z = 0; z < 4; ++z) for (y = 0; y < 4; ++y) for (x = 0; x < 4; ++x) {
        offset = (z+1)*120+(y+1)*20+(x+1)*3;
        padded[offset] = x%2 ? 0 : 255;
        padded[offset+1] = x%2 ? 255 : 0;
    }
    glBindTexture(GL_TEXTURE_3D,texture[2]);
    glPixelStorei(GL_UNPACK_ALIGNMENT,4); glPixelStorei(GL_UNPACK_IMAGE_HEIGHT,6);
    glPixelStorei(GL_UNPACK_SKIP_IMAGES,1);
    check(gluBuild3DMipmaps(GL_TEXTURE_3D,GL_RGB,4,4,4,GL_RGB,GL_UNSIGNED_BYTE,padded) == 0,
          "volume mipmaps accept padded rows, padded images and skipped source images");
    glGetTexImage(GL_TEXTURE_3D,0,GL_RGB,GL_UNSIGNED_BYTE,image);
    check(checker_image(image,4,4,4),"volume base image preserves all selected source slices");
    glGetTexLevelParameteriv(GL_TEXTURE_3D,1,GL_TEXTURE_DEPTH,&value);
    check(value == 2,"volume mipmap chain halves image depth");
    glGetTexLevelParameteriv(GL_TEXTURE_3D,2,GL_TEXTURE_DEPTH,&value);
    check(value == 1,"volume mipmap chain reaches one slice");
    glGetTexImage(GL_TEXTURE_3D,2,GL_RGB,GL_UNSIGNED_BYTE,texel);
    check(abs((int)texel[0]-128) <= 1 && abs((int)texel[1]-128) <= 1 && texel[2] == 0,
          "volume mipmap filtering produces the source color average");
    glGetIntegerv(GL_UNPACK_ALIGNMENT,&value);
    check(value == 4,"volume mipmaps restore unpack alignment");
    glGetIntegerv(GL_UNPACK_ROW_LENGTH,&value);
    check(value == 6,"volume mipmaps restore unpack row length");
    glGetIntegerv(GL_UNPACK_IMAGE_HEIGHT,&value);
    check(value == 6,"volume mipmaps restore unpack image height");
    glGetIntegerv(GL_UNPACK_SKIP_IMAGES,&value);
    check(value == 1,"volume mipmaps restore skipped source images");
    glGetIntegerv(GL_UNPACK_SKIP_ROWS,&value);
    check(value == 1,"volume mipmaps restore skipped source rows");
    glGetIntegerv(GL_UNPACK_SKIP_PIXELS,&value);
    check(value == 1,"volume mipmaps restore skipped source pixels");
    glPixelStorei(GL_UNPACK_IMAGE_HEIGHT,0); glPixelStorei(GL_UNPACK_SKIP_IMAGES,0);
    glDeleteTextures(3,texture); no_gl_error();
}

static GLenum quadric_error;
static void GLAPIENTRY record_quadric_error(GLenum error) { quadric_error = error; }

static void quadric_tests(void)
{
    static const GLubyte blue[12] = {0,0,255,0,0,255,0,0,255,0,0,255};
    GLfloat zero[4] = {0,0,0,1}, white[4] = {1,1,1,1}, red[4] = {1,0,0,1};
    GLfloat positive_z[4] = {0,0,1,0}, negative_z[4] = {0,0,-1,0};
    GLUquadric *quadric;
    GLfloat coordinate[4];
    GLubyte center[4], edge[4];
    GLuint texture;
    reset("quadrics");
    quadric = gluNewQuadric(); check(quadric != NULL,"create quadric object");
    if (!quadric) return;
    gluQuadricCallback(quadric,GLU_ERROR,(_GLUfuncptr)record_quadric_error);
    quadric_error = 0; gluQuadricNormals(quadric,0xdead);
    check(quadric_error == GLU_INVALID_ENUM,"invalid quadric state invokes the error callback");
    quadric_error = 0; gluSphere(quadric,-1,16,8);
    check(quadric_error == GLU_INVALID_VALUE,"negative sphere radius invokes the error callback");
    gluQuadricNormals(quadric,GLU_SMOOTH); gluQuadricOrientation(quadric,GLU_OUTSIDE);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT,zero);
    glLightfv(GL_LIGHT0,GL_AMBIENT,zero); glLightfv(GL_LIGHT0,GL_DIFFUSE,white);
    glLightfv(GL_LIGHT0,GL_SPECULAR,zero); glLightfv(GL_LIGHT0,GL_POSITION,positive_z);
    glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT,zero); glMaterialfv(GL_FRONT_AND_BACK,GL_DIFFUSE,red);
    glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,zero); glMaterialfv(GL_FRONT_AND_BACK,GL_EMISSION,zero);
    glEnable(GL_LIGHT0); glEnable(GL_LIGHTING);
    glEnable(GL_CULL_FACE); glCullFace(GL_BACK); glColor3f(1,0,0);
    gluDisk(quadric,0,0.75,32,4); glFinish();
    pixel(32,32,255,0,0,"outward disk has front winding and a positive-Z normal");
    glClear(GL_COLOR_BUFFER_BIT); gluQuadricOrientation(quadric,GLU_INSIDE);
    gluDisk(quadric,0,0.75,32,4); glFinish();
    pixel(32,32,0,0,0,"inward disk reverses winding and is culled from the outside");
    glCullFace(GL_FRONT); glLightfv(GL_LIGHT0,GL_POSITION,negative_z);
    glClear(GL_COLOR_BUFFER_BIT); gluDisk(quadric,0,0.75,32,4); glFinish();
    pixel(32,32,255,0,0,"inward disk has back winding and a negative-Z normal");
    glDisable(GL_LIGHTING); glDisable(GL_LIGHT0);
    glDisable(GL_CULL_FACE); gluQuadricOrientation(quadric,GLU_OUTSIDE);
    glClear(GL_COLOR_BUFFER_BIT); glColor3f(0,1,0);
    gluPartialDisk(quadric,0,0.75,32,4,0,90); glFinish();
    pixel(44,44,0,255,0,"partial disk covers the selected quadrant");
    pixel(20,20,0,0,0,"partial disk leaves the opposite quadrant untouched");
    glGenTextures(1,&texture); glBindTexture(GL_TEXTURE_2D,texture);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,2,2,0,GL_RGB,GL_UNSIGNED_BYTE,blue);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE); glEnable(GL_TEXTURE_2D);
    gluQuadricTexture(quadric,GL_TRUE); glTexCoord2f(-3,-3);
    glClear(GL_COLOR_BUFFER_BIT); gluSphere(quadric,0.7,32,16); glFinish();
    pixel(32,32,0,0,255,"textured quadric sphere renders through the GL library");
    glGetFloatv(GL_CURRENT_TEXTURE_COORDS,coordinate);
    check(coordinate[0] >= 0 && coordinate[0] <= 1 && coordinate[1] >= 0 && coordinate[1] <= 1,
          "quadric texture coordinates replace the application's previous coordinates");
    glDisable(GL_TEXTURE_2D); glDeleteTextures(1,&texture);
    glClear(GL_COLOR_BUFFER_BIT); glEnable(GL_LIGHTING); glEnable(GL_LIGHT0);
    glEnable(GL_CULL_FACE); glCullFace(GL_BACK);
    glLightfv(GL_LIGHT0,GL_POSITION,positive_z);
    glMaterialfv(GL_FRONT_AND_BACK,GL_DIFFUSE,white);
    gluSphere(quadric,0.7,32,16); glFinish();
    glReadPixels(32,32,1,1,GL_RGBA,GL_UNSIGNED_BYTE,center);
    glReadPixels(50,32,1,1,GL_RGBA,GL_UNSIGNED_BYTE,edge);
    check(center[0] > 230 && center[1] > 230 && center[2] > 230,
          "sphere normals face its directional light at the front center");
    check(edge[0] > 60 && edge[0] < 210 && edge[0] < center[0]-30,
          "sphere normals vary smoothly toward its silhouette");
    glDisable(GL_LIGHTING); glDisable(GL_LIGHT0); glDisable(GL_CULL_FACE);
    glClear(GL_COLOR_BUFFER_BIT); glColor3f(1,1,0);
    glRotatef(90,1,0,0); glTranslatef(0,0,-0.5f);
    gluCylinder(quadric,0.4,0.4,1,32,4); glFinish();
    pixel(32,32,255,255,0,"cylinder sides render without requiring GLU-generated caps");
    gluDeleteQuadric(quadric); no_gl_error();
}

static GLenum tess_mode, tess_error;
static GLdouble tess_first[3], tess_previous[2][3], tess_generated[16][3];
static unsigned int tess_vertices, tess_combines, tess_begins;
static double tess_area;

static double triangle_area(const GLdouble *a, const GLdouble *b, const GLdouble *c)
{
    return fabs((b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]))*0.5;
}

static void GLAPIENTRY tess_begin(GLenum mode)
{
    tess_mode = mode; tess_vertices = 0; ++tess_begins; glBegin(mode);
}

static void GLAPIENTRY tess_vertex(void *data)
{
    const GLdouble *vertex = (const GLdouble *)data;
    if (tess_vertices == 0) memcpy(tess_first,vertex,sizeof(tess_first));
    if (tess_vertices >= 2) {
        if (tess_mode == GL_TRIANGLE_FAN)
            tess_area += triangle_area(tess_first,tess_previous[1],vertex);
        else if (tess_mode == GL_TRIANGLE_STRIP ||
                 (tess_mode == GL_TRIANGLES && tess_vertices%3 == 2))
            tess_area += triangle_area(tess_previous[0],tess_previous[1],vertex);
    }
    memcpy(tess_previous[0],tess_previous[1],sizeof(tess_previous[0]));
    memcpy(tess_previous[1],vertex,sizeof(tess_previous[1]));
    ++tess_vertices; glVertex3dv(vertex);
}

static void GLAPIENTRY tess_end(void) { glEnd(); }
static void GLAPIENTRY record_tess_error(GLenum error) { tess_error = error; }

static void GLAPIENTRY tess_combine(GLdouble coordinate[3], void *data[4],
                                     GLfloat weight[4], void **result)
{
    (void)data; (void)weight;
    if (tess_combines >= 16) { *result = NULL; return; }
    memcpy(tess_generated[tess_combines],coordinate,sizeof(tess_generated[0]));
    *result = tess_generated[tess_combines++];
}

static void submit_contour(GLUtesselator *tess, GLdouble (*vertex)[3], int count)
{
    int i;
    gluTessBeginContour(tess);
    for (i = 0; i < count; ++i) gluTessVertex(tess,vertex[i],vertex[i]);
    gluTessEndContour(tess);
}

static void tessellation_tests(void)
{
    GLdouble concave[6][3] = {{-0.8,-0.8,0},{0.8,-0.8,0},{0.8,0,0},
        {0,0,0},{0,0.8,0},{-0.8,0.8,0}};
    GLdouble outer[4][3] = {{-0.8,-0.8,0},{0.8,-0.8,0},{0.8,0.8,0},{-0.8,0.8,0}};
    GLdouble inner[4][3] = {{-0.3,-0.3,0},{-0.3,0.3,0},{0.3,0.3,0},{0.3,-0.3,0}};
    GLdouble crossing[4][3] = {{-0.8,-0.8,0},{0.8,0.8,0},{-0.8,0.8,0},{0.8,-0.8,0}};
    GLdouble property;
    GLUtesselator *tess;
    reset("polygon tessellation");
    tess = gluNewTess(); check(tess != NULL,"create tessellator");
    if (!tess) return;
    gluTessCallback(tess,GLU_TESS_BEGIN,(_GLUfuncptr)tess_begin);
    gluTessCallback(tess,GLU_TESS_VERTEX,(_GLUfuncptr)tess_vertex);
    gluTessCallback(tess,GLU_TESS_END,(_GLUfuncptr)tess_end);
    gluTessCallback(tess,GLU_TESS_ERROR,(_GLUfuncptr)record_tess_error);
    gluTessCallback(tess,GLU_TESS_COMBINE,(_GLUfuncptr)tess_combine);
    gluTessNormal(tess,0,0,1); gluTessProperty(tess,GLU_TESS_WINDING_RULE,GLU_TESS_WINDING_ODD);
    gluGetTessProperty(tess,GLU_TESS_WINDING_RULE,&property);
    check(property == GLU_TESS_WINDING_ODD,"tessellator properties round-trip");
    tess_error = 0; tess_area = 0; tess_begins = tess_combines = 0;
    glColor3f(1,0,0); gluTessBeginPolygon(tess,NULL);
    submit_contour(tess,concave,6); gluTessEndPolygon(tess); glFinish();
    check(tess_error == 0 && tess_begins > 0,"concave polygon produces real primitive callbacks");
    check(near(tess_area,1.92),"concave polygon triangulation preserves its area");
    pixel(16,48,255,0,0,"concave polygon fills its upper-left arm");
    pixel(48,48,0,0,0,"concave polygon leaves its missing upper-right quadrant empty");
    glClear(GL_COLOR_BUFFER_BIT); tess_area = 0; tess_error = 0;
    gluTessBeginPolygon(tess,NULL); submit_contour(tess,outer,4);
    submit_contour(tess,inner,4); gluTessEndPolygon(tess); glFinish();
    check(tess_error == 0 && near(tess_area,2.2),"polygon holes subtract from the tessellated area");
    pixel(12,32,255,0,0,"polygon with a hole fills its outer band");
    pixel(32,32,0,0,0,"polygon hole remains empty after rendering");
    glClear(GL_COLOR_BUFFER_BIT); tess_area = 0; tess_error = 0; tess_combines = 0;
    gluTessBeginPolygon(tess,NULL); submit_contour(tess,crossing,4);
    gluTessEndPolygon(tess); glFinish();
    check(tess_error == 0 && tess_combines > 0,"self-intersection invokes the combine callback");
    check(near(tess_area,1.28),"self-intersecting polygon preserves odd-winding filled area");
    pixel(32,48,255,0,0,"combined intersection vertices produce rendered geometry");
    tess_error = 0; gluTessProperty(tess,0xdead,0);
    check(tess_error == GLU_INVALID_ENUM,"invalid tessellation property invokes its error callback");
    tess_error = 0; gluTessProperty(tess,GLU_TESS_TOLERANCE,2);
    check(tess_error == GLU_INVALID_VALUE,"invalid tessellation tolerance is rejected");
    gluDeleteTess(tess); no_gl_error();
}

struct nurbs_result {
    unsigned int begins, ends, vertices;
    int valid;
    GLfloat minimum[3], maximum[3];
};
static GLenum nurbs_error;
static struct nurbs_result *expected_nurbs_data;

static void GLAPIENTRY nurbs_begin(GLenum mode, void *data)
{
    struct nurbs_result *result = (struct nurbs_result *)data;
    if (result != expected_nurbs_data) abort();
    ++result->begins; glBegin(mode);
}

static void GLAPIENTRY nurbs_vertex(GLfloat *vertex, void *data)
{
    struct nurbs_result *result = (struct nurbs_result *)data;
    int i;
    if (result != expected_nurbs_data) abort();
    for (i = 0; i < 3; ++i) {
        if (!(vertex[i] >= -1 && vertex[i] <= 1)) result->valid = 0;
        if (vertex[i] < result->minimum[i]) result->minimum[i] = vertex[i];
        if (vertex[i] > result->maximum[i]) result->maximum[i] = vertex[i];
    }
    ++result->vertices; glVertex3fv(vertex);
}

static void GLAPIENTRY nurbs_end(void *data)
{
    struct nurbs_result *result = (struct nurbs_result *)data;
    if (result != expected_nurbs_data) abort();
    ++result->ends; glEnd();
}

static void GLAPIENTRY record_nurbs_error(GLenum error) { nurbs_error = error; }

static void reset_nurbs_result(struct nurbs_result *result)
{
    int i;
    memset(result,0,sizeof(*result)); result->valid = 1;
    for (i = 0; i < 3; ++i) { result->minimum[i] = 1; result->maximum[i] = -1; }
}

static void nurbs_tests(void)
{
    GLfloat cubic_knots[8] = {0,0,0,0,1,1,1,1};
    GLfloat linear_knots[4] = {0,0,1,1};
    GLfloat curve[4][3] = {{-0.75f,-0.5f,0},{-0.25f,0.5f,0},
                          {0.25f,-0.5f,0},{0.75f,0.5f,0}};
    GLfloat surface[2][2][3] = {{{-0.7f,-0.7f,0},{-0.7f,0.7f,0}},
                               {{0.7f,-0.7f,0},{0.7f,0.7f,0}}};
    GLfloat outer_trim[5][2] = {{0,0},{1,0},{1,1},{0,1},{0,0}};
    GLfloat inner_trim[5][2] = {{0.3f,0.3f},{0.3f,0.7f},{0.7f,0.7f},
                               {0.7f,0.3f},{0.3f,0.3f}};
    GLfloat identity[16], property;
    GLint viewport[4] = {0,0,WIDTH,HEIGHT};
    GLUnurbs *nurbs;
    struct nurbs_result result;
    int i;
    reset("NURBS curves and surfaces");
    nurbs = gluNewNurbsRenderer(); check(nurbs != NULL,"create NURBS renderer");
    if (!nurbs) return;
    gluNurbsCallback(nurbs,GLU_ERROR,(_GLUfuncptr)record_nurbs_error);
    gluNurbsCallback(nurbs,GLU_NURBS_BEGIN_DATA,(_GLUfuncptr)nurbs_begin);
    gluNurbsCallback(nurbs,GLU_NURBS_VERTEX_DATA,(_GLUfuncptr)nurbs_vertex);
    gluNurbsCallback(nurbs,GLU_NURBS_END_DATA,(_GLUfuncptr)nurbs_end);
    expected_nurbs_data = &result; gluNurbsCallbackData(nurbs,&result);
    gluNurbsProperty(nurbs,GLU_NURBS_MODE,GLU_NURBS_TESSELLATOR);
    gluNurbsProperty(nurbs,GLU_SAMPLING_METHOD,GLU_DOMAIN_DISTANCE);
    gluNurbsProperty(nurbs,GLU_U_STEP,16); gluNurbsProperty(nurbs,GLU_V_STEP,16);
    gluNurbsProperty(nurbs,GLU_AUTO_LOAD_MATRIX,GL_FALSE);
    memset(identity,0,sizeof(identity));
    for (i = 0; i < 4; ++i) identity[i*5] = 1;
    gluLoadSamplingMatrices(nurbs,identity,identity,viewport);
    gluGetNurbsProperty(nurbs,GLU_NURBS_MODE,&property);
    check(property == GLU_NURBS_TESSELLATOR,"NURBS tessellator mode round-trips");
    gluGetNurbsProperty(nurbs,GLU_SAMPLING_METHOD,&property);
    check(property == GLU_DOMAIN_DISTANCE,"NURBS sampling method round-trips");
    gluGetNurbsProperty(nurbs,GLU_U_STEP,&property);
    check(property == 16,"NURBS sampling density round-trips");
    reset_nurbs_result(&result); nurbs_error = 0; glColor3f(1,0,1);
    gluBeginCurve(nurbs); gluNurbsCurve(nurbs,8,cubic_knots,3,&curve[0][0],4,GL_MAP1_VERTEX_3);
    gluEndCurve(nurbs); glFinish();
    check(nurbs_error == 0 && result.valid && result.vertices >= 2 && result.begins == result.ends,
          "cubic NURBS curve emits balanced callbacks with valid positions");
    check(fabs(result.minimum[0]+0.75) < 0.0001 && fabs(result.maximum[0]-0.75) < 0.0001,
          "clamped cubic curve reaches its endpoint control positions");
    gluNurbsCallbackDataEXT(nurbs,&result); reset_nurbs_result(&result);
    nurbs_error = 0; glClear(GL_COLOR_BUFFER_BIT); glColor3f(0,1,0);
    gluBeginSurface(nurbs);
    gluNurbsSurface(nurbs,4,linear_knots,4,linear_knots,6,3,&surface[0][0][0],2,2,GL_MAP2_VERTEX_3);
    gluEndSurface(nurbs); glFinish();
    check(nurbs_error == 0 && result.valid && result.vertices >= 3 && result.begins == result.ends,
          "bilinear NURBS surface emits balanced tessellator callbacks");
    check(result.minimum[0] < -0.69f && result.maximum[0] > 0.69f &&
          result.minimum[1] < -0.69f && result.maximum[1] > 0.69f,
          "NURBS surface tessellation spans the supplied control patch");
    pixel(32,32,0,255,0,"NURBS callback geometry renders through the GL library");
    pixel(2,2,0,0,0,"NURBS surface preserves pixels outside the patch");
    reset_nurbs_result(&result); nurbs_error = 0; glClear(GL_COLOR_BUFFER_BIT);
    gluBeginSurface(nurbs);
    gluNurbsSurface(nurbs,4,linear_knots,4,linear_knots,6,3,&surface[0][0][0],2,2,GL_MAP2_VERTEX_3);
    gluBeginTrim(nurbs); gluPwlCurve(nurbs,5,&outer_trim[0][0],2,GLU_MAP1_TRIM_2); gluEndTrim(nurbs);
    gluBeginTrim(nurbs); gluPwlCurve(nurbs,5,&inner_trim[0][0],2,GLU_MAP1_TRIM_2); gluEndTrim(nurbs);
    gluEndSurface(nurbs); glFinish();
    check(nurbs_error == 0 && result.valid && result.vertices >= 3 && result.begins == result.ends,
          "piecewise-linear trim loops generate balanced surface callbacks");
    pixel(16,32,0,255,0,"trimmed NURBS surface retains its outer band");
    pixel(32,32,0,0,0,"clockwise NURBS trim loop removes the center of the patch");
    nurbs_error = 0; gluNurbsProperty(nurbs,0xdead,1);
    check(nurbs_error == GLU_INVALID_ENUM,"invalid NURBS property invokes the error callback");
    gluDeleteNurbsRenderer(nurbs); expected_nurbs_data = NULL; no_gl_error();
}

int main(void)
{
    OSMesaContext context;
    group = "context";
    context = OSMesaCreateContextExt(OSMESA_RGBA,24,8,0,NULL);
    check(context != NULL,"create offscreen GL context");
    if (!context) return 1;
    check(OSMesaMakeCurrent(context,framebuffer,GL_UNSIGNED_BYTE,WIDTH,HEIGHT),
          "bind offscreen framebuffer");
    glViewport(0,0,WIDTH,HEIGHT);
    matrix_tests(); image_tests(); mipmap_storage_tests();
    quadric_tests(); tessellation_tests(); nurbs_tests();
    OSMesaDestroyContext(context);
    printf("GLU real-renderer behavior: %d checks, %d failures\n",checks,failures);
    return failures ? 1 : 0;
}
