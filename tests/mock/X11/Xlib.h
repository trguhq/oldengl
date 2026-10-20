/* Test fixture only: this is not a replacement for the Solaris Xlib SDK. */
#ifndef OLDENGL_TEST_XLIB_H
#define OLDENGL_TEST_XLIB_H

typedef struct oldengl_mock_display Display;
typedef unsigned long XID;
typedef XID Window;
typedef unsigned long VisualID;
typedef int Bool;
typedef int Status;

typedef struct {
    unsigned long visualid;
    int class;
    unsigned long red_mask, green_mask, blue_mask;
    int bits_per_rgb, map_entries;
} Visual;

typedef struct oldengl_mock_screen Screen;
typedef struct {
    int x, y, width, height, border_width, depth;
    Visual *visual;
    Window root;
    int class;
    int bit_gravity, win_gravity, backing_store;
    unsigned long backing_planes, backing_pixel;
    Bool save_under;
    unsigned long colormap;
    Bool map_installed;
    int map_state;
    long all_event_masks, your_event_mask, do_not_propagate_mask;
    Bool override_redirect;
    Screen *screen;
} XWindowAttributes;

#define False 0
#define True 1
#define None 0L
#define InputOutput 1
#define TrueColor 4
#define DirectColor 5
#define DefaultScreen(dpy) XDefaultScreen(dpy)

int XGetWindowAttributes(Display *, Window, XWindowAttributes *);
int XSync(Display *, Bool);
int XFlush(Display *);
int XDefaultScreen(Display *);
int XScreenNumberOfScreen(Screen *);

#endif
