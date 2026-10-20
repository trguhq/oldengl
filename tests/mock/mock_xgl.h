#ifndef OLDENGL_MOCK_CONTROL_H
#define OLDENGL_MOCK_CONTROL_H

#include <X11/Xlib.h>
#include <xgl/xgl.h>

Display *MockXGLDisplay(void);
void MockXGLReset(void);
void MockXGLWindow(Window, int, int, int);
void MockXGLPacking(unsigned int, unsigned int, unsigned int);
void MockXGLFailOpen(int);
void MockXGLFailCreate(int, int);
void MockXGLErrorNextCopy(void);
void *MockXGLFramebuffer(Window);
int MockXGLTriangles(void);
int MockXGLLines(void);
int MockXGLMarkers(void);
int MockXGLUploads(void);
int MockXGLDownloads(void);
int MockXGLClears(void);
Xgl_pt_color_f3d MockXGLLastVertex(unsigned int);
unsigned int MockXGLPixel(Window, int, int);
int MockXGLCopies(void);
int MockXGLFlushes(void);
int MockXGLSynchronizes(void);
int MockXGLLiveObjects(void);
int MockXGLOpens(void);
int MockXGLCloses(void);
int MockXGLResizes(void);

#endif
