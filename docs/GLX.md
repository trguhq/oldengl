# Client-side GLX binding

The library exports the 39 core GLX entry points through GLX 1.4. Public
headers are `GL/glx.h`, `GL/glxtokens.h`, and `GL/glxext.h`. The implementation
uses Mesa's XMesa renderer plus Xlib; it works without a server GLX extension.
It is a direct client-side binding, not an indirect GLX protocol encoder.
Pass `True` when creating contexts; explicit indirect requests fail.

Visual and FBConfig selection expose RGB/color-index modes where the X
visual supports them, depth/stencil/accumulation storage, and single or
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
provide cross-process context import, indirect contexts, video channels,
swap groups, stereo buffers, or vertical synchronization. The current build
is single-threaded. Keep the display open until contexts and drawables are
released. Native XGL failure leaves the software rendering path available.

Build with plain `make` (or `make xgl`) on Solaris with the XGL SDK to obtain
`libGL.a`/`.so` with native dispatch enabled by default. `make install` installs that build.
Use explicit `make glx-software` with an X11 SDK for `libGLsoft`.
Use `make install-software` for the separate offscreen renderer installation.
Do not link a second GL implementation into the application.

Native XGL window rendering accepts object-space vertices before Mesa's
transform and lighting stages. Eligible geometry uses XGL transformations,
clipping, ambient/diffuse lighting, depth testing and per-pixel texture
mapping. Ordinary OpenGL texture objects, uploads, subimages, sharing and
texture coordinates work through this GLX binding; unsupported texture modes
use Mesa. See [ACCELERATION.md](ACCELERATION.md) for the precise native subset.

For `GL_BACK`, the native path requests a second XGL window buffer and
transfers retained XMesa color and depth data around each native batch.
`glXSwapBuffers` presents the retained back image through XMesa. This preserves
software fallback and readback; it does not provide synchronized native buffer
flipping. Pixmaps, pbuffers, alpha-buffer visuals and unsupported state remain
software rendered. `OLDENGL_NO_XGL=1` provides a software comparison path.

`examples/glx_textured_cube.c` combines perspective, lighting, texturing,
depth testing and double buffering. Run with `--frames N` to exit after a
specified number of frames.

`tests/test_glx.c` runs against a real X server. It exercises window/front/
back readback and swap, split draw/read surfaces, sharing, context state
copying, X fonts, legacy APIs, FBConfigs, pixmaps, pbuffers, event masks,
queries, procedure lookup, and deferred destruction. `check_glx_api.py`
audits the 39 core declarations and exports separately from behavior.

`tests/test_glx_accel.c` separately exercises the actual native XMesa hooks
against real Xlib windows and a synthetic XGL implementation. It verifies
native dispatch and exchanges with software rendering, but cannot establish
Sun SDK ABI compatibility or execution on a graphics processor.
