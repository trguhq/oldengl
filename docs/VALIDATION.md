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

The selected GL window configuration is single-buffered and non-stereo;
`GL_DOUBLEBUFFER` is false. Requests for `GL_BACK` should produce the
appropriate GL error. No front/back exchange, vertical synchronization,
native color-index window, GLX ABI or hardware geometry acceleration is
claimed.

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
the native XGL SDK. `make test-glx` passes 91 assertions, including
framebuffers, separate draw/read, sharing, fonts, drawable lifecycle and
context enable-state copying. The static/shared GLX libraries export all
39 core GLX 1.0–1.4 functions. `make test-accel` passes 29 synthetic SDK
checks for native primitive/raster submission, unsupported-state fallback,
shared-buffer software blending/readback, culling and cleanup.

The native GLX build compiles against test SDK declarations on the host;
this does not establish a build against Sun's actual headers or libraries.
On Solaris, run both the GLX integration tests and application-specific
accelerated scenes, compare against `OLDENGL_NO_XGL=1`, and check actual
XGL driver/device behavior. See ACCELERATION.md for the exact eligible
front-buffer operations; vertex transformation/lighting and back-buffer
rendering remain software.
