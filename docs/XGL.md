The original `XGLMesa*` binding described below remains available. The new
GLX binding and native primitive/raster dispatch are documented in
[GLX.md](GLX.md) and [ACCELERATION.md](ACCELERATION.md).

# XGL binding and software rendering

`GL/xglmesa.h` supplies a window-system binding for the bundled Mesa rendering
core. Applications use the ordinary `gl*` functions from `GL/gl.h`. The core
implements transformation, clipping, lighting, texturing, rasterization,
depth/stencil/alpha tests, blending, logic operations, accumulation, evaluators,
feedback, selection, display lists, vertex arrays, and pixel transfer in
software. XGL copies the completed RGB image into the application's X window.
No OpenGL primitive is approximated by an XGL primitive with different rules.

This is an initial correctness-oriented backend. It does not accelerate GL
geometry with XGL and is not a GLX implementation. Existing applications that
create GLX contexts must adapt their window-system calls to `XGLMesa*`; their GL
rendering calls can stay unchanged. GLU and GLUT are separate libraries.

## Context and presentation contract

Create an `InputOutput` X window with a `TrueColor` or `DirectColor` visual, then
call `XGLMesaCreateContext(display, window, share)` and
`XGLMesaMakeCurrent(context)`. A non-null `share` shares Mesa display-list and
texture-object namespaces. The application owns the X display, window, and any
non-default colormap. Keep them alive until the context is destroyed.

Contexts are RGBA, single buffered, and non-stereo, with 8-bit color channels,
a requested 24-bit software depth buffer, 8-bit stencil, and 16-bit accumulation
components. Query the actual formats with `glGetIntegerv`. The rendering core's
compiled image-size limits apply. Window resizing preserves viewport and
scissor state; update them explicitly if desired. Pseudocolor visuals, color
index window contexts, and hardware double buffering are not exposed by this
binding. The software renderer's offscreen API remains available separately.

`glFlush`, `glFinish`, and `XGLMesaSwapBuffers` all present the single color
buffer. The last name is a convenience for render loops: it copies pixels and
does not exchange front and back buffers. `GL_DOUBLEBUFFER` reports false;
applications should draw to `GL_FRONT`. The software framebuffer remains
available for readback and Expose repair. Call `XGLMesaSwapBuffers` on Expose,
and call `XGLMesaResizeBuffers` after ConfigureNotify before rendering the new
frame. Presentation requires the context to be current.

`XGLMesaMakeCurrent` flushes pending rendering in the old context before changing
the binding. This completes buffered vertices against their original framebuffer
before it can be resized or destroyed while noncurrent.
`XGLMesaMakeCurrent(NULL)` releases the current context. Destroying the current
context finishes its rendering and releases it. Binding operations are not allowed inside
`glBegin`/`glEnd`. The API is single-threaded: serialize context operations and
GL calls on the calling thread. Do not mix independently created XGL sessions
with this library's session: the XGL specification allows only one System State
object. Multiple `XGLMesaContext` instances share a reference-counted session.

`XGLMesaGetColorBuffer` returns read-only RGBA bytes with the bottom row first.
Finish rendering before inspecting these bytes. Resize and destruction invalidate
the pointer. `XGLMesaGetLastError` reports binding and XGL failures separately
from OpenGL's `glGetError`; the returned string belongs to the library. Xlib's
normal error handler still handles invalid X resources and X protocol errors.

## Pixel layout and synchronization

The XGL source raster is 32 bits deep with `XGL_DEV_COLOR_TYPE` set to
`XGL_COLOR_RGB` as its first creation attribute. XGL allocates and owns its
storage; the library gets the address using
`XGL_MEM_RAS_IMAGE_BUFFER_ADDR`. It does not substitute an application-owned
buffer or assume that an X window's native pixel layout matches RGBA.

The supplied manuals give inconsistent wording about RGB component order: the
reference's memory-raster entry describes an unused upper byte, while the
programmer's guide's KCMS example describes a different bit assignment. To
avoid choosing an unverified interpretation, the backend writes red, green,
and blue test pixels with `xgl_context_set_pixel` into a temporary context
attached to the source memory raster, synchronizes, then reads the resulting
32-bit words. It verifies that three distinct byte-aligned channels exist and
uses those shifts for conversion. Unexpected layouts cause a reported failure.

The conversion uses 32-bit integer shifts and stores words, as required by the
reference manual; it does not depend on the host byte order. It flips rows from
OpenGL's lower-left origin to XGL's upper-left raster origin and discards alpha
only during presentation. Alpha remains in the software buffer for GL tests,
blending, and readback. XGL handles conversion from its logical RGB raster to
the window device's format.

The copy uses `xgl_context_copy_buffer(copy_context, NULL, NULL, memory_raster)`.
The destination context specifies `XGL_ROP_COPY` and `XGL_RAS_FILL_COPY`.
`XSync(display, False)` runs before XGL touches the window, and
`xgl_context_flush(..., XGL_FLUSH_SYNCHRONIZE)` completes the copy before source
memory is modified or released. These synchronization points also make
`glFinish` wait for presentation. A single-buffer copy can tear; there is no
vertical synchronization guarantee.

## Source references

The attached manuals are used as API references; their text is not redistributed
in the source tree. Printed page numbers below refer to the supplied editions.

| Manual | Relevant entries |
| --- | --- |
| XGL Programmer's Guide, August 1997, 802-5865 | Hello XGL example, pp. 31–34 (`Xgl_X_window`, window descriptor, `XGL_WIN_RAS`, `XGL_2D_CTX`); memory raster attributes, pp. 64–65; memory-raster RGB examples, pp. 153–155 and 455–456 |
| XGL Reference Manual, 802-5864 | `XGL_DEV_COLOR_MAP`, p. 281 (default color-map object); `XGL_DEV_COLOR_TYPE`, pp. 282–283 (creation attribute ordering); `XGL_MEM_RAS`, pp. 331–332; `XGL_MEM_RAS_IMAGE_BUFFER_ADDR`, pp. 333–334 (allocation, row layout, word access); `XGL_RAS_DEPTH`, pp. 350–351 |
| XGL Reference Manual, 802-5864 | `xgl_close`, p. 410 (one System State); `xgl_context_copy_buffer`, pp. 417–420 (RGB memory-to-RGB window copy); `xgl_context_flush`, pp. 422–423 (synchronization and Xlib interoperation); `xgl_context_set_pixel`, pp. 434–435; System State error attributes, pp. 364–369 |
| OpenGL 1.1 specification, March 4, 1997 | GL rendering and state semantics; Mesa supplies the implementation rather than replacing unsupported functionality with empty functions |

## Validation boundary

Portable software rendering and mock XGL tests can run without a Sun system.
Mocks exercise the API calls, context lifecycle, error paths, row orientation,
channel packing, and synchronization contract; they do not establish compatibility
with Sun's binary SDK or drivers. Compile against the actual Solaris 2.6 XGL
headers and run the window example on SPARC before claiming target-platform
validation. In particular, confirm RGB calibration, Expose repair,
ConfigureNotify resizing, readback, context switching, and the real display's
color conversion. No OpenGL conformance certification is claimed.
