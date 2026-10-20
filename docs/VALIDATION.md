# Validation

## Development-host checks

The current development host is x86-64 Linux with GCC 14.2. The native Sun
XGL SDK, Solaris 2.6 compiler/runtime, and SPARC hardware are unavailable.
The following results distinguish software behavior from native XGL execution.

| Check | Coverage |
| --- | --- |
| `make software CFLAGS='-O2 -fPIC -std=c89'` | Static and shared portable renderer libraries; no system GL dependency |
| `./tests/test_gl` | 93 assertions in 13 behavior groups, using real offscreen rendering |
| `./tests/test_xgl` | 141 assertions using the real renderer and test-only XGL/Xlib fixtures |
| `make test-accel` | 29 checks of native raster dispatch and software fallback using a synthetic SDK |
| `make test-tnl` | 97 checks of object-space primitive expansion, raw attribute import and pre-transform stage dispatch |
| `make test-texture` | 40 strict-C89 checks of native texture eligibility, image upload, sampling state and coordinate conversion |
| `make test-glx` with Xvfb | 91 assertions using real Xlib windows and the software GLX binding |
| `python3 tests/check_glx_api.py lib/libGLsoft.a` | All 39 GLX 1.0–1.4 functions declared and defined |
| `python3 tests/check_api.py lib/liboldengl.a` | All 336 OpenGL 1.1 functions declared and defined |
| Same API audit with `lib/liboldengl.so` | All 336 functions in the shared library |
| Strict C89 adapter/test compilation | `-std=c89 -Wall -Wextra -pedantic` |
| Adapter, conversion, fixture and integration-test sanitizers | AddressSanitizer and UndefinedBehaviorSanitizer; renderer archive is not instrumented |
| SPARC preprocessor configurations | Sun `__sparc`, `__sparc__`, and `sparc` spellings select big-endian packing and C89 float-math fallbacks |

These are regression checks, not official OpenGL conformance certification.
An API export audit proves coverage of names, not every possible GL behavior.
The public headers are a superset from Mesa 4.0.4; the audit counts only the
336 core 1.1 functions.

The GL behavior groups cover:

- State queries, error reporting, matrix and attribute stacks.
- Transformations, user clipping, homogeneous clipping, and triangle readback.
- Depth, stencil, alpha tests, blending, scissor, and channel masks.
- Accumulation operations, logic operations, and polygon stippling.
- 1D/2D texture upload/readback, object lifetime, subimages, copying, and mipmaps.
- Display lists, vertex arrays, interleaved arrays, and evaluators.
- Selection hit/name records and feedback data.
- Pixel drawing, bitmap rendering, copying and packing.
- Lighting and fog, shared texture/list objects, and color-index rendering.

XGL integration tests use deliberately synthetic attribute numbers and a
minimal fixture API derived from the manuals. They verify calls, lifecycle
and failure handling; they cannot verify Sun SDK constants, ABI, driver
semantics or actual device behavior. They cover two RGB channel layouts,
row inversion, flush/finish presentation, pending primitives on a context
switch, sharing, resizing and rollback, invalid calls, and resource cleanup.

## Target-machine checks before deployment

1. Build `make xgl` and `make example` using the Solaris compiler/linker
   variables in the root README. This is the first actual SDK/ABI check.
   The output must be a 32-bit SPARC library; a library built on Linux is
   for the development host and must not be copied to Solaris as a binary.
2. Build and run `tests/test_gl` with the same native compiler settings.
   It runs without an X server. All behavioral groups must pass. If GNU
   `nm` and Python 3 are available, run the API audit too; Solaris's native
   `nm` table format is not supported by that helper.
3. Run `examples/triangle` on the target XGL display. It selects a 24-bit
   TrueColor visual. Verify the red, green and blue vertices, resize the
   window, cover and uncover it, and confirm Expose repairs the image.
   Close using a key or the window manager. Binding failures print a
   specific error from `XGLMesaGetLastError`.
4. Exercise two application windows and shared contexts. Confirm that
   `glFlush`/`glFinish`, switching contexts, and destroying the current
   context finish queued rendering. Read back pixels with `glReadPixels`
   and compare with the software image returned by `XGLMesaGetColorBuffer`
   after `glFinish`.
5. Confirm actual framebuffer depths using GL queries, expected XGL RGB
   calibration and color conversion on the selected display, and clean
   termination/recreation of the shared XGL session. Repeat with the
   relevant target graphics adapter and XGL driver, including remote
   displays if the application needs them.

These single-buffer restrictions apply to the original `XGLMesa*`
convenience binding: `GL_DOUBLEBUFFER` is false and `GL_BACK` is invalid.
The GLX binding separately supports single/double buffering and eligible
native front/back window rendering. Neither binding provides stereo or
vertical synchronization; native color-index acceleration is not claimed.

For GLX deployment, also run `examples/glx_triangle` and
`examples/glx_textured_cube --frames 120` on the target display. Compare
with `OLDENGL_NO_XGL=1`, inspect transformed/clipped geometry, texture
orientation and filtering, lighting, depth readback, and alternating native
and software draws. Repeat with GL_FRONT/GL_BACK, resizing, shared texture
updates, and each target graphics adapter. Verify the XGL driver actually
accelerates the desired operations; host fixture results cannot establish it.

## Local corrections to the historical renderer

The original source provenance, hashes, and an exact patch are kept in
`vendor/mesa/README.vendor`, `UPSTREAM-SHA256`, and `LOCAL-PATCHES.diff`.
The changes fix texture-name object detection, color-index ancillary
buffer selection, SPARC byte order/C89 math, Sun diagnostic compatibility,
and portable extension-field offsets. The behavioral suite includes the
texture-name and color-index regressions. No upstream checksum was changed
to hide a local correction.

## GLX and native dispatch additions

The client GLX binding is tested on a real Xvfb X server, separately from
the native XGL SDK. Its checks include front/back readback and swap, separate
draw/read surfaces, sharing, fonts, drawable lifecycle, context enable-state
copying, pixmaps and pbuffers. The static/shared GLX libraries export the
39 core GLX 1.0–1.4 functions.

`tests/test_glx_accel.c` adds integration through the actual native XMesa
hooks, real Xlib windows, and a synthetic XGL runtime. The fixture implements
model/view and projection transforms, homogeneous clipping, compatible
ambient/diffuse lighting, perspective texture sampling, Z comparison and
copies, and multiple window buffers so mixed-rendering results can be checked. This is distinct from
merely calling native adapter helpers in isolation. The expanded suite passes
88 checks, including texture sampling, mipmaps, updates, perspective correction,
and software/native depth-equality transitions.

The native GLX library compiles against test SDK declarations on the host.
Those declarations deliberately use synthetic constants and cannot establish
a build against Sun's actual headers or libraries. An Xlib-backed synthetic
rasterizer likewise cannot establish Sun raster coverage, precision or
hardware execution. `examples/glx_textured_cube.c` was also compiled in
strict C89 mode and completed a bounded software GLX smoke run under Xvfb.

On Solaris, build against the real XGL SDK and run both the GLX integration
checks and application-specific accelerated scenes. Compare native results
with `OLDENGL_NO_XGL=1`, then inspect actual XGL driver/device behavior.
See [ACCELERATION.md](ACCELERATION.md) for exact native eligibility and
remaining software features.
