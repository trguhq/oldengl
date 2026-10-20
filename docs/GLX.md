# Client-side GLX binding

The library exports the 39 core GLX entry points through GLX 1.4. Public
headers are `GL/glx.h`, `GL/glxtokens.h`, and `GL/glxext.h`. The implementation
uses Mesa's XMesa renderer plus Xlib; it works without a server GLX extension.
It is a direct client-side binding, not an indirect GLX protocol encoder.
Pass `True` when creating contexts; explicit indirect requests fail.

Visual and FBConfig selection expose RGB/color-index modes where the X
visual supports them, software depth/stencil/accumulation, and single or
double buffering. GLX windows and pixmaps have handles separate from the
application's X resource. Pbuffers use retained X pixmaps. Contexts preserve
GL state across drawable changes; shared contexts share textures and lists.
Separate draw/read bindings feed their respective Mesa framebuffers.

Core operations include context copying, X font bitmap display lists,
current-context/display/draw/read queries, wait/synchronization, FBConfig
attributes, drawable sizes and pbuffer flags, event-mask selection/query,
and GL/GLX procedure lookup. Context destruction is deferred while current;
drawable wrapper destruction is deferred while bound. Destroying a wrapper
does not destroy an application-owned X window or pixmap. Pbuffer pixmaps
are library-owned and released with their wrappers.

The binding advertises only the implemented extension set. It does not
provide cross-process context import, indirect contexts, video channels,+swap groups, stereo buffers, or vertical synchronization. The current build
is single-threaded. Keep the display open until contexts and drawables are
released. Native XGL failure leaves the software rendering path available.

Build `make xgl` on Solaris with the XGL SDK to obtain `libGL.a`/`.so` with
native dispatch, or `make glx-software` with an X11 SDK for `libGLsoft`.
Do not link a second GL implementation into the application.

`tests/test_glx.c` runs against a real X server. It exercises window/front/
back readback and swap, split draw/read surfaces, sharing, context state
copying, X fonts, legacy APIs, FBConfigs, pixmaps, pbuffers, event masks,
queries, procedure lookup, and deferred destruction. `check_glx_api.py`
audits the 39 core declarations and exports separately from behavior.
