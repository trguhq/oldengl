
/*
 * Mesa 3-D graphics library
 * Version:  4.0.4
 *
 * Copyright (C) 1999-2002  Brian Paul   All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */


/*
 * This is an emulation of the GLX API which allows Mesa/GLX-based programs
 * to run on X servers which do not have the real GLX extension.
 *
 * Thanks to the contributors:
 *
 * Initial version:  Philip Brown (phil@bolthole.com)
 * Better glXGetConfig() support: Armin Liebchen (liebchen@asylum.cs.utah.edu)
 * Further visual-handling refinements: Wolfram Gloger
 *    (wmglo@Dent.MED.Uni-Muenchen.DE).
 *
 * Notes:
 *   Don't be fooled, stereo isn't supported yet.
 */



#include "glxheader.h"
#include "glxapi.h"
#include "GL/xmesa.h"
#include "context.h"
#include "config.h"
#include "macros.h"
#include "mem.h"
#include "mmath.h"
#include "mtypes.h"
#include "xfonts.h"
#include "xmesaP.h"


/* This indicates the client-side GLX API and GLX encoder version. */
#define CLIENT_MAJOR_VERSION 1
#define CLIENT_MINOR_VERSION 4  /* core API, including client-side pbuffers */

/* This indicates the server-side GLX decoder version.
 * GLX 1.4 indicates OpenGL 1.3 support
 */
#define SERVER_MAJOR_VERSION 1
#define SERVER_MINOR_VERSION 4

/* This is appended onto the glXGetClient/ServerString version strings. */
#define MESA_GLX_VERSION "Mesa 4.0.4"

/* Who implemented this GLX? */
#define VENDOR "OldenGL / Mesa"



/* Silence compiler warnings */
extern void Fake_glXDummyFunc( void );
void Fake_glXDummyFunc( void )
{
   (void) kernel8;
   (void) DitherValues;
   (void) HPCR_DRGB;
   (void) kernel1;
}


/*
 * Our fake GLX context will contain a "real" GLX context and an XMesa context.
 *
 * Note that a pointer to a __GLXcontext is a pointer to a fake_glx_context,
 * and vice versa.
 *
 * We really just need this structure in order to make the libGL functions
 * glXGetCurrentContext(), glXGetCurrentDrawable() and glXGetCurrentDisplay()
 * work correctly.
 */
struct fake_glx_context {
   __GLXcontext glxContext;   /* this MUST be first! */
   XMesaContext xmesaContext;
   Display *owner;
   XMesaVisual visual;
   GLboolean deleted;
   struct oldengl_drawable *draw, *read;
   struct fake_glx_context *next;
};



/**********************************************************************/
/***                       GLX Visual Code                          ***/
/**********************************************************************/

#define DONT_CARE -1


#define MAX_VISUALS 8192
static XMesaVisual VisualTable[MAX_VISUALS];
static int NumVisuals = 0;

struct oldengl_visual_handle {
   Display *display;
   XVisualInfo *handle;
   XMesaVisual visual;
   struct oldengl_visual_handle *next;
};
static struct oldengl_visual_handle *VisualHandles;

static void remember_visual(Display *display, XVisualInfo *handle, XMesaVisual visual)
{
   struct oldengl_visual_handle *entry;
   for (entry = VisualHandles; entry; entry = entry->next)
      if (entry->display == display && entry->handle == handle) {
         entry->visual = visual;
         return;
      }
   entry = CALLOC_STRUCT(oldengl_visual_handle);
   if (!entry) return;
   entry->display = display;
   entry->handle = handle;
   entry->visual = visual;
   entry->next = VisualHandles;
   VisualHandles = entry;
}


/*
 * This struct and some code fragments borrowed
 * from Mark Kilgard's GLUT library.
 */
typedef struct _OverlayInfo {
  /* Avoid 64-bit portability problems by being careful to use
     longs due to the way XGetWindowProperty is specified. Note
     that these parameters are passed as CARD32s over X
     protocol. */
  unsigned long overlay_visual;
  long transparent_type;
  long value;
  long layer;
} OverlayInfo;



/* Macro to handle c_class vs class field name in XVisualInfo struct */
#if defined(__cplusplus) || defined(c_plusplus)
#define CLASS c_class
#else
#define CLASS class
#endif




/* Client-side GLX resources. Handles are distinct from the X drawable. */
struct oldengl_drawable {
   Display *display;
   GLXDrawable handle;
   XMesaBuffer buffer;
   XMesaVisual visual;
   int kind;                         /* 0 raw window, 1 GLX window, 2 pixmap, 3 pbuffer */
   GLboolean owns_pixmap, deleted, largest;
   unsigned long event_mask;
   struct oldengl_drawable *next;
};
static struct fake_glx_context *ContextList;
static struct oldengl_drawable *DrawableList;

static struct fake_glx_context *
context_resource(Display *display, GLXContext handle)
{
   struct fake_glx_context *c;
   for (c = ContextList; c; c = c->next)
      if ((GLXContext)c == handle && c->owner == display)
         return c;
   return NULL;
}

GLXContext _mesa_validate_glx_current(GLXContext handle)
{
   struct fake_glx_context *c;
   GLcontext *current = _mesa_get_current_context();
   for (c = ContextList; c; c = c->next)
      if ((GLXContext)c == handle)
         return c->xmesaContext->gl_ctx == current ? handle : NULL;
   return NULL;
}

static struct oldengl_drawable *
drawable_resource(Display *display, GLXDrawable handle)
{
   struct oldengl_drawable *d;
   for (d = DrawableList; d; d = d->next)
      if (d->display == display && d->handle == handle && !d->deleted)
         return d;
   return NULL;
}

static GLboolean outside_begin_end(void)
{
   GLcontext *c = _mesa_get_current_context();
   return !c || c->Driver.CurrentExecPrimitive == PRIM_OUTSIDE_BEGIN_END;
}

static int visual_config_id(XMesaVisual visual)
{
   int i;
   for (i = 0; i < NumVisuals; ++i)
      if (VisualTable[i] == visual)
         return i + 1;
   return 0;
}

static XMesaVisual config_resource(Display *display, GLXFBConfig handle)
{
   int i;
   for (i = 0; i < NumVisuals; ++i)
      if ((GLXFBConfig)VisualTable[i] == handle &&
          VisualTable[i]->display == display)
         return VisualTable[i];
   return NULL;
}

static struct oldengl_drawable *
register_drawable(Display *display, XMesaVisual visual, XMesaBuffer buffer,
                  int kind, GLboolean owns_pixmap)
{
   struct oldengl_drawable *d = CALLOC_STRUCT(oldengl_drawable);
   if (!d)
      return NULL;
   d->display = display;
   d->visual = visual;
   d->buffer = buffer;
   d->kind = kind;
   d->owns_pixmap = owns_pixmap;
   /* Reserve an actual XID through public Xlib. XAllocID is an internal
    * allocator requiring Xlib's display lock on modern implementations. */
   d->handle = kind == 0 ? buffer->frontbuffer :
      XCreatePixmap(display, RootWindow(display, visual->visinfo->screen), 1, 1, 1);
   if (!d->handle) {
      FREE(d);
      return NULL;
   }
   d->next = DrawableList;
   DrawableList = d;
   return d;
}

static void reap_drawables(void)
{
   struct oldengl_drawable **link = &DrawableList;
   while (*link) {
      struct oldengl_drawable *d = *link;
      struct fake_glx_context *c;
      GLboolean used = GL_FALSE;
      for (c = ContextList; c; c = c->next)
         if (c->draw == d || c->read == d)
            used = GL_TRUE;
      if (d->deleted && !used) {
         Pixmap pixmap = d->buffer->frontbuffer;
         XMesaDestroyBuffer(d->buffer);
         if (d->owns_pixmap)
            XFreePixmap(d->display, pixmap);
         if (d->kind != 0)
            XFreePixmap(d->display, d->handle);
         *link = d->next;
         FREE(d);
      } else {
         link = &d->next;
      }
   }
}

static void destroy_drawable(Display *display, GLXDrawable handle, int kind)
{
   struct oldengl_drawable *d = drawable_resource(display, handle);
   if (d && d->kind == kind) {
      d->deleted = GL_TRUE;
      reap_drawables();
   }
}

static void free_context(struct fake_glx_context *c)
{
   struct fake_glx_context **link = &ContextList;
   while (*link && *link != c)
      link = &(*link)->next;
   if (*link)
      *link = c->next;
   XMesaDestroyContext(c->xmesaContext);
   if (c->glxContext.xid)
      XFreePixmap(c->owner, c->glxContext.xid);
   FREE(c);
}

static GLXContext
create_context_for_visual(Display *display, XMesaVisual visual,
                          GLXContext share, Bool direct)
{
   struct fake_glx_context *c, *shared = NULL;
   /* An explicitly indirect request needs an unavailable GLX wire encoder. */
   if (!direct || !display || !visual || !outside_begin_end())
      return NULL;
   if (share) {
      shared = context_resource(display, share);
      if (!shared || shared->deleted ||
          shared->visual->mesa_visual.rgbMode != visual->mesa_visual.rgbMode)
         return NULL;
   }
   c = CALLOC_STRUCT(fake_glx_context);
   if (!c)
      return NULL;
   c->xmesaContext = XMesaCreateContext(visual,
                                      shared ? shared->xmesaContext : NULL);
   if (!c->xmesaContext) {
      FREE(c);
      return NULL;
   }
   c->owner = display;
   c->visual = visual;
   c->xmesaContext->direct = GL_TRUE;
   c->glxContext.isDirect = GL_TRUE;
   c->glxContext.xid = XCreatePixmap(display,
      RootWindow(display, visual->visinfo->screen), 1, 1, 1);
   c->next = ContextList;
   ContextList = c;
   return (GLXContext)c;
}

/*
 * Test if the given XVisualInfo is usable for Mesa rendering.
 */
static GLboolean is_usable_visual( XVisualInfo *vinfo )
{
   switch (vinfo->CLASS) {
      case StaticGray:
      case GrayScale:
         /* Any StaticGray/GrayScale visual works in RGB or CI mode */
         return GL_TRUE;
      case StaticColor:
      case PseudoColor:
	 /* Any StaticColor/PseudoColor visual of at least 4 bits */
	 if (vinfo->depth>=4) {
	    return GL_TRUE;
	 }
	 else {
	    return GL_FALSE;
	 }
      case TrueColor:
      case DirectColor:
	 /* Any depth of TrueColor or DirectColor works in RGB mode */
	 return GL_TRUE;
      default:
	 /* This should never happen */
	 return GL_FALSE;
   }
}



/*
 * Return the level (overlay, normal, underlay) of a given XVisualInfo.
 * Input:  dpy - the X display
 *         vinfo - the XVisualInfo to test
 * Return:  level of the visual:
 *             0 = normal planes
 *            >0 = overlay planes
 *            <0 = underlay planes
 */
static int level_of_visual( Display *dpy, XVisualInfo *vinfo )
{
   Atom overlayVisualsAtom;
   OverlayInfo *overlay_info = NULL;
   int numOverlaysPerScreen;
   Status status;
   Atom actualType;
   int actualFormat;
   unsigned long sizeData, bytesLeft;
   int i;

   /*
    * The SERVER_OVERLAY_VISUALS property on the root window contains
    * a list of overlay visuals.  Get that list now.
    */
   overlayVisualsAtom = XInternAtom(dpy,"SERVER_OVERLAY_VISUALS", True);
   if (overlayVisualsAtom == None) {
      return 0;
   }

   status = XGetWindowProperty(dpy, RootWindow( dpy, vinfo->screen ),
                               overlayVisualsAtom, 0L, (long) 10000, False,
                               overlayVisualsAtom, &actualType, &actualFormat,
                               &sizeData, &bytesLeft,
                               (unsigned char **) &overlay_info );

   if (status != Success || actualType != overlayVisualsAtom ||
       actualFormat != 32 || sizeData < 4) {
      /* something went wrong */
      XFree((void *) overlay_info);
      return 0;
   }

   /* search the overlay visual list for the visual ID of interest */
   numOverlaysPerScreen = (int) (sizeData / 4);
   for (i=0;i<numOverlaysPerScreen;i++) {
      OverlayInfo *ov;
      ov = overlay_info + i;
      if (ov->overlay_visual==vinfo->visualid) {
         /* found the visual */
         if (/*ov->transparent_type==1 &&*/ ov->layer!=0) {
            int level = ov->layer;
            XFree((void *) overlay_info);
            return level;
         }
         else {
            XFree((void *) overlay_info);
            return 0;
         }
      }
   }

   /* The visual ID was not found in the overlay list. */
   XFree((void *) overlay_info);
   return 0;
}




/*
 * Given an XVisualInfo and RGB, Double, and Depth buffer flags, save the
 * configuration in our list of GLX visuals.
 */
static XMesaVisual
save_glx_visual( Display *dpy, XVisualInfo *vinfo,
                 GLboolean rgbFlag, GLboolean alphaFlag, GLboolean dbFlag,
                 GLboolean stereoFlag,
                 GLint depth_size, GLint stencil_size,
                 GLint accumRedSize, GLint accumGreenSize,
                 GLint accumBlueSize, GLint accumAlphaSize,
                 GLint level )
{
   GLboolean ximageFlag = GL_TRUE;
   XMesaVisual xmvis;
   GLint i;
   GLboolean comparePointers;

   if (dbFlag) {
      /* Check if the MESA_BACK_BUFFER env var is set */
      char *backbuffer = getenv("MESA_BACK_BUFFER");
      if (backbuffer) {
         if (backbuffer[0]=='p' || backbuffer[0]=='P') {
            ximageFlag = GL_FALSE;
         }
         else if (backbuffer[0]=='x' || backbuffer[0]=='X') {
            ximageFlag = GL_TRUE;
         }
         else {
            fprintf(stderr, "Mesa: invalid value for MESA_BACK_BUFFER ");
            fprintf(stderr, "environment variable, using an XImage.\n");
         }
      }
   }

   /* Comparing IDs uses less memory but sometimes fails. */
   /* XXX revisit this after 3.0 is finished. */
   if (getenv("MESA_GLX_VISUAL_HACK"))
      comparePointers = GL_TRUE;
   else
      comparePointers = GL_FALSE;

   /* First check if a matching visual is already in the list */
   for (i=0; i<NumVisuals; i++) {
      XMesaVisual v = VisualTable[i];
      if (v->display == dpy
          && v->level == level
          && v->ximage_flag == ximageFlag
          && v->mesa_visual.rgbMode == rgbFlag
          && v->mesa_visual.doubleBufferMode == dbFlag
          && v->mesa_visual.stereoMode == stereoFlag
          && (v->mesa_visual.alphaBits > 0) == alphaFlag
          && (v->mesa_visual.depthBits >= depth_size || depth_size == 0)
          && (v->mesa_visual.stencilBits >= stencil_size || stencil_size == 0)
          && (v->mesa_visual.accumRedBits >= accumRedSize || accumRedSize == 0)
          && (v->mesa_visual.accumGreenBits >= accumGreenSize || accumGreenSize == 0)
          && (v->mesa_visual.accumBlueBits >= accumBlueSize || accumBlueSize == 0)
          && (v->mesa_visual.accumAlphaBits >= accumAlphaSize || accumAlphaSize == 0)) {
         /* now either compare XVisualInfo pointers or visual IDs */
         if ((!comparePointers && v->visinfo->visualid == vinfo->visualid)
             || (comparePointers && v->vishandle == vinfo)) {
            return v;
         }
      }
   }

   /* Create a new visual and add it to the list. */

   if (NumVisuals>=MAX_VISUALS) {
      fprintf( stderr, "GLX Error: maximum number of visuals exceeded\n");
      return NULL;
   }

   xmvis = XMesaCreateVisual( dpy, vinfo, rgbFlag, alphaFlag, dbFlag,
                              stereoFlag, ximageFlag,
                              depth_size, stencil_size,
                              accumRedSize, accumGreenSize,
                              accumBlueSize, accumAlphaSize, 0, level,
                              GLX_NONE_EXT );
   if (xmvis) {
      VisualTable[NumVisuals] = xmvis;
      NumVisuals++;
   }
   return xmvis;
}



/*
 * Create a GLX visual from a regular XVisualInfo.
 * This is called when Fake GLX is given an XVisualInfo which wasn't
 * returned by glXChooseVisual.  Since this is the first time we're
 * considering this visual we'll take a guess at reasonable values
 * for depth buffer size, stencil size, accum size, etc.
 * This is the best we can do with a client-side emulation of GLX.
 */
static XMesaVisual
create_glx_visual( Display *dpy, XVisualInfo *visinfo )
{
   int vislevel;

   vislevel = level_of_visual( dpy, visinfo );
   if (vislevel) {
      /* Configure this visual as a CI, single-buffered overlay */
      return save_glx_visual( dpy, visinfo,
                              GL_FALSE,  /* rgb */
                              GL_FALSE,  /* alpha */
                              GL_FALSE,  /* double */
                              GL_FALSE,  /* stereo */
                              0,         /* depth bits */
                              0,         /* stencil bits */
                              0,0,0,0,   /* accum bits */
                              vislevel   /* level */
                            );
   }
   else if (is_usable_visual( visinfo )) {
      if (getenv("MESA_GLX_FORCE_CI")) {
         /* Configure this visual as a COLOR INDEX visual. */
         return save_glx_visual( dpy, visinfo,
                                 GL_FALSE,   /* rgb */
                                 GL_FALSE,  /* alpha */
                                 GL_TRUE,   /* double */
                                 GL_FALSE,  /* stereo */
                                 DEFAULT_SOFTWARE_DEPTH_BITS,
                                 8 * sizeof(GLstencil),
                                 0 * sizeof(GLaccum), /* r */
                                 0 * sizeof(GLaccum), /* g */
                                 0 * sizeof(GLaccum), /* b */
                                 0 * sizeof(GLaccum), /* a */
                                 0          /* level */
                               );
      }
      else {
         /* Configure this visual as RGB, double-buffered, depth-buffered. */
         /* This is surely wrong for some people's needs but what else */
         /* can be done?  They should use glXChooseVisual(). */
         return save_glx_visual( dpy, visinfo,
                                 GL_TRUE,   /* rgb */
                                 GL_FALSE,  /* alpha */
                                 GL_TRUE,   /* double */
                                 GL_FALSE,  /* stereo */
                                 DEFAULT_SOFTWARE_DEPTH_BITS,
                                 8 * sizeof(GLstencil),
                                 8 * sizeof(GLaccum), /* r */
                                 8 * sizeof(GLaccum), /* g */
                                 8 * sizeof(GLaccum), /* b */
                                 8 * sizeof(GLaccum), /* a */
                                 0          /* level */
                               );
      }
   }
   else {
      fprintf(stderr,"Mesa: error in glXCreateContext: bad visual\n");
      return NULL;
   }
}



/*
 * Find the GLX visual associated with an XVisualInfo.
 */
static XMesaVisual
find_glx_visual( Display *dpy, XVisualInfo *vinfo )
{
   struct oldengl_visual_handle *handle;
   int i;

   for (handle = VisualHandles; handle; handle = handle->next)
      if (handle->display == dpy && handle->handle == vinfo)
         return handle->visual;

   /* First try to match pointers */
   for (i=0;i<NumVisuals;i++) {
      if (VisualTable[i]->display==dpy && VisualTable[i]->vishandle==vinfo) {
         return VisualTable[i];
      }
   }
   /* try to match visual id */
   for (i=0;i<NumVisuals;i++) {
      if (VisualTable[i]->display==dpy
          && VisualTable[i]->visinfo->visualid == vinfo->visualid) {
         return VisualTable[i];
      }
   }
   return NULL;
}



/*
 * Return the transparent pixel value for a GLX visual.
 * Input:  glxvis - the glx_visual
 * Return:  a pixel value or -1 if no transparent pixel
 */
static int transparent_pixel( XMesaVisual glxvis )
{
   Display *dpy = glxvis->display;
   XVisualInfo *vinfo = glxvis->visinfo;
   Atom overlayVisualsAtom;
   OverlayInfo *overlay_info = NULL;
   int numOverlaysPerScreen;
   Status status;
   Atom actualType;
   int actualFormat;
   unsigned long sizeData, bytesLeft;
   int i;

   /*
    * The SERVER_OVERLAY_VISUALS property on the root window contains
    * a list of overlay visuals.  Get that list now.
    */
   overlayVisualsAtom = XInternAtom(dpy,"SERVER_OVERLAY_VISUALS", True);
   if (overlayVisualsAtom == None) {
      return -1;
   }

   status = XGetWindowProperty(dpy, RootWindow( dpy, vinfo->screen ),
                               overlayVisualsAtom, 0L, (long) 10000, False,
                               overlayVisualsAtom, &actualType, &actualFormat,
                               &sizeData, &bytesLeft,
                               (unsigned char **) &overlay_info );

   if (status != Success || actualType != overlayVisualsAtom ||
       actualFormat != 32 || sizeData < 4) {
      /* something went wrong */
      XFree((void *) overlay_info);
      return -1;
   }

   /* search the overlay visual list for the visual ID of interest */
   numOverlaysPerScreen = (int) (sizeData / 4);
   for (i=0;i<numOverlaysPerScreen;i++) {
      OverlayInfo *ov;
      ov = overlay_info + i;
      if (ov->overlay_visual==vinfo->visualid) {
         /* found it! */
         if (ov->transparent_type==0) {
            /* type 0 indicates no transparency */
            XFree((void *) overlay_info);
            return -1;
         }
         else {
            /* ov->value is the transparent pixel */
            XFree((void *) overlay_info);
            return ov->value;
         }
      }
   }

   /* The visual ID was not found in the overlay list. */
   XFree((void *) overlay_info);
   return -1;
}



/*
 * Try to get an X visual which matches the given arguments.
 */
static XVisualInfo *get_visual( Display *dpy, int scr,
			        unsigned int depth, int xclass )
{
   XVisualInfo temp, *vis;
   long mask;
   int n;
   unsigned int default_depth;
   int default_class;

   mask = VisualScreenMask | VisualDepthMask | VisualClassMask;
   temp.screen = scr;
   temp.depth = depth;
   temp.CLASS = xclass;

   default_depth = DefaultDepth(dpy,scr);
   default_class = DefaultVisual(dpy,scr)->CLASS;

   if (depth==default_depth && xclass==default_class) {
      /* try to get root window's visual */
      temp.visualid = DefaultVisual(dpy,scr)->visualid;
      mask |= VisualIDMask;
   }

   vis = XGetVisualInfo( dpy, mask, &temp, &n );

   /* In case bits/pixel > 24, make sure color channels are still <=8 bits.
    * An SGI Infinite Reality system, for example, can have 30bpp pixels:
    * 10 bits per color channel.  Mesa's limited to a max of 8 bits/channel.
    */
   if (vis && depth > 24 && (xclass==TrueColor || xclass==DirectColor)) {
      if (_mesa_bitcount((GLuint) vis->red_mask  ) <= 8 &&
          _mesa_bitcount((GLuint) vis->green_mask) <= 8 &&
          _mesa_bitcount((GLuint) vis->blue_mask ) <= 8) {
         return vis;
      }
      else {
         XFree((void *) vis);
         return NULL;
      }
   }

   return vis;
}



/*
 * Retrieve the value of the given environment variable and find
 * the X visual which matches it.
 * Input:  dpy - the display
 *         screen - the screen number
 *         varname - the name of the environment variable
 * Return:  an XVisualInfo pointer to NULL if error.
 */
static XVisualInfo *get_env_visual(Display *dpy, int scr, const char *varname)
{
   char value[100], type[100];
   int depth, xclass = -1;
   XVisualInfo *vis;

   if (!getenv( varname )) {
      return NULL;
   }

   strncpy( value, getenv(varname), 100 );
   value[99] = 0;

   sscanf( value, "%s %d", type, &depth );

   if (strcmp(type,"TrueColor")==0)          xclass = TrueColor;
   else if (strcmp(type,"DirectColor")==0)   xclass = DirectColor;
   else if (strcmp(type,"PseudoColor")==0)   xclass = PseudoColor;
   else if (strcmp(type,"StaticColor")==0)   xclass = StaticColor;
   else if (strcmp(type,"GrayScale")==0)     xclass = GrayScale;
   else if (strcmp(type,"StaticGray")==0)    xclass = StaticGray;

   if (xclass>-1 && depth>0) {
      vis = get_visual( dpy, scr, depth, xclass );
      if (vis) {
	 return vis;
      }
   }

   fprintf( stderr, "Mesa: GLX unable to find visual class=%s, depth=%d.\n",
	    type, depth );
   return NULL;
}



/*
 * Select an X visual which satisfies the RGBA/CI flag and minimum depth.
 * Input:  dpy, screen - X display and screen number
 *         rgba - GL_TRUE = RGBA mode, GL_FALSE = CI mode
 *         min_depth - minimum visual depth
 *         preferred_class - preferred GLX visual class or DONT_CARE
 * Return:  pointer to an XVisualInfo or NULL.
 */
static XVisualInfo *choose_x_visual( Display *dpy, int screen,
				     GLboolean rgba, int min_depth,
                                     int preferred_class )
{
   XVisualInfo *vis;
   int xclass, visclass = 0;
   int depth;

   if (rgba) {
      Atom hp_cr_maps = XInternAtom(dpy, "_HP_RGB_SMOOTH_MAP_LIST", True);
      /* First see if the MESA_RGB_VISUAL env var is defined */
      vis = get_env_visual( dpy, screen, "MESA_RGB_VISUAL" );
      if (vis) {
	 return vis;
      }
      /* Otherwise, search for a suitable visual */
      if (preferred_class==DONT_CARE) {
         for (xclass=0;xclass<6;xclass++) {
            switch (xclass) {
               case 0:  visclass = TrueColor;    break;
               case 1:  visclass = DirectColor;  break;
               case 2:  visclass = PseudoColor;  break;
               case 3:  visclass = StaticColor;  break;
               case 4:  visclass = GrayScale;    break;
               case 5:  visclass = StaticGray;   break;
            }
            if (min_depth==0) {
               /* start with shallowest */
               for (depth=0;depth<=32;depth++) {
                  if (visclass==TrueColor && depth==8 && !hp_cr_maps) {
                     /* Special case:  try to get 8-bit PseudoColor before */
                     /* 8-bit TrueColor */
                     vis = get_visual( dpy, screen, 8, PseudoColor );
                     if (vis) {
                        return vis;
                     }
                  }
                  vis = get_visual( dpy, screen, depth, visclass );
                  if (vis) {
                     return vis;
                  }
               }
            }
            else {
               /* start with deepest */
               for (depth=32;depth>=min_depth;depth--) {
                  if (visclass==TrueColor && depth==8 && !hp_cr_maps) {
                     /* Special case:  try to get 8-bit PseudoColor before */
                     /* 8-bit TrueColor */
                     vis = get_visual( dpy, screen, 8, PseudoColor );
                     if (vis) {
                        return vis;
                     }
                  }
                  vis = get_visual( dpy, screen, depth, visclass );
                  if (vis) {
                     return vis;
                  }
               }
            }
         }
      }
      else {
         /* search for a specific visual class */
         switch (preferred_class) {
            case GLX_TRUE_COLOR_EXT:    visclass = TrueColor;    break;
            case GLX_DIRECT_COLOR_EXT:  visclass = DirectColor;  break;
            case GLX_PSEUDO_COLOR_EXT:  visclass = PseudoColor;  break;
            case GLX_STATIC_COLOR_EXT:  visclass = StaticColor;  break;
            case GLX_GRAY_SCALE_EXT:    visclass = GrayScale;    break;
            case GLX_STATIC_GRAY_EXT:   visclass = StaticGray;   break;
            default:   return NULL;
         }
         if (min_depth==0) {
            /* start with shallowest */
            for (depth=0;depth<=32;depth++) {
               vis = get_visual( dpy, screen, depth, visclass );
               if (vis) {
                  return vis;
               }
            }
         }
         else {
            /* start with deepest */
            for (depth=32;depth>=min_depth;depth--) {
               vis = get_visual( dpy, screen, depth, visclass );
               if (vis) {
                  return vis;
               }
            }
         }
      }
   }
   else {
      /* First see if the MESA_CI_VISUAL env var is defined */
      vis = get_env_visual( dpy, screen, "MESA_CI_VISUAL" );
      if (vis) {
	 return vis;
      }
      /* Otherwise, search for a suitable visual, starting with shallowest */
      if (preferred_class==DONT_CARE) {
         for (xclass=0;xclass<4;xclass++) {
            switch (xclass) {
               case 0:  visclass = PseudoColor;  break;
               case 1:  visclass = StaticColor;  break;
               case 2:  visclass = GrayScale;    break;
               case 3:  visclass = StaticGray;   break;
            }
            /* try 8-bit up through 16-bit */
            for (depth=8;depth<=16;depth++) {
               vis = get_visual( dpy, screen, depth, visclass );
               if (vis) {
                  return vis;
               }
            }
            /* try min_depth up to 8-bit */
            for (depth=min_depth;depth<8;depth++) {
               vis = get_visual( dpy, screen, depth, visclass );
               if (vis) {
                  return vis;
               }
            }
         }
      }
      else {
         /* search for a specific visual class */
         switch (preferred_class) {
            case GLX_TRUE_COLOR_EXT:    visclass = TrueColor;    break;
            case GLX_DIRECT_COLOR_EXT:  visclass = DirectColor;  break;
            case GLX_PSEUDO_COLOR_EXT:  visclass = PseudoColor;  break;
            case GLX_STATIC_COLOR_EXT:  visclass = StaticColor;  break;
            case GLX_GRAY_SCALE_EXT:    visclass = GrayScale;    break;
            case GLX_STATIC_GRAY_EXT:   visclass = StaticGray;   break;
            default:   return NULL;
         }
         /* try 8-bit up through 16-bit */
         for (depth=8;depth<=16;depth++) {
            vis = get_visual( dpy, screen, depth, visclass );
            if (vis) {
               return vis;
            }
         }
         /* try min_depth up to 8-bit */
         for (depth=min_depth;depth<8;depth++) {
            vis = get_visual( dpy, screen, depth, visclass );
            if (vis) {
               return vis;
            }
         }
      }
   }

   /* didn't find a visual */
   return NULL;
}



/*
 * Find the deepest X over/underlay visual of at least min_depth.
 * Input:  dpy, screen - X display and screen number
 *         level - the over/underlay level
 *         trans_type - transparent pixel type: GLX_NONE_EXT,
 *                      GLX_TRANSPARENT_RGB_EXT, GLX_TRANSPARENT_INDEX_EXT,
 *                      or DONT_CARE
 *         trans_value - transparent pixel value or DONT_CARE
 *         min_depth - minimum visual depth
 *         preferred_class - preferred GLX visual class or DONT_CARE
 * Return:  pointer to an XVisualInfo or NULL.
 */
static XVisualInfo *choose_x_overlay_visual( Display *dpy, int scr,
                                             GLboolean rgbFlag,
                                             int level, int trans_type,
                                             int trans_value,
                                             int min_depth,
                                             int preferred_class )
{
   Atom overlayVisualsAtom;
   OverlayInfo *overlay_info;
   int numOverlaysPerScreen;
   Status status;
   Atom actualType;
   int actualFormat;
   unsigned long sizeData, bytesLeft;
   int i;
   XVisualInfo *deepvis;
   int deepest;

   /*DEBUG int tt, tv; */

   switch (preferred_class) {
      case GLX_TRUE_COLOR_EXT:    preferred_class = TrueColor;    break;
      case GLX_DIRECT_COLOR_EXT:  preferred_class = DirectColor;  break;
      case GLX_PSEUDO_COLOR_EXT:  preferred_class = PseudoColor;  break;
      case GLX_STATIC_COLOR_EXT:  preferred_class = StaticColor;  break;
      case GLX_GRAY_SCALE_EXT:    preferred_class = GrayScale;    break;
      case GLX_STATIC_GRAY_EXT:   preferred_class = StaticGray;   break;
      default:                    preferred_class = DONT_CARE;
   }

   /*
    * The SERVER_OVERLAY_VISUALS property on the root window contains
    * a list of overlay visuals.  Get that list now.
    */
   overlayVisualsAtom = XInternAtom(dpy,"SERVER_OVERLAY_VISUALS", True);
   if (overlayVisualsAtom == (Atom) None) {
      return NULL;
   }

   status = XGetWindowProperty(dpy, RootWindow( dpy, scr ),
                               overlayVisualsAtom, 0L, (long) 10000, False,
                               overlayVisualsAtom, &actualType, &actualFormat,
                               &sizeData, &bytesLeft,
                               (unsigned char **) &overlay_info );

   if (status != Success || actualType != overlayVisualsAtom ||
       actualFormat != 32 || sizeData < 4) {
      /* something went wrong */
      return NULL;
   }

   /* Search for the deepest overlay which satisifies all criteria. */
   deepest = min_depth;
   deepvis = NULL;

   numOverlaysPerScreen = (int) (sizeData / 4);
   for (i=0;i<numOverlaysPerScreen;i++) {
      XVisualInfo *vislist, vistemplate;
      int count;
      OverlayInfo *ov;
      ov = overlay_info + i;

      if (ov->layer!=level) {
         /* failed overlay level criteria */
         continue;
      }
      if (!(trans_type==DONT_CARE
            || (trans_type==GLX_TRANSPARENT_INDEX_EXT
                && ov->transparent_type>0)
            || (trans_type==GLX_NONE_EXT && ov->transparent_type==0))) {
         /* failed transparent pixel type criteria */
         continue;
      }
      if (trans_value!=DONT_CARE && trans_value!=ov->value) {
         /* failed transparent pixel value criteria */
         continue;
      }

      /* get XVisualInfo and check the depth */
      vistemplate.visualid = ov->overlay_visual;
      vistemplate.screen = scr;
      vislist = XGetVisualInfo( dpy, VisualIDMask | VisualScreenMask,
                                &vistemplate, &count );

      if (count!=1) {
         /* something went wrong */
         continue;
      }
      if (preferred_class!=DONT_CARE && preferred_class!=vislist->CLASS) {
         /* wrong visual class */
         continue;
      }

      /* if RGB was requested, make sure we have True/DirectColor */
      if (rgbFlag && vislist->CLASS != TrueColor
          && vislist->CLASS != DirectColor)
         continue;

      /* if CI was requested, make sure we have a color indexed visual */
      if (!rgbFlag
          && (vislist->CLASS == TrueColor || vislist->CLASS == DirectColor))
         continue;

      if (deepvis==NULL || vislist->depth > deepest) {
         /* YES!  found a satisfactory visual */
         if (deepvis) {
            XFree( deepvis );
         }
         deepest = vislist->depth;
         deepvis = vislist;
         /* DEBUG  tt = ov->transparent_type;*/
         /* DEBUG  tv = ov->value; */
      }
   }

/*DEBUG
   if (deepvis) {
      printf("chose 0x%x:  layer=%d depth=%d trans_type=%d trans_value=%d\n",
             deepvis->visualid, level, deepvis->depth, tt, tv );
   }
*/
   return deepvis;
}


/**********************************************************************/
/***                  Begin Fake GLX API Functions                  ***/
/**********************************************************************/


static XVisualInfo *
Fake_glXChooseVisual( Display *dpy, int screen, int *list )
{
   int *parselist;
   XVisualInfo *vis;
   int min_ci = 0;
   int min_red=0, min_green=0, min_blue=0;
   GLboolean rgb_flag = GL_FALSE;
   GLboolean alpha_flag = GL_FALSE;
   GLboolean double_flag = GL_FALSE;
   GLboolean stereo_flag = GL_FALSE;
   GLint depth_size = 0;
   GLint stencil_size = 0;
   GLint accumRedSize = 0;
   GLint accumGreenSize = 0;
   GLint accumBlueSize = 0;
   GLint accumAlphaSize = 0;
   int level = 0;
   int visual_type = DONT_CARE;
   int trans_type = DONT_CARE;
   int trans_value = DONT_CARE;
   GLint caveat = DONT_CARE;

   parselist = list;

   while (*parselist) {

      switch (*parselist) {
	 case GLX_USE_GL:
	    /* ignore */
	    parselist++;
	    break;
	 case GLX_BUFFER_SIZE:
	    parselist++;
	    min_ci = *parselist++;
	    break;
	 case GLX_LEVEL:
	    parselist++;
            level = *parselist++;
	    break;
	 case GLX_RGBA:
	    rgb_flag = GL_TRUE;
	    parselist++;
	    break;
	 case GLX_DOUBLEBUFFER:
	    double_flag = GL_TRUE;
	    parselist++;
	    break;
	 case GLX_STEREO:
            stereo_flag = GL_TRUE;
            return NULL;
	 case GLX_AUX_BUFFERS:
	    /* ignore */
	    parselist++;
	    parselist++;
	    break;
	 case GLX_RED_SIZE:
	    parselist++;
	    min_red = *parselist++;
	    break;
	 case GLX_GREEN_SIZE:
	    parselist++;
	    min_green = *parselist++;
	    break;
	 case GLX_BLUE_SIZE:
	    parselist++;
	    min_blue = *parselist++;
	    break;
	 case GLX_ALPHA_SIZE:
	    parselist++;
            {
               GLint size = *parselist++;
               alpha_flag = size>0 ? 1 : 0;
            }
	    break;
	 case GLX_DEPTH_SIZE:
	    parselist++;
	    depth_size = *parselist++;
	    break;
	 case GLX_STENCIL_SIZE:
	    parselist++;
	    stencil_size = *parselist++;
	    break;
	 case GLX_ACCUM_RED_SIZE:
	    parselist++;
            {
               GLint size = *parselist++;
               accumRedSize = MAX2( accumRedSize, size );
            }
            break;
	 case GLX_ACCUM_GREEN_SIZE:
	    parselist++;
            {
               GLint size = *parselist++;
               accumGreenSize = MAX2( accumGreenSize, size );
            }
            break;
	 case GLX_ACCUM_BLUE_SIZE:
	    parselist++;
            {
               GLint size = *parselist++;
               accumBlueSize = MAX2( accumBlueSize, size );
            }
            break;
	 case GLX_ACCUM_ALPHA_SIZE:
	    parselist++;
            {
               GLint size = *parselist++;
               accumAlphaSize = MAX2( accumAlphaSize, size );
            }
	    break;

         /*
          * GLX_EXT_visual_info extension
          */
         case GLX_X_VISUAL_TYPE_EXT:
            parselist++;
            visual_type = *parselist++;
            break;
         case GLX_TRANSPARENT_TYPE_EXT:
            parselist++;
            trans_type = *parselist++;
            break;
         case GLX_TRANSPARENT_INDEX_VALUE_EXT:
            parselist++;
            trans_value = *parselist++;
            break;
         case GLX_TRANSPARENT_RED_VALUE_EXT:
         case GLX_TRANSPARENT_GREEN_VALUE_EXT:
         case GLX_TRANSPARENT_BLUE_VALUE_EXT:
         case GLX_TRANSPARENT_ALPHA_VALUE_EXT:
	    /* ignore */
	    parselist++;
	    parselist++;
	    break;

         /*
          * GLX_EXT_visual_info extension
          */
         case GLX_VISUAL_CAVEAT_EXT:
            parselist++;
            caveat = *parselist++; /* ignored for now */
            break;

	 case None:
	    break;
	 default:
	    /* undefined attribute */
	    return NULL;
      }
   }

   /*
    * Since we're only simulating the GLX extension this function will never
    * find any real GL visuals.  Instead, all we can do is try to find an RGB
    * or CI visual of appropriate depth.  Other requested attributes such as
    * double buffering, depth buffer, etc. will be associated with the X
    * visual and stored in the VisualTable[].
    */
   if (level==0) {
      /* normal color planes */
      if (rgb_flag) {
         /* Get an RGB visual */
         int min_rgb = min_red + min_green + min_blue;
         if (min_rgb>1 && min_rgb<8) {
            /* a special case to be sure we can get a monochrome visual */
            min_rgb = 1;
         }
         vis = choose_x_visual( dpy, screen, rgb_flag, min_rgb, visual_type );
      }
      else {
         /* Get a color index visual */
         vis = choose_x_visual( dpy, screen, rgb_flag, min_ci, visual_type );
         accumRedSize = accumGreenSize = accumBlueSize = accumAlphaSize = 0;
      }
   }
   else {
      /* over/underlay planes */
      if (rgb_flag) {
         /* rgba overlay */
         int min_rgb = min_red + min_green + min_blue;
         if (min_rgb>1 && min_rgb<8) {
            /* a special case to be sure we can get a monochrome visual */
            min_rgb = 1;
         }
         vis = choose_x_overlay_visual( dpy, screen, rgb_flag, level,
                              trans_type, trans_value, min_rgb, visual_type );
      }
      else {
         /* color index overlay */
         vis = choose_x_overlay_visual( dpy, screen, rgb_flag, level,
                              trans_type, trans_value, min_ci, visual_type );
      }
   }

   if (vis) {
      /* Note: we're not exactly obeying the glXChooseVisual rules here.
       * When GLX_DEPTH_SIZE = 1 is specified we're supposed to choose the
       * largest depth buffer size, which is 32bits/value.  Instead, we
       * return 16 to maintain performance with earlier versions of Mesa.
       */
      if (depth_size > 24)
         depth_size = 31;   /* 32 causes int overflow problems */
      else if (depth_size > 16)
         depth_size = 24;
      else if (depth_size > 0)
         depth_size = DEFAULT_SOFTWARE_DEPTH_BITS; /*16*/

      /* we only support one size of stencil and accum buffers. */
      if (stencil_size > 0)
         stencil_size = STENCIL_BITS;
      if (accumRedSize > 0 || accumGreenSize > 0 || accumBlueSize > 0 ||
          accumAlphaSize > 0) {
         accumRedSize = ACCUM_BITS;
         accumGreenSize = ACCUM_BITS;
         accumBlueSize = ACCUM_BITS;
         accumAlphaSize = alpha_flag ? ACCUM_BITS : 0;
      }

      {
      XMesaVisual selected = save_glx_visual( dpy, vis, rgb_flag, alpha_flag, double_flag,
                            stereo_flag, depth_size, stencil_size,
                            accumRedSize, accumGreenSize,
                            accumBlueSize, accumAlphaSize,
                            level );
      if (!selected)
         return NULL;
      remember_visual(dpy, vis, selected);
      }
   }

   return vis;
}




static GLXContext
Fake_glXCreateContext( Display *dpy, XVisualInfo *visinfo,
                       GLXContext share_list, Bool direct )
{
   XMesaVisual visual;
   if (!dpy || !visinfo || !outside_begin_end())
      return NULL;
   visual = find_glx_visual(dpy, visinfo);
   if (!visual)
      visual = create_glx_visual(dpy, visinfo);
   return create_context_for_visual(dpy, visual, share_list, direct);
}


/* GLX 1.3 and later */
static Bool
Fake_glXMakeContextCurrent( Display *dpy, GLXDrawable draw,
                            GLXDrawable read, GLXContext ctx )
{
   struct fake_glx_context *next = NULL;
   struct fake_glx_context *old = (struct fake_glx_context *)glXGetCurrentContext();
   struct oldengl_drawable *draw_resource = NULL, *read_resource = NULL;
   XMesaBuffer draw_buffer = NULL, read_buffer = NULL;
   GLXDrawable handles[2];
   struct oldengl_drawable **resources[2];
   int i;
   if (!dpy || !outside_begin_end())
      return False;
   if (ctx) {
      next = context_resource(dpy, ctx);
      if (!next || next->deleted || !draw || !read)
         return False;
      handles[0] = draw; handles[1] = read;
      resources[0] = &draw_resource; resources[1] = &read_resource;
      for (i = 0; i < 2; ++i) {
         struct oldengl_drawable *resource = drawable_resource(dpy, handles[i]);
         if (!resource) {
            XWindowAttributes attributes;
            XMesaBuffer buffer;
            if (!XGetWindowAttributes(dpy, handles[i], &attributes) ||
                attributes.class != InputOutput ||
                XVisualIDFromVisual(attributes.visual) !=
                   next->visual->visinfo->visualid)
               return False;
            buffer = XMesaFindBuffer(dpy, handles[i]);
            if (!buffer)
               buffer = XMesaCreateWindowBuffer2(next->visual, handles[i],
                                                 next->xmesaContext);
            if (!buffer)
               return False;
            resource = register_drawable(dpy, next->visual, buffer, 0, GL_FALSE);
            if (!resource) {
               XMesaDestroyBuffer(buffer);
               return False;
            }
         }
         if (resource->visual->visinfo->screen != next->visual->visinfo->screen ||
             resource->visual->visinfo->visualid != next->visual->visinfo->visualid ||
             resource->visual->mesa_visual.rgbMode != next->visual->mesa_visual.rgbMode ||
             resource->visual->mesa_visual.doubleBufferMode != next->visual->mesa_visual.doubleBufferMode)
            return False;
         *resources[i] = resource;
      }
      draw_buffer = draw_resource->buffer;
      read_buffer = read_resource->buffer;
   } else if (draw || read) {
      return False;
   }
   /* Mesa's context binder does not finish the previous TNL vertex batch. */
   if (_mesa_get_current_context())
      glFlush();
   if (!XMesaMakeCurrent2(next ? next->xmesaContext : NULL,
                          draw_buffer, read_buffer))
      return False;
   if (old) {
      old->draw = old->read = NULL;
      old->glxContext.currentDrawable = 0;
      old->glxContext.currentReadable = 0;
      old->glxContext.currentDpy = NULL;
   }
   if (next) {
      next->draw = draw_resource;
      next->read = read_resource;
      next->glxContext.currentDpy = dpy;
      next->glxContext.currentDrawable = draw;
      next->glxContext.currentReadable = read;
   }
   if (old && old != next && old->deleted)
      free_context(old);
   reap_drawables();
   return True;
}



static Bool
Fake_glXMakeCurrent( Display *dpy, GLXDrawable drawable, GLXContext ctx )
{
   return Fake_glXMakeContextCurrent( dpy, drawable, drawable, ctx );
}



static GLXPixmap
Fake_glXCreateGLXPixmap( Display *dpy, XVisualInfo *visinfo, Pixmap pixmap )
{
   XMesaVisual visual;
   XMesaBuffer buffer;
   struct oldengl_drawable *resource;
   if (!dpy || !visinfo || !pixmap)
      return 0;
   visual = find_glx_visual(dpy, visinfo);
   if (!visual)
      visual = create_glx_visual(dpy, visinfo);
   if (!visual)
      return 0;
   buffer = XMesaCreatePixmapBuffer(visual, pixmap, 0);
   if (!buffer)
      return 0;
   resource = register_drawable(dpy, visual, buffer, 2, GL_FALSE);
   if (!resource) {
      XMesaDestroyBuffer(buffer);
      return 0;
   }
   return resource->handle;
}


/*** GLX_MESA_pixmap_colormap ***/

static GLXPixmap
Fake_glXCreateGLXPixmapMESA( Display *dpy, XVisualInfo *visinfo,
                             Pixmap pixmap, Colormap cmap )
{
   XMesaVisual v;
   XMesaBuffer b;

   v = find_glx_visual( dpy, visinfo );
   if (!v) {
      v = create_glx_visual( dpy, visinfo );
      if (!v) {
         /* unusable visual */
         return 0;
      }
   }

   b = XMesaCreatePixmapBuffer( v, pixmap, cmap );
   if (!b) {
      return 0;
   }
   return b->frontbuffer;
}


static void
Fake_glXDestroyGLXPixmap( Display *dpy, GLXPixmap pixmap )
{
   destroy_drawable(dpy, pixmap, 2);
}



static void
Fake_glXCopyContext( Display *dpy, GLXContext src, GLXContext dst,
                     unsigned long mask )
{
   struct fake_glx_context *source = context_resource(dpy, src);
   struct fake_glx_context *destination = context_resource(dpy, dst);
   if (!source || !destination || source->deleted || destination->deleted ||
       glXGetCurrentContext() == dst || !outside_begin_end())
      return;
   if (glXGetCurrentContext() == src) {
      glFlush();
      FLUSH_CURRENT(source->xmesaContext->gl_ctx, 0);
   }
   _mesa_copy_context(source->xmesaContext->gl_ctx,
                      destination->xmesaContext->gl_ctx, (GLuint)mask);
}



static Bool
Fake_glXQueryExtension( Display *dpy, int *errorb, int *event )
{
   if (!dpy)
      return False;
   /* This library implements GLX client-side; there is no server error range. */
   if (errorb) *errorb = 0;
   if (event) *event = 0;
   return True;
}


extern void _kw_ungrab_all( Display *dpy );
void _kw_ungrab_all( Display *dpy )
{
   XUngrabPointer( dpy, CurrentTime );
   XUngrabKeyboard( dpy, CurrentTime );
}


static void
Fake_glXDestroyContext( Display *dpy, GLXContext ctx )
{
   struct fake_glx_context *resource = context_resource(dpy, ctx);
   if (!resource || resource->deleted)
      return;
   resource->deleted = GL_TRUE;
   if (glXGetCurrentContext() != ctx) {
      free_context(resource);
      reap_drawables();
   }
}



static Bool
Fake_glXIsDirect( Display *dpy, GLXContext ctx )
{
   struct fake_glx_context *resource = context_resource(dpy, ctx);
   return resource ? resource->glxContext.isDirect : False;
}



static void
Fake_glXSwapBuffers( Display *dpy, GLXDrawable drawable )
{
   struct oldengl_drawable *resource = drawable_resource(dpy, drawable);
   XMesaBuffer buffer = resource ? resource->buffer : XMesaFindBuffer(dpy, drawable);
   if (!outside_begin_end())
      return;
   if (_mesa_get_current_context())
      glFlush();
   if (buffer)
      XMesaSwapBuffers(buffer);
}



/*** GLX_MESA_copy_sub_buffer ***/

static void
Fake_glXCopySubBufferMESA( Display *dpy, GLXDrawable drawable,
                           int x, int y, int width, int height )
{
   XMesaBuffer buffer = XMesaFindBuffer( dpy, drawable );
   if (buffer) {
      XMesaCopySubBuffer(buffer, x, y, width, height);
   }
   else if (getenv("MESA_DEBUG")) {
      fprintf(stderr, "Mesa Warning: glXCopySubBufferMESA: invalid drawable\n");
   }
}



static Bool
Fake_glXQueryVersion( Display *dpy, int *maj, int *min )
{
   (void) dpy;
   /* Return GLX version, not Mesa version */
   assert(CLIENT_MAJOR_VERSION == SERVER_MAJOR_VERSION);
   *maj = CLIENT_MAJOR_VERSION;
   *min = MIN2( CLIENT_MINOR_VERSION, SERVER_MINOR_VERSION );
   return True;
}



/*
 * Query the GLX attributes of the given XVisualInfo.
 */
static int
Fake_glXGetConfig( Display *dpy, XVisualInfo *visinfo,
                   int attrib, int *value )
{
   XMesaVisual glxvis;

   glxvis = find_glx_visual( dpy, visinfo );
   if (!glxvis) {
      /* this visual wasn't obtained with glXChooseVisual */
      glxvis = create_glx_visual( dpy, visinfo );
      if (!glxvis) {
	 /* this visual can't be used for GL rendering */
	 if (attrib==GLX_USE_GL) {
	    *value = (int) False;
	    return 0;
	 }
	 else {
	    /*fprintf( stderr, "Mesa: Error in glXGetConfig: bad visual\n");*/
	    return GLX_BAD_VISUAL;
	 }
      }
   }

   switch(attrib) {
      case GLX_USE_GL:
         *value = (int) True;
	 return 0;
      case GLX_BUFFER_SIZE:
	 *value = visinfo->depth;
	 return 0;
      case GLX_LEVEL:
	 *value = glxvis->level;
	 return 0;
      case GLX_RGBA:
	 if (glxvis->mesa_visual.rgbMode) {
	    *value = True;
	 }
	 else {
	    *value = False;
	 }
	 return 0;
      case GLX_DOUBLEBUFFER:
	 *value = (int) glxvis->mesa_visual.doubleBufferMode;
	 return 0;
      case GLX_STEREO:
	 *value = (int) glxvis->mesa_visual.stereoMode;
	 return 0;
      case GLX_AUX_BUFFERS:
	 *value = (int) False;
	 return 0;
      case GLX_RED_SIZE:
         *value = glxvis->mesa_visual.redBits;
	 return 0;
      case GLX_GREEN_SIZE:
         *value = glxvis->mesa_visual.greenBits;
	 return 0;
      case GLX_BLUE_SIZE:
         *value = glxvis->mesa_visual.blueBits;
	 return 0;
      case GLX_ALPHA_SIZE:
         *value = glxvis->mesa_visual.alphaBits;
	 return 0;
      case GLX_DEPTH_SIZE:
         *value = glxvis->mesa_visual.depthBits;
	 return 0;
      case GLX_STENCIL_SIZE:
	 *value = glxvis->mesa_visual.stencilBits;
	 return 0;
      case GLX_ACCUM_RED_SIZE:
	 *value = glxvis->mesa_visual.accumRedBits;
	 return 0;
      case GLX_ACCUM_GREEN_SIZE:
	 *value = glxvis->mesa_visual.accumGreenBits;
	 return 0;
      case GLX_ACCUM_BLUE_SIZE:
	 *value = glxvis->mesa_visual.accumBlueBits;
	 return 0;
      case GLX_ACCUM_ALPHA_SIZE:
         *value = glxvis->mesa_visual.accumAlphaBits;
	 return 0;

      /*
       * GLX_EXT_visual_info extension
       */
      case GLX_X_VISUAL_TYPE_EXT:
         switch (glxvis->visinfo->CLASS) {
            case StaticGray:   *value = GLX_STATIC_GRAY_EXT;   return 0;
            case GrayScale:    *value = GLX_GRAY_SCALE_EXT;    return 0;
            case StaticColor:  *value = GLX_STATIC_GRAY_EXT;   return 0;
            case PseudoColor:  *value = GLX_PSEUDO_COLOR_EXT;  return 0;
            case TrueColor:    *value = GLX_TRUE_COLOR_EXT;    return 0;
            case DirectColor:  *value = GLX_DIRECT_COLOR_EXT;  return 0;
         }
         return 0;
      case GLX_TRANSPARENT_TYPE_EXT:
         if (glxvis->level==0) {
            /* normal planes */
            *value = GLX_NONE_EXT;
         }
         else if (glxvis->level>0) {
            /* overlay */
            if (glxvis->mesa_visual.rgbMode) {
               *value = GLX_TRANSPARENT_RGB_EXT;
            }
            else {
               *value = GLX_TRANSPARENT_INDEX_EXT;
            }
         }
         else if (glxvis->level<0) {
            /* underlay */
            *value = GLX_NONE_EXT;
         }
         return 0;
      case GLX_TRANSPARENT_INDEX_VALUE_EXT:
         {
            int pixel = transparent_pixel( glxvis );
            if (pixel>=0) {
               *value = pixel;
            }
            /* else undefined */
         }
         return 0;
      case GLX_TRANSPARENT_RED_VALUE_EXT:
         /* undefined */
         return 0;
      case GLX_TRANSPARENT_GREEN_VALUE_EXT:
         /* undefined */
         return 0;
      case GLX_TRANSPARENT_BLUE_VALUE_EXT:
         /* undefined */
         return 0;
      case GLX_TRANSPARENT_ALPHA_VALUE_EXT:
         /* undefined */
         return 0;

      /*
       * GLX_EXT_visual_info extension
       */
      case GLX_VISUAL_CAVEAT_EXT:
         /* test for zero, just in case */
         if (glxvis->VisualCaveat > 0)
            *value = glxvis->VisualCaveat;
         else
            *value = GLX_NONE_EXT;
         return 0;

      /*
       * Extensions
       */
      default:
	 return GLX_BAD_ATTRIBUTE;
   }
}



static void
Fake_glXWaitGL( void )
{
   if (_mesa_get_current_context() && outside_begin_end())
      glFinish();
}



static void
Fake_glXWaitX( void )
{
   Display *display = glXGetCurrentDisplay();
   if (display)
      XSync(display, False);
}


/*
 * Return the extensions string, which is 3Dfx-dependant.
 */
static const char *get_extensions( void )
{
   return "GLX_ARB_get_proc_address GLX_EXT_visual_info GLX_EXT_visual_rating GLX_SGI_make_current_read";
}



/* GLX 1.1 and later */
static const char *
Fake_glXQueryExtensionsString( Display *dpy, int screen )
{
   (void) dpy;
   (void) screen;
   return get_extensions();
}



/* GLX 1.1 and later */
static const char *
Fake_glXQueryServerString( Display *dpy, int screen, int name )
{
   static char version[1000];
   sprintf(version, "%d.%d %s", SERVER_MAJOR_VERSION, SERVER_MINOR_VERSION,
           MESA_GLX_VERSION);

   (void) dpy;
   (void) screen;

   switch (name) {
      case GLX_EXTENSIONS:
         return get_extensions();
      case GLX_VENDOR:
	 return VENDOR;
      case GLX_VERSION:
	 return version;
      default:
         return NULL;
   }
}



/* GLX 1.1 and later */
static const char *
Fake_glXGetClientString( Display *dpy, int name )
{
   static char version[1000];
   sprintf(version, "%d.%d %s", CLIENT_MAJOR_VERSION, CLIENT_MINOR_VERSION,
           MESA_GLX_VERSION);

   (void) dpy;

   switch (name) {
      case GLX_EXTENSIONS:
         return get_extensions();
      case GLX_VENDOR:
	 return VENDOR;
      case GLX_VERSION:
	 return version;
      default:
         return NULL;
   }
}



/*
 * GLX 1.3 and later
 */

/* All FBConfigs are immutable XMesaVisual resources. */
static int Fake_glXGetFBConfigAttrib(Display *, GLXFBConfig, int, int *);

static void populate_fbconfigs(Display *display, int screen)
{
   XVisualInfo template_info, *visuals;
   int count, i, double_buffer, alpha_buffer;
   if (!display || screen < 0 || screen >= ScreenCount(display))
      return;
   template_info.screen = screen;
   visuals = XGetVisualInfo(display, VisualScreenMask, &template_info, &count);
   if (!visuals)
      return;
   for (i = 0; i < count; ++i) {
      if (!is_usable_visual(&visuals[i]) || level_of_visual(display, &visuals[i]) != 0)
         continue;
      for (double_buffer = 0; double_buffer < 2; ++double_buffer) {
         XMesaVisual visual;
         for (alpha_buffer = 0; alpha_buffer < 2; ++alpha_buffer) {
            visual = save_glx_visual(display, &visuals[i], GL_TRUE,
              alpha_buffer ? GL_TRUE : GL_FALSE,
              double_buffer ? GL_TRUE : GL_FALSE, GL_FALSE,
              24, STENCIL_BITS, ACCUM_BITS, ACCUM_BITS, ACCUM_BITS,
              alpha_buffer ? ACCUM_BITS : 0, 0);
            if (visual)
               visual->vishandle = visual->visinfo;
         }
         if (visuals[i].CLASS != TrueColor && visuals[i].CLASS != DirectColor) {
            visual = save_glx_visual(display, &visuals[i],
              GL_FALSE, GL_FALSE, double_buffer ? GL_TRUE : GL_FALSE, GL_FALSE,
              24, STENCIL_BITS, 0, 0, 0, 0, 0);
            if (visual)
               visual->vishandle = visual->visinfo;
         }
      }
   }
   XFree(visuals);
}

static int config_matches(Display *display, XMesaVisual visual, const int *attributes)
{
   int default_attributes[] = {GLX_LEVEL, 0, GLX_RENDER_TYPE, GLX_RGBA_BIT,
      GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT, GLX_STEREO, False,
      GLX_TRANSPARENT_TYPE, GLX_NONE, None};
   const int *lists[2];
   int list;
   lists[0] = default_attributes;
   lists[1] = attributes;
   for (list = 0; list < 2; ++list) {
      const int *attribute = lists[list];
      while (attribute && *attribute != None) {
         int token = attribute[0], desired = attribute[1], actual;
         const int *override;
         attribute += 2;
         if (!list && attributes) {
            for (override = attributes; *override != None; override += 2)
               if (*override == token)
                  break;
            if (*override != None)
               continue;
         }
         if ((unsigned int)desired == (unsigned int)GLX_DONT_CARE)
            continue;
         if (Fake_glXGetFBConfigAttrib(display, (GLXFBConfig)visual, token, &actual) != Success)
            return 0;
         switch (token) {
            case GLX_RED_SIZE: case GLX_GREEN_SIZE: case GLX_BLUE_SIZE:
            case GLX_ALPHA_SIZE: case GLX_DEPTH_SIZE: case GLX_STENCIL_SIZE:
            case GLX_ACCUM_RED_SIZE: case GLX_ACCUM_GREEN_SIZE:
            case GLX_ACCUM_BLUE_SIZE: case GLX_ACCUM_ALPHA_SIZE:
            case GLX_BUFFER_SIZE: case GLX_AUX_BUFFERS:
               if (actual < desired)
                  return 0;
               break;
            case GLX_RENDER_TYPE: case GLX_DRAWABLE_TYPE:
               if ((actual & desired) != desired)
                  return 0;
               break;
            case GLX_FBCONFIG_ID: case GLX_LEVEL: case GLX_DOUBLEBUFFER:
            case GLX_STEREO: case GLX_X_RENDERABLE: case GLX_X_VISUAL_TYPE:
            case GLX_CONFIG_CAVEAT: case GLX_VISUAL_ID:
            case GLX_TRANSPARENT_TYPE: case GLX_TRANSPARENT_INDEX_VALUE:
            case GLX_TRANSPARENT_RED_VALUE: case GLX_TRANSPARENT_GREEN_VALUE:
            case GLX_TRANSPARENT_BLUE_VALUE: case GLX_TRANSPARENT_ALPHA_VALUE:
               if (actual != desired)
                  return 0;
               break;
            default:
               return 0;
         }
      }
   }
   return 1;
}

static GLXFBConfig *
Fake_glXChooseFBConfig( Display *dpy, int screen,
                        const int *attribList, int *nitems )
{
   GLXFBConfig *result;
   int i, count = 0;
   if (nitems)
      *nitems = 0;
   if (!dpy || !nitems || screen < 0 || screen >= ScreenCount(dpy))
      return NULL;
   populate_fbconfigs(dpy, screen);
   result = (GLXFBConfig *)malloc(NumVisuals * sizeof(GLXFBConfig));
   if (!result)
      return NULL;
   for (i = 0; i < NumVisuals; ++i) {
      XMesaVisual visual = VisualTable[i];
      if (visual->display == dpy && visual->visinfo->screen == screen &&
          config_matches(dpy, visual, attribList))
         result[count++] = (GLXFBConfig)visual;
   }
   if (!count) {
      free(result);
      return NULL;
   }
   *nitems = count;
   return result;
}


static int
Fake_glXGetFBConfigAttrib( Display *dpy, GLXFBConfig config,
                           int attribute, int *value )
{
   XMesaVisual v = config_resource(dpy, config);
   if (!v || !value)
      return GLX_BAD_VISUAL;
   switch (attribute) {
      case GLX_FBCONFIG_ID: *value = visual_config_id(v); break;
      case GLX_VISUAL_ID: *value = v->visinfo->visualid; break;
      case GLX_BUFFER_SIZE:
         *value = v->mesa_visual.rgbMode ? v->mesa_visual.redBits +
                  v->mesa_visual.greenBits + v->mesa_visual.blueBits +
                  v->mesa_visual.alphaBits : v->mesa_visual.indexBits; break;
      case GLX_LEVEL: *value = v->level; break;
      case GLX_DOUBLEBUFFER: *value = v->mesa_visual.doubleBufferMode; break;
      case GLX_STEREO: *value = v->mesa_visual.stereoMode; break;
      case GLX_AUX_BUFFERS: *value = v->mesa_visual.numAuxBuffers; break;
      case GLX_RED_SIZE: *value = v->mesa_visual.redBits; break;
      case GLX_GREEN_SIZE: *value = v->mesa_visual.greenBits; break;
      case GLX_BLUE_SIZE: *value = v->mesa_visual.blueBits; break;
      case GLX_ALPHA_SIZE: *value = v->mesa_visual.alphaBits; break;
      case GLX_DEPTH_SIZE: *value = v->mesa_visual.depthBits; break;
      case GLX_STENCIL_SIZE: *value = v->mesa_visual.stencilBits; break;
      case GLX_ACCUM_RED_SIZE: *value = v->mesa_visual.accumRedBits; break;
      case GLX_ACCUM_GREEN_SIZE: *value = v->mesa_visual.accumGreenBits; break;
      case GLX_ACCUM_BLUE_SIZE: *value = v->mesa_visual.accumBlueBits; break;
      case GLX_ACCUM_ALPHA_SIZE: *value = v->mesa_visual.accumAlphaBits; break;
      case GLX_RENDER_TYPE:
         *value = v->mesa_visual.rgbMode ? GLX_RGBA_BIT : GLX_COLOR_INDEX_BIT; break;
      case GLX_DRAWABLE_TYPE:
         *value = GLX_WINDOW_BIT | GLX_PIXMAP_BIT | GLX_PBUFFER_BIT; break;
      case GLX_X_RENDERABLE: *value = True; break;
      case GLX_X_VISUAL_TYPE:
         switch (v->visinfo->CLASS) {
            case GrayScale: *value = GLX_GRAY_SCALE; break;
            case StaticGray: *value = GLX_STATIC_GRAY; break;
            case StaticColor: *value = GLX_STATIC_COLOR; break;
            case PseudoColor: *value = GLX_PSEUDO_COLOR; break;
            case TrueColor: *value = GLX_TRUE_COLOR; break;
            case DirectColor: *value = GLX_DIRECT_COLOR; break;
            default: return GLX_BAD_VISUAL;
         }
         break;
      case GLX_CONFIG_CAVEAT: *value = GLX_SLOW_CONFIG; break;
      case GLX_TRANSPARENT_TYPE: *value = GLX_NONE; break;
      case GLX_TRANSPARENT_INDEX_VALUE: case GLX_TRANSPARENT_RED_VALUE:
      case GLX_TRANSPARENT_GREEN_VALUE: case GLX_TRANSPARENT_BLUE_VALUE:
      case GLX_TRANSPARENT_ALPHA_VALUE: *value = 0; break;
      case GLX_MAX_PBUFFER_WIDTH: *value = MAX_WIDTH; break;
      case GLX_MAX_PBUFFER_HEIGHT: *value = MAX_HEIGHT; break;
      case GLX_MAX_PBUFFER_PIXELS: *value = MAX_WIDTH * MAX_HEIGHT; break;
      default: return GLX_BAD_ATTRIBUTE;
   }
   return Success;
}


static GLXFBConfig *
Fake_glXGetFBConfigs( Display *dpy, int screen, int *nelements )
{
   GLXFBConfig *result;
   int count = 0, i;
   if (nelements)
      *nelements = 0;
   if (!dpy || !nelements || screen < 0 || screen >= ScreenCount(dpy))
      return NULL;
   populate_fbconfigs(dpy, screen);
   result = (GLXFBConfig *)malloc(NumVisuals * sizeof(GLXFBConfig));
   if (!result)
      return NULL;
   for (i = 0; i < NumVisuals; ++i)
      if (VisualTable[i]->display == dpy && VisualTable[i]->visinfo->screen == screen)
         result[count++] = (GLXFBConfig)VisualTable[i];
   if (!count) {
      free(result);
      return NULL;
   }
   *nelements = count;
   return result;
}


static XVisualInfo *
Fake_glXGetVisualFromFBConfig( Display *dpy, GLXFBConfig config )
{
   XMesaVisual visual = config_resource(dpy, config);
   XVisualInfo *result;
   if (!visual)
      return NULL;
   result = (XVisualInfo *)malloc(sizeof(XVisualInfo));
   if (result) {
      *result = *visual->visinfo;
      remember_visual(dpy, result, visual);
   }
   return result;
}


static GLXWindow
Fake_glXCreateWindow( Display *dpy, GLXFBConfig config, Window win,
                      const int *attribList )
{
   XMesaVisual visual = config_resource(dpy, config);
   XWindowAttributes attributes;
   XMesaBuffer buffer;
   struct oldengl_drawable *resource;
   if (!visual || !win || (attribList && *attribList != None) ||
       !XGetWindowAttributes(dpy, win, &attributes) ||
       attributes.class != InputOutput ||
       XVisualIDFromVisual(attributes.visual) != visual->visinfo->visualid)
      return 0;
   buffer = XMesaCreateWindowBuffer(visual, win);
   if (!buffer)
      return 0;
   resource = register_drawable(dpy, visual, buffer, 1, GL_FALSE);
   if (!resource) {
      XMesaDestroyBuffer(buffer);
      return 0;
   }
   return resource->handle;
}


static void
Fake_glXDestroyWindow( Display *dpy, GLXWindow window )
{
   destroy_drawable(dpy, window, 1);
}


/* XXX untested */
static GLXPixmap
Fake_glXCreatePixmap( Display *dpy, GLXFBConfig config, Pixmap pixmap,
                      const int *attribList )
{
   XMesaVisual visual = config_resource(dpy, config);
   XMesaBuffer buffer;
   struct oldengl_drawable *resource;
   Window root;
   int x, y;
   unsigned int width, height, border, depth;
   if (!visual || !pixmap || (attribList && *attribList != None) ||
       !XGetGeometry(dpy, pixmap, &root, &x, &y, &width, &height, &border, &depth) ||
       depth != (unsigned int)visual->visinfo->depth)
      return 0;
   buffer = XMesaCreatePixmapBuffer(visual, pixmap, 0);
   if (!buffer)
      return 0;
   resource = register_drawable(dpy, visual, buffer, 2, GL_FALSE);
   if (!resource) {
      XMesaDestroyBuffer(buffer);
      return 0;
   }
   return resource->handle;
}


static void
Fake_glXDestroyPixmap( Display *dpy, GLXPixmap pixmap )
{
   destroy_drawable(dpy, pixmap, 2);
}


static GLXPbuffer
Fake_glXCreatePbuffer( Display *dpy, GLXFBConfig config,
                       const int *attribList )
{
   XMesaVisual visual = config_resource(dpy, config);
   const int *attribute;
   unsigned int width = 0, height = 0;
   GLboolean largest = GL_FALSE;
   XMesaBuffer buffer;
   Pixmap pixmap;
   struct oldengl_drawable *resource;
   if (!visual)
      return 0;
   for (attribute = attribList; attribute && *attribute != None; attribute += 2) {
      switch (attribute[0]) {
         case GLX_PBUFFER_WIDTH:
            if (attribute[1] < 0) return 0;
            width = (unsigned int)attribute[1]; break;
         case GLX_PBUFFER_HEIGHT:
            if (attribute[1] < 0) return 0;
            height = (unsigned int)attribute[1]; break;
         case GLX_LARGEST_PBUFFER: largest = attribute[1] ? GL_TRUE : GL_FALSE; break;
         case GLX_PRESERVED_CONTENTS: break; /* Pixmaps always retain contents. */
         default: return 0;
      }
   }
   if (!width || !height)
      return 0;
   if (width > MAX_WIDTH || height > MAX_HEIGHT) {
      if (!largest)
         return 0;
      width = MIN2(width, MAX_WIDTH);
      height = MIN2(height, MAX_HEIGHT);
   }
   pixmap = XCreatePixmap(dpy, RootWindow(dpy, visual->visinfo->screen),
                          width, height, visual->visinfo->depth);
   if (!pixmap)
      return 0;
   buffer = XMesaCreatePixmapBuffer(visual, pixmap, 0);
   if (!buffer) {
      XFreePixmap(dpy, pixmap);
      return 0;
   }
   buffer->pbuffer_flag = GL_TRUE;
   resource = register_drawable(dpy, visual, buffer, 3, GL_TRUE);
   if (!resource) {
      XMesaDestroyBuffer(buffer);
      XFreePixmap(dpy, pixmap);
      return 0;
   }
   resource->largest = largest;
   return resource->handle;
}


static void
Fake_glXDestroyPbuffer( Display *dpy, GLXPbuffer pbuf )
{
   destroy_drawable(dpy, pbuf, 3);
}


static void
Fake_glXQueryDrawable( Display *dpy, GLXDrawable draw, int attribute,
                       unsigned int *value )
{
   struct oldengl_drawable *resource = drawable_resource(dpy, draw);
   if (!resource || !value)
      return;
   switch (attribute) {
      case GLX_WIDTH: case GLX_HEIGHT: {
         Window root;
         int x, y;
         unsigned int width, height, border, depth;
         if (XGetGeometry(dpy, resource->buffer->frontbuffer, &root, &x, &y,
                          &width, &height, &border, &depth))
            *value = attribute == GLX_WIDTH ? width : height;
         break;
      }
      case GLX_PRESERVED_CONTENTS: *value = True; break;
      case GLX_LARGEST_PBUFFER: *value = resource->largest; break;
      case GLX_FBCONFIG_ID: *value = visual_config_id(resource->visual); break;
      default: break;
   }
}


static GLXContext
Fake_glXCreateNewContext( Display *dpy, GLXFBConfig config,
                          int renderType, GLXContext shareList, Bool direct )
{
   XMesaVisual visual = config_resource(dpy, config);
   if (!visual ||
       (renderType == GLX_RGBA_TYPE && !visual->mesa_visual.rgbMode) ||
       (renderType == GLX_COLOR_INDEX_TYPE && visual->mesa_visual.rgbMode) ||
       (renderType != GLX_RGBA_TYPE && renderType != GLX_COLOR_INDEX_TYPE))
      return NULL;
   return create_context_for_visual(dpy, visual, shareList, direct);
}


static int
Fake_glXQueryContext( Display *dpy, GLXContext ctx, int attribute, int *value )
{
   struct fake_glx_context *resource = context_resource(dpy, ctx);
   if (!resource || !value)
      return GLX_BAD_CONTEXT;
   switch (attribute) {
      case GLX_FBCONFIG_ID: *value = visual_config_id(resource->visual); break;
      case GLX_RENDER_TYPE:
         *value = resource->visual->mesa_visual.rgbMode ? GLX_RGBA_TYPE : GLX_COLOR_INDEX_TYPE; break;
      case GLX_SCREEN: *value = resource->visual->visinfo->screen; break;
      default: return GLX_BAD_ATTRIBUTE;
   }
   return Success;
}


static void
Fake_glXSelectEvent( Display *dpy, GLXDrawable drawable, unsigned long mask )
{
   struct oldengl_drawable *resource = drawable_resource(dpy, drawable);
   if (resource && !(mask & ~((unsigned long)GLX_PBUFFER_CLOBBER_MASK)))
      resource->event_mask = mask;
}


static void
Fake_glXGetSelectedEvent( Display *dpy, GLXDrawable drawable,
                          unsigned long *mask )
{
   struct oldengl_drawable *resource = drawable_resource(dpy, drawable);
   if (resource && mask)
      *mask = resource->event_mask;
}



/*** GLX_SGI_swap_control ***/

static int
Fake_glXSwapIntervalSGI(int interval)
{
   (void) interval;
   return 0;
}



/*** GLX_SGI_video_sync ***/

static int
Fake_glXGetVideoSyncSGI(unsigned int *count)
{
   (void) count;
   return 0;
}

static int
Fake_glXWaitVideoSyncSGI(int divisor, int remainder, unsigned int *count)
{
   (void) divisor;
   (void) remainder;
   (void) count;
   return 0;
}



/*** GLX_SGI_make_current_read ***/

static Bool
Fake_glXMakeCurrentReadSGI(Display *dpy, GLXDrawable draw, GLXDrawable read, GLXContext ctx)
{
   (void) dpy;
   (void) draw;
   (void) read;
   (void) ctx;
   return False;
}

/* not used
static GLXDrawable
Fake_glXGetCurrentReadDrawableSGI(void)
{
   return 0;
}
*/


/*** GLX_SGIX_video_source ***/
#if defined(_VL_H)

static GLXVideoSourceSGIX
Fake_glXCreateGLXVideoSourceSGIX(Display *dpy, int screen, VLServer server, VLPath path, int nodeClass, VLNode drainNode)
{
   (void) dpy;
   (void) screen;
   (void) server;
   (void) path;
   (void) nodeClass;
   (void) drainNode;
   return 0;
}

static void
Fake_glXDestroyGLXVideoSourceSGIX(Display *dpy, GLXVideoSourceSGIX src)
{
   (void) dpy;
   (void) src;
}

#endif


/*** GLX_EXT_import_context ***/

static void
Fake_glXFreeContextEXT(Display *dpy, GLXContext context)
{
   (void) dpy;
   (void) context;
}

static GLXContextID
Fake_glXGetContextIDEXT(const GLXContext context)
{
   (void) context;
   return 0;
}

static GLXContext
Fake_glXImportContextEXT(Display *dpy, GLXContextID contextID)
{
   (void) dpy;
   (void) contextID;
   return 0;
}

static int
Fake_glXQueryContextInfoEXT(Display *dpy, GLXContext context, int attribute, int *value)
{
   (void) dpy;
   (void) context;
   (void) attribute;
   (void) value;
   return 0;
}



/*** GLX_SGIX_fbconfig ***/

static int
Fake_glXGetFBConfigAttribSGIX(Display *dpy, GLXFBConfigSGIX config, int attribute, int *value)
{
   (void) dpy;
   (void) config;
   (void) attribute;
   (void) value;
   return 0;
}

static GLXFBConfigSGIX *
Fake_glXChooseFBConfigSGIX(Display *dpy, int screen, int *attrib_list, int *nelements)
{
   (void) dpy;
   (void) screen;
   (void) attrib_list;
   (void) nelements;
   return 0;
}

static GLXPixmap
Fake_glXCreateGLXPixmapWithConfigSGIX(Display *dpy, GLXFBConfigSGIX config, Pixmap pixmap)
{
   (void) dpy;
   (void) config;
   (void) pixmap;
   return 0;
}

static GLXContext
Fake_glXCreateContextWithConfigSGIX(Display *dpy, GLXFBConfigSGIX config, int render_type, GLXContext share_list, Bool direct)
{
   (void) dpy;
   (void) config;
   (void) render_type;
   (void) share_list;
   (void) direct;
   return 0;
}

static XVisualInfo *
Fake_glXGetVisualFromFBConfigSGIX(Display *dpy, GLXFBConfigSGIX config)
{
   (void) dpy;
   (void) config;
   return NULL;
}

static GLXFBConfigSGIX
Fake_glXGetFBConfigFromVisualSGIX(Display *dpy, XVisualInfo *vis)
{
   (void) dpy;
   (void) vis;
   return 0;
}



/*** GLX_SGIX_pbuffer ***/

static GLXPbufferSGIX
Fake_glXCreateGLXPbufferSGIX(Display *dpy, GLXFBConfigSGIX config, unsigned int width, unsigned int height, int *attrib_list)
{
   (void) dpy;
   (void) config;
   (void) width;
   (void) height;
   (void) attrib_list;
   return 0;
}

static void
Fake_glXDestroyGLXPbufferSGIX(Display *dpy, GLXPbufferSGIX pbuf)
{
   (void) dpy;
   (void) pbuf;
}

static int
Fake_glXQueryGLXPbufferSGIX(Display *dpy, GLXPbufferSGIX pbuf, int attribute, unsigned int *value)
{
   (void) dpy;
   (void) pbuf;
   (void) attribute;
   (void) value;
   return 0;
}

static void
Fake_glXSelectEventSGIX(Display *dpy, GLXDrawable drawable, unsigned long mask)
{
   (void) dpy;
   (void) drawable;
   (void) mask;
}

static void
Fake_glXGetSelectedEventSGIX(Display *dpy, GLXDrawable drawable, unsigned long *mask)
{
   (void) dpy;
   (void) drawable;
   (void) mask;
}



/*** GLX_SGI_cushion ***/

static void
Fake_glXCushionSGI(Display *dpy, Window win, float cushion)
{
   (void) dpy;
   (void) win;
   (void) cushion;
}



/*** GLX_SGIX_video_resize ***/

static int
Fake_glXBindChannelToWindowSGIX(Display *dpy, int screen, int channel , Window window)
{
   (void) dpy;
   (void) screen;
   (void) channel;
   (void) window;
   return 0;
}

static int
Fake_glXChannelRectSGIX(Display *dpy, int screen, int channel, int x, int y, int w, int h)
{
   (void) dpy;
   (void) screen;
   (void) channel;
   (void) x;
   (void) y;
   (void) w;
   (void) h;
   return 0;
}

static int
Fake_glXQueryChannelRectSGIX(Display *dpy, int screen, int channel, int *x, int *y, int *w, int *h)
{
   (void) dpy;
   (void) screen;
   (void) channel;
   (void) x;
   (void) y;
   (void) w;
   (void) h;
   return 0;
}

static int
Fake_glXQueryChannelDeltasSGIX(Display *dpy, int screen, int channel, int *dx, int *dy, int *dw, int *dh)
{
   (void) dpy;
   (void) screen;
   (void) channel;
   (void) dx;
   (void) dy;
   (void) dw;
   (void) dh;
   return 0;
}

static int
Fake_glXChannelRectSyncSGIX(Display *dpy, int screen, int channel, GLenum synctype)
{
   (void) dpy;
   (void) screen;
   (void) channel;
   (void) synctype;
   return 0;
}



/*** GLX_SGIX_dmbuffer **/

#if defined(_DM_BUFFER_H_)
static Bool
Fake_glXAssociateDMPbufferSGIX(Display *dpy, GLXPbufferSGIX pbuffer, DMparams *params, DMbuffer dmbuffer)
{
   (void) dpy;
   (void) pbuffer;
   (void) params;
   (void) dmbuffer;
   return False;
}
#endif


/*** GLX_SGIX_swap_group ***/

static void
Fake_glXJoinSwapGroupSGIX(Display *dpy, GLXDrawable drawable, GLXDrawable member)
{
   (void) dpy;
   (void) drawable;
   (void) member;
}



/*** GLX_SGIX_swap_barrier ***/

static void
Fake_glXBindSwapBarrierSGIX(Display *dpy, GLXDrawable drawable, int barrier)
{
   (void) dpy;
   (void) drawable;
   (void) barrier;
}

static Bool
Fake_glXQueryMaxSwapBarriersSGIX(Display *dpy, int screen, int *max)
{
   (void) dpy;
   (void) screen;
   (void) max;
   return False;
}



/*** GLX_SUN_get_transparent_index ***/

static Status
Fake_glXGetTransparentIndexSUN(Display *dpy, Window overlay, Window underlay, long *pTransparent)
{
   (void) dpy;
   (void) overlay;
   (void) underlay;
   (void) pTransparent;
   return 0;
}



/*** GLX_MESA_release_buffers ***/

/*
 * Release the depth, stencil, accum buffers attached to a GLXDrawable
 * (a window or pixmap) prior to destroying the GLXDrawable.
 */
static Bool
Fake_glXReleaseBuffersMESA( Display *dpy, GLXDrawable d )
{
   XMesaBuffer b = XMesaFindBuffer(dpy, d);
   if (b) {
      XMesaDestroyBuffer(b);
      return True;
   }
   return False;
}



/*** GLX_MESA_set_3dfx_mode ***/

static Bool
Fake_glXSet3DfxModeMESA( int mode )
{
   return XMesaSetFXmode( mode );
}



/*** AGP memory allocation ***/
static void *
Fake_glXAllocateMemoryNV( GLsizei size,
                          GLfloat readFrequency,
                          GLfloat writeFrequency,
                          GLfloat priority )
{
   (void) size;
   (void) readFrequency;
   (void) writeFrequency;
   (void) priority;
   return NULL;
}


static void 
Fake_glXFreeMemoryNV( GLvoid *pointer )
{
   (void) pointer;
}


static GLuint
Fake_glXGetAGPOffsetMESA( const GLvoid *pointer )
{
   return ~0;
}


extern struct _glxapi_table *_mesa_GetGLXDispatchTable(void);
struct _glxapi_table *_mesa_GetGLXDispatchTable(void)
{
   static struct _glxapi_table glx;

   /* be sure our dispatch table size <= libGL's table */
   {
      GLuint size = sizeof(struct _glxapi_table) / sizeof(void *);
      (void) size;
      assert(_glxapi_get_dispatch_table_size() >= size);
   }

   /* initialize the whole table to no-ops */
   _glxapi_set_no_op_table(&glx);

   /* now initialize the table with the functions I implement */
   glx.ChooseVisual = Fake_glXChooseVisual;
   glx.CopyContext = Fake_glXCopyContext;
   glx.CreateContext = Fake_glXCreateContext;
   glx.CreateGLXPixmap = Fake_glXCreateGLXPixmap;
   glx.DestroyContext = Fake_glXDestroyContext;
   glx.DestroyGLXPixmap = Fake_glXDestroyGLXPixmap;
   glx.GetConfig = Fake_glXGetConfig;
   /*glx.GetCurrentContext = Fake_glXGetCurrentContext;*/
   /*glx.GetCurrentDrawable = Fake_glXGetCurrentDrawable;*/
   glx.IsDirect = Fake_glXIsDirect;
   glx.MakeCurrent = Fake_glXMakeCurrent;
   glx.QueryExtension = Fake_glXQueryExtension;
   glx.QueryVersion = Fake_glXQueryVersion;
   glx.SwapBuffers = Fake_glXSwapBuffers;
   glx.UseXFont = Fake_glXUseXFont;
   glx.WaitGL = Fake_glXWaitGL;
   glx.WaitX = Fake_glXWaitX;

   /*** GLX_VERSION_1_1 ***/
   glx.GetClientString = Fake_glXGetClientString;
   glx.QueryExtensionsString = Fake_glXQueryExtensionsString;
   glx.QueryServerString = Fake_glXQueryServerString;

   /*** GLX_VERSION_1_2 ***/
   /*glx.GetCurrentDisplay = Fake_glXGetCurrentDisplay;*/

   /*** GLX_VERSION_1_3 ***/
   glx.ChooseFBConfig = Fake_glXChooseFBConfig;
   glx.CreateNewContext = Fake_glXCreateNewContext;
   glx.CreatePbuffer = Fake_glXCreatePbuffer;
   glx.CreatePixmap = Fake_glXCreatePixmap;
   glx.CreateWindow = Fake_glXCreateWindow;
   glx.DestroyPbuffer = Fake_glXDestroyPbuffer;
   glx.DestroyPixmap = Fake_glXDestroyPixmap;
   glx.DestroyWindow = Fake_glXDestroyWindow;
   /*glx.GetCurrentReadDrawable = Fake_glXGetCurrentReadDrawable;*/
   glx.GetFBConfigAttrib = Fake_glXGetFBConfigAttrib;
   glx.GetFBConfigs = Fake_glXGetFBConfigs;
   glx.GetSelectedEvent = Fake_glXGetSelectedEvent;
   glx.GetVisualFromFBConfig = Fake_glXGetVisualFromFBConfig;
   glx.MakeContextCurrent = Fake_glXMakeContextCurrent;
   glx.QueryContext = Fake_glXQueryContext;
   glx.QueryDrawable = Fake_glXQueryDrawable;
   glx.SelectEvent = Fake_glXSelectEvent;

   /*** GLX_SGI_swap_control ***/
   glx.SwapIntervalSGI = Fake_glXSwapIntervalSGI;

   /*** GLX_SGI_video_sync ***/
   glx.GetVideoSyncSGI = Fake_glXGetVideoSyncSGI;
   glx.WaitVideoSyncSGI = Fake_glXWaitVideoSyncSGI;

   /*** GLX_SGI_make_current_read ***/
   glx.MakeCurrentReadSGI = Fake_glXMakeCurrentReadSGI;
   /*glx.GetCurrentReadDrawableSGI = Fake_glXGetCurrentReadDrawableSGI;*/

/*** GLX_SGIX_video_source ***/
#if defined(_VL_H)
   glx.CreateGLXVideoSourceSGIX = Fake_glXCreateGLXVideoSourceSGIX;
   glx.DestroyGLXVideoSourceSGIX = Fake_glXDestroyGLXVideoSourceSGIX;
#endif

   /*** GLX_EXT_import_context ***/
   glx.FreeContextEXT = Fake_glXFreeContextEXT;
   glx.GetContextIDEXT = Fake_glXGetContextIDEXT;
   /*glx.GetCurrentDisplayEXT = Fake_glXGetCurrentDisplayEXT;*/
   glx.ImportContextEXT = Fake_glXImportContextEXT;
   glx.QueryContextInfoEXT = Fake_glXQueryContextInfoEXT;

   /*** GLX_SGIX_fbconfig ***/
   glx.GetFBConfigAttribSGIX = Fake_glXGetFBConfigAttribSGIX;
   glx.ChooseFBConfigSGIX = Fake_glXChooseFBConfigSGIX;
   glx.CreateGLXPixmapWithConfigSGIX = Fake_glXCreateGLXPixmapWithConfigSGIX;
   glx.CreateContextWithConfigSGIX = Fake_glXCreateContextWithConfigSGIX;
   glx.GetVisualFromFBConfigSGIX = Fake_glXGetVisualFromFBConfigSGIX;
   glx.GetFBConfigFromVisualSGIX = Fake_glXGetFBConfigFromVisualSGIX;

   /*** GLX_SGIX_pbuffer ***/
   glx.CreateGLXPbufferSGIX = Fake_glXCreateGLXPbufferSGIX;
   glx.DestroyGLXPbufferSGIX = Fake_glXDestroyGLXPbufferSGIX;
   glx.QueryGLXPbufferSGIX = Fake_glXQueryGLXPbufferSGIX;
   glx.SelectEventSGIX = Fake_glXSelectEventSGIX;
   glx.GetSelectedEventSGIX = Fake_glXGetSelectedEventSGIX;

   /*** GLX_SGI_cushion ***/
   glx.CushionSGI = Fake_glXCushionSGI;

   /*** GLX_SGIX_video_resize ***/
   glx.BindChannelToWindowSGIX = Fake_glXBindChannelToWindowSGIX;
   glx.ChannelRectSGIX = Fake_glXChannelRectSGIX;
   glx.QueryChannelRectSGIX = Fake_glXQueryChannelRectSGIX;
   glx.QueryChannelDeltasSGIX = Fake_glXQueryChannelDeltasSGIX;
   glx.ChannelRectSyncSGIX = Fake_glXChannelRectSyncSGIX;

   /*** GLX_SGIX_dmbuffer **/
#if defined(_DM_BUFFER_H_)
   glx.AssociateDMPbufferSGIX = NULL;
#endif

   /*** GLX_SGIX_swap_group ***/
   glx.JoinSwapGroupSGIX = Fake_glXJoinSwapGroupSGIX;

   /*** GLX_SGIX_swap_barrier ***/
   glx.BindSwapBarrierSGIX = Fake_glXBindSwapBarrierSGIX;
   glx.QueryMaxSwapBarriersSGIX = Fake_glXQueryMaxSwapBarriersSGIX;

   /*** GLX_SUN_get_transparent_index ***/
   glx.GetTransparentIndexSUN = Fake_glXGetTransparentIndexSUN;

   /*** GLX_MESA_copy_sub_buffer ***/
   glx.CopySubBufferMESA = Fake_glXCopySubBufferMESA;

   /*** GLX_MESA_release_buffers ***/
   glx.ReleaseBuffersMESA = Fake_glXReleaseBuffersMESA;

   /*** GLX_MESA_pixmap_colormap ***/
   glx.CreateGLXPixmapMESA = Fake_glXCreateGLXPixmapMESA;

   /*** GLX_MESA_set_3dfx_mode ***/
   glx.Set3DfxModeMESA = Fake_glXSet3DfxModeMESA;

   /*** GLX_NV_vertex_array_range ***/
   glx.AllocateMemoryNV = Fake_glXAllocateMemoryNV;
   glx.FreeMemoryNV = Fake_glXFreeMemoryNV;

   /*** GLX_MESA_agp_offset ***/
   glx.GetAGPOffsetMESA = Fake_glXGetAGPOffsetMESA;

   return &glx;
}
